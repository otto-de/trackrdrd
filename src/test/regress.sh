#!/bin/bash

# The regression test reads from a binary dump of the Varnish SHM log
# obtained from:
#
# $ varnishlog -w varnish.binlog
#
# Load was created from JMeter running 50 request in 100 threads.
#
# The regression runs trackrdrd, reading from the binary dump and
# logging to stdout in debug mode, and obtains a cksum from
# stdout. The cksum must match an expected value.

CKSUM=$(./trackrdrd -f test/varnish.binlog -l - -d | cksum)
if [ "$CKSUM" != '915150825 166426' ]; then
    echo "ERROR: Regression test incorrect cksum: $CKSUM"
    exit 1
fi

echo "*** Regression test ok ***"
exit 0
