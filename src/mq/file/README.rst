.. _ref-trackrdrd:

=================
 libtrackrdr-file
=================

-------------------------------------------------------------------
File implementation of the MQ interface for the Tracking Log Reader
-------------------------------------------------------------------

:Author: Geoffrey Simmons
:Date:   2015-04-27
:Version: 1.0.0
:Manual section: 3


DESCRIPTION
===========

``libtrackrdr-file.so`` provides an implementation of the tracking
reader's MQ interface to write messages to a file, as an aid to
testing and debugging. See ``include/mq.h`` in the ``trackrdrd``
source distribution for documentation of the interface.

To use this implementation with ``trackrdrd``, specify the shared
object as the value of ``mq.module`` in the tracking reader's
configuration (see trackrdrd(3)). The configuration value may be the
absolute path of the shared object; or its name, provided that it can
be found by the dynamic linker (see ld.so(8)).

``libtrackrdr-file`` also requires a configuration file, whose path is
specified as ``mq.config_fname`` in the configuration of
``trackrdrd``.

BUILD/INSTALL
=============

The sources for ``libtrackrdr-file`` are provided in the source
repository for ``trackrdrd``, in the subdirectory ``src/mq/file/``.

``libtrackrdr-file`` is built as part of the global build for
``trackrdrd``; for details and requirements of the build, see
trackrdrd(3).

To specifically build the MQ implementation (without building all of
the rest of ``trackrdrd``), it suffices to invoke ``make`` commands in
the subdirectory ``src/mq/file`` (after having executed the
``configure`` script for ``trackrdrd``)::

        # in the trackrdrd repo
	$ cd src/mq/file
	$ make

The global ``make`` command for ``trackrdrd`` also executes both of
these for the file plugin.

To install the shared object ``libtrackrdr-file.so``, run ``make
install`` as root, for example with ``sudo``::

	$ sudo make install

In standard configuration, the ``.so`` file will be installed by
``libtool(1)``, and its location may be affected by the ``--libdir``
option to ``configure``.

CONFIGURATION
=============

As mentioned above, a configuration file for ``libtrackrdr-file``
MUST be specified in the configuration parameter ``mq.config_fname``
for ``trackrdrd``, and initialization of the MQ implementation fails
if this file cannot be found or read by the process owner of
``trackrdrd`` (or if its syntax is false, or if required parameters
are missing).

The syntax of the configuration file is the same as that of
``trackrdrd``.

These parameters can be specified:

=================================== ============================================
Parameter                           Description
=================================== ============================================
``output.file``                     The file to which messages are written.
                                    If exactly equal to ``-``, then messages
                                    are written to stdout.
----------------------------------- --------------------------------------------
``append``                          If true, then the output file is appended
                                    to, otherwise it is overwritten. True by
                                    default. This parameter has no effect if
                                    output is written to stdout.
=================================== ============================================

The parameter ``output.file`` MUST be set.

SEE ALSO
========

* ``trackrdrd(3)``
* ``ld.so(8)``

COPYRIGHT AND LICENCE
=====================

Both the software and this document are governed by a BSD 2-clause
licence.

| Copyright (c) 2015 UPLEX Nils Goroll Systemoptimierung
| Copyright (c) 2015 Otto Gmbh & Co KG
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
