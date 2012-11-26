ACLOCAL_AMFLAGS = -I m4

SUBDIRS = src src/test

dist_man_MANS = trackrdrd.3
MAINTAINERCLEANFILES = $(dist_man_MANS)

trackrdrd.3: README.rst
if HAVE_RST2MAN
	${RST2MAN} README.rst $@
else
	@echo "========================================"
	@echo "You need rst2man installed to make dist"
	@echo "========================================"
	@false
endif

include doxygen-include.am

EXTRA_DIST = README.rst autogen.sh m4/* $(DX_CONFIG) doc/html

doc/html: doxygen-run