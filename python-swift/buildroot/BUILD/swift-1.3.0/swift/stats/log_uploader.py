# Copyright (c) 2010-2011 OpenStack, LLC.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
# implied.
# See the License for the specific language governing permissions and
# limitations under the License.

from __future__ import with_statement
import os
import hashlib
import time
import gzip
import re
from paste.deploy import appconfig

from swift.common.internal_proxy import InternalProxy
from swift.common.daemon import Daemon
from swift.common import utils


class LogUploader(Daemon):
    '''
    Given a local directory, a swift account, and a container name, LogParser
    will upload all files in the local directory to the given account/
    container.  All but the newest files will be uploaded, and the files' md5
    sum will be computed. The hash is used to prevent duplicate data from
    being uploaded multiple times in different files (ex: log lines). Since
    the hash is computed, it is also used as the uploaded object's etag to
    ensure data integrity.

    Note that after the file is successfully uploaded, it will be unlinked.

    The given proxy server config is used to instantiate a proxy server for
    the object uploads.
    '''

    def __init__(self, uploader_conf, plugin_name):
        super(LogUploader, self).__init__(uploader_conf)
        log_name = '%s-log-uploader' % plugin_name
        self.logger = utils.get_logger(uploader_conf, log_name,
                                       log_route=plugin_name)
        self.log_dir = uploader_conf.get('log_dir', '/var/log/swift/')
        self.swift_account = uploader_conf['swift_account']
        self.container_name = uploader_conf['container_name']
        proxy_server_conf_loc = uploader_conf.get('proxy_server_conf',
                                            '/etc/swift/proxy-server.conf')
        proxy_server_conf = appconfig('config:%s' % proxy_server_conf_loc,
                                      name='proxy-server')
        self.internal_proxy = InternalProxy(proxy_server_conf)
        self.new_log_cutoff = int(uploader_conf.get('new_log_cutoff', '7200'))
        self.unlink_log = uploader_conf.get('unlink_log', 'True').lower() in \
                utils.TRUE_VALUES

        # source_filename_format is deprecated
        source_filename_format = uploader_conf.get('source_filename_format')
        source_filename_pattern = uploader_conf.get('source_filename_pattern')
        if source_filename_format and not source_filename_pattern:
            self.logger.warning(_('source_filename_format is unreliable and '
                                  'deprecated; use source_filename_pattern'))
            self.pattern = self.convert_glob_to_regex(source_filename_format)
        else:
            self.pattern = source_filename_pattern or '%Y%m%d%H'

    def run_once(self, *args, **kwargs):
        self.logger.info(_("Uploading logs"))
        start = time.time()
        self.upload_all_logs()
        self.logger.info(_("Uploading logs complete (%0.2f minutes)") %
            ((time.time() - start) / 60))

    def convert_glob_to_regex(self, glob):
        """
        Make a best effort to support old style config globs

        :param : old style config source_filename_format

        :returns : new style config source_filename_pattern
        """
        pattern = glob
        pattern = pattern.replace('.', r'\.')
        pattern = pattern.replace('*', r'.*')
        pattern = pattern.replace('?', r'.?')
        return pattern

    def validate_filename_pattern(self):
        """
        Validate source_filename_pattern

        :returns : valid regex pattern based on soruce_filename_pattern with
                   group matches substituded for date fmt markers
        """
        pattern = self.pattern
        markers = {
            '%Y': ('year', '(?P<year>[0-9]{4})'),
            '%m': ('month', '(?P<month>[0-1][0-9])'),
            '%d': ('day', '(?P<day>[0-3][0-9])'),
            '%H': ('hour', '(?P<hour>[0-2][0-9])'),
        }
        for marker, (mtype, group) in markers.items():
            if marker not in self.pattern:
                self.logger.error(_('source_filename_pattern much contain a '
                                    'marker %(marker)s to match the '
                                    '%(mtype)s') % {'marker': marker,
                                                   'mtype': mtype})
                return
            pattern = pattern.replace(marker, group)
        return pattern

    def get_relpath_to_files_under_log_dir(self):
        """
        Look under log_dir recursively and return all filenames as relpaths

        :returns : list of strs, the relpath to all filenames under log_dir
        """
        all_files = []
        for path, dirs, files in os.walk(self.log_dir):
            all_files.extend(os.path.join(path, f) for f in files)
        return [os.path.relpath(f, start=self.log_dir) for f in all_files]

    def filter_files(self, all_files, pattern):
        """
        Filter files based on regex pattern

        :param all_files: list of strs, relpath of the filenames under log_dir
        :param pattern: regex pattern to match against filenames

        :returns : dict mapping full path of file to match group dict
        """
        filename2match = {}
        found_match = False
        for filename in all_files:
            match = re.match(pattern, filename)
            if match:
                found_match = True
                full_path = os.path.join(self.log_dir, filename)
                filename2match[full_path] = match.groupdict()
            else:
                self.logger.debug(_('%(filename)s does not match '
                                    '%(pattern)s') % {'filename': filename,
                                                      'pattern': pattern})
        return filename2match

    def upload_all_logs(self):
        """
        Match files under log_dir to source_filename_pattern and upload to swift
        """
        pattern = self.validate_filename_pattern()
        if not pattern:
            self.logger.error(_('Invalid filename_format'))
            return
        all_files = self.get_relpath_to_files_under_log_dir()
        filename2match = self.filter_files(all_files, pattern)
        if not filename2match:
            self.logger.info(_('No files in %(log_dir)s match %(pattern)s') %
                             {'log_dir': self.log_dir, 'pattern': pattern})
            return
        if not self.internal_proxy.create_container(self.swift_account,
                                                    self.container_name):
            self.logger.error(_('Unable to create container for '
                                '%(account)s/%(container)s') % {
                                    'account': self.swift_account,
                                    'container': self.container_name})
            return
        for filename, match in filename2match.items():
            # don't process very new logs
            seconds_since_mtime = time.time() - os.stat(filename).st_mtime
            if seconds_since_mtime < self.new_log_cutoff:
                self.logger.debug(_("Skipping log: %(file)s "
                                    "(< %(cutoff)d seconds old)") % {
                                        'file': filename,
                                        'cutoff': self.new_log_cutoff})
                continue
            self.upload_one_log(filename, **match)

    def upload_one_log(self, filename, year, month, day, hour):
        """
        Upload one file to swift
        """
        if os.path.getsize(filename) == 0:
            self.logger.debug(_("Log %s is 0 length, skipping") % filename)
            return
        self.logger.debug(_("Processing log: %s") % filename)
        filehash = hashlib.md5()
        already_compressed = True if filename.endswith('.gz') else False
        opener = gzip.open if already_compressed else open
        f = opener(filename, 'rb')
        try:
            for line in f:
                # filter out bad lines here?
                filehash.update(line)
        finally:
            f.close()
        filehash = filehash.hexdigest()
        # By adding a hash to the filename, we ensure that uploaded files
        # have unique filenames and protect against uploading one file
        # more than one time. By using md5, we get an etag for free.
        target_filename = '/'.join([year, month, day, hour, filehash + '.gz'])
        if self.internal_proxy.upload_file(filename,
                                          self.swift_account,
                                          self.container_name,
                                          target_filename,
                                          compress=(not already_compressed)):
            self.logger.debug(_("Uploaded log %(file)s to %(target)s") %
                {'file': filename, 'target': target_filename})
            if self.unlink_log:
                os.unlink(filename)
        else:
            self.logger.error(_("ERROR: Upload of log %s failed!") % filename)
