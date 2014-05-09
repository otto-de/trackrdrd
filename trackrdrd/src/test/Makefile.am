INCLUDES = -I$(VARNISHSRC)/include -I$(VARNISHSRC)

TESTS = test_parse test_data test_hash test_mq test_spmcq		\
	test_spmcq_loop.sh test_worker regress.sh

check_PROGRAMS = test_parse test_data test_hash test_mq test_spmcq	\
	test_worker

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
	$(VARNISHSRC)/lib/libvarnish/libvarnish.la \
	../data.$(OBJEXT)

test_hash_SOURCES = \
	$(VARNISHSRC)/lib/libvarnish/libvarnish.la \
	../child.c \
	../trackrdrd.h

test_hash_LDADD = \
	-ldl \
	$(VARNISHSRC)/lib/libvarnish/libvarnish.la \
	$(VARNISHSRC)/lib/libvarnishapi/libvarnishapi.la \
	../worker.$(OBJEXT) \
	../log.$(OBJEXT) \
	../spmcq.$(OBJEXT) \
	../data.$(OBJEXT) \
	../assert.$(OBJEXT) \
	../monitor.$(OBJEXT) \
	../parse.$(OBJEXT) \
	../config.$(OBJEXT) \
	../sandbox.$(OBJEXT)

test_hash_CFLAGS = -DTEST_DRIVER

test_mq_SOURCES = \
	minunit.h \
	test_mq.c \
	../trackrdrd.h \
	../methods.h

test_mq_LDADD = \
	-ldl \
	-lm \
	$(VARNISHSRC)/lib/libvarnish/libvarnish.la

test_spmcq_SOURCES = \
	minunit.h \
	test_spmcq.c \
	../trackrdrd.h

test_spmcq_LDADD = \
	$(VARNISHSRC)/lib/libvarnish/libvarnish.la \
	../spmcq.$(OBJEXT)

test_worker_SOURCES = \
	minunit.h \
	test_worker.c \
	../monitor.c \
	../trackrdrd.h

test_worker_LDADD = \
	-ldl \
	$(VARNISHSRC)/lib/libvarnish/libvarnish.la \
	../worker.$(OBJEXT) \
	../log.$(OBJEXT) \
	../spmcq.$(OBJEXT) \
	../data.$(OBJEXT)

test_worker_CFLAGS = -DTEST_DRIVER
