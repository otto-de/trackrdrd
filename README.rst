.. _ref-varnishd:

==========
 trackrdrd
==========

-------------------------
Tracking Log Reader demon
-------------------------

:Author: Geoffrey Simmons
:Date:   2015-04-27
:Version: 3.0
:Manual section: 3

**IMPORTANT NOTE: A new version of the Tracking Reader that is
compatible with Varnish version 4 is currently under development in
the master branch, but it WILL NOT WORK in its present form, and this
documentation has not been updated.**

Use branch 3.0 for a version that is compatible with Varnish 3.0.x.

Check back here again if you're looking for a version that runs with
Varnish 4. When this message goes away, and the documentation is
updated, you can give it a try.

SYNOPSIS
========

  trackrdrd [[-n varnish_name] | [-f varnishlog_bindump]]
            [-c config_file] [-u user] [-P pid_file]
            [[-l log_file] | [-y syslog_facility]]
            [-D] [-d] [-V] [-h]

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

The source distribution for ``trackrdrd`` includes implementations of
the MQ interface for Kafka and ActiveMQ; see libtrackrdr-activemq(3)
and libtrackrdr-kafka(3) for details.

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

    -n varnish_logfile
        Same as the -n option for varnishd and other Varnish binaries;
        i.e. the 'varnish name' indicating the path of the mmap'd file
        used by varnishd for the shared memory log. By default, the
        host name is assumed (as with varnishd). Also set by the
        config parameter 'varnish.name'. The -n and -f options are
        mutually exclusive.

    -c config_file
        Path of a configuration file. If /etc/trackrdrd.conf exists
        and is readable, then its values are read first. If a file is
        specified by the -c option, then that file is read next, and
        config values that it specifies override values specified in
        /etc/trackrdrd.conf. Finally, config values specified on the
        command line override values specified in any config file. If
        no config files or other command line options are set, default
        config values hold.

    -u user
        Owner of the child process. By default, the child process runs
        as 'nobody'. Also set by the config parameter 'user'.

    -P pid_file
        Path of a file written by the management process that contains
        its process ID. By default, no PID file is written. Also set
        by the config parameter 'pid.file'.

    -l log_file
        Log file for status, warning, debug and error messages. If '-'
        is specified, then log messages are written to stdout. By
        default, syslog(3) is used for logging. Log levels correspond
        to the 'priorities' defined by syslog(3). Also set by the config
        parameter 'log.file'.

    -y syslog_facility
        Set the syslog facility; legal values are 'user' or 'local0'
        through 'local7', and the default is 'local0'. Options -y and
        -l are mutually exclusive. Also set by the config parameter
        'syslog.facility'.

    -D
        Run as a non-demon single process (for testing and
        debugging). By default, trackrdrd runs as a demon with a
        management (parent) process and worker (child) process.

    -f varnishlog_bindump
        A binary dump of the Varnish SHM log produced by 'varnishlog
        -w'. If this option is specified, trackrdrd reads from the
        dump instead of a live SHM log (useful for debugging and
        replaying traffic). The options -f and -n are mutually
        exclusive; -n is the default. Also set by the config parameter
        'varnish.bindump'.

    -d
       Sets the log level to LOG_DEBUG. The default log level is
       LOG_INFO.

    -V
       Print version and exit

    -h
       Print usage and exit

BUILD/INSTALL
=============

The build requires a source directory for Varnish 3.0.x in which sources
have been compiled. It also requires the unique XID patch available at::

	https://code.uplex.de/uplex-varnish/unique-xids

To build the messaging plugin for ActiveMQ (``libtrackrdr-activemq``)
it is neccessary to link with the CMS or ActiveMQ-CPP library
(``libactivemq-cpp``). The sources can be obtained from::

        http://activemq.apache.org/cms/

The messaging plugin for Kafka (``libtrackrdr-kafka``) requires the
rdkafka library (``librdkafka``)::

        https://github.com/edenhill/librdkafka

Building Varnish
----------------

The Varnish build requires the following tools/packages:

* git
* autoconf
* automake
* pkg-config
* pcre-devel (so that Varnish can link to the runtime libs)
* python-docutils (for rst2man)

Check out the repository and apply the unique-xids patch.

The tracking reader and the Varnish instances against which it built
and run must be built for the same architecture; in particular, they
must match as to 32- or 64-bit modes (and 64-bit is strongly
recommended for Varnish).  If the builds are executed on the same
machine (with the same architecture on which they will run), then they
will likely match by default. When in doubt, set compile-time flags
such as ``CFLAGS=-m64`` for ``gcc``.

The following sequence builds Varnish as needed for the ``trackrdrd``
build::

	$ ./autogen.sh
	$ CFLAGS=-m64 ./configure
	$ make

Building and installing packaged MQ implementations
---------------------------------------------------

The ``trackrdrd`` distribution includes implementations of the MQ
interface for Kafka and ActiveMQ message brokers. For details of the
builds and their dependencies, see libtrackrdr-kafka(3) and
libtrackrdr-activemq(3) (``README.rst`` in ``src/mq/kafka`` and
``src/mq/activemq``).

The global make targets for ``trackrdrd`` also build the MQ
implementations, unless their builds are disabled in the ``configure``
step as explained below. If they are enabled, then it is necessary to
configure the build for them as well, for example by setting
``CXXFLAGS`` to compile C++ sources.

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
* For ActiveMQ, the flag ``CXXFLAGS`` should be set similarly to
  ``CFLAGS``, because C++ code is also compiled. Settings for
  ``CXXFLAGS`` can be obtained from ``pkg-config --cflags apr-1``.

At minimum, run these steps::

	$ git clone $TRACKRDRD_GIT_URL
	$ cd trackrdrd
	$ ./autogen.sh
	$ CXXFLAGS=-m64 CFLAGS=-m64 ./configure \\
          VARNISHSRC=/path/to/compiled/varnish-cache
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

To disable the build of the Kafka or ActiveMQ MQ implementations,
specify the options ``--disable-kafka`` or ``disable-activemq`` for
``configure``. Both are enabled by default.

To specify a non-standard installation prefix, add the ``--prefix``
option::

	$ CFLAGS=-m64 CXXFLAGS=-m64 ./configure \\
          VARNISHSRC=/path/to/varnish_build \\
	  --prefix=/path/to/trackrdrd_install

If Varnish is installed at a non-standard location, it is necessary to
set runtime paths to the Varnish libraries with the option
``LDFLAGS=-Wl,-rpath=$LIB_PATHS``::

        $ export VARNISH_PREFIX=/path/to/varnish_install
	$ CFLAGS=-m64 CXXFLAGS=-m64 ./configure \\
          VARNISHSRC=/path/to/varnish_build \\
	  --prefix=/path/to/trackrdrd_install \\
          LDFLAGS=-Wl,-rpath=$VARNISH_PREFIX/lib/varnish:$VARNISH_PREFIX/lib

Developers can add a number of options as an aid to compiling and debugging::

	$ CFLAGS=-m64 CXXFLAGS=-m64 ./configure \\
          VARNISHSRC=/path/to/varnish_build \\
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

==================== ========== ========================================================================================= =======
Parameter            CLI Option Description                                                                               Default
==================== ========== ========================================================================================= =======
``varnish.name``     ``-n``     Like the ``-n`` option for Varnish, this is the path to the file that is mmap'd to the    default for Varnish (the host name)
                                shared memory segment for the Varnish log. This parameter and ``varnish.bindump`` are
                                mutually exclusive.
-------------------- ---------- ----------------------------------------------------------------------------------------- -------
``mq.module``                   Name of the shared object implementing the MQ interface. May be an absolute path, or the  None, this parameter is required.
                                name of a library that the dynamic linker finds according to the rules described in
                                ld.so(8).
-------------------- ---------- ----------------------------------------------------------------------------------------- -------
``mq.config_file``              Path of a configuration file used by the MQ implementation                                None, this parameter is optional.
-------------------- ---------- ----------------------------------------------------------------------------------------- -------
``nworkers``                    Number of worker threads used to send messages to the message broker(s).                  1
-------------------- ---------- ----------------------------------------------------------------------------------------- -------
``maxopen.scale``               log\ :sub:`2`\(max number of concurrent requests in Varnish). For example, if             10 (= 1024 concurrent requests)
                                ``maxopen.scale`` = 10, then ``trackrdrd`` can support up to 1024 concurrent requests.
                                More precisely, this number describes the maximum number of request XIDs for which
                                ``ReqStart`` has been read, but not yet ``ReqEnd``. It should specify at least the next
                                power of two larger than (``thread_pools`` * ``thread_pool_max``) in the Varnish
                                configuration.
-------------------- ---------- ----------------------------------------------------------------------------------------- -------
``maxdone``                     The maximum number of finished records waiting to be sent to message brokers. That is,    1024
                                the largest number of request XIDs for which ``ReqEnd`` has been read, but the data have
                                not yet been sent to a message queue.
-------------------- ---------- ----------------------------------------------------------------------------------------- -------
``maxdata``                     The maximum length of a data record in characters. Should be at least as large the        1024
                                Varnish parameter ``shm_reclen``.
-------------------- ---------- ----------------------------------------------------------------------------------------- -------
``hash.max_probes``             The maximum number of insert or find probes used for the hash table of XIDs. Hash lookups 10
                                fail if a hit is not found after this many probes.
-------------------- ---------- ----------------------------------------------------------------------------------------- -------
``hash.ttl``                    Maximum time to live in seconds for an unfinished record. If ``ReqEnd`` is not read for   120
                                a request XID within this time, then ``trackrdrd`` no longer waits for it, and schedules
                                the data read thus far to be sent to a message broker. This should be a bit longer than
                                the sum of all timeouts configured for a Varnish request.
-------------------- ---------- ----------------------------------------------------------------------------------------- -------
``hash.mlt``                    Minimum lifetime of an open record in seconds. That is, after ``ReqStart`` has been read  5
                                for a request XID, then ``trackrdrd`` will not evacuate it if space is needed in its hash
                                table before this interval has elapsed.
-------------------- ---------- ----------------------------------------------------------------------------------------- -------
``qlen.goal``                   A goal length for the internal queue from the reader thread to the worker thread.         ``maxdone``/2
                                ``trackrdrd`` uses this value to determine whether a new worker thread should be started
                                to support increasing load.
-------------------- ---------- ----------------------------------------------------------------------------------------- -------
``user``             ``-u``     Owner of the child process                                                                ``nobody``, or the user starting ``trackrdrd``
-------------------- ---------- ----------------------------------------------------------------------------------------- -------
``pid.file``         ``-P``     Path to the file to which the management process writes its process ID. If the value is   ``/var/run/trackrdrd.pid``
                                set to be empty (by the line ``pid.file=``, with no value), then no PID file is written.
-------------------- ---------- ----------------------------------------------------------------------------------------- -------
``restarts``                    Maximum number of restarts of the child process by the management process                 1
-------------------- ---------- ----------------------------------------------------------------------------------------- -------
``restart.pause``               Seconds to pause before restarting a child process                                        1
-------------------- ---------- ----------------------------------------------------------------------------------------- -------
``thread.restarts``             Maximum number of restarts of a worker thread by the child process. A thread is restarted 1
                                after a message send, message system reconnect and message resend have all failed. If the
                                restart limit for a thread is reached, then the thread goes into the state ``abandoned``
                                and no more restarts are attempted. If all worker threads are abandoned, then the child
                                process stops.
-------------------- ---------- ----------------------------------------------------------------------------------------- -------
``monitor.interval``            Interval in seconds at which monitoring statistics are emitted to the log. If set to 0,   30
                                then no statistics are logged.
-------------------- ---------- ----------------------------------------------------------------------------------------- -------
``monitor.workers``             Whether statistics about worker threads should be logged (boolean)                        false
-------------------- ---------- ----------------------------------------------------------------------------------------- -------
``log.file``         ``-l``     Log file for status, warning, debug and error messages, and monitoring statistics. If '-' ``syslog(3)``
                                is specified, then log messages are written to stdout. This parameter and
                                ``syslog.facility`` are mutually exclusive.
-------------------- ---------- ----------------------------------------------------------------------------------------- -------
``syslog.facility``  ``-y``     See ``syslog(3)``; legal values are ``user`` or ``local0`` through ``local7``. This       ``local0``
                                parameter and ``log.file`` are mutually exclusive. 
-------------------- ---------- ----------------------------------------------------------------------------------------- -------
``varnish.bindump``  ``-f``     A binary dump of the Varnish shared memory log obtained from ``varnishlog -w``. If a
                                value is specified, ``trackrdrd`` reads from that file instead of a live Varnish log
                                (useful for testing, debugging and replaying traffic). This parameter and
                                ``varnish.name`` are mutually exclusive. 
==================== ========== ========================================================================================= =======

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

================= =============================================================
Field             Description
================= =============================================================
``len``           Size of the hash table (2\ :sup:``maxopen.scale``\)
----------------- -------------------------------------------------------------
``seen``          Number of request records read (``ReqStart`` seen)
----------------- -------------------------------------------------------------
``drop_reqstart`` Number of records that could not be inserted into internal
                  tables (no data from ``ReqStart`` inserted, nor any other
                  data for that XID)
----------------- -------------------------------------------------------------
``drop_vcl_log``  How often data from ``VCL_log`` could not be inserted
                  (usually because the XID could not be found)
----------------- -------------------------------------------------------------
``drop_reqend``   How often data from ``ReqStart`` could not be inserted
                  (usually because the XID could not be found)
----------------- -------------------------------------------------------------
``expired``       Number of records for which ``hash.ttl`` expired (data sent
                  to message broker without waiting for ``ReqEnd``)
----------------- -------------------------------------------------------------
``evacuated``     Number of records removed to recover space in the hash table
                  (``hash.mlt`` expired, data possibly incomplete)
----------------- -------------------------------------------------------------
``open``          Current number of open records in the table
----------------- -------------------------------------------------------------
``load``          Current open records as percent (``open``/``len`` * 100)
----------------- -------------------------------------------------------------
``collisions``    Number of hash collisions
----------------- -------------------------------------------------------------
``insert_probes`` Number of hash insert probes
----------------- -------------------------------------------------------------
``find_probes``   Number of hash find probes
----------------- -------------------------------------------------------------
``fail``          Number of failed hash operations (insert or find)
----------------- -------------------------------------------------------------
``occ_hi``        Occupancy high watermark -- highest number of open records
                  since startup
----------------- -------------------------------------------------------------
``occ_hi_this``   Occupancy high watermark in the current monitoring interval
================= =============================================================

The line prefixed by ``Data table`` describes the table of request
records, including records in the open and done states -- for "done"
records, ``ReqEnd`` has been read for the XID and the record is
complete, but it has not yet been sent to a message broker. The fields
``open``, ``done``, ``load`` and ``occ_hi_this`` are gauges, and
``occ_hi`` is monotonic increasing; the rest are cumulative counters:

================== ============================================================
Field              Description
================== ============================================================
``len``            Size of the data table
                   (``maxdone`` + 2\ :sup:``maxopen.scale``\)
------------------ ------------------------------------------------------------
``nodata``         Number of request records that contained no data (nothing to
                   track in a ``VCL_log`` entry). These records are discarded
                   without sending a message to a message broker.
------------------ ------------------------------------------------------------
``submitted``      Number of records passed from the reader thread to worker
                   threads to be sent to a message broker
------------------ ------------------------------------------------------------
``wait_room``      How often the reader thread had to wait for space in the
                   data table
------------------ ------------------------------------------------------------
``data_hi``        Data high watermark -- longest record since startup (in
                   bytes)
------------------ ------------------------------------------------------------
``data_overflows`` How often the accumulated length of a record exceeded
                   ``maxdata``
------------------ ------------------------------------------------------------
``data_truncated`` How often data from the Varnish log was truncated due to
                   the presence of a null byte. This can happen if the data was
                   already truncated in the log, due to exceeding
                   ``shm_reclen``.
------------------ ------------------------------------------------------------
``done``           Current number of records in state "done"
------------------ ------------------------------------------------------------
``open``           Current number of open records in the table
------------------ ------------------------------------------------------------
``load``           Current number records in the table as percent
                   (100 * (``open`` + ``done``)/``len``)
------------------ ------------------------------------------------------------
``sent``           Number of records successfully sent to a message broker
------------------ ------------------------------------------------------------
``reconnects``     How often worker threads reconnected to a message broker
                   after an unsuccessful send
------------------ ------------------------------------------------------------
``restarts``       How often worker threads were restarted after a message
                   send, reconnect and resend all failed
------------------ ------------------------------------------------------------
``abandoned``      Number of worker threads that have been abandoned due to
                   reaching the restart limit (``thread.restarts``)
------------------ ------------------------------------------------------------
``failed``         Number of failed sends (failure after reconnect)
------------------ ------------------------------------------------------------
``occ_hi``         Occupancy high watermark -- highest number of records (open
                   and done) since startup
------------------ ------------------------------------------------------------
``occ_hi_this``    Occupancy high watermark in the current monitoring interval
================== ============================================================

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

================== ============================================================
Field              Description
================== ============================================================
``len``            Size of the data table
                   (``maxdone`` + 2\ :sup:``maxopen.scale``\)
------------------ ------------------------------------------------------------
``nodata``         Number of request records that contained no data (nothing to
                   track in a ``VCL_log`` entry). These records are discarded
                   without sending a message to a message broker.
------------------ ------------------------------------------------------------
``submitted``      Number of records passed from the reader thread to worker
                   threads to be sent to a message broker
------------------ ------------------------------------------------------------
``wait_room``      How often the reader thread had to wait for space in the
                   data table
------------------ ------------------------------------------------------------
``data_hi``        Data high watermark -- longest record since startup (in
                   bytes)
------------------ ------------------------------------------------------------
``data_overflows`` How often the accumulated length of a record exceeded
                   ``maxdata``
------------------ ------------------------------------------------------------
``data_truncated`` How often data from the Varnish log was truncated due to
                   the presence of a null byte. This can happen if the data was
                   already truncated in the log, due to exceeding
                   ``shm_reclen``.
------------------ ------------------------------------------------------------
``done``           Current number of records in state "done"
------------------ ------------------------------------------------------------
``open``           Current number of open records in the table
------------------ ------------------------------------------------------------
``load``           Current number records in the table as percent
                   (100 * (``open`` + ``done``)/``len``)
------------------ ------------------------------------------------------------
``sent``           Number of records successfully sent to a message broker
------------------ ------------------------------------------------------------
``reconnects``     How often worker threads reconnected to a message broker
                   after an unsuccessful send
------------------ ------------------------------------------------------------
``restarts``       How often worker threads were restarted after a message
                   send, reconnect and resend all failed
------------------ ------------------------------------------------------------
``abandoned``      Number of worker threads that have been abandoned due to
                   reaching the restart limit (``thread.restarts``)
------------------ ------------------------------------------------------------
``failed``         Number of failed sends (failure after reconnect)
------------------ ------------------------------------------------------------
``occ_hi``         Occupancy high watermark -- highest number of records (open
                   and done) since startup
------------------ ------------------------------------------------------------
``occ_hi_this``    Occupancy high watermark in the current monitoring interval
================== ============================================================

SIGNALS
=======

The management and child process respond to the following signals (all
other signals have the default handlers):

====== ========== ============
Signal Parent     Child
====== ========== ============
TERM   Shutdown   Shutdown
------ ---------- ------------
INT    Shutdown   Shutdown
------ ---------- ------------
HUP    Graceful   Ignore
       restart
------ ---------- ------------
USR1   Graceful   Dump data
       restart    table to log
------ ---------- ------------
USR2   Ignore     Ignore
------ ---------- ------------
ABRT   Abort with Abort with
       stacktrace stacktrace
------ ---------- ------------
SEGV   Abort with Abort with
       stacktrace stacktrace
------ ---------- ------------
BUS    Abort with Abort with
       stacktrace stacktrace
====== ========== ============

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


| Copyright (c) 2012-2015 UPLEX Nils Goroll Systemoptimierung
| Copyright (c) 2012-2015 Otto Gmbh & Co KG
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
