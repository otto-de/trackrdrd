AUTOMAKE_OPTIONS = subdir-objects

AM_CPPFLAGS = -I${VARNISH_SHARE_INCLUDE} -I${VARNISH_PKG_INCLUDE} \
	-I$(top_srcdir)/include

bin_PROGRAMS = trackrdrd

nodist_trackrdrd_SOURCES = usage.h vcs_version.h

trackrdrd_SOURCES = \
	trackrdrd.h \
	signals.h \
	trackrdrd.c \
	parse.c \
	log.c \
	$(top_srcdir)/include/config_common.h \
	config_common.c \
	config.c \
	data.c \
	monitor.c \
	spmcq.c \
	worker.c \
	sandbox.c \
	child.c \
	handler.c \
	assert.c \
	vfl.h \
	vfl.c \
	vpf.h \
	vpf.c \
	$(top_srcdir)/options.txt \
	$(top_srcdir)/synopsis.txt

trackrdrd_LDADD = \
	${PTHREAD_LIBS} @VARNISH_LIBS@ ${LIBM}

trackrdrd_LDFLAGS = -ldl

BUILT_SOURCES = usage.h vcs_version.h
DISTCLEANFILES = usage.h vcs_version.h 

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

# Derive a revision name from the git commit
# Adopted from Varnish include/Makefile.am
vcs_version.h: FORCE
	@if [ -d "$(top_srcdir)/.git" ]; then \
		V="$$(git show -s --pretty=format:%h)" \
		B="$$(git rev-parse --abbrev-ref HEAD)" \
		H="$$(head -n 1 vcs_version.h 2>/dev/null || true)"; \
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
		  echo "#define VCS_Version \"$$V\"" ; \
		  echo "#define VCS_Branch \"$$B\"" \
		) > vcs_version.h ; \
		fi \
	else \
		if [ ! -f vcs_version.h ]; then \
		( \
		  echo "/* NOGIT */" ; \
		  echo '/* No git commit ID available, see include/Makefile.am for explanation */' ; \
		  echo '#define VCS_Version "NOGIT"' ; \
		  echo '#define VCS_Branch "NOGIT"' \
		) >  vcs_version.h ; \
		fi \
	fi

FORCE:
