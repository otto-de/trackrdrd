============== ================================================================
Field          Description
============== ================================================================
``seen``       Number of request records received by this thread
-------------- ----------------------------------------------------------------
``waits``      How many times this thread went into the ``waiting`` state
-------------- ----------------------------------------------------------------
``sent``       Number of records successfully sent to a message broker
-------------- ----------------------------------------------------------------
``reconnects`` How often this thread reconnected to a message broker (after
               failing to send a message)
-------------- ----------------------------------------------------------------
``restarts``   How often this thread restarted (after a failed reconnection
               attempt)
-------------- ----------------------------------------------------------------
``failed``     Number of failed attempts to send a message (after reconnection
               succeeded, but the send retry failed)
============== ================================================================

