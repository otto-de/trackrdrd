ACLOCAL_AMFLAGS = -I m4

if KAFKA_OPT
MAYBE_KAFKA = src/mq/kafka src/mq/kafka/test
endif

SUBDIRS = src src/mq/file src/test $(MAYBE_KAFKA)

if HAVE_RST2MAN
dist_man_MANS = trackrdrd.1
MAINTAINERCLEANFILES = $(dist_man_MANS) *~
endif

trackrdrd.1: README.rst
if HAVE_RST2MAN
	${RST2MAN} README.rst $@
else
	@echo "==========================================="
	@echo "rst2man not installed, no man pages created"
	@echo "==========================================="
endif

include doxygen-include.am

EXTRA_DIST = README.rst autogen.sh etc/trackrdrd.conf etc/trackrdr-kafka.conf \
	LICENSE COPYING INSTALL.rst

doc_DATA = README.rst LICENSE COPYING INSTALL.rst

doc/html: doxygen-run

clean-local:
	-rm -rf doc/
