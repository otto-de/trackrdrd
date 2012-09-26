.. _ref-varnishd:

==========
 trackrdrd
==========

-------------------------
Tracking Log Reader demon
-------------------------

:Author: Geoffrey Simmons
:Date:   2012-09-23
:Version: 0.1
:Manual section: 3


SYNOPSIS
========

.. include:: synopsis.txt

DESCRIPTION
===========

The ``trackrdrd`` demon reads from the shared memory log of a running
instance of Varnish and collects data relevant to tracking for the
Otto project.

OPTIONS
=======

.. include:: options.txt

BUILD/INSTALL
=============

The source repository for ``trackrdrd`` is in the subdirectory
``trackrdrd/`` of::

	git@repo.org:trackrdrd

The build requires a source directory for Varnish in which sources
have been compiled. Varnish sources with features added for Otto are
in::

	git@repo.org:varnish-cache

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
which custom features for Otto are implemented::

	$ git clone git@repo.org:varnish-cache
	$ cd varnish-cache/
	$ git checkout 3.0_bestats

Varnish as deployed for Otto is built in 64-bit mode, and since
``trackrdrd`` needs to link with its libraries, it must be built in
64-bit mode as well. This means that the Varnish build for
``trackrdrd`` must also be 64-bit; for ``gcc``, this is accomplished
with ``CFLAGS=-m64``.

The following sequence builds Varnish as needed for the ``trackrdrd``
build::

	$ ./autogen.sh
	$ CFLAGS=-m64 ./configure
	$ make

Building and installing trackrdrd
---------------------------------

Requirements for ``trackrdrd`` are the same as for Varnish, in
addition to the Varnish build itself. (``pcre-devel`` is not strictly
necessary for ``trackrdrd``, but since you are building ``trackrdrd``
on the same platform as the Varnish build, all requirements are
fulfilled.)

The steps to build ``trackrdrd`` are very similar to those for
building Varnish. The only difference is that in the ``configure``
step, the path to the Varnish source directory must be given in the
variable ``VARNISHSRC``::

	$ git clone git@repo.org:trackrdrd
	$ cd trackrdrd/trackrdrd/
	$ ./autogen.sh
	$ CFLAGS=-m64 ./configure VARNISHSRC=/path/to/varnish-cache
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

	$ CFLAGS=-m64 ./configure VARNISHSRC=/path/to/varnish-cache \\
	  --prefix=/path/to/varnish_tracking

Developers can add a number of options as an aid to compiling and debugging::

	$ CFLAGS=-m64 ./configure VARNISHSRC=/path/to/varnish-cache \\
          --enable-developer-warnings --enable-debugging-symbols \\
          --enable-extra-developer-warnings --enable-werror

``--enable-werror`` activates the ``-Werror`` option for compilers,
which causes compiles to fail on any warning. ``trackrdrd`` should
*always* build successfully with this option.

``--enable-developer-warnings`` and
``--enable-extra-developer-warnings`` turn on additional compiler
switches for warnings -- ``trackrdrd`` builds should succeed with
these as well.

``--enable-debugging-symbols`` ensures that symbols and source code
file names are saved in the executable, and thus are available in core
dumps, in stack traces on assertion failures, for debuggers and so
forth. It is advisable to turn this switch on for production builds
(not just for developer builds), so that runtime errors can more
easily be debugged.
