====== ========== ============
Signal Parent     Child
====== ========== ============
TERM   Shutdown   Shutdown
------ ---------- ------------
INT    Shutdown   Shutdown
------ ---------- ------------
HUP    Graceful   Ignore
       restart
------ ---------- ------------
USR1   Graceful   Dump data
       restart    table to log
------ ---------- ------------
USR2   Ignore     Ignore
------ ---------- ------------
ABRT   Abort with Abort with
       stacktrace stacktrace
------ ---------- ------------
SEGV   Abort with Abort with
       stacktrace stacktrace
------ ---------- ------------
BUS    Abort with Abort with
       stacktrace stacktrace
====== ========== ============

