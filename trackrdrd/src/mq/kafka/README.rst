.. _ref-trackrdrd:

==================
 libtrackrdr-kafka
==================

--------------------------------------------------------------------
Kafka implementation of the MQ interface for the Tracking Log Reader
--------------------------------------------------------------------

:Author: Geoffrey Simmons
:Date:   2014-06-01
:Version: 3.0.0
:Manual section: 3


DESCRIPTION
===========

``libtrackrdr-kafka.so`` provides an implementation of the tracking
reader's MQ interface to send messages to Apache Kafka message
brokers. See ``include/mq.h`` in the ``trackrdrd`` source distribution
for documentation of the interface.

To use this implementation with ``trackrdrd``, specify the shared
object as the value of ``mq.module`` in the tracking reader's
configuration (see trackrdrd(3)). The configuration value may be the
absolute path of the shared object; or its name, provided that it can
be found by the dynamic linker (see ld.so(8)).

``libtrackrdr-kafka`` also requires a configuration file, whose path
is specified as ``mq.config_fname`` in the configuration of
``trackrdrd``.

``libtrackrdrd-kafka`` in turn depends on these libraries:

* ``rdkafka``, a client library for Kafka
* ``zookeeper_mt``, a client library for Apache ZooKeeper with
  multi-threading
* ``pcre``, a regular expression library (used for JSON parsing)

The dynamic linker must also be able to find ``librdkafka.so``,
``libzookeeper_mt.so`` and ``libpcre.so`` at runtime.

BUILD/INSTALL
=============

The sources for ``libtrackrdr-kafka`` are provided in the source
repository for ``trackrdrd``, in the subdirectory ``src/mq/kafka/``
of::

	git@git.lhotse.ov.otto.de:lhotse-tracking-varnish

The sources for the library dependencies can be obtained from:

* https://github.com/edenhill/librdkafka
* http://zookeeper.apache.org/
* http://www.pcre.org/

Building and installing the library dependencies
------------------------------------------------

The Kafka interface has been tested with these library versions:

* rdkafka 0.8.3
* zookeeper_mt 3.4.6
* pcre 8.30 2012-02-04

If the libraries are already installed on the platform where
``trackrdrd`` will run, then no further action is necessary. This is
almost certainly the case for the pcre library, since it is a
requirement for Varnish.

To build the libraries from source, it suffices to follow the
instructions in the source distributions -- no special configuration
for the plugin is necessary.

Building and installing libtrackrdr-kafka
-----------------------------------------

``libtrackrdr-kafka`` is built as part of the global build for
``trackrdrd``; for details and requirements of the build, see
trackrdrd(3).

To specifically build the MQ implementation (without building all of
the rest of ``trackrdrd``), it suffices to invoke ``make`` commands in
the subdirectory ``src/mq/kafka`` (after having executed the
``configure`` script for ``trackrdrd``)::

        # in lhotse-tracking-varnish/trackrdrd
	$ cd src/mq/kafka
	$ make

For self-tests after the build::

        $ cd src/mq/kafka/test
	$ make check

The global ``make`` and ``make check`` commands for ``trackrdrd`` also
execute both of these for the Kafka plugin.

The self-tests depend on the configuration file ``kafka.conf`` in the
``test/`` subdirectory, which specifies ``localhost:2181`` as the
address of a ZooKeeper server. If a ZooKeeper is listening, then tests
are run against that instance of ZooKeeper and any running Kafka
brokers that the ZooKeeper server is managing. If connections to a
ZooKeeper server or Kafka brokers fail, then the ``make check`` test
exits with the status ``SKIPPED``.

To install the shared object ``libtrackrdr-kafka.so``, run ``make
install`` as root, for example with ``sudo``::

	$ sudo make install

In standard configuration, the ``.so`` file will be installed by
``libtool(1)``, and its location may be affected by the ``--libdir``
option to ``configure``.

CONFIGURATION
=============

As mentioned above, a configuration file for ``libtrackrdr-kafka``
MUST be specified in the configuration parameter ``mq.config_fname``
for ``trackrdrd``, and initialization of the MQ implementation fails
if this file cannot be found or read by the process owner of
``trackrdrd`` (or if its syntax is false, or if required parameters
are missing).

The syntax of the configuration file is the same as that of
``trackrdrd``, and it may contain configuration parameters for
``rdkafka``, except as noted below -- thus the configuration applies
to both the messaging plugin and the ``rdkafka`` client library.

If the config parameter ``zookeeper.connect`` is set, then the plugin
obtains information about Kafka brokers from the specified ZooKeeper
server(s), and the value of the ``rdkafka`` parameter
``metadata.broker.list`` is ignored. If ``zookeeper.connect`` is not
set, then an initial list brokers MUST be specified by
``metadata.broker.list`` -- if neither of ``zookeeper.connect`` and
``metadata.broker.list`` are set, then the configuration fails and
``trackrdrd`` will exit.

The ``topic`` parameter MUST be set.

In addition to configuration parameters for ``rdkafka``, these
parameters can be specified:

===================== ==========================================================
Parameter             Description
===================== ==========================================================
``zookeeper.connect`` Comma-separated list of ``host:port`` pairs specifying
                      the addresses of ZooKeeper servers. If not set, then
                      ``metadata.broker.list`` MUST be set, as described above.
--------------------- ----------------------------------------------------------
``zookeeper.timeout`` Timeout in milliseconds for connections to ZooKeeper
                      servers. If 0, then a connection attempt fails immediately
                      if the servers cannot be reached. (optional, default 0)
--------------------- ----------------------------------------------------------
``zookeeper.log``     Path of a log file for the ZooKeeper client (optional)
--------------------- ----------------------------------------------------------
``mq.log``            Path of a log file for the messaging plugin and Kafka
                      client (optional)
--------------------- ----------------------------------------------------------
``topic``             Name of the Kafka topic to which messages are sent
                      (required)
--------------------- ----------------------------------------------------------
``mq.debug``          If set to true, then log at DEBUG level
===================== ==========================================================

Except as noted below, the configuration can specify any parameters for
the ``rdkafka`` client, as documented at::

	https://github.com/edenhill/librdkafka/blob/master/CONFIGURATION.md

The following ``rdkafka`` parameters in the config file are ignored
(they are set internally by the messaging plugin, or are only relevant
to consumers):

* ``client.id``
* ``error_cb``
* ``stats_cb``
* ``log_cb``
* ``socket_cb``
* ``open_cb``
* ``opaque``
* ``queued.*``
* ``fetch.*``
* ``group.id``
* ``dr_cb``
* ``dr_msg_cb``
* ``partitioner``
* ``opaque``
* ``auto.*``
* ``offset.*``

SHARDING
========

The plugin requires that calls to ``MQ_Send()`` supply a hexadecimal
string of up to 8 characters as the sharding key; ``MQ_Send()`` fails
if a key is not specified, or if it contains non-hex characters in the
first 8 bytes.

Only the first 8 hex digits of the key are significant; if the string
is longer, then the remainder of the key from the 9th byte is ignored.

LOGGING AND STATISTICS
======================

The parameter ``mq.log`` sets the path of a log file for
informational, error and debug messages from both the messaging plugin
and the rdkafka client library. If the parameter is not set, then no
log file is written.

If the rdkafka parameter ``statistics.interval.ms`` is set and
non-zero, then statistics from both the plugin and the client library
are emitted to the log at that interval for each worker object
(i.e. for each worker thread of the tracking reader).

Log lines beginning with ``rdkafka stats`` contain statistics from the
rdkafka library for a worker object. The format and content of these
lines are determined by the rdkafka library.

Log lines beginning with ``mq stats`` are generated by the MQ plugin,
and have the following form (possibly with additional formatting and
information from the logger)::

        mq stats (ID = $CLIENTID): seen=2 produced=2 delivered=2 failed=0 nokey=0 badkey=0 nodata=0
        mq stats summary: seen=47 produced=47 delivered=47 failed=0 nokey=0 badkey=0 nodata=0

``$CLIENTID`` is the ID of a worker object (as returned from
``MQ_ClientID()``), and the statistics in that line pertain to that
object. The line containing ``mq stats summary`` contains sums of the
stats for all worker objects.

The statistics are all cumulative counters:

===================== ==========================================================
Statistic             Description
===================== ==========================================================
``seen``              The number of times that ``MQ_Send()`` was called
--------------------- ----------------------------------------------------------
``produced``          The number of successful invocations of the rdkafka
                      client library's "produce" operation
--------------------- ----------------------------------------------------------
``delivered``         The number of messages successfully delivered to a broker
--------------------- ----------------------------------------------------------
``failed``            The number of failures, either of "produce" or failed
                      deliveries to a broker
--------------------- ----------------------------------------------------------
``nokey``             The number of ``MQ_Send()`` operations called with no
                      shard key.
--------------------- ----------------------------------------------------------
``badkey``            The number of send operations called with an illegal
                      shard key (not a hex string in the first 8 bytes)
--------------------- ----------------------------------------------------------
``nodata``            The number of send operations called with no message
                      payload.
===================== ==========================================================

The log level can be toggled to DEBUG and back by sending signal
``USR2`` to the process, as described below.

MESSAGE SEND FAILURE AND RECOVERY
=================================

The messaging plugin uses the rdkafka client library, whose send
operations are asynchronous -- messages to be sent are placed on an
internal queue, from which they are sent to Kafka brokers as
determined by the ``queue.*`` configuration parameters. Unless there
is some exceptional condition (for example, the internal queue is
full), rdkafka's "produce" operation succeeds immediately after the
message is placed on the queue. If a failure occurs when delivery of a
message to a broker is attempted, then the rdkafka library saves the
error status in its internal state, but this ordinarily becomes known
some time after the "produce" operation has been completed.

The rdkafka library attempts error recovery on its own, for example by
restoring lost connections to brokers, and then retries the delivery
of messages that failed on prior attemepts.

This means that in ordinary operation, the plugin's ``MQ_Send()`` call
will not fail immediately if in fact it turns out that, on the first
attempt, the message cannot be delivered to a broker. The only
unrecoverable error for ``MQ_Send()`` occurs when the "produce"
operation fails immediately (such as when an rdkafka queue is full).

The messaging plugin polls the internal state of an rdkafka producer
associated with a worker object during ``MQ_Send()`` once before
invoking the "produce" operation, once afterward, and also every time
rdkafka internal statistics are queried as described above. If a prior
error state is determined during the call to ``MQ_Send()``, then a log
message at level ERROR is generated. It should be understood these
messages describe an error that may have occurred at an earlier point
in time, and recovery may have already succeeded (which can be
ascertained from messages that appear earlier in the log).

SIGNALS
=======

The message plugin overrides the signal handler of the tracking
reader's child process for signal ``USR2`` (see signal(7)), so that it
toggles the DEBUG log level when the process receives the signal.

The initial log level is set by the configuration parameter
``mq.debug`` when the plugin is initialized, and the level is changed
from this level to DEBUG, or from DEBUG back to the initial level,
when ``USR2`` is sent to the process (for example by using
kill(1)). Log level toggling affects logging for the messaging plugin
as well as the rdkafka and zookeeper client libraries.

Logging at DEBUG level may be very verbose, so that log files may
become very large (and partitions may overflow) if DEBUG level is left
on for a long time.

SEE ALSO
========

* ``trackrdrd(3)``
* ``ld.so(8)``
* http://kafka.apache.org/
* http://zookeeper.apache.org/
* https://github.com/edenhill/librdkafka
* http://zookeeper.apache.org/doc/r3.4.6/zookeeperProgrammers.html#C+Binding

COPYRIGHT AND LICENCE
=====================

Both the software and this document are governed by a BSD 2-clause
licence.

| Copyright (c) 2014 UPLEX Nils Goroll Systemoptimierung
| Copyright (c) 2014 Otto Gmbh & Co KG
| All rights reserved
| Use only with permission

| Author: Geoffrey Simmons <geoffrey.simmons@uplex.de>

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
