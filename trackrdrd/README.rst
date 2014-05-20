.. _ref-varnishd:

==========
 trackrdrd
==========

-------------------------
Tracking Log Reader demon
-------------------------

:Author: Geoffrey Simmons
:Date:   2014-05-20
:Version: 3.0
:Manual section: 3


SYNOPSIS
========

.. include:: synopsis.txt

DESCRIPTION
===========

The ``trackrdrd`` demon reads from the shared memory log of a running
instance of Varnish, aggregates data for a request that are relevant
to tracking, and forwards the data to a messaging system (such as
ActiveMQ or Kafka).

``trackrdrd`` reads data from ``VCL_Log`` entries that are displayed
in this format by the ``varnishlog`` tool::

  <FD> VCL_Log      c track <XID> <DATA>

* ``FD``: file descriptor of a client connection
* ``XID``: XID (request identifier) assigned by Varnish
* ``DATA``: data to be logged

``VCL_Log`` entries result from use of the ``log()`` function provided
by the standard vmod ``std`` distributed with Varnish. The ``log()``
call must include the prefix ``track``, the XID and the data to be
logged. These log entries can be created with VCL code such as::

  import std;

  sub vcl_recv {
      /* ... */
      std.log("track " + req.xid + " url=" + req.url);
      std.log("track " + req.xid + " http_Host=" + req.http.Host);
      /* ... */
  }

Thus the data to be logged can be any information available in VCL.

``trackrdrd`` collects all data logged for each XID, and combines
their data fields with the ampersand (``&``) character. Note that (in
Varnish 3) the same XID is assigned to a request and all requests that
it includes via ESI; so ``trackrdrd`` combines all logged data for a
request and its ESI includes.

When the request processing for an XID is complete (i.e. when
``trackrdrd`` reads ``ReqEnd`` for that XID), the data record is
complete and ready to be forwarded to the messaging
system. ``trackrdrd`` comprises a reader thread, which reads from the
shared memory log, and one or more worker threads, which send records
to one or more message brokers.

In addition to the data logged for an XID, ``trackrdrd`` prepends a
field ``XID=<xid>`` to the data, and appends a field ``req_endt=<t>``
containg the epoch time at which request processing ended (from the
``ReqEnd`` entry).

The interface to the messaging system is implemented in a messaging
plugin -- a shared object that provides definitions for the functions
declared in the MQ interface in ``include/mq.h``. See ``mq.h`` for
documentation of the interface.

The source distribution for ``trackrdrd`` includes an implementation
of the MQ interface for ActiveMQ, see libtrackrdr-activemq(3) for
details.

EXAMPLE
=======

The VCL example shown above may result in log entries such as these::

   29 ReqEnd       c 881964201 1363144515.280081511 1363144515.284164190 0.052356958 0.003843069 0.000239611
   29 VCL_Log      c track 881964202 url=/index.html
   29 VCL_Log      c track 881964202 http_Host=foo.bar.org
   29 ReqEnd       c 881964202 1363144515.433386803 1363144515.436567307 0.149222612 0.000135660 0.003044844

In this case, ``trackrdrd`` send this data to a message broker::

  XID=881964202&url=/index.html&http_Host=foo.bar.org&req_endt=1363144515.436567307

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

To build the messaging plugin for ActiveMQ (``libtrackrdr-activemq``)
it is neccessary to link with the CMS or ActiveMQ-CPP library
(``libactivemq-cpp``). The sources can be obtained from::

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

Building and installing packaged MQ implementations
---------------------------------------------------

The ``trackrdrd`` distribution includes an implementation of the MQ
interface for ActiveMQ message brokers. For details of the build and
its dependencies, see libtrackrdr-activemq(3) (``README.rst`` in
``src/mq/activemq``).

Since the global make targets for ``trackrdrd`` also build the MQ
implementations, it is necessary to configure the build for them as
well, for example by setting ``CXXFLAGS`` to compile C++ sources.

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

* The path to the Varnish source directory must be given in the
  variable ``VARNISHSRC``.
* The flag ``CXXFLAGS``, like ``CFLAGS``, must also be set to
  ``-m64``, because C++ code is also compiled. It may be necessary to
  add additional ``CXXFLAGS`` to compile the ActiveMQ API calls, for
  example as obtained from ``pkg-config --cflags apr-1``.

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

STARTUP AND SHUTDOWN
====================

On startup (unless the ``-D`` option is chosen), ``trackrdrd`` reads
any config files specified, and then demonizes, spawning a management
process that in turn spawns a worker process.

The management process runs with the privileges of the user who
started ``trackrdrd``; these privileges must be sufficient to write
the PID file and log file, if required by the configuration.

The worker process is started (and may be restarted) by the management
process, and runs with the privileges of the user specified by the
``-u`` option or configuration parameter ``user``. This process does
the work of reading the Varnish log, and creates the worker threads
that send data to message brokers.

To stop ``trackrdrd``, send the ``TERM`` signal to the management
process (e.g. with ``kill(1)``); the management process in turn shuts
down the worker process. Other responses to signals are detailed below
in SIGNALS_. If the worker process stops without being directed by the
management process, then the management process starts another one, up
to the limit defined by the config parameter ``restarts``.

After being instructed to terminate, the child process continues
reading data from the Varnish log for open records (request records
for which ``ReqEnd`` has not yet been read), and sends all pending
messages to the message broker, but does not open any new records on
reading ``ReqStart``. It stops when all open records are complete and
have been sent to message brokers.

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

The parameter ``mq.module`` is required (has no default value), and
``mq.config_file`` is optional (depending on whether the MQ
implementation requires a configuration file). All other config
parameters have default values, and some of them correspond to
command-line options, as shown below.

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

 Hash table: len=8192 seen=375862067 drop_reqstart=0 drop_vcl_log=0 drop_reqend=14 expired=50 evacuated=0 open=29 load=0.35 collisions=1526027 insert_probes=1534686 find_probes=45907 fail=0 occ_hi=530 occ_hi_this=85
 Data table: len=18192 nodata=280295531 submitted=95566507 wait_room=0 data_hi=6217 data_overflows=0 done=9 open=29 load=0.21 sent=95566498 reconnects=0 failed=0 restarts=0 abandoned=0 occ_hi=944 occ_hi_this=111

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

If worker threads are monitored, then the running state if logged for
each worker thread, one of:

* ``not started``
* ``initializing``
* ``running``
* ``waiting``
* ``abandoned``
* ``shutting down``
* ``exited``

In normal operation, the state should be either ``running``, when the
thread is actively reading finished data records and sending them to
message brokers, or ``waiting``, when the threads has exhausted all
pending records, or has not yet been awakened to handle more records.

The remaining fields in a log line for a worker thread are cumulative
counters:

.. include:: workerlog.rst

SIGNALS
=======

The management and child process respond to the following signals (all
other signals have the default handlers):

.. include:: signals.rst

Shutdown proceeds as described above in `STARTUP AND SHUTDOWN`_.

When signaled for graceful restart, the management process stops the
running worker process and starts another one. This has the effect
that the first process finishes reading data for open requests, and
the second one begins reading data for new requests, so that few or no
records are lost. The new process reads the same config files as the
original worker process, and retains any command-line configuration,
unless these values are overridden by config files. This allows for
configuration changes "on-the-fly".

On receiving signal ``USR1``, the worker process writes the contents
of all records in the "open" or "done" states to the log (syslog, or
log file specified by config), for troubleshooting or debugging.

Where "abort with stacktrace" is specified above, a process write a
stack trace to the log (syslog or otherwise) before aborting
execution; in addition, the worker process executes the following
actions:

* dump the current contents of the data table (as for the ``USR1`` signal)
* emit the monitoring stats to the log

RETURN VALUES
=============

Both the management and worker processes return 0 on normal
termination, and non-zero on error. When the worker process stops, the
management process records its return value in the log, as well as any
signal the worker process may have received.

SEE ALSO
========

* ``varnishd(1)``
* ``libtrackrdr-activemq(3)``
* ``ld.so(8)``
* ``syslog(3)``

COPYRIGHT AND LICENCE
=====================

For both the software and this document are governed by a BSD 2-clause
licence.


| Copyright (c) 2012-2014 UPLEX Nils Goroll Systemoptimierung
| Copyright (c) 2012-2014 Otto Gmbh & Co KG
| All rights reserved
| Use only with permission

| Authors: Geoffrey Simmons <geoffrey.simmons@uplex.de>
|          Nils Goroll <nils.goroll@uplex.de>

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
SUCH DAMAGE.
