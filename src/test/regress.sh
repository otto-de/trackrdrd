#!/bin/bash

# The regression test reads from a binary dump of the Varnish SHM log
# obtained from:
#
# $ varnishlog -w varnish.binlog
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

#
# $1 the binary log to read
# $2 the log with Worker entries filtered out
# $3 the log filtered for Worker entries
# $4 output of the file mq implementation
#
function regress {
    rm -f $LOG $MSG

    ../trackrdrd -D -f $1 -l $LOG -d -c test.conf

    # Check ckums of the log with and without logs from the worker thread,
    # since these are written asynchronously.

    # the first sed removes the version/revision from the "initializing" line
    # the second sed removes the user under which the child process runs
    # "Not running as root" filtered so that the test is independent of
    # the user running it
    CKSUM=$( grep -v 'Worker 1' $LOG |  sed -e 's/\(initializing\) \(.*\)/\1/' | sed -e 's/\(Running as\) \([a-zA-Z0-9]*\)$/\1/' | grep -v 'Not running as root' | cksum)
    if [ "$CKSUM" != "$2" ]; then
        echo "ERROR: Regression test incorrect reader log cksum: $CKSUM"
        exit 1
    fi

    # Now check the logs from the worker thread
    # Filter the 'returned to free list' messages, since these may be different
    # in different runs.
    # Also filter the version/revision from the "connected" line.
    CKSUM=$( grep 'Worker 1' $LOG | egrep -v 'returned [0-9]+ [^ ]+ to free list' | sed -e 's/\(connected\) \(.*\)/\1/' | cksum)
    if [ "$CKSUM" != "$3" ]; then
        echo "ERROR: Regression test incorrect worker log cksum: $CKSUM"
        exit 1
    fi

    # Check the messages and keys
    CKSUM=$(cksum $MSG)
    if [ "$CKSUM" != "$4 $MSG" ]; then
        echo "ERROR: Regression test incorrect output cksum: $CKSUM"
        exit 1
    fi
}

echo '... standard VCL_Log syntax'
regress 'varnish.binlog' '4193098095 333282' '1274763305 56045' \
        '1485621276 46141'

echo '... legacy VCL_Log syntax'
regress 'varnish.legacy.binlog' '3334052518 375477' '3908916621 57319' \
        '1139478852 48689'

exit 0
