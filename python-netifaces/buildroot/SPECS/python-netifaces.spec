%{!?python_sitearch: %global python_sitearch %(%{__python} -c "from distutils.sysconfig import get_python_lib; print(get_python_lib(1))")}
%{!?pyver: %global pyver %(%{__python} -c "import sys ; print sys.version[:3]")}

Name:		python-netifaces
Version:	0.4
Release:	1%{?dist}
Summary:	Portable network interface information	

Group:		Development/Languages
License:	MIT
URL:		http://pypi.python.org/pypi/netifaces/
Source0:	http://pypi.python.org/packages/source/n/netifaces/netifaces-%{version}.tar.gz
BuildRoot:	%{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

BuildRequires:	python-devel, python-setuptools
Requires:	python

%description
netifaces provides a (hopefully portable-ish) way for Python programmers to get 
access to a list of the network interfaces on the local machine, and to obtain the 
addresses of those network interfaces

%prep
%setup -q -n netifaces-%{version}

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
%{python_sitearch}/netifaces.so
%{python_sitearch}/netifaces-%{version}-py%{pyver}.egg-info
%{_prefix}/src/debug/netifaces-%{version}/netifaces.c

%changelog
* Wed May 11 2011 Jeffrey Ness <jeffrey.ness@rackspace.com> - 0.4-1
- Initial build
