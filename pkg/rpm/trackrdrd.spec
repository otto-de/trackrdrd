
Summary: Tracking Log Reader for Varnish Cache
Name: trackrdrd
Version: trunk
Release: 0%{?dist}
License: BSD
Group: System Environment/Daemons
URL: https://code.uplex.de/uplex-varnish/trackrdrd
Source0: %{name}-%{version}.tar.gz
#Source1: varnish.initrc
#Source2: varnish.sysconfig
#Source3: varnish.logrotate
#Source4: varnish_reload_vcl
#Source5: varnish.params
#Source6: varnish.service
#Source9: varnishncsa.initrc
#Source10: varnishncsa.service
#Source11: find-provides

# varnish from varnish5 at packagecloud
# zookeeper-native from cloudera-cdh5.repo
Requires: varnish >= 5.2.0
Requires: librdkafka
Requires: zookeeper-native
Requires: lz4
Requires: pcre

BuildRequires: varnish-devel >= 5.2.0
BuildRequires: pkgconfig
BuildRequires: make
BuildRequires: gcc
BuildRequires: librdkafka-devel
BuildRequires: zookeeper-native
BuildRequires: pcre-devel
BuildRequires: lz4-devel
BuildRequires: python-docutils >= 0.6

# git builds
#BuildRequires: automake
#BuildRequires: autoconf
#BuildRequires: autoconf-archive
#BuildRequires: libtool
#BuildRequires: python-docutils >= 0.6

#Requires(pre): shadow-utils
#Requires(post): /sbin/chkconfig, /usr/bin/uuidgen
#Requires(preun): /sbin/chkconfig
#Requires(preun): /sbin/service
#%if %{undefined suse_version}
#Requires(preun): initscripts
#%endif
#%if 0%{?fedora} >= 17 || 0%{?rhel} >= 7
#Requires(post): systemd-units
#Requires(post): systemd-sysv
#Requires(preun): systemd-units
#Requires(postun): systemd-units
#BuildRequires: systemd-units
#%endif
#Requires: gcc

Provides: trackrdrd, trackrdrd-debuginfo

%description
The Tracking Log Reader demon reads from the shared memory log of a
running instance of Varnish, aggregates data logged in a specific
format for requests and ESI subrequests, and forwards the data to a
messaging system (such as Kafka).

%prep
#%setup -n varnish-%{version}%{?vd_rc}
%setup -q -n trackrdrd-trunk

%build
# ./autogen.sh

%configure

make

%check

# Temporarily allow trackrdrd to write logs as nobody during make check
chmod o+w %{_builddir}/%{name}-%{version}/src/test
make check
chmod o-w %{_builddir}/%{name}-%{version}/src/test

%install

make install DESTDIR=%{buildroot}

install -D -m 0644 etc/trackrdrd.conf %{buildroot}%{_sysconfdir}/trackrdrd.conf
install -D -m 0644 etc/trackrdr-kafka.conf %{buildroot}%{_sysconfdir}/trackrdr-kafka.conf

# None of these for fedora/epel
find %{buildroot}/%{_libdir}/ -name '*.la' -exec rm -f {} ';'
find %{buildroot}/%{_libdir}/ -name '*.a' -exec rm -f {} ';'

%clean
rm -rf %{buildroot}

%files
%defattr(-,root,root,-)
%{_bindir}/*
%{_libdir}/*
%{_sysconfdir}/*
%{_mandir}/man3/*.3*
%doc README.rst
#%license LICENSE

%changelog
