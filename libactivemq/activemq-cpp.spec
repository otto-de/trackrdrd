BuildRequires: gcc-c++
BuildRequires: libapr1-devel
BuildRequires: libapr-util1-devel 
BuildRequires: libuuid-devel
BuildRequires: autoconf
BuildRequires: automake
BuildRequires: openldap2
BuildRequires: openldap2-devel
Name: activemq-cpp-library
Version: 3.5.0
Source0: %{name}-%{version}-src.tar.bz2
Release: 1
Summary: A C++ library to interact with Apache ActiveMQ
Group: Development/Libraries/C and C++
License: Apache
BuildRoot: %{_tmppath}/%{name}-%{version}-build
AutoReqProv: on

%description
A C++ library to interact with Apache ActiveMQ.

%package -n libactivemq-cpp6
Summary: A C++ library to interact with Apache ActiveMQ
Group: Development/Libraries/C and C++

%description -n libactivemq-cpp6
A C++ library to interact with Apache ActiveMQ.

%package devel
Requires: gcc-c++
Requires: libactivemq-cpp6 = %{version}-%{release}
Requires: libuuid-devel
Summary: A C++ library to interact with Apache ActiveMQ
Group: Development/Libraries/C and C++

%description devel
A C++ library to interact with Apache ActiveMQ.

%prep
%setup -n %{name}-%{version}

%build
sh ./autogen.sh
%configure
%__make

%install
%makeinstall
rm $RPM_BUILD_ROOT/%{_bindir}/example
rm $RPM_BUILD_ROOT/%{_libdir}/*.*a

%clean
rm -rf $RPM_BUILD_ROOT

%post -n libactivemq-cpp6 -p /sbin/ldconfig

%postun -n libactivemq-cpp6 -p /sbin/ldconfig

%files -n libactivemq-cpp6
%defattr(-, root, root)
%{_libdir}/*.so.*

%files devel
%defattr(-, root, root)
%{_bindir}/activemqcpp-config

%{_includedir}/activemq-cpp-3.5.0
%{_libdir}/pkgconfig/*.pc
%{_libdir}/*.so

%changelog
