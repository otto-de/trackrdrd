AUTOMAKE_OPTIONS = subdir-objects

AM_CPPFLAGS = -I$(top_srcdir)/include

CURRENT = 5
REVISION = 0
AGE = 0

pkglib_LTLIBRARIES = libtrackrdr-file.la

libtrackrdr_file_la_SOURCES = \
	$(top_srcdir)/include/mq.h \
	$(top_srcdir)/include/config_common.h \
	$(top_srcdir)/include/miniobj.h \
	$(top_srcdir)/src/config_common.c \
	mq.c

libtrackrdr_file_la_LDFLAGS = -version-info ${CURRENT}:${REVISION}:${AGE}

libtrackrdr_file_la_CFLAGS = \
	-DCURRENT=${CURRENT} \
	-DREVISION=${REVISION} \
	-DAGE=${AGE}

if HAVE_RST2MAN
dist_man_MANS = libtrackrdr-file.3
MAINTAINERCLEANFILES = $(dist_man_MANS)
endif

libtrackrdr-file.3: README.rst
if HAVE_RST2MAN
	${RST2MAN} README.rst $@
endif

CLEANFILES = *~
