INCLUDES = -I$(VARNISHSRC)/include -I$(VARNISHSRC) @AMQ_CFLAGS@

bin_PROGRAMS = trackrdrd

nodist_trackrdrd_SOURCES = revision.h usage.h

trackrdrd_SOURCES = \
	trackrdrd.h \
	trackrdrd.c \
	parse.c \
	log.c \
	config.c \
	data.c \
	monitor.c \
	mq.c \
	activemq/amq.h \
	activemq/amq.cpp \
	activemq/amq_connection.h \
	activemq/amq_connection.cpp \
	spmcq.c \
	worker.c \
	sandbox.c \
	child.c \
	handler.c \
	assert.c

trackrdrd_LDADD = \
	$(VARNISHSRC)/lib/libvarnishcompat/libvarnishcompat.la \
	$(VARNISHSRC)/lib/libvarnishapi/libvarnishapi.la \
	$(VARNISHSRC)/lib/libvarnish/libvarnish.la \
	${PTHREAD_LIBS} \
	@AMQ_LIBS@

BUILT_SOURCES = revision.h usage.h
MAINTAINERCLEANFILES = revision.h usage.h

# Derive a revision name from the git commit
# Adopted from Varnish include/Makefile.am
revision.h: FORCE
	@if [ -d "$(top_srcdir)/../.git" ]; then \
		V="$$(git show -s --pretty=format:%h)" \
		H="$$(head -n 1 revision.h 2>/dev/null || true)"; \
		if [ "/* $$V */" != "$$H" ]; then \
		( \
		  echo "/* $$V */" ;\
		  echo '/*' ;\
		  echo ' * NB:  This file is machine generated, DO NOT EDIT!' ;\
		  echo ' *' ;\
		  echo ' * Run make to regenerate' ;\
		  echo ' *'  ;\
		  echo ' */' ;\
		  echo "/* $$V */" ;\
		  echo '' ;\
		  echo "#define REVISION \"$$V\"" \
		) > revision.h ; \
		fi \
	else \
		if [ ! -f revision.h ]; then \
		( \
		  echo "/* NOGIT */" ; \
		  echo '/* No git commit ID available, see include/Makefile.am for explanation */' ; \
		  echo '#define REVISION "NOGIT"' \
		) >  revision.h ; \
		fi \
	fi

usage.h: $(top_srcdir)/synopsis.txt $(top_srcdir)/options.txt
	( \
	  echo '/*' ;\
	  echo ' * NB:  This file is machine generated, DO NOT EDIT!' ;\
	  echo ' *' ;\
	  echo ' * Run make to regenerate' ;\
	  echo ' *'  ;\
	  echo ' */' ;\
	  echo '' ;\
	  echo 'const char *synopsis = ' \
	) > usage.h 
	sed -e 's/.*/\"&\\n\"/' $(top_srcdir)/synopsis.txt >> usage.h
	( \
	  echo ';' ;\
	  echo 'const char *options = ' \
	) >> usage.h
	sed -e 's/.*/\"&\\n\"/' $(top_srcdir)/options.txt >> usage.h
	echo ';' >> usage.h

FORCE: