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

echo
echo "TEST: $0"
CKSUM=$(../trackrdrd -f varnish.binlog -l - -d -c test.conf | cksum)
if [ "$CKSUM" != '3372973632 229022' ]; then
    echo "ERROR: Regression test incorrect cksum: $CKSUM"
    exit 1
fi

exit 0
