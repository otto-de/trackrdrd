#!/bin/bash

# The regression test reads from a binary dump of the Varnish SHM log
# obtained from:
#
# $ varnishlog -B -w varnish.binlog
#
# The regression runs trackrdrd, reading from the binary dump and
# logging to stdout in debug mode, and obtains a cksum from stdout. It
# uses the file MQ implementation to write an output file. The cksums
# from the log and the output file must match expected values.

echo
echo "TEST: $0"
echo "... testing log output at debug level against a known checksum"
CMD="../trackrdrd -D -f varnish.binlog -l - -d -c test.conf"

# the first sed removes the version/revision from the "initializing" line
# the second sed removes the user under which the child process runs
# "Not running as root" filtered so that the test is independent of
# the user running it
CKSUM=$( $CMD | sed -e 's/\(initializing\) \(.*\)/\1/' | sed -e 's/\(Running as\) \([a-zA-Z0-9]*\)$/\1/' | grep -v 'Not running as root' | cksum)

if [ "$CKSUM" != '1219831915 249619' ]; then
    echo "ERROR: Regression test incorrect log cksum: $CKSUM"
    exit 1
fi

CKSUM=$(cksum mq_test.log)
if [ "$CKSUM" != '3675018591 29491 mq_test.log' ]; then
    echo "ERROR: Regression test incorrect output cksum: $CKSUM"
    exit 1
fi

exit 0
