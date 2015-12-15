AM_CPPFLAGS = -I$(top_srcdir)/include

TESTS = test_partition test_kafka

check_PROGRAMS = test_partition test_kafka test_send

test_partition_SOURCES = \
	../../minunit.h \
	../mq_kafka.h \
	test_partition.c

test_partition_LDADD = \
	../callback.$(OBJEXT) \
	../log.$(OBJEXT) \
	-lrdkafka

test_kafka_SOURCES = \
	../../minunit.h \
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
	../../minunit.h \
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

CLEANFILES = kafka.log zoo.log *~
