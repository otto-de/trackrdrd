AM_CPPFLAGS = @VARNISH_CFLAGS@ -I$(top_srcdir)/include \
	 @AMQ_CFLAGS@ @APR_CFLAGS@ @APU_CFLAGS@

pkglib_LTLIBRARIES = libtrackrdr-activemq.la

libtrackrdr_activemq_la_SOURCES = \
	$(top_srcdir)/include/mq.h \
	$(top_srcdir)/include/config_common.h \
	mq.c \
	amq.h \
	amq.cpp \
	amq_connection.h \
	amq_connection.cpp \
	$(top_srcdir)/src/config_common.c

libtrackrdr_activemq_la_LIBADD = \
	${PTHREAD_LIBS} \
	@AMQ_LIBS@ \
	@APR_LIBS@ \
	@APU_LIBS@

libtrackrdr_activemq_la_LDFLAGS = -version-info 5:0:0

if HAVE_RST2MAN
dist_man_MANS = libtrackrdr-activemq.3
MAINTAINERCLEANFILES = $(dist_man_MANS)
endif

libtrackrdr-activemq.3: README.rst
if HAVE_RST2MAN
	${RST2MAN} README.rst $@
endif
