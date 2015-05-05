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
echo '... testing messages & log output at debug level against known checksums'

LOG=mq_log.log
MSG=mq_test.log

rm -f $LOG $MSG

../trackrdrd -D -f varnish.binlog -l $LOG -d -c test.conf

# Check ckums of the log with and without logs from the worker thread,
# since these are written asynchronously.

# the first sed removes the version/revision from the "initializing" line
# the second sed removes the user under which the child process runs
# "Not running as root" filtered so that the test is independent of
# the user running it
CKSUM=$( grep -v 'Worker 1' $LOG |  sed -e 's/\(initializing\) \(.*\)/\1/' | sed -e 's/\(Running as\) \([a-zA-Z0-9]*\)$/\1/' | grep -v 'Not running as root' | cksum)
if [ "$CKSUM" != '3927549947 214347' ]; then
    echo "ERROR: Regression test incorrect reader log cksum: $CKSUM"
    exit 1
fi

# Now check the logs from the worker thread
# Filter the 'returned to free list' messages, since these may be different
# in different runs.
CKSUM=$( grep 'Worker 1' $LOG | egrep -v 'returned [0-9]+ [^ ]+ to free list' | cksum)
if [ "$CKSUM" != '1219614274 35546' ]; then
    echo "ERROR: Regression test incorrect worker log cksum: $CKSUM"
    exit 1
fi

# Check the messages and keys
CKSUM=$(cksum $MSG)
if [ "$CKSUM" != "3675018591 29491 $MSG" ]; then
    echo "ERROR: Regression test incorrect output cksum: $CKSUM"
    exit 1
fi

exit 0
