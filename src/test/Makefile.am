AUTOMAKE_OPTIONS = subdir-objects

AM_CPPFLAGS = @VARNISH_CFLAGS@ -I$(top_srcdir)/include -I$(builddir)/.. \
	-DTESTDIR=\"$(srcdir)/\"

TESTS = test_parse test_data test_append test_mq test_spmcq	\
	test_config test_spmcq_loop.sh test_worker regress.sh

check_PROGRAMS = test_parse test_data test_append test_mq	\
	test_spmcq test_config test_worker

dist_check_SCRIPTS = test_spmcq_loop.sh regress.sh

AM_TESTS_ENVIRONMENT = TESTDIR=$(srcdir)

CLEANFILES = testing.log stderr.txt trackrdrd.pid trackrdrd_*.conf.new
DISTCLEANFILES = mq_test.log mq_log.log

OBJECTS = ../assert.$(OBJEXT)		\
          ../child.$(OBJEXT)		\
          ../config_common.$(OBJEXT)	\
          ../config.$(OBJEXT)		\
          ../data.$(OBJEXT)		\
          ../handler.$(OBJEXT)		\
          ../log.$(OBJEXT)		\
          ../monitor.$(OBJEXT)		\
          ../parse.$(OBJEXT)		\
          ../sandbox.$(OBJEXT)		\
          ../spmcq.$(OBJEXT)		\
          ../vfl.$(OBJEXT)		\
          ../vpf.$(OBJEXT)		\
          ../worker.$(OBJEXT)

LIBS = -ldl -lm

test_parse_SOURCES = \
	minunit.h \
	test_parse.c \
	../trackrdrd.h

test_parse_LDADD = \
	$(LIBS) \
	$(OBJECTS) \
	@VARNISH_LIBS@

test_data_SOURCES = \
	minunit.h \
	test_data.c \
	../data.h \
	../trackrdrd.h

test_data_LDADD = \
	$(LIBS) \
	$(OBJECTS) \
	@VARNISH_LIBS@

test_append_SOURCES = \
	../child.c \
	../trackrdrd.h

test_append_LDADD = \
	$(LIBS) \
	../config_common.$(OBJEXT)	\
	../config.$(OBJEXT)		\
	../data.$(OBJEXT)		\
	../handler.$(OBJEXT)		\
	../log.$(OBJEXT)		\
	../monitor.$(OBJEXT)		\
	../parse.$(OBJEXT)		\
	../sandbox.$(OBJEXT)		\
	../spmcq.$(OBJEXT)		\
	../worker.$(OBJEXT)		\
	@VARNISH_LIBS@

test_append_CFLAGS = -DTEST_DRIVER

test_mq_SOURCES = \
	minunit.h \
	test_mq.c \
	../trackrdrd.h \
	../methods.h

test_mq_LDADD = \
	$(LIBS) \
	$(OBJECTS) \
	@VARNISH_LIBS@

test_spmcq_SOURCES = \
	minunit.h \
	test_spmcq.c \
	../trackrdrd.h

test_spmcq_LDADD = \
	$(LIBS) \
	$(OBJECTS) \
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
	$(LIBS) \
	../assert.$(OBJEXT)		\
	../child.$(OBJEXT)		\
	../data.$(OBJEXT)		\
	../handler.$(OBJEXT)		\
	../monitor.$(OBJEXT)		\
	../parse.$(OBJEXT)		\
	../sandbox.$(OBJEXT)		\
	../spmcq.$(OBJEXT)		\
	../vfl.$(OBJEXT)		\
	../vpf.$(OBJEXT)		\
	../worker.$(OBJEXT)		\
	@VARNISH_LIBS@

test_worker_SOURCES = \
	minunit.h \
	test_worker.c \
	../data.h \
	../trackrdrd.h

test_worker_LDADD = \
	$(LIBS) \
	$(OBJECTS) \
	@VARNISH_LIBS@

EXTRA_DIST = file_mq.conf test.conf trackrdrd_001.conf trackrdrd_002.conf \
	trackrdrd_003.conf trackrdrd_010.conf varnish.binlog \
	varnish.legacy.binlog
