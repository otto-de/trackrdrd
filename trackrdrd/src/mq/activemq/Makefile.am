INCLUDES = -I$(VARNISHSRC)/include -I$(VARNISHSRC) -I$(top_srcdir)/include \
	 @AMQ_CFLAGS@ @APR_CFLAGS@ @APU_CFLAGS@

pkglib_LTLIBRARIES = libtrackrdr-activemq.la

libtrackrdr_activemq_la_SOURCES = \
	$(top_srcdir)/include/mq.h \
	mq.c \
	amq.h \
	amq.cpp \
	amq_connection.h \
	amq_connection.cpp

libtrackrdr_activemq_la_LIBADD = \
	${PTHREAD_LIBS} \
	@AMQ_LIBS@ \
	@APR_LIBS@ \
	@APU_LIBS@

libtrackrdr_activemq_la_LDFLAGS = -version-info 0:0:0
