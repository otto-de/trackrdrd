INCLUDES = -I$(VARNISHSRC)/include -I$(VARNISHSRC)

TESTS = test_parse regress.sh

check_PROGRAMS = test_parse

test_parse_SOURCES = \
	minunit.h \
	test_parse.c \
	../trackrdrd.h

test_parse_LDADD = \
	-lm \
	../parse.$(OBJEXT)