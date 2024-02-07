AUTOMAKE_OPTIONS = subdir-objects

AM_CPPFLAGS = -I$(top_srcdir)/include

CURRENT = 5
REVISION = 0
AGE = 0

pkglib_LTLIBRARIES = libtrackrdr-kafka.la

libtrackrdr_kafka_la_SOURCES = \
	$(top_srcdir)/include/mq.h \
	$(top_srcdir)/include/config_common.h \
	mq_kafka.h \
	mq.c \
	log.c \
	monitor.c \
	zookeeper.c \
	worker.c \
	callback.c \
	config.c \
	$(top_builddir)/src/config_common.c

libtrackrdr_kafka_la_LIBADD = \
	${PTHREAD_LIBS} \
	-lrdkafka -lz -lpthread -lrt -lzookeeper_mt -lpcre2-8

libtrackrdr_kafka_la_LDFLAGS = -version-info ${CURRENT}:${REVISION}:${AGE}

libtrackrdr_kafka_la_CFLAGS = \
	-DCURRENT=${CURRENT} \
	-DREVISION=${REVISION} \
	-DAGE=${AGE}

if HAVE_RST2MAN
dist_man_MANS = libtrackrdr-kafka.3
MAINTAINERCLEANFILES = $(dist_man_MANS)
endif

libtrackrdr-kafka.3: README.rst
if HAVE_RST2MAN
	${RST2MAN} README.rst $@
endif

EXTRA_DIST = README.rst

CLEANFILES = *~
