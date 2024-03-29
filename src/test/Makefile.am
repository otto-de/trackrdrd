AUTOMAKE_OPTIONS = subdir-objects

AM_CPPFLAGS = @VARNISH_CFLAGS@ -I$(top_srcdir)/include -I$(builddir)/.. \
	-DTESTDIR=\"$(srcdir)/\"

TESTS = test_parse test_data test_append test_mq test_spmcq	\
	test_config test_spmcq_loop.sh test_worker regress.sh

check_PROGRAMS = test_parse test_data test_append test_mq	\
	test_spmcq test_config test_worker

dist_check_SCRIPTS = test_spmcq_loop.sh regress.sh

AM_TESTS_ENVIRONMENT = TESTDIR=$(srcdir)

CLEANFILES = testing.log stderr.txt trackrdrd.pid trackrdrd_*.conf.new \
	varnish.binlog
DISTCLEANFILES = mq_test.log mq_log.log

test_parse_SOURCES = \
	minunit.h \
	test_parse.c \
	../trackrdrd.h

test_parse_LDADD = \
	-lm \
	../parse.$(OBJEXT) \
	../assert.$(OBJEXT) \
	../log.$(OBJEXT) \
	../config.$(OBJEXT) \
	../config_common.$(OBJEXT) \
	@VARNISH_LIBS@

test_data_SOURCES = \
	minunit.h \
	test_data.c \
	../data.h \
	../trackrdrd.h

test_data_LDADD = \
	../data.$(OBJEXT) \
	../assert.$(OBJEXT) \
	../log.$(OBJEXT) \
	../config.$(OBJEXT) \
	../config_common.$(OBJEXT) \
	@VARNISH_LIBS@

test_append_SOURCES = \
	../child.c \
	../trackrdrd.h

test_append_LDADD = \
	-ldl -lm \
	../worker.$(OBJEXT) \
	../log.$(OBJEXT) \
	../spmcq.$(OBJEXT) \
	../data.$(OBJEXT) \
	../assert.$(OBJEXT) \
	../monitor.$(OBJEXT) \
	../parse.$(OBJEXT) \
	../config.$(OBJEXT) \
	../config_common.$(OBJEXT) \
	../handler.$(OBJEXT) \
	../sandbox.$(OBJEXT) \
	@VARNISH_LIBS@

test_append_CFLAGS = -DTEST_DRIVER

test_mq_SOURCES = \
	minunit.h \
	test_mq.c \
	../trackrdrd.h \
	../methods.h

test_mq_LDADD = \
	-ldl \
	-lm \
	../worker.$(OBJEXT) \
	../config.$(OBJEXT) \
	../config_common.$(OBJEXT) \
	../log.$(OBJEXT) \
	../spmcq.$(OBJEXT) \
	../data.$(OBJEXT) \
	@VARNISH_LIBS@

test_spmcq_SOURCES = \
	minunit.h \
	test_spmcq.c \
	../trackrdrd.h

test_spmcq_LDADD = \
	../spmcq.$(OBJEXT) \
	../assert.$(OBJEXT) \
	../config.$(OBJEXT) \
	../config_common.$(OBJEXT) \
	../log.$(OBJEXT) \
	@VARNISH_LIBS@

test_config_SOURCES = \
	minunit.h \
	../config.c \
	../config_common.c \
	../log.c \
	test_config.c \
	test_utils.c \
	test_utils.h \
	../trackrdrd.h

test_config_LDADD = \
	../assert.$(OBJEXT) \
	../spmcq.$(OBJEXT) \
	-lm \
	@VARNISH_LIBS@

test_worker_SOURCES = \
	minunit.h \
	test_worker.c \
	../data.h \
	../trackrdrd.h

test_worker_LDADD = \
	-ldl -lm \
	../worker.$(OBJEXT) \
	../log.$(OBJEXT) \
	../spmcq.$(OBJEXT) \
	../data.$(OBJEXT) \
	../assert.$(OBJEXT) \
	../config.$(OBJEXT) \
	../config_common.$(OBJEXT) \
	@VARNISH_LIBS@

EXTRA_DIST = file_mq.conf test.conf trackrdrd_001.conf trackrdrd_002.conf \
	trackrdrd_003.conf trackrdrd_010.conf varnish.binlog.gz
