INSTALLATION
============

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
