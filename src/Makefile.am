INCLUDES = -I$(VARNISHSRC)/include -I$(VARNISHSRC)

bin_PROGRAMS = trackrdrd

trackrdrd_SOURCES = \
	trackrdrd.c \
	parse.c \
	log.c
#	$(VARNISHSRC)/lib/libvarnish/assert.c \
#	$(VARNISHSRC)/lib/libvarnish/flopen.c \
#	$(VARNISHSRC)/lib/libvarnish/version.c \
#	$(VARNISHSRC)/lib/libvarnish/vsb.c \
#	$(VARNISHSRC)/lib/libvarnish/vpf.c

trackrdrd_LDADD = \
	$(VARNISHSRC)/lib/libvarnishcompat/libvarnishcompat.la \
	$(VARNISHSRC)/lib/libvarnishapi/libvarnishapi.la \
	$(VARNISHSRC)/lib/libvarnish/libvarnish.la \
	${PTHREAD_LIBS}

check:
	test/regress.sh
