INCLUDES = -I$(VARNISHSRC)/include -I$(VARNISHSRC)

bin_PROGRAMS = trackrdrd

nodist_trackrdrd_SOURCES = revision.h

trackrdrd_SOURCES = \
	trackrdrd.h \
	trackrdrd.c \
	parse.c \
	log.c

trackrdrd_LDADD = \
	$(VARNISHSRC)/lib/libvarnishcompat/libvarnishcompat.la \
	$(VARNISHSRC)/lib/libvarnishapi/libvarnishapi.la \
	$(VARNISHSRC)/lib/libvarnish/libvarnish.la \
	${PTHREAD_LIBS}

check:
	test/regress.sh


# Derive a revision name from the git commit
# Adopted from Varnish include/Makefile.am
BUILT_SOURCES = revision.h
MAINTAINERCLEANFILES = revision.h 
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
FORCE: