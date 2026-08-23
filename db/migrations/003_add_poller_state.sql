-- Durable "last run" bookkeeping for the poller's scheduler. The poller is
-- deployed as a cron job invoked fresh once an hour, not a single
-- long-lived process, so an in-memory "when did I last run SATCAT" tracker
-- resets on every invocation and can't tell a due daily poll from one that
-- ran ten minutes ago. This table is that memory instead.
CREATE TABLE poller_state (
    poller_name  VARCHAR(32) PRIMARY KEY,
    last_run_at  TIMESTAMPTZ NOT NULL
);
