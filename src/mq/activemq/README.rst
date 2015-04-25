.. _ref-trackrdrd:

=====================
 libtrackrdr-activemq
=====================

-----------------------------------------------------------------------
ActiveMQ implementation of the MQ interface for the Tracking Log Reader
-----------------------------------------------------------------------

:Author: Geoffrey Simmons
:Date:   2014-05-20
:Version: 3.0
:Manual section: 3


DESCRIPTION
===========

``libtrackrdr-activemq.so`` provides an implementation of the tracking
reader's MQ interface to send messages to ActiveMQ message
brokers. See ``include/mq.h`` in the ``trackrdrd`` source distribution
for documentation of the interface.

To use this implementation with ``trackrdrd``, specify the shared
object as the value of ``mq.module`` in the tracking reader's
configuration (see trackrdrd(3)). The configuration value may be the
absolute path of the shared object; or its name, provided that it can
be found by the dynamic linker (see ld.so(8)).

``libtrackrdr-activemq`` also requires a configuration file, whose
path is specified as ``mq.config_fname`` in the configuration of
``trackrdrd``.

``libtrackrdrd-activemq`` in turn depends on the library ActiveMQ-CPP
(or CMS), a client library for ActiveMQ, packaged on many systems as
``libactivemq-cpp``. The dynamic linker must also be able to find
``libactivemq-cpp.so`` at runtime.

This implementation does not use sharding keys; the key data in the
call to ``MQ_Send()`` are silently discarded.

BUILD/INSTALL
=============

The sources for ``libtrackrdr-activemq`` are provided in the source
repository for ``trackrdrd``, in the subdirectory ``src/mq/activemq/``
of::

	git@git.lhotse.ov.otto.de:lhotse-tracking-varnish

The sources for ActiveMQ-CPP can be obtained from::

        http://activemq.apache.org/cms/

Building and installing ActiveMQ-CPP
------------------------------------

The ActiveMQ interface has been tested with versions 3.4.4 through
3.8.2 of ActiveMQ-CPP. If the library ``libactivemq-cpp`` is already
installed on the platform where ``trackrdrd`` will run, then no
further action is necessary. To build the library from source, follow
the instructions in the ``README.txt`` file of its source
distribution.

Building and installing libtrackrdr-activemq
--------------------------------------------

``libactivemq-trackrdr`` is built as part of the global build for
``trackrdrd``; for details and requirements of the build, see
trackrdrd(3). Note that for the ActiveMQ implementation, which
includes C++ classes, it is necessary to configure the build for C++
compilation (for example by setting ``CXXFLAGS``), and it may be
necessary to add additional configuration for compiling and linking
with the ActiveMQ-CPP library, for example as obtained from
``pkg-config --cflags apr-1``.

To specifically build the MQ implementation (without building all of
the rest of ``trackrdrd``), it suffices to invoke ``make`` commands in
the subdirectory ``src/mq/activemq`` (after having executed the
``configure`` script for ``trackrdrd``)::

        # in lhotse-tracking-varnish/trackrdrd
	$ cd src/mq/activemq
	$ make

For self-tests after the build, run::

	$ make check

If a connection is open at port 61616 on the host where the self-tests
are run, then it is assumed that an ActiveMQ message broker is
listening, and tests are run against the MQ URI
``tcp://localhost:61616``. If the port is not open, then the ``make
check`` test exits with the status ``SKIPPED``.

(When ``make check`` is run globally for the ``trackrdrd`` build, then
it is also run for ``libtrackrdr-activemq``).

To install the shared object ``libtrackrdr-activemq``, run ``make
install`` as root, for example with ``sudo``::

	$ sudo make install

In standard configuration, the ``.so`` file will be installed by
``libtool(1)``, and its location may be affected by the ``--libdir``
option to ``configure``.

CONFIGURATION
=============

As mentioned above, a configuration file for ``libtrackrdr-activemq``
must be specified in the configuration parameter ``mq.config_fname``
for ``trackrdrd``, and initialization of the MQ implementation fails
if this file cannot be found or read by the process owner of
``trackrdrd`` (or if its syntax is false, or if either of the two
required parameters are missing).

The syntax of the configuration file is the same as that of
``trackrdrd``. Both of the parameters ``mq.uri`` and ``mq.qname`` are
required (there are no default values), and (only) ``mq.uri`` may be
specified more than once.

================== ============================================================
Parameter          Description
================== ============================================================
``mq.uri``         URIs for the message broker. If more than one MQ URI is
                   specified, than worker threads distribute their connections
                   to the different message brokers.
------------------ ------------------------------------------------------------
``mq.qname``       Name of the queue (destination) to which messages are sent
                   at the message broker(s)
================== ============================================================

SEE ALSO
========

* ``trackrdrd(3)``
* ``ld.so(8)``
* http://activemq.apache.org/cms/

COPYRIGHT AND LICENCE
=====================

For both the software and this document are governed by a BSD 2-clause
licence.

| Copyright (c) 2012-2014 UPLEX Nils Goroll Systemoptimierung
| Copyright (c) 2012-2014 Otto Gmbh & Co KG
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
