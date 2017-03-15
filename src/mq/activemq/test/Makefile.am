AM_CPPFLAGS = @VARNISH_CFLAGS@ -I$(top_srcdir)/include

TESTS = test_activemq

check_PROGRAMS = test_activemq

test_activemq_SOURCES = \
	../../minunit.h \
	../../../../include/mq.h \
	test_activemq.c

test_activemq_LDADD = \
	../amq_connection.$(OBJEXT)  \
	../amq.$(OBJEXT)  \
	../config_common.$(OBJEXT)  \
	../mq.$(OBJEXT) \
	${PTHREAD_LIBS} \
	@AMQ_LIBS@ \
	@APR_LIBS@ \
	@APU_LIBS@ \
	-lstdc++
