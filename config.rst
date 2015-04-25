==================== ========== ========================================================================================= =======
Parameter            CLI Option Description                                                                               Default
==================== ========== ========================================================================================= =======
``varnish.name``     ``-n``     Like the ``-n`` option for Varnish, this is the path to the file that is mmap'd to the    default for Varnish (the host name)
                                shared memory segment for the Varnish log. This parameter and ``varnish.bindump`` are
                                mutually exclusive.
-------------------- ---------- ----------------------------------------------------------------------------------------- -------
``mq.module``                   Name of the shared object implementing the MQ interface. May be an absolute path, or the  None, this parameter is required.
                                name of a library that the dynamic linker finds according to the rules described in
                                ld.so(8).
-------------------- ---------- ----------------------------------------------------------------------------------------- -------
``mq.config_file``              Path of a configuration file used by the MQ implementation                                None, this parameter is optional.
-------------------- ---------- ----------------------------------------------------------------------------------------- -------
``nworkers``                    Number of worker threads used to send messages to the message broker(s).                  1
-------------------- ---------- ----------------------------------------------------------------------------------------- -------
``maxopen.scale``               log\ :sub:`2`\(max number of concurrent requests in Varnish). For example, if             10 (= 1024 concurrent requests)
                                ``maxopen.scale`` = 10, then ``trackrdrd`` can support up to 1024 concurrent requests.
                                More precisely, this number describes the maximum number of request XIDs for which
                                ``ReqStart`` has been read, but not yet ``ReqEnd``. It should specify at least the next
                                power of two larger than (``thread_pools`` * ``thread_pool_max``) in the Varnish
                                configuration.
-------------------- ---------- ----------------------------------------------------------------------------------------- -------
``maxdone``                     The maximum number of finished records waiting to be sent to message brokers. That is,    1024
                                the largest number of request XIDs for which ``ReqEnd`` has been read, but the data have
                                not yet been sent to a message queue.
-------------------- ---------- ----------------------------------------------------------------------------------------- -------
``maxdata``                     The maximum length of a data record in characters. Should be at least as large the        1024
                                Varnish parameter ``shm_reclen``.
-------------------- ---------- ----------------------------------------------------------------------------------------- -------
``hash.max_probes``             The maximum number of insert or find probes used for the hash table of XIDs. Hash lookups 10
                                fail if a hit is not found after this many probes.
-------------------- ---------- ----------------------------------------------------------------------------------------- -------
``hash.ttl``                    Maximum time to live in seconds for an unfinished record. If ``ReqEnd`` is not read for   120
                                a request XID within this time, then ``trackrdrd`` no longer waits for it, and schedules
                                the data read thus far to be sent to a message broker. This should be a bit longer than
                                the sum of all timeouts configured for a Varnish request.
-------------------- ---------- ----------------------------------------------------------------------------------------- -------
``hash.mlt``                    Minimum lifetime of an open record in seconds. That is, after ``ReqStart`` has been read  5
                                for a request XID, then ``trackrdrd`` will not evacuate it if space is needed in its hash
                                table before this interval has elapsed.
-------------------- ---------- ----------------------------------------------------------------------------------------- -------
``qlen.goal``                   A goal length for the internal queue from the reader thread to the worker thread.         ``maxdone``/2
                                ``trackrdrd`` uses this value to determine whether a new worker thread should be started
                                to support increasing load.
-------------------- ---------- ----------------------------------------------------------------------------------------- -------
``user``             ``-u``     Owner of the child process                                                                ``nobody``, or the user starting ``trackrdrd``
-------------------- ---------- ----------------------------------------------------------------------------------------- -------
``pid.file``         ``-P``     Path to the file to which the management process writes its process ID. If the value is   ``/var/run/trackrdrd.pid``
                                set to be empty (by the line ``pid.file=``, with no value), then no PID file is written.
-------------------- ---------- ----------------------------------------------------------------------------------------- -------
``restarts``                    Maximum number of restarts of the child process by the management process                 1
-------------------- ---------- ----------------------------------------------------------------------------------------- -------
``restart.pause``               Seconds to pause before restarting a child process                                        1
-------------------- ---------- ----------------------------------------------------------------------------------------- -------
``thread.restarts``             Maximum number of restarts of a worker thread by the child process. A thread is restarted 1
                                after a message send, message system reconnect and message resend have all failed. If the
                                restart limit for a thread is reached, then the thread goes into the state ``abandoned``
                                and no more restarts are attempted. If all worker threads are abandoned, then the child
                                process stops.
-------------------- ---------- ----------------------------------------------------------------------------------------- -------
``monitor.interval``            Interval in seconds at which monitoring statistics are emitted to the log. If set to 0,   30
                                then no statistics are logged.
-------------------- ---------- ----------------------------------------------------------------------------------------- -------
``monitor.workers``             Whether statistics about worker threads should be logged (boolean)                        false
-------------------- ---------- ----------------------------------------------------------------------------------------- -------
``log.file``         ``-l``     Log file for status, warning, debug and error messages, and monitoring statistics. If '-' ``syslog(3)``
                                is specified, then log messages are written to stdout. This parameter and
                                ``syslog.facility`` are mutually exclusive.
-------------------- ---------- ----------------------------------------------------------------------------------------- -------
``syslog.facility``  ``-y``     See ``syslog(3)``; legal values are ``user`` or ``local0`` through ``local7``. This       ``local0``
                                parameter and ``log.file`` are mutually exclusive. 
-------------------- ---------- ----------------------------------------------------------------------------------------- -------
``varnish.bindump``  ``-f``     A binary dump of the Varnish shared memory log obtained from ``varnishlog -w``. If a
                                value is specified, ``trackrdrd`` reads from that file instead of a live Varnish log
                                (useful for testing, debugging and replaying traffic). This parameter and
                                ``varnish.name`` are mutually exclusive. 
==================== ========== ========================================================================================= =======


