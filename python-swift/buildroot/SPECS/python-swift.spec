%{!?python_sitelib: %global python_sitelib %(%{__python} -c "from distutils.sysconfig import get_python_lib; print get_python_lib()")}
%{!?pyver: %global pyver %(%{__python} -c "import sys ; print sys.version[:3]")}

Name:		python-swift
Version:	1.3.0
Release:	1%{?dist}
Summary:	Highly available, distributed, eventually consistent object/blob store	

Group:		Development/Languages
License:	ASL 2.0	
URL:		https://launchpad.net/swift
Source0:	http://launchpad.net/swift/1.3/%{version}/+download/swift-%{version}.tar.gz
BuildRoot:	%{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

BuildArch:	noarch

BuildRequires:	python-devel, python-setuptools
BuildRequires:	python-eventlet, python-webob, python-simplejson
BuildRequires:	python-sphinx, python-nose, python-paste-deploy
BuildRequires:	pyxattr
BuildRequires:	python-netifaces

Requires:	python >= 2.6
Requires:	rsync >= 3.0
Requires:	python-eventlet, python-webob, python-simplejson
Requires:	python-sphinx, python-nose, python-paste-deploy
Requires:	pyxattr
Requires:	python-netifaces

%description
Swift is a highly available, distributed, eventually consistent object/blob store

%prep
%setup -q -n swift-%{version}

# build the docs
mkdir doc/build
%{__python} setup.py build_sphinx

%check
%{__python} setup.py test

%build
%{__python} setup.py build

%install
rm -rf $RPM_BUILD_ROOT

%{__python} setup.py install -O1 --skip-build --root $RPM_BUILD_ROOT

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root,-)
%doc README PKG-INFO
%doc doc/build/html
%{_bindir}/swift-*
%{_bindir}/swauth-*
%{_bindir}/st
%{python_sitelib}/swift/
%{python_sitelib}/test/
%{python_sitelib}/swift-%{version}-py%{pyver}.egg-info

%changelog
* Wed May 11 2011 Jeffrey Ness <jeffrey.ness@rackspace.com> - 1.3.0-1
- Initial build
