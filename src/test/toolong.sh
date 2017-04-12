#!/bin/bash

# Test length overflow and chunk exhaustion.
#
# When the tracking reader is unable to write to a data entry, either
# because max.reclen would be exceeded (length overflow) or because
# the chunk table is exhausted, then it doesn't append the data, logs
# the discarded data, and continues reading. At the end of the log
# transaction, it sends data it was able to read into the entry.
#
# This tests runs similarly to regress.sh, see the comments there for
# details.

echo
echo "TEST: $0"
echo '... testing recovery from length overflow and chunk exhaustion'

LOG=mq_log.log
MSG=mq_test.log

rm -f $LOG $MSG

../trackrdrd -D -f toolong.log -l $LOG -d -c toolong.conf

# Just filter out the worker thread entries, and the read the last lines
# in which the data read was logged, as well as the error messages.
# Transaction 8 (the backend transaction) is filtered due to custom
# patches in the logging API that suppress all backend transaction
# reads when the -c flag is set.
CKSUM=$( grep -v 'Worker 1' $LOG |  grep -v 'Reader read tx: \[8\]' | tail -31 | cksum)
if [ "$CKSUM" != '2418589262 1994' ]; then
    echo "ERROR: Too long test incorrect reader log cksum: $CKSUM"
    exit 1
fi

# Check the contents of the file MQ output.
CKSUM=$(cksum $MSG)
if [ "$CKSUM" != "4088438759 125 $MSG" ]; then
    echo "ERROR: Too long test incorrect output log cksum: $CKSUM"
    exit 1
fi

exit 0
