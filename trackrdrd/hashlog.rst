================= =============================================================
Field             Description
================= =============================================================
``len``           Size of the hash table (2\ :sup:``maxopen.scale``\)
----------------- -------------------------------------------------------------
``seen``          Number of request records read (``ReqStart`` seen)
----------------- -------------------------------------------------------------
``drop_reqstart`` Number of records that could not be inserted into internal
                  tables (no data from ``ReqStart`` inserted, nor any other
                  data for that XID)
----------------- -------------------------------------------------------------
``drop_vcl_log``  How often data from ``VCL_log`` could not be inserted
                  (usually because the XID could not be found)
----------------- -------------------------------------------------------------
``drop_reqend``   How often data from ``ReqStart`` could not be inserted
                  (usually because the XID could not be found)
----------------- -------------------------------------------------------------
``expired``       Number of records for which ``hash.ttl`` expired (data sent
                  to message broker without waiting for ``ReqEnd``)
----------------- -------------------------------------------------------------
``evacuated``     Number of records removed to recover space in the hash table
                  (``hash.mlt`` expired, data possibly incomplete)
----------------- -------------------------------------------------------------
``open``          Current number of open records in the table
----------------- -------------------------------------------------------------
``load``          Current open records as percent (``open``/``len`` * 100)
----------------- -------------------------------------------------------------
``collisions``    Number of hash collisions
----------------- -------------------------------------------------------------
``insert_probes`` Number of hash insert probes
----------------- -------------------------------------------------------------
``find_probes``   Number of hash find probes
----------------- -------------------------------------------------------------
``fail``          Number of failed hash operations (insert or find)
----------------- -------------------------------------------------------------
``occ_hi``        Occupancy high watermark -- highest number of open records
                  since startup
----------------- -------------------------------------------------------------
``occ_hi_this``   Occupancy high watermark in the current monitoring interval
================= =============================================================

