AC_PREREQ(2.59)
AC_COPYRIGHT([Copyright (c) 2012 Otto Gmbh & Co KG])
AC_INIT([trackrdrd], [0.1])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_SRCDIR(src/trackrdrd.c)
AM_CONFIG_HEADER(config.h)

AC_CANONICAL_SYSTEM
AC_LANG(C)

AM_INIT_AUTOMAKE([foreign])

AC_GNU_SOURCE
AC_PROG_CC
AC_PROG_CXX
AC_PROG_CC_STDC
if test "x$ac_cv_prog_cc_c99" = xno; then
	AC_MSG_ERROR([Could not find a C99 compatible compiler])
fi
AC_PROG_CPP
AC_PROG_CXXCPP

AX_PTHREAD(,[AC_MSG_ERROR([Could not configure pthreads support])])

LIBS="$PTHREAD_LIBS $LIBS"
CFLAGS="$CFLAGS $PTHREAD_CFLAGS"
CXXFLAGS="$CXXFLAGS $PTHREAD_CFLAGS"
CC="$PTHREAD_CC"

AC_PROG_INSTALL
AC_PROG_LIBTOOL
AC_PROG_MAKE_SET

# Check for rst utilities
AC_CHECK_PROGS(RST2MAN, [rst2man rst2man.py], "no")
if test "x$RST2MAN" = "xno"; then
	AC_MSG_WARN([rst2man not found - not building man pages])
fi
AM_CONDITIONAL(HAVE_RST2MAN, [test "x$RST2MAN" != "xno"])

# Check for pkg-config
PKG_PROG_PKG_CONFIG

# Checks for header files.
AC_HEADER_STDC
#AC_CHECK_HEADERS([sys/stdlib.h])

# Varnish source tree
AC_ARG_VAR([VARNISHSRC], [path to Varnish source tree (mandatory)])
if test "x$VARNISHSRC" = x; then
	AC_MSG_ERROR([No Varnish source tree specified])
fi
VARNISHSRC=`cd $VARNISHSRC && pwd`
AC_CHECK_FILE([$VARNISHSRC/include/varnishapi.h],
	[],
	[AC_MSG_FAILURE(["$VARNISHSRC" is not a Varnish source directory])]
)

PKG_CHECK_MODULES([AMQ], [activemq-cpp])

# From Varnish configure.ac
# Now that we're done using the compiler to look for functions and
# libraries, set CFLAGS to what we want them to be for our own code

# This corresponds to FreeBSD's WARNS level 6
DEVELOPER_CFLAGS="-Wall -Wstrict-prototypes -Wmissing-prototypes -Wpointer-arith -Wreturn-type -Wcast-qual -Wwrite-strings -Wswitch -Wshadow -Wcast-align -Wunused-parameter -Wchar-subscripts -Winline -Wnested-externs -Wredundant-decls -Wformat"

# Added for compiling C++ for ActiveMQ.
# -Wstrict-prototypes, -Wmissing-prototypes & -Wnested-externs invalid for C++
# activemq-cpp fails on -Wshadow
DEVELOPER_CXXFLAGS="-Wall -Wpointer-arith -Wreturn-type -Wcast-qual -Wwrite-strings -Wswitch -Wcast-align -Wunused-parameter -Wchar-subscripts -Winline -Wredundant-decls -Wformat"

# Additional flags for GCC 4
EXTRA_DEVELOPER_CFLAGS="-Wextra -Wno-missing-field-initializers -Wno-sign-compare"

# --enable-developer-warnings
AC_ARG_ENABLE(developer-warnings,
        AS_HELP_STRING([--enable-developer-warnings],[enable strict warnings (default is NO)]),
        CFLAGS="${CFLAGS} ${DEVELOPER_CFLAGS}"
        CXXFLAGS="${CXXFLAGS} ${DEVELOPER_CXXFLAGS}"
        )

# --enable-debugging-symbols
AC_ARG_ENABLE(debugging-symbols,
        AS_HELP_STRING([--enable-debugging-symbols],[enable debugging symbols (default is NO)]),
        CFLAGS="${CFLAGS} -O0 -g -fno-inline"
        CXXFLAGS="${CXXFLAGS} -O0 -g -fno-inline"
        )

# --enable-diagnostics
AC_ARG_ENABLE(diagnostics,
        AS_HELP_STRING([--enable-diagnostics],[enable run-time diagnostics (default is NO)]),
        CFLAGS="${CFLAGS} -DDIAGNOSTICS"
        CXXFLAGS="${CXXFLAGS} -DDIAGNOSTICS"
        )

# --enable-extra-developer-warnings
AC_ARG_ENABLE(extra-developer-warnings,
        AS_HELP_STRING([--enable-extra-developer-warnings],[enable even stricter warnings (default is NO)]),
        [],
        [enable_extra_developer_warnings=no])

# --enable-werror
AC_ARG_ENABLE(werror,
        AS_HELP_STRING([--enable-werror],[use -Werror (default is NO)]),
        CFLAGS="${CFLAGS} -Werror"
        CXXFLAGS="${CXXFLAGS} -Werror"
        )

AC_CONFIG_FILES([
	Makefile
	src/Makefile
        src/test/Makefile
])
AC_OUTPUT
