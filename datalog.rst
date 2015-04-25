================== ============================================================
Field              Description
================== ============================================================
``len``            Size of the data table
                   (``maxdone`` + 2\ :sup:``maxopen.scale``\)
------------------ ------------------------------------------------------------
``nodata``         Number of request records that contained no data (nothing to
                   track in a ``VCL_log`` entry). These records are discarded
                   without sending a message to a message broker.
------------------ ------------------------------------------------------------
``submitted``      Number of records passed from the reader thread to worker
                   threads to be sent to a message broker
------------------ ------------------------------------------------------------
``wait_room``      How often the reader thread had to wait for space in the
                   data table
------------------ ------------------------------------------------------------
``data_hi``        Data high watermark -- longest record since startup (in
                   bytes)
------------------ ------------------------------------------------------------
``data_overflows`` How often the accumulated length of a record exceeded
                   ``maxdata``
------------------ ------------------------------------------------------------
``data_truncated`` How often data from the Varnish log was truncated due to
                   the presence of a null byte. This can happen if the data was
                   already truncated in the log, due to exceeding
                   ``shm_reclen``.
------------------ ------------------------------------------------------------
``done``           Current number of records in state "done"
------------------ ------------------------------------------------------------
``open``           Current number of open records in the table
------------------ ------------------------------------------------------------
``load``           Current number records in the table as percent
                   (100 * (``open`` + ``done``)/``len``)
------------------ ------------------------------------------------------------
``sent``           Number of records successfully sent to a message broker
------------------ ------------------------------------------------------------
``reconnects``     How often worker threads reconnected to a message broker
                   after an unsuccessful send
------------------ ------------------------------------------------------------
``restarts``       How often worker threads were restarted after a message
                   send, reconnect and resend all failed
------------------ ------------------------------------------------------------
``abandoned``      Number of worker threads that have been abandoned due to
                   reaching the restart limit (``thread.restarts``)
------------------ ------------------------------------------------------------
``failed``         Number of failed sends (failure after reconnect)
------------------ ------------------------------------------------------------
``occ_hi``         Occupancy high watermark -- highest number of records (open
                   and done) since startup
------------------ ------------------------------------------------------------
``occ_hi_this``    Occupancy high watermark in the current monitoring interval
================== ============================================================

