CREATE TABLE objects (
    norad_cat_id     INTEGER PRIMARY KEY,
    object_name      VARCHAR(25),
    object_id        VARCHAR(12),
    object_type      VARCHAR(12),
    country_code     VARCHAR(6),
    launch_date      DATE,
    site             VARCHAR(5),
    rcs_size         VARCHAR(6),
    decay_date       DATE,
    updated_at       TIMESTAMPTZ DEFAULT now()
);

CREATE TABLE snapshots (
    id                  BIGSERIAL PRIMARY KEY,
    norad_cat_id        INTEGER REFERENCES objects(norad_cat_id),
    gp_id               INTEGER UNIQUE NOT NULL,
    epoch               TIMESTAMPTZ NOT NULL,
    fetched_at          TIMESTAMPTZ DEFAULT now(),
    mean_motion         NUMERIC(13,8),
    eccentricity        NUMERIC(13,8),
    inclination         NUMERIC(7,4),
    ra_of_asc_node      NUMERIC(7,4),
    arg_of_pericenter   NUMERIC(7,4),
    mean_anomaly        NUMERIC(7,4),
    semimajor_axis      DOUBLE PRECISION,
    period              DOUBLE PRECISION,
    apoapsis            DOUBLE PRECISION,
    periapsis            DOUBLE PRECISION,
    bstar               NUMERIC(19,14),
    mean_motion_dot     NUMERIC(9,8),
    mean_motion_ddot    NUMERIC(22,13),
    element_set_no      SMALLINT,
    rev_at_epoch        INTEGER,
    tle_line0           VARCHAR(27),
    tle_line1           VARCHAR(71),
    tle_line2           VARCHAR(71)
);

CREATE INDEX idx_snapshots_norad_epoch ON snapshots(norad_cat_id, epoch DESC);

CREATE TABLE event_types (
    code                 VARCHAR(64) PRIMARY KEY,
    display_name         VARCHAR(128),
    description_template TEXT,
    severity             VARCHAR(16),
    created_at           TIMESTAMPTZ DEFAULT now()
);

CREATE TABLE audt_events (
    id                BIGSERIAL PRIMARY KEY,
    norad_cat_id      INTEGER REFERENCES objects(norad_cat_id),
    event_type_code   VARCHAR(64) REFERENCES event_types(code),
    event_time        TIMESTAMPTZ NOT NULL,
    detail_json       JSONB,
    prev_snapshot_id  BIGINT REFERENCES snapshots(id),
    new_snapshot_id   BIGINT REFERENCES snapshots(id),
    created_at        TIMESTAMPTZ DEFAULT now()
);
