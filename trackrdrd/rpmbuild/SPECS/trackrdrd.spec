Summary: Varnish log tracking reader demon
Name: trackrdrd
Version: %{?version}
Release: %{?build_number}_rev%{?revision}
Vendor: Otto Gmbh & Co KG
License: BSD
Group: System Environment/Daemons
URL: https://qspa.otto.de/confluence/display/LHOT/C-Implementierung
Packager: LHOTSE Operations <lhotse-ops@dv.otto.de>
#Source0: git@git.lhotse.ov.otto.de:lhotse-tracking-varnish

# Enforce dependencies on RedHat (don't bother with SuSE)
%if "%{_vendor}" == "suse"
AutoReqProv: no
%endif
%if "%{_vendor}" == "redhat"
BuildRequires: activemq-cpp-library-devel
Requires: lhotse-varnish
Requires: libtrackrdr-activemq
%endif
#Requires: logrotate
#Requires(post): /sbin/chkconfig
#Requires(preun): /sbin/chkconfig
#Requires(preun): /sbin/service

%description
This is the Varnish log tracking reader demon, which reads data
intended for tracking from the Varnish shared memory log, collects all
data for each XID, and sends each data record to an ActiveMQ message
broker.

%package -n libtrackrdr-activemq
Summary: ActiveMQ implementation of the trackrdrd MQ interface
Group: System Environment/Libraries
Requires: libactivemq-cpp6

%description -n libtrackrdr-activemq
ActiveMQ implementation of the messaging interface for the Varnish log
tracking reader

%prep
#Empty section.

%build
# Empty section.

%install
cp -rP . $RPM_BUILD_ROOT

%clean
rm -rf %{buildroot}
rm -rf %{_builddir}/*

%files
%defattr(-,root,root,-)
%{prefix}/bin/trackrdrd
%config %{prefix}/etc/sample.conf
# no rst2man on RedHat, so there's no man page
%doc %{prefix}/share/man/man3/%{name}.3
%config(noreplace) %{_sysconfdir}/%{name}.conf
%config(noreplace) %{_sysconfdir}/init.d/%{name}

%post -n libtrackrdr-activemq -p /sbin/ldconfig

%postun -n libtrackrdr-activemq -p /sbin/ldconfig

%files -n libtrackrdr-activemq
%defattr(-,root,root,-)
%{prefix}/lib/trackrdrd/*
%doc %{prefix}/share/man/man3/libtrackrdr-activemq.3

%changelog
* Tue May 20 2014  Geoff Simmons <groff@uplex.de> 3.0
- Add man page for libtrackrdr-activemq

* Mon May 12 2014  Geoff Simmons <groff@uplex.de> 3.0
- Add package libtrackrdr-activemq, and adjust dependencies

* Mon Apr 29 2013  Geoff Simmons <geoff@uplex.de> 1.0
- Fix dependencies for RedHat

* Tue Nov 27 2012  Geoff Simmons <geoff@uplex.de> 0.1
- First RPM build
