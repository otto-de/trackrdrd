INCLUDES = -I$(top_srcdir)/include

TESTS = test_kafka

check_PROGRAMS = test_kafka

test_kafka_SOURCES = \
	../../minunit.h \
	../../../../include/mq.h \
	test_kafka.c

test_kafka_LDADD = \
	../../../config_common.$(OBJEXT)  \
	../mq.$(OBJEXT) \
	../log.$(OBJEXT) \
	../monitor.$(OBJEXT) \
	${PTHREAD_LIBS} \
	-lrdkafka -lz -lpthread -lrt -lzookeeper_mt -lyajl

CLEANFILES = kafka.log zoo.log *~
