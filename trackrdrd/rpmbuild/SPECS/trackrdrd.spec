Summary: Varnish log tracking reader demon
Name: trackrdrd
Version: %{?version}
Release: build%{?build_number}_rev%{?revision}
Vendor: Otto Gmbh & Co KG
License: BSD
Group: System Environment/Daemons
URL: https://qspa.otto.de/confluence/display/LHOT/C-Implementierung
Packager: LHOTSE Operations <lhotse-ops@dv.otto.de>
#Source0: git@git.lhotse.ov.otto.de:lhotse-tracking-varnish

# Varnish dependency currently can't be resolved
AutoReqProv: no
#Requires: varnish_bestats
#Requires: libactivemq
#Requires: logrotate
#Requires(post): /sbin/chkconfig
#Requires(preun): /sbin/chkconfig
#Requires(preun): /sbin/service

%description
This is the Varnish log tracking reader demon, which reads data
intended for tracking from the Varnish shared memory log, collects all
data for each XID, and sends each data record to an ActiveMQ message
broker.

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
%doc %{prefix}/share/man/man3/%{name}.3
%config(noreplace) %{_sysconfdir}/%{name}.conf
%config(noreplace) %{_sysconfdir}/init.d/%{name}

%changelog
* Tue Nov 27 2012  Geoff Simmons <geoff@uplex.de> 0.1
- First RPM build
