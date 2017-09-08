.. _ref-varnishd:

==========
 trackrdrd
==========

-------------------------
Tracking Log Reader demon
-------------------------

:Author: Geoffrey Simmons
:Date:   2017-09-08
:Version: trunk
:Manual section: 3

SYNOPSIS
========


|  trackrdrd [[-n varnish_name] | [-f varnish_binlog]]
|            [-c config_file] [-u user] [-P pid_file]
|            [[-l log_file] | [-y syslog_facility]]
|            [-L tx_limit] [-T tx_timeout]
|            [-D] [-d] [-V] [-h]

DESCRIPTION
===========

The ``trackrdrd`` demon reads from the shared memory log of a running
instance of Varnish, aggregates data logged in a specific format for
requests and ESI subrequests, and forwards the data to a messaging
system, such as ActiveMQ or Kafka.

``trackrdrd`` reads data from ``VCL_Log`` entries that are displayed
in this format by the ``varnishlog`` tool for client request
transactions::

  VCL_Log        track <DATA>

* ``DATA``: data to be logged

The ``VCL_Log`` entries may also specify a sharding key for the
message brokers, in this format::

  VCL_Log        track key <KEY>

* ``KEY``: the sharding key

``VCL_Log`` entries result from use of the ``log()`` function provided
by the standard vmod ``std`` distributed with Varnish. The ``log()``
call must include the prefix ``track`` and the data to be logged, or a
sharding key. Note that DATA entries cannot begin with the word "key"
followed by a space; these will be interpreted as KEY entries.

These log entries can be created with VCL code such as::

  import std;

  sub vcl_recv {
      /* ... */
      std.log("track url=" + req.url);
      std.log("track http_Host=" + req.http.Host);
      std.log("track key " + req.http.X-Key);
      /* ... */
  }

Thus the data to be logged can be any information available in VCL. In
this example, the data to be forwarded includes the URL and the
``Host`` header, and the sharding key was obtained from a request
header ``X-Key``.

``trackrdrd`` collects all of the data logged for a request and its
ESI subrequests, combining their data fields with the ampersand
character (``&``). The data record is then buffered and ready to be
forwarded to the messaging system, using the sharding key if required
by the system. ``trackrdrd`` comprises a reader thread, which reads
from the shared memory log and buffers data, and one or more worker
threads, which read from the buffers and send data to message brokers.

In addition to the data obtained from the ``VCL_Log`` payloads,
``trackrdrd`` prepends a field ``XID=<xid>`` to the data, where
``<xid>`` is the VXID of the parent request (that includes any other
ESI subrequests). It also appends a field ``req_endt=<t>`` containing
the epoch time at which request processing ended, obtained from the
``Timestamp`` entry with the prefix ``Resp:`` (see vsl(3)). When there
is more than one such ``Timestamp`` entry in a group of requests (for
example in the logs of the ESI subrequests), the latest time is used.

The interface to the messaging system is implemented by a messaging
plugin -- a shared object that provides definitions for the functions
declared in the MQ interface in ``include/mq.h``. See ``mq.h`` for
documentation of the interface.

The source distribution for ``trackrdrd`` includes implementations of
the MQ interface for Kafka, ActiveMQ and for file output (the latter
for testing and debugging); see libtrackrdr-kafka(3),
libtrackrdr-activemq(3) and libtrackrdr-file(3) for details.

EXAMPLE
=======

The data read by the tracking reader from the Varnish log corresponds
to the data displayed with this ``varnishlog`` command::

  $ varnishlog -c -I 'VCL_log:^track ' -I Timestamp:^Resp: -g request \\
    -q 'VCL_log ~ "^track "' -i VSL

Thus the VCL example shown above may result in log entries such as::

  *   << Request  >> 591570    
  -   VCL_Log        track url=/index.html
  -   VCL_Log        track http_Host=foo.bar.org
  -   VCL_Log        track key 12345678
  -   Timestamp      Resp: 1430835449.167329 0.000681 0.000352

In this case, ``trackrdrd`` sends this data to message brokers, with
the sharding key ``12345678``::

  XID=591570&url=/index.html&http_Host=foo.bar.org&req_endt=1430835449.167329

A ``VSL`` record is synthesized by the Varnish logging API if there
was an error reading from the log; when this happens, ``trackrdrd``
logs the error message or warning in the ``VSL`` payload (by default
using syslog(3)).

OPTIONS
=======

    -n varnish_name
        Same as the -n option for varnishd and other Varnish binaries;
        i.e. the 'varnish name' indicating the directory containing
        the mmap'd file used by varnishd for the shared memory log. By
        default, the host name is assumed (as with varnishd). Also set
        by the config parameter 'varnish.name'. The -n and -f options
        are mutually exclusive.

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

    -f varnish_binlog
        A binary dump of the Varnish SHM log produced by 'varnishlog
        -w'. If this option is specified, trackrdrd reads from the
        dump instead of a live SHM log (useful for debugging and
        replaying traffic). The options -f and -n are mutually
        exclusive; -n is the default. Also set by the config parameter
        'varnish.bindump'.

    -L limit
        Sets the upper limit of incomplete transactions kept by the
        Varnish logging API before the oldest transaction is force
        completed. An error message is logged when this happens. This
        setting keeps an upper bound on the memory usage of running
        queries. Defaults to 1000 transactions. The same as the -L
        option for standard Varnish logging tools such as
        varnishlog(3).

    -T seconds
        Sets the transaction timeout in seconds for the Varnish
        logging API. This defines the maximum number of seconds
        elapsed between the beginning and end of the log
        transaction. If the timeout expires, the error message from
        the API is logged, and the transaction is force
        completed. Defaults to 120 seconds. The same as the -T option
        for standard Varnish logging tools such as varnishlog(3).

    -d
       Sets the log level to LOG_DEBUG. The default log level is
       LOG_INFO.

    -V
       Print version and exit

    -h
       Print usage and exit

BUILD/INSTALL
=============

Requirements
------------

This version of the tracking reader is compatible with Varnish since
version 5.2. ``trackrdrd`` is built against an existing Varnish
installation on the same host, which in the standard case can be found
with usual settings for the ``PATH`` environment variable in the
``configure`` step described below.

The build requires the following tools/packages:

* git
* autoconf
* automake
* autoheader
* pkg-config
* python-docutils (for rst2man)

The messaging plugin for Kafka (``libtrackrdr-kafka``) requires
libraries for Kafka (``librdkafka``) and the multi-threaded libary for
Zookeeper (``libzookeeper_mt``)::

        https://github.com/edenhill/librdkafka
        http://zookeeper.apache.org/

To build the messaging plugin for ActiveMQ (``libtrackrdr-activemq``)
it is neccessary to link with the CMS or ActiveMQ-CPP library
(``libactivemq-cpp``). The sources can be obtained from::

        http://activemq.apache.org/cms/

The messaging plugins for Kafka and ActiveMQ are optional, and you can
choose to disable the builds of either or both of them in the
``configure`` step, as explained below. Requirements do not need to be
met for plugins that are not built.

Building and installing trackrdrd
---------------------------------

The tracking reader and the Varnish instances against which it built
and run must be built for the same architecture; in particular, they
must match as to 32- or 64-bit modes (and 64-bit is strongly
recommended for Varnish).  If the builds are executed on the same
machine (with the same architecture on which they will run), then they
will likely match by default. When in doubt, set compile-time flags
such as ``CFLAGS=-m64`` for ``gcc``.

For ActiveMQ, the flag ``CXXFLAGS`` should be set similarly to
``CFLAGS``, because C++ code is also compiled (unless you choose to
disable the ActiveMQ plugin). Settings for ``CXXFLAGS`` can be
obtained from ``pkg-config --cflags apr-1``.

At minimum, run these steps::

	$ git clone $TRACKRDRD_GIT_URL
	$ cd trackrdrd
	$ ./autogen.sh
	$ CXXFLAGS=-m64 CFLAGS=-m64 ./configure
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
``configure``. Both are enabled by default. A file output plugin,
suitable for testing and debugging, is always built.

To specify a non-standard installation prefix, add the ``--prefix``
option::

	$ CFLAGS=-m64 CXXFLAGS=-m64 ./configure \\
          --prefix=/path/to/trackrdrd_install

If the Varnish installation against which ``trackrdrd`` is *built* has
a non-standard location, set these env variables before running
``configure``:

* PREFIX=/path/to/varnish/install/prefix
* export PKG_CONFIG_PATH=$PREFIX/lib/pkgconfig
* export ACLOCAL_PATH=$PREFIX/share/aclocal
* export PATH=$PREFIX/bin:$PREFIX/sbin:$PATH

``PKG_CONFIG_PATH`` might also have to include pkg-config directories
for other requirements, such as the ActiveMQ C++ libraries, if they
have been installed into non-default locations.

If the Varnish installation against which ``trackrdrd`` is *run* has a
non-standard location, it is necessary to specify runtime paths to the
Varnish libraries by setting ``LDFLAGS=-Wl,-rpath=$LIB_PATHS`` for the
configure step::

        $ export VARNISH_PREFIX=/path/to/varnish_install
	$ CFLAGS=-m64 CXXFLAGS=-m64 ./configure \\
          LDFLAGS=-Wl,-rpath=$VARNISH_PREFIX/lib/varnish:$VARNISH_PREFIX/lib

Developers can add a number of options as an aid to compiling and
debugging::

	$ CFLAGS=-m64 CXXFLAGS=-m64 ./configure \\
          --enable-debugging-symbols --enable-developer-warnings

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
interface for Kafka and ActiveMQ message brokers, as well as the file
output plugin. For details of the builds and their dependencies, see
libtrackrdr-kafka(3), libtrackrdr-activemq(3) and libtrackrdr-file(3)
(``README.rst`` in ``src/mq/kafka``, ``src/mq/activemq`` and
``src/mq/file``).

The global make targets for ``trackrdrd`` also build the MQ
implementations, unless their builds are disabled in the ``configure``
step as explained above. If they are enabled, then it is necessary to
configure the build for them as well, for example by setting
``CXXFLAGS`` to compile C++ sources.

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
in SIGNALS_. If the worker process stops without being directed to do
so by the management process, then the management process starts
another one, up to the limit defined by the config parameter
``restarts``.

After being instructed to terminate, the child process requests the
Varnish logging API to flush open log transactions (transactions that
have not yet been read to the ``End`` tag), and sends all pending
messages to the message broker, but does not open any new
transactions. It stops when all pending data have been sent to message
brokers.

DATA BUFFERS
============

The tracking reader reads and writes data asynchronously -- a reader
thread reads from the Varnish log and saves messages ready for sending
in buffers, while worker threads read from the buffer and send
messages to brokers.

Objects in the buffer are *records* and *chunks*. A record comprises a
complete message ready to be sent to brokers, made up of one or more
chunks, which store the message payload in fixed-size blocks.

The maximal length of a message payload is set by the config parameter
``max.reclen`` (payloads longer than the maximum are truncated), and
the ``chunk.size`` sets the fixed length of data blocks. The best
choice for these parameters depends on the distribution of message
lengths.  If the majority of messages are shorter than the maximum,
then less memory is wasted by setting a smaller chunk size. Ideally,
most messages should fit into the chunk size, and if nearly all
messages require the maximum length, then ``chunk.size`` can be set
equal to ``max.reclen``.

The choice constitutes a time-space tradeoff -- if the chunk size is
too large, then space is wasted; it if is too small, then the tracking
reader spends too much time iterating over and copying chunks.

The ``max.records`` parameter sets the maximum number of records that
can be stored in the buffers; the tracking reader computes the number
of chunks necessary for that many records. ``max.records`` should be
large enough for the buffering necessary during load spikes, and when
the delivery of messages to the brokers is slow.  ``max.records`` and
``chunk.size`` together determine the memory footprint of the tracking
reader.

Free entries in the buffers for records and chunks are structured in
free lists. The reader and worker threads each have local free lists,
and exchange data via global free lists. That is, the reader thread
takes free entries from its local free lists, and gets new entries
from the global lists when the local lists are exhausted. Worker
threads return free data to their local free lists, and return free
lists to the global free lists periodically.

If the reader thread cannot obtain free data from the buffers --
meaning that the buffers are full and the worker threads have not yet
returned free data -- then the reader discards the transaction that is
currently being read from the Varnish log. No data are buffered from
the transaction, leading to a loss of data. To avoid that, configure
the throughput of message sends and the size of the data buffers so
that free space is available as needed.

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
``varnish.name``     ``-n``     Like the ``-n`` option for Varnish, this is the directory containing the file that is     default for Varnish (the host name)
                                mmap'd to the shared memory segment for the Varnish log. This parameter and
                                ``varnish.bindump`` are mutually exclusive.
-------------------- ---------- ----------------------------------------------------------------------------------------- -------
``mq.module``                   Name of the shared object implementing the MQ interface. May be an absolute path, or the  None, this parameter is required.
                                SO name of a library that the dynamic linker finds according to the rules described in
                                ld.so(8).
-------------------- ---------- ----------------------------------------------------------------------------------------- -------
``mq.config_file``              Path of a configuration file used by the MQ implementation                                None, this parameter is optional.
-------------------- ---------- ----------------------------------------------------------------------------------------- -------
``nworkers``                    Number of worker threads used to send messages to the message broker(s).                  1
-------------------- ---------- ----------------------------------------------------------------------------------------- -------
``worker.stack``                Stack size for worker threads started by trackrdrd.                                       131072
                                Note: mq modules may start additional threads to which this limit does not apply
                                Observed actual stack sizes are <64k, so the default leaves plenty of room.               (128 KB)
                                Increase only if segmentation faults on stack addresses are observed
-------------------- ---------- ----------------------------------------------------------------------------------------- -------
``max.records``                 The maximum number of buffered records waiting to be sent to message brokers.             1024
-------------------- ---------- ----------------------------------------------------------------------------------------- -------
``max.reclen``                  The maximum length of a data record in characters. Should be at least as large the        1024
                                Varnish parameter ``shm_reclen``.
-------------------- ---------- ----------------------------------------------------------------------------------------- -------
``chunk.size``                  The size of fixed data blocks to store message data, as described above. This value may   256
                                not be smaller than 64.
-------------------- ---------- ----------------------------------------------------------------------------------------- -------
``maxkeylen``                   The maximum length of a sharding key. Keys longer than this limit are discarded, with an  128
                                error message in the log.
-------------------- ---------- ----------------------------------------------------------------------------------------- -------
``idle.pause``                  When the reader thread encounters the end of the Varnish log, i.e. no new transactions    0.01 seconds
                                have been added to the log since the last read, then the thread pauses for this length
                                of time in seconds. If the pause is too short, then the reader thread may waste CPU
                                time in a busy-wait loop. If too long, the reader may fall too far behind in the log
                                read, running a risk of log overruns.
-------------------- ---------- ----------------------------------------------------------------------------------------- -------
``tx.limit``         ``-L``     The upper limit for incomplete transactions to be aggregated by the Varnish logging API,  default for the logging API (1000 transactions)
                                as explained above.
-------------------- ---------- ----------------------------------------------------------------------------------------- -------
``tx.timeout``       ``-T``     The transaction timeout in seconds for the logging API, as explained above.               default for the logging API (120 seconds)
-------------------- ---------- ----------------------------------------------------------------------------------------- -------
``qlen.goal``                   A goal length for the internal queue from the reader thread to the worker threads.        ``max.records``/2
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

 Data table: len=1000 occ_rec=0 occ_rec_hi=8 occ_rec_hi_this=2 occ_chunk=0 occ_chunk_hi=8 occ_chunk_hi_this=2 global_free_rec=0 global_free_chunk=0
 Reader: seen=1896 submitted=1896 nodata=0 free_rec=1000 free_chunk=8000 no_free_rec=0 no_free_chunk=0 len_hi=728 key_hi=39 len_overflows=0 truncated=0 key_overflows=0 vcl_log_err=0 vsl_err=0 closed=0 overrun=0 ioerr=0 reacquire=0
 Workers: active=20 running=0 waiting=20 exited=0 abandoned=0 reconnects=0 restarts=0 sent=1896 failed=0 bytes=1050591

If monitoring of worker threads is switched on, then monitoring logs
such as this are emitted for each thread::

 Worker 1 (waiting): seen=105 waits=85 sent=105 bytes=57664 free_rec=0 free_chunk=0 reconnects=0 restarts=0 failed_recoverable=0 failed=0

The line prefixed by ``Data table`` describes the state of the data
buffers -- completed messages waiting to be forwarded by worker
threads.  The field ``len`` is constant; ``occ_rec_hi`` and
``occ_chunk_hi`` are monotone increasing.  All other fields are
gauges, expressing a current level that may rise or fall:

===================== ===================================================
Field                 Description
===================== ===================================================
``len``               Max number of records in the data table
--------------------- ---------------------------------------------------
``occ_rec``           Number of records currently buffered
--------------------- ---------------------------------------------------
``occ_rec_hi``        Occupancy high watermark for records -- highest
                      number of buffered records since startup
--------------------- ---------------------------------------------------
``occ_rec_hi_this``   Occupancy high watermark for records in the current
                      monitoring interval
--------------------- ---------------------------------------------------
``occ_chunk``         Number of chunks currently buffered
--------------------- ---------------------------------------------------
``occ_chunk_hi``      Occupancy high watermark for chunks since startup
--------------------- ---------------------------------------------------
``occ_chunk_hi_this`` Occupancy high watermark for chunks in the current
                      monitoring interval
--------------------- ---------------------------------------------------
``global_free_rec``   Current length of the global free record list
--------------------- ---------------------------------------------------
``global_free_chunk`` Current length of the global free record list
===================== ===================================================

The line prefixed by ``Reader`` describes the state of the reader
thread.  The fields ``free_rec`` and ``free_chunk`` are gauges, and
``len_hi`` and ``key_hi`` are monotone increasing; the rest are
cumulative counters:

================== ============================================================
Field              Description
================== ============================================================
``seen``           Number of log transactions read since startup, natching the
                   filters for the tracking reader as shown above
------------------ ------------------------------------------------------------
``submitted``      Number of records passed from the reader thread to worker
                   threads, to be sent to message brokers
------------------ ------------------------------------------------------------
``no_data``        Number of log transactions read with no data payloads in the
                   ``VCL_Log`` entries
------------------ ------------------------------------------------------------
``free_rec``       Number of records in the reader thread's local free list
------------------ ------------------------------------------------------------
``free_chunk``     Number of chunks in the reader thread's local free list
------------------ ------------------------------------------------------------
``no_free_rec``    How often data was discarded because no free records were
                   available
------------------ ------------------------------------------------------------
``no_free_chunk``  How often data was discarded because no free chunks were
                   available
------------------ ------------------------------------------------------------
``len_hi``         Length high watermark -- longest complete message formed
                   since startup
------------------ ------------------------------------------------------------
``key_hi``         Key length high watermark -- longest sharding key since
                   startup
------------------ ------------------------------------------------------------
``len_overflows``  How often the length of a message exceeded ``max.reclen``
------------------ ------------------------------------------------------------
``truncated``      How often data from the Varnish log was truncated due to
                   the presence of a null byte. This can happen if the data was
                   already truncated in the log, due to exceeding
                   ``shm_reclen``.
------------------ ------------------------------------------------------------
``key_overflows``  How often the length of a sharding key exceeded
                   ``maxkeylen``
------------------ ------------------------------------------------------------
``vcl_log_err``    How often a ``VCL_Log`` entry beginning with ``track`` could
                   not be parsed
------------------ ------------------------------------------------------------
``vsl_err``        Number of errors/warnings signaled by the Varnish logging
                   API with a ``VSL`` entry in the log transaction
------------------ ------------------------------------------------------------
``closed``         Number of times the Varnish log was closed or abandoned
------------------ ------------------------------------------------------------
``overrun``        Number of times log reads were overrun
------------------ ------------------------------------------------------------
``ioerr``          Number of times log reads failed due to I/O errors
------------------ ------------------------------------------------------------
``reacquire``      Number of times the Varnish log was re-acquired
================== ============================================================

The line prefixed by ``Workers`` gives an overview of the worker
threads.  The field ``active`` is constant, and ``running`` and
``waiting`` are gauges; the rest are cumulative counters:

================== ============================================================
Field              Description
================== ============================================================
``active``         Number of worker threads created, equal to the config param
                   ``nworkers``
------------------ ------------------------------------------------------------
``running``        Number of worker threads currently in the running state
------------------ ------------------------------------------------------------
``waiting``        Number of threads currently in the waiting state
------------------ ------------------------------------------------------------
``exited``         Number of threads currently in the exited state
------------------ ------------------------------------------------------------
``abandoned``      Number of worker threads that have been abandoned due to
                   reaching the restart limit (``thread.restarts``)
------------------ ------------------------------------------------------------
``reconnects``     How often worker threads reconnected to a message broker
                   after an unsuccessful send
------------------ ------------------------------------------------------------
``restarts``       How often worker threads were restarted after a message
                   send, reconnect and resend all failed
------------------ ------------------------------------------------------------
``sent``           Total number of messages successfully sent to a message
                   broker
------------------ ------------------------------------------------------------
``failed``         Number of failed sends (failure after reconnect, or after
                   non-recoverable failures of the message plugin)
------------------ ------------------------------------------------------------
``bytes``          Total number of bytes in successfully sent messages
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
thread is actively reading data buffers and sending them to message
brokers, or ``waiting``, when the threads have exhausted all pending
records, or has not yet been awakened to handle more records.

The fields ``free_rec`` and ``free_chunks`` are gauges, and all other
fields in a log line for a worker thread are cumulative counters:

====================== ========================================================
Field                  Description
====================== ========================================================
``seen``               Number of messages read by the worker thread from the
                       internal queue (which is filled by the reader thread)
---------------------- --------------------------------------------------------
``waits``              How often the worker thread was in the waiting state (no
                       new messages on the queue)
---------------------- --------------------------------------------------------
``sent``               Number of messages successfully sent by the worker
                       thread
---------------------- --------------------------------------------------------
``bytes``              Total number of bytes in messages successfully sent by
                       the worker
---------------------- --------------------------------------------------------
``free_rec``           Number of records currently in the worker's local free
                       list
---------------------- --------------------------------------------------------
``free_chunk``         Number of chunks currently in the worker's local free
                       list
---------------------- --------------------------------------------------------
``reconnects``         How often this worker reconnected to a message broker
                       after an unsuccessful send
---------------------- --------------------------------------------------------
``restarts``           How often this worker was restarted after a message
                       send, reconnect and resend all failed, or after
                       non-recoverable message failures
---------------------- --------------------------------------------------------
``failed_recoverable`` How often this worker had recoverable message failures
                       (failures that do not corrupt the state of the message
                       plugin and do not require thread restart)
---------------------- --------------------------------------------------------
``failed``             Number of non-recoverable message failures, requiring a
                       thread restart
====================== ========================================================

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
HUP    Graceful   Flush
       restart    transactions
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
that the first process finishes reading data for open log
transactions, and the second one begins reading data for new requests,
so that as few records as possible are lost. The new process reads the
same config files as the original worker process, and retains any
command-line configuration, unless these values are overridden by
config files. This allows for configuration changes "on-the-fly".

On receiving signal ``USR1``, the worker process writes the contents
of all buffered data as well as the current configuration to the log
(syslog, or log file specified by config), for troubleshooting or
debugging.

On receivng the ``HUP`` signal, the worker process requests the
Varnish log API to flush all transactions that it is currently
aggregating, even if they are not yet complete (to the ``End`` tag).
These are consumed by the reader thread and processed normally
(although data may be missing).

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
* ``libtrackrdr-file(3)``
* ``libtrackrdr-kafka(3)``
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
