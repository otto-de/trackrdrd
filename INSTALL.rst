INSTALLATION
============

RPMs
~~~~

Binary, debuginfo and source RPMs for the Tracking Reader are
available at packagecloud:

	https://packagecloud.io/uplex/varnish

The packages are built for Enterprise Linux 7 (el7), and hence will
run on compatible distros (such as RHEL7, Fedora and CentOS 7).

To set up your YUM repository for the RPMs, follow these instructions:

	https://packagecloud.io/uplex/varnish/install#manual-rpm

You will also need these additional repositories:

* EPEL7

  * ``rpm -Uvh https://dl.fedoraproject.org/pub/epel/epel-release-latest-7.noarch.rpm``

* Official Varnish packages from packagecloud (since version 5.2.0)

  * Follow the instructions at: https://packagecloud.io/varnishcache/varnish52/install#manual-rpm

* Cloudera CDH5 repository for the zookeeper-native package:

  * ``yum install https://archive.cloudera.com/cdh5/one-click-install/redhat/7/x86_64/cloudera-cdh-5-0.x86_64.rpm``

In addition to installing the binary, libraries, man pages and
documentation, the RPM install does the following:

* Creates a user ``trackrdrd`` and adds it to the group ``varnish``

  * The ``varnish`` group is created by the varnish package.

* Installs a default configuration at ``/etc/trackrdrd.conf``, which
  sets up the following:

  * The child process runs as ``trackrdrd``.

  * The Kafka MQ plugin is used.

  * A PID file is saved at ``/var/run/trackrdrd.pid``.

* Installs a default configuration for the Kafka plugin at
  ``/etc/trackrdr-kafka.conf``:

  * Defines a broker listening at the default port on the loopback
    address: ``127.0.0.1:9092``.

  * Sends messages to the topic ``tracking``.

  * Logs to the file ``/var/log/trackrdrd/libtrackrdr-kafka.log``

* Installs a logrotate configuration for the Kafka plugin's log file
  at: ``/etc/logrotate.d/trackrdr-kafka``.

* Installs a systemd unit file for the Tracking Reader. The
  ``trackrdrd`` service requires that the ``varnish`` service is
  running.

If you change the config files in ``/etc``, they are not overwritten
by RPM updates. Changed config files from the package are saved with
the ``.rpmnew`` extension.

To change the broker address in ``/etc/trackrdr-kafka.conf``, specify
either a comma-separated host:port list of initial Kafka brokers with
``metadata.broker.list``, or a comma-separated host:port list of
ZooKeeper servers with ``zookeeper.connect``. See
`libtrackrdrd-kafka(3) <src/mq/kafka/README.rst>`_ for details.

You can, of course, specify another configuration file with the ``-c``
option (modify the systemd unit file to do this with systemd).

The systemd service is *not* started or enabled automatically when the
package is installed, and is not stopped on package update or
removal. This will have to be done with separate ``systemctl``
commands.  This is because the Tracking Reader using Kafka is not
fully functional unless the configured brokers are listening, which
cannot be detected with systemd dependencies, and because most
deployments will change the default broker address. It is probably
best to automate package management with a script that issues ``yum``
and ``systemctl`` commands as needed.

The RPM does not support SysV init.

If you have problems or questions concerning the RPMs, post an issue
to one of the source repository web sites, or contact
<varnish-support@uplex.de>.

BUILDING FROM SOURCE
~~~~~~~~~~~~~~~~~~~~

``trackrdrd`` is built against an existing Varnish installation on the
same host, which in the standard case can be found with usual settings
for the ``PATH`` environment variable in the ``configure`` step
described below.

The build requires the following tools/packages:

* git
* autoconf
* autoconf-archive
* automake
* autoheader
* libtool
* pkg-config
* python-docutils (for rst2man)

The messaging plugin for Kafka (``libtrackrdr-kafka``) requires
libraries for Kafka (``librdkafka``) and the multi-threaded libary for
Zookeeper (``libzookeeper_mt``)::

        https://github.com/edenhill/librdkafka
        http://zookeeper.apache.org/

The messaging plugin for Kafka is optional, and you can choose to
disable its build in the ``configure`` step, as explained
below. Requirements do not need to be met for plugins that are not
built.

Building and installing trackrdrd
---------------------------------

At minimum, run these steps::

  	# To build from the git repo
	$ git clone $TRACKRDRD_GIT_URL
	$ cd trackrdrd
	$ ./autogen.sh

        # Builds from a source tarball begin here
        $ ./configure
	$ make

For self-tests after the build, run::

	$ make check

To install ``trackrdrd``, run ``make install`` as root, for example
with ``sudo``::

	$ sudo make install

Alternative configurations
--------------------------

In the ``configure`` step, a wide range of additional options may be
given to affect the configuration. Most of these are standard, and can
be shown with::

	$ configure --help

To disable the build of the Kafka MQ implementation, specify the
option ``--disable-kafka`` for ``configure``. It is enabled by
default. A file output plugin, suitable for testing and debugging, is
always built.

To specify a non-standard installation prefix, add the ``--prefix``
option::

	$ ./configure --prefix=/path/to/trackrdrd_install

If the Varnish installation against which ``trackrdrd`` is *built* has
a non-standard location, set these env variables before running
``configure``:

* PREFIX=/path/to/varnish/install/prefix
* export PKG_CONFIG_PATH=$PREFIX/lib/pkgconfig
* export ACLOCAL_PATH=$PREFIX/share/aclocal
* export PATH=$PREFIX/bin:$PREFIX/sbin:$PATH

``PKG_CONFIG_PATH`` might also have to include pkg-config directories
for other requirements, such as the Kafka client library, if they have
been installed into non-default locations.

If the Varnish installation against which ``trackrdrd`` is *run* has a
non-standard location, it is necessary to specify runtime paths to the
Varnish libraries by setting ``LDFLAGS=-Wl,-rpath=$LIB_PATHS`` for the
configure step::

        $ export VARNISH_PREFIX=/path/to/varnish_install
	$ ./configure \\
          LDFLAGS=-Wl,-rpath=$VARNISH_PREFIX/lib/varnish:$VARNISH_PREFIX/lib

Developers can add a number of options as an aid to compiling and
debugging::

	$ ./configure --enable-debugging-symbols --enable-developer-warnings

``--enable-debugging-symbols`` ensures that symbols and source code
file names are saved in the executable, and thus are available in core
dumps, in stack traces on assertion failures, for debuggers and so
forth.

``--enable-developer-warnings`` activates stricter compiler switches
for errors and warnings, such as ``-Werror`` to cause compiles to fail
on any warning. ``trackrdrd`` should *always* build successfully with
this option.

Building and installing packaged MQ implementations
---------------------------------------------------

The ``trackrdrd`` distribution includes implementations of the MQ
interface for the Kafka message broker as well as the file output
plugin. For details of the builds and their dependencies, see
libtrackrdr-kafka(3) and libtrackrdr-file(3) (``README.rst`` in
``src/mq/kafka`` and ``src/mq/file``).

The global make targets for ``trackrdrd`` also build the MQ
implementations, unless their builds are disabled in the ``configure``
step as explained above. If they are enabled, then it is necessary to
configure the build for them as well.
