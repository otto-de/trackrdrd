.. _ref-varnishd:

==========
 trackrdrd
==========

-------------------------
Tracking Log Reader demon
-------------------------

:Author: Geoffrey Simmons
:Date:   2013-03-12
:Version: 2.0
:Manual section: 3


SYNOPSIS
========

.. include:: synopsis.txt

DESCRIPTION
===========

The ``trackrdrd`` demon reads from the shared memory log of a running
instance of Varnish, collects data relevant to tracking for the Lhotse
project, and forwards the data to ActiveMQ message brokers.

OPTIONS
=======

.. include:: options.txt

BUILD/INSTALL
=============

The source repository for ``trackrdrd`` is in the subdirectory
``trackrdrd/`` of::

	git@git.lhotse.ov.otto.de:lhotse-tracking-varnish

The build requires a source directory for Varnish in which sources
have been compiled. Varnish sources with custom features for Lhotse
are in::

	git@git.lhotse.ov.otto.de:lhotse-varnish-cache

``trackrdrd`` must link with the CMS or ActiveMQ-CPP library
(``libactivemq-cpp``) at runtime. The sources can be obtained from::

        http://activemq.apache.org/cms/

Building Varnish
----------------

The Varnish build requires the following tools/packages:

* git
* autoconf
* automake
* pkg-config
* pcre-devel (so that Varnish can link to the runtime libs)
* python-docutils (for rst2man)

Check out the repository and switch to the branch ``3.0_bestats``, in
which custom features for Lhotse are implemented::

	$ git clone git@git.lhotse.ov.otto.de:lhotse-varnish-cache
	$ cd lhotse-varnish-cache/
	$ git checkout 3.0_bestats

Varnish as deployed for Lhotse is built in 64-bit mode, and since
``trackrdrd`` needs to link with its libraries, it must be built in
64-bit mode as well. This means that the Varnish build for
``trackrdrd`` must also be 64-bit; for ``gcc``, this is accomplished
with ``CFLAGS=-m64``.

The following sequence builds Varnish as needed for the ``trackrdrd``
build::

	$ ./autogen.sh
	$ CFLAGS=-m64 ./configure
	$ make

Building and installing ActiveMQ-CPP
------------------------------------

``trackrdrd`` has been tested with versions 3.4.4 and 3.5.0 of
ActiveMQ-CPP. If the library ``libactivemq-cpp`` is already installed
on the platform where ``trackrdrd`` will run, then no further action
is necessary. To build the library from source, follow the
instructions in the ``README.txt`` file of its source distribution.

Building and installing trackrdrd
---------------------------------

Requirements for ``trackrdrd`` are the same as for Varnish, in
addition to the Varnish build itself. (``pcre-devel`` is not strictly
necessary for ``trackrdrd``, but since you are building ``trackrdrd``
on the same platform as the Varnish build, all requirements are
fulfilled.)

The steps to build ``trackrdrd`` are very similar to those for
building Varnish. The only difference is in the ``configure``
step:

* The path to the Varnish source directory must be given in the variable ``VARNISHSRC``.
* The flag ``CXXFLAGS``, like ``CFLAGS``, must also be set to ``-m64``, because C++ code is also compiled. It may be necessary to add additional ``CXXFLAGS`` to compile the ActiveMQ API calls, for example as obtained from ``pkg-config --cflags apr-1``.

At minimum, run these steps::

	$ git clone git@git.lhotse.ov.otto.de:lhotse-tracking-varnish
	$ cd lhotse-tracking-varnish/trackrdrd/
	$ ./autogen.sh
	$ CXXFLAGS=-m64 CFLAGS=-m64 ./configure \\
          VARNISHSRC=/path/to/lhotse-varnish-cache
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

For example, to specify a non-standard installation prefix, add the
``--prefix`` option::

	$ CFLAGS=-m64 CXXFLAGS=-m64 ./configure \\
          VARNISHSRC=/path/to/lhotse-varnish-cache \\
	  --prefix=/var/opt/varnish_tracking

For Lhotse, runtime paths for Varnish libraries are at non-standard
locations, so it is necessary to add the option
``LDFLAGS=-Wl,-rpath=$LIB_PATHS``::

        $ export LHOTSE_VARNISH_PREFIX=/var/opt/varnish
	$ CFLAGS=-m64 CXXFLAGS=-m64 ./configure \\
          VARNISHSRC=/path/to/lhotse-varnish-cache \\
	  --prefix=/var/opt/varnish_tracking \\
          LDFLAGS=-Wl,-rpath=$LHOTSE_VARNISH_PREFIX/lib/varnish:$LHOTSE_VARNISH_PREFIX/lib

Developers can add a number of options as an aid to compiling and debugging::

	$ CFLAGS=-m64 CXXFLAGS=-m64 ./configure \\
          VARNISHSRC=/path/to/lhotse-varnish-cache \\
          --enable-debugging-symbols --enable-werror \\
          --enable-developer-warnings --enable-extra-developer-warnings \\
          --enable-diagnostics

``--enable-debugging-symbols`` ensures that symbols and source code
file names are saved in the executable, and thus are available in core
dumps, in stack traces on assertion failures, for debuggers and so
forth. It is advisable to turn this switch on for production builds
(not just for developer builds), so that runtime errors can more
easily be debugged.

``--enable-werror`` activates the ``-Werror`` option for compilers,
which causes compiles to fail on any warning. ``trackrdrd`` should
*always* build successfully with this option.

``--enable-developer-warnings``, ``--enable-extra-developer-warnings``
and ``--enable-diagnostics`` turn on additional compiler switches for
errors and warnings. ``trackrdrd`` builds should succeed with these as
well.

It may be necessary to set ``PKG_CONFIG_PATH`` to point to the
appropriate pkg-config directories, if any of the needed requirements
(such as the ActiveMQ C++ library) have been installed into
non-default locations, as in this example::

	$ PKG_CONFIG_PATH=/usr/local/lib/pkgconfig ./configure #...

CONFIGURATION
=============

As mentioned above for command-line option ``-c``, configuration values
are read in this hierarchy:

1. ``/etc/trackrdrd.conf``, if it exists and is readable
2. a config file specified with the ``-c`` option
3. config values specified with other command-line options

If the same config parameter is specified in one or more of these
sources, then the value at the "higher" level is used. For example, if
``varnish.name`` is specified in both ``/etc/trackrdrd.conf`` and a
``-c`` file, then the value from the ``-c`` file is used, unless a
value is specified with the ``-n`` option, in which case that value is
used.

The syntax of a configuration file is simply::

        # comment
        <param> = <value>

The ``<value>`` is all of the data from the first non-whitespace
character after the equals sign up to the last non-whitespace
character on the line. Comments begin with the hash character and
extend to the end of the line. There are no continuation lines.

The parameters ``mq.uri`` and ``mq.qname`` are required (have no
default values), and (only) ``mq.uri`` may be specified more than
once. All other config parameters have default values, and some of
them correspond to command-line options, as shown below.

.. include:: config.rst

LOGGING AND MONITORING
======================

By default, ``trackrdrd`` uses ``syslog(3)`` for logging with facility
``local0`` (unless otherwise specified by configuration as shown
above). In addition to informational, error and warning messages about
the running processes, monitoring information is periodically emitted
to the log (as configured with the parameter
``monitor.interval``). The monitoring logs have this form (at the
``info`` log level, with additional formatting of the log lines,
depending on how syslog is configured)::

 Hash table: len=16384 seen=23591972 drop_reqstart=0 drop_vcl_log=0 drop_reqend=0 expired=0 evacuated=0 open=0 load=0.00 collisions=58534 insert_probes=58974 find_probes=2625 fail=0 occ_hi=591 occ_hi_this=1 
 Data table: len=116384 nodata=11590516 submitted=12001456 wait_room=0 data_hi=3187 data_overflows=0 done=0 open=0 load=0.00 sent=12001456 reconnects=17 failed=0 occ_hi=4345 occ_hi_this=1 

If monitoring of worker threads is switched on, then monitoring logs
such as this are emitted for each thread::

 Worker 1 (waiting): seen=576414 waits=86682 sent=576414 reconnects=0 restarts=0 failed=0

The line prefixed by ``Hash table`` describes the hash table for open
records -- records for request XIDs for which ``ReqStart`` has been
read, but not yet ``ReqEnd``. The fields ``open``, ``load`` and
``occ_hi_this`` are gauges (expressing a current state), and
``occ_hi`` is monotonic increasing; all other fields are cumulative
counters:

.. include:: hashlog.rst

The line prefixed by ``Data table`` describes the table of request
records, including records in the open and done states -- for "done"
records, ``ReqEnd`` has been read for the XID and the record is
complete, but it has not yet been sent to a message broker. The fields
``open``, ``done``, ``load`` and ``occ_hi_this`` are gauges, and
``occ_hi`` is monotonic increasing; the rest are cumulative counters:

.. include:: datalog.rst
