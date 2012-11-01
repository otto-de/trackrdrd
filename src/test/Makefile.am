INCLUDES = -I$(VARNISHSRC)/include -I$(VARNISHSRC) @AMQ_CFLAGS@

TESTS = test_parse test_data test_mq test_spmcq test_spmcq_loop.sh regress.sh

check_PROGRAMS = test_parse test_data test_mq test_spmcq

test_parse_SOURCES = \
	minunit.h \
	test_parse.c \
	../trackrdrd.h

test_parse_LDADD = \
	-lm \
	../parse.$(OBJEXT)

test_data_SOURCES = \
	minunit.h \
	test_data.c \
	../trackrdrd.h

test_data_LDADD = \
	../data.$(OBJEXT)

test_mq_SOURCES = \
	minunit.h \
	test_mq.c \
	../trackrdrd.h

test_mq_LDADD = \
	-lm \
	../mq.$(OBJEXT) \
	../amq.$(OBJEXT) \
	@AMQ_LIBS@

test_spmcq_SOURCES = \
	minunit.h \
	test_spmcq.c \
	../trackrdrd.h

test_spmcq_LDADD = \
	$(VARNISHSRC)/lib/libvarnish/libvarnish.la \
	../spmcq.$(OBJEXT)

