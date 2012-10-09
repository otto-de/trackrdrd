INCLUDES = -I$(VARNISHSRC)/include -I$(VARNISHSRC)

TESTS = test_parse test_data regress.sh

check_PROGRAMS = test_parse test_data

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
