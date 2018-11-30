# -D MUST pass in _version and _release, and SHOULD pass in dist.

Summary: Tracking Log Reader for Varnish Cache
Name: trackrdrd
Version: %{_version}
Release: %{_release}%{?dist}
License: BSD
Group: System Environment/Daemons
URL: https://code.uplex.de/uplex-varnish/trackrdrd
Source0: %{name}-%{version}.tar.gz
Source1: trackrdrd.service
Source2: trackrdr-kafka.logrotate

# varnish from varnish60 at packagecloud
# zookeeper-native from cloudera-cdh5.repo
Requires: varnish >= 6.0.0, varnish < 6.1.0
Requires: librdkafka
Requires: zookeeper-native
Requires: zlib
Requires: pcre
Requires: logrotate

BuildRequires: varnish-devel >= 6.0.0, varnish-devel < 6.1.0
BuildRequires: pkgconfig
BuildRequires: make
BuildRequires: gcc
BuildRequires: librdkafka-devel
BuildRequires: zookeeper-native
BuildRequires: pcre-devel
BuildRequires: zlib-devel
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
%setup -q -n %{name}-%{version}

%build
# ./autogen.sh

%configure

make

%check

# Temporarily allow trackrdrd to write logs as nobody during make check
chmod o+w %{_builddir}/%{name}-%{version}/src/test
make check
chmod o-w %{_builddir}/%{name}-%{version}/src/test

%pre
if [ -z $(getent group varnish) ]; then
    echo 'Group varnish not found (should have been created by package varnish)'
    exit 1
fi
getent passwd trackrdrd >/dev/null || \
        useradd -r -g varnish -s /sbin/nologin \
                -c "Tracking Log Reader" trackrdrd
exit 0

%install

make install DESTDIR=%{buildroot}

install -D -m 0644 etc/trackrdrd.conf %{buildroot}%{_sysconfdir}/trackrdrd.conf
install -D -m 0644 etc/trackrdr-kafka.conf %{buildroot}%{_sysconfdir}/trackrdr-kafka.conf

install -D -m 0644 %{SOURCE1} %{buildroot}%{_unitdir}/trackrdrd.service

mkdir -p %{buildroot}/var/log/trackrdrd
install -D -m 0644 %{SOURCE2} %{buildroot}%{_sysconfdir}/logrotate.d/trackrdr-kafka

cp src/mq/file/README.rst README-mq-file.rst
cp src/mq/kafka/README.rst README-mq-kafka.rst
# Only use the version-specific docdir created by %doc below
rm -rf %{buildroot}%{_docdir}

# None of these for fedora/epel
find %{buildroot}/%{_libdir}/ -name '*.la' -exec rm -f {} ';'
find %{buildroot}/%{_libdir}/ -name '*.a' -exec rm -f {} ';'

%clean
rm -rf %{buildroot}

%files
%defattr(-,root,root,-)
%{_bindir}/*
%{_libdir}/*
%{_var}/log/trackrdrd
%{_mandir}/man1/*.1*
%{_mandir}/man3/*.3*
%doc README.rst COPYING INSTALL.rst LICENSE README-mq-file.rst README-mq-kafka.rst
%config(noreplace) %{_sysconfdir}/trackrdrd.conf
%config(noreplace) %{_sysconfdir}/trackrdr-kafka.conf
%config(noreplace) %{_sysconfdir}/logrotate.d/trackrdr-kafka

%{_unitdir}/trackrdrd.service

%post
chown trackrdrd:varnish /var/log/trackrdrd
/sbin/ldconfig

%changelog
