AM_CPPFLAGS = -I$(top_srcdir)/include -DTESTDIR=\"$(srcdir)/\"

TESTS = test_partition test_kafka

check_PROGRAMS = test_partition test_kafka test_send test_send_ssl

test_partition_SOURCES = \
	$(top_srcdir)/src/test/minunit.h \
	../mq_kafka.h \
	test_partition.c

test_partition_LDADD = \
	../callback.$(OBJEXT) \
	../config.$(OBJEXT) \
	../log.$(OBJEXT) \
	-lrdkafka

test_kafka_SOURCES = \
	$(top_srcdir)/src/test/minunit.h \
	../../../../include/mq.h \
	test_kafka.c

test_kafka_LDADD = \
	../../../config_common.$(OBJEXT)  \
	../mq.$(OBJEXT) \
	../log.$(OBJEXT) \
	../monitor.$(OBJEXT) \
	../zookeeper.$(OBJEXT) \
	../worker.$(OBJEXT) \
	../callback.$(OBJEXT) \
	../config.$(OBJEXT) \
	${PTHREAD_LIBS} \
	-lrdkafka -lz -lpthread -lrt -lzookeeper_mt -lpcre

test_send_SOURCES = \
	$(top_srcdir)/src/test/minunit.h \
	../../../../include/mq.h \
	test_send.c

test_send_LDADD = \
	../../../config_common.$(OBJEXT)  \
	../mq.$(OBJEXT) \
	../log.$(OBJEXT) \
	../monitor.$(OBJEXT) \
	../zookeeper.$(OBJEXT) \
	../worker.$(OBJEXT) \
	../callback.$(OBJEXT) \
	../config.$(OBJEXT) \
	${PTHREAD_LIBS} \
	-lrdkafka -lz -lpthread -lrt -lzookeeper_mt -lpcre

test_send_ssl_SOURCES = \
	$(top_srcdir)/src/test/minunit.h \
	../../../../include/mq.h \
	test_send_ssl.c

test_send_ssl_LDADD = \
	../../../config_common.$(OBJEXT)  \
	../mq.$(OBJEXT) \
	../log.$(OBJEXT) \
	../monitor.$(OBJEXT) \
	../zookeeper.$(OBJEXT) \
	../worker.$(OBJEXT) \
	../callback.$(OBJEXT) \
	../config.$(OBJEXT) \
	${PTHREAD_LIBS} \
	-lrdkafka -lz -lpthread -lrt -lzookeeper_mt -lpcre

CLEANFILES = kafka.log zoo.log *~

EXTRA_DIST = kafka.conf kafka_ssl.conf
