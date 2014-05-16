ACLOCAL_AMFLAGS = -I m4

SUBDIRS = src src/test src/mq/activemq src/mq/activemq/test

if HAVE_RST2MAN
dist_man_MANS = trackrdrd.3
MAINTAINERCLEANFILES = $(dist_man_MANS) *~
endif

trackrdrd.3: README.rst
if HAVE_RST2MAN
	${RST2MAN} README.rst $@
else
	@echo "==========================================="
	@echo "rst2man not installed, no man pages created"
	@echo "==========================================="
endif

include doxygen-include.am

EXTRA_DIST = README.rst autogen.sh m4/* $(DX_CONFIG) doc/html

doc/html: doxygen-run