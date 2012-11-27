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
echo "... testing log output at debug level against a known checksum"
CMD="../trackrdrd -D -f varnish.binlog -l - -d -c test.conf"

# the first sed removes the version/revision from the "initializing" line
# the second sed removes the user under which the child process runs
CKSUM=$( $CMD | sed -e 's/\(initializing\) \(.*\)/\1/' | sed -e 's/\(, running as\) \(.*\)/\1/' | cksum)

if [ "$CKSUM" != '646018814 229297' ]; then
    echo "ERROR: Regression test incorrect cksum: $CKSUM"
    exit 1
fi

exit 0
