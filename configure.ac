AC_PREREQ(2.63)
AC_COPYRIGHT([Copyright (c) 2012-2015 Otto Gmbh & Co KG])
AC_INIT([trackrdrd], [4.0])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_SRCDIR(src/trackrdrd.c)
AM_CONFIG_HEADER(config.h)

AC_CANONICAL_SYSTEM
AC_LANG(C)

AM_INIT_AUTOMAKE([foreign])
AC_USE_SYSTEM_EXTENSIONS

LT_PREREQ(2.2)
LT_INIT

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

PKG_CHECK_MODULES([VARNISH], [varnishapi >= 4.0.0],
                  [ac_varnish_pkgdatadir=`$PKG_CONFIG --variable=pkgdatadir varnishapi`],
                  [AC_MSG_ERROR([Varnish >= 4.0.0 installation is required])])
if test "x$ac_varnish_pkgdatadir" = x; then
   AC_MSG_ERROR([couldn't get Varnish data dir from pkg-config])
fi
ac_varnish_pkgincludedir=`$PKG_CONFIG --variable=pkgincludedir varnishapi`
ac_varnish_libdir=`$PKG_CONFIG --variable=libdir varnishapi`
AC_SUBST(VARNISH_CFLAGS)
AC_SUBST(VARNISH_LIBS)
AC_SUBST(VARNISH_PKG_INCLUDE, $ac_varnish_pkgincludedir)
AC_SUBST(VARNISH_SHARE_INCLUDE, "$ac_varnish_pkgdatadir/include")
AC_SUBST(VARNISH_PKG_LIB, $ac_varnish_libdir)
AC_SUBST(VARNISH_LIBVARNISH_LIB, "-L$ac_varnish_libdir/varnish")

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([execinfo.h])

## From Varnish configure.ac
# white lie - we don't actually test it
AC_MSG_CHECKING([whether daemon() works])
case $target in
*-*-darwin*)
        # present but not functional
        AC_MSG_RESULT([no])
        ac_cv_func_daemon=no
        ;;
*)
        AC_CHECK_FUNCS([daemon])
        ;;
esac

PKG_CHECK_MODULES([AMQ], [activemq-cpp])
PKG_CHECK_MODULES([APR], [apr-1])
PKG_CHECK_MODULES([APU], [apr-util-1])

# optionally choose the MQ plugins to build, by default all
AC_ARG_ENABLE([kafka], [AS_HELP_STRING([--enable-kafka],
                       [build the Kafka MQ plugin @<:@default=yes@:>@])],
                       [], [enable_kafka=yes])
AM_CONDITIONAL([KAFKA_OPT], [test "$enable_kafka" = yes])

AC_ARG_ENABLE([activemq], [AS_HELP_STRING([--enable-activemq],
                          [build the ActiveMQ MQ plugin @<:@default=yes@:>@])],
                          [], [enable_activemq=yes])
AM_CONDITIONAL([AMQ_OPT], [test "$enable_activemq" = yes])

# From Varnish configure.ac
# Now that we're done using the compiler to look for functions and
# libraries, set CFLAGS to what we want them to be for our own code

# This corresponds to FreeBSD's WARNS level 6
DEVELOPER_CFLAGS=" \
	-Werror \
	-Wall \
	-Wno-format-y2k \
	-W \
	-Wstrict-prototypes \
	-Wmissing-prototypes \
	-Wpointer-arith \
	-Wreturn-type \
	-Wcast-qual \
	-Wwrite-strings \
	-Wswitch \
	-Wshadow \
	-Wunused-parameter \
	-Wcast-align \
	-Wchar-subscripts \
	-Wnested-externs \
	-Wextra \
	-Wno-sign-compare \
"

# Added for compiling C++ for ActiveMQ.
# -Wstrict-prototypes, -Wmissing-prototypes & -Wnested-externs invalid for C++
# activemq-cpp fails on -Wshadow and -Wcast-qual
DEVELOPER_CXXFLAGS=" \
	-Werror \
	-Wall \
	-Wno-format-y2k \
	-W \
	-Wpointer-arith \
	-Wreturn-type \
	-Wwrite-strings \
	-Wswitch \
	-Wunused-parameter \
	-Wcast-align \
	-Wchar-subscripts \
	-Wextra \
	-Wno-sign-compare \
"

# These are not compliable yet
DEVELOPER_GCC_CFLAGS="-Wold-style-definition -Wredundant-decls "
#DEVELOPER_CFLAGS="${DEVELOPER_CFLAGS} ${DEVELOPER_GCC_CFLAGS}"

# These are compilable
DEVELOPER_CLANG_CFLAGS="-Wmissing-variable-declarations -Wno-string-plus-int"
# -Wno-empty-body
#DEVELOPER_CFLAGS="${DEVELOPER_CFLAGS} ${DEVELOPER_CLANG_CFLAGS}"

# --enable-stack-protector
AC_ARG_ENABLE(stack-protector,
       AS_HELP_STRING([--enable-stack-protector],[enable stack protector (default is YES)]),
       [],
       [enable_stack_protector=yes])

if test "x$enable_stack_protector" != "xno"; then
	AX_CHECK_COMPILE_FLAG([-fstack-protector],
		AX_CHECK_LINK_FLAG([-fstack-protector],
			[DEVELOPER_CFLAGS="${DEVELOPER_CFLAGS} -fstack-protector"], [], []),
		[], [])
fi

# --enable-developer-warnings
# cf. Varnish configure.ac, which checks compile flags more carefully
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

## From activemq-cpp configure.ac
## Execute Doxygen macros
DX_HTML_FEATURE(ON)
DX_CHM_FEATURE(OFF)
DX_CHI_FEATURE(OFF)
DX_MAN_FEATURE(OFF)
DX_RTF_FEATURE(OFF)
DX_XML_FEATURE(OFF)
DX_PDF_FEATURE(OFF)
DX_PS_FEATURE(OFF)
DX_INIT_DOXYGEN(trackrdrd, doxygen.cfg, doc)

AC_CONFIG_FILES([
	Makefile
	src/Makefile
        src/mq/file/Makefile
        src/test/Makefile
        src/mq/activemq/Makefile
        src/mq/activemq/test/Makefile
        src/mq/kafka/Makefile
        src/mq/kafka/test/Makefile
])
AC_OUTPUT
