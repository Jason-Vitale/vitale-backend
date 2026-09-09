-- Persists GpPoller's position in its rotating sweep through the full
-- `objects` catalog (see DbWriter::next_gp_rotation_batch /
-- advance_gp_rotation_cursor in services/poller/db_writer.cpp), the same
-- way last_run_at persists scheduling state: the poller is a fresh process
-- every cron invocation, so "which slice of the catalog did we cover last
-- time" has to live in Postgres, not in memory.
--
-- NULL means "never rotated yet, start from the beginning of the catalog."
-- Only the 'gp' row ever sets this; 'satcat' has no rotation concept.
ALTER TABLE poller_state ADD COLUMN rotation_cursor INTEGER;
