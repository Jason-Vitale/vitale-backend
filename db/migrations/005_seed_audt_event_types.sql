-- Seeds event_types for the 8 rules now implemented in libs/rule_engine:
-- 4 GP-snapshot rules (maneuver_detected, raan_shift, eccentricity_change,
-- drag_change) and 4 SATCAT/objects-row rules (decay_detected,
-- object_renamed, object_type_changed, rcs_size_changed).
--
-- maneuver_detected and decay_detected already have placeholder rows from
-- 001_init.sql (severity 'warning'/'info', no rule behind them yet) --
-- ON CONFLICT DO UPDATE brings those in line with the now-implemented
-- rules' severity/description, while the other 6 codes are inserted fresh.
INSERT INTO event_types (code, display_name, description_template, severity) VALUES
    ('maneuver_detected', 'Maneuver Detected',
     'Inclination changed by {{inclination_delta_deg}} deg and/or semimajor axis changed by {{semimajor_axis_delta_km}} km between {{prev_epoch}} and {{curr_epoch}}.',
     'notable'),
    ('raan_shift', 'Orbital Plane Shift',
     'Right ascension of ascending node changed by {{ra_of_asc_node_delta_deg}} deg between {{prev_epoch}} and {{curr_epoch}}.',
     'notable'),
    ('eccentricity_change', 'Eccentricity Change',
     'Orbital eccentricity changed by {{eccentricity_delta}} between {{prev_epoch}} and {{curr_epoch}}.',
     'info'),
    ('drag_change', 'Drag Coefficient Change',
     'BSTAR drag term changed from {{prev_bstar}} to {{curr_bstar}} between {{prev_epoch}} and {{curr_epoch}}.',
     'info'),
    ('decay_detected', 'Object Decayed',
     'Object decay_date was set to {{decay_date}}.',
     'critical'),
    ('object_renamed', 'Object Renamed',
     'Catalog name changed from {{old_name}} to {{new_name}}.',
     'info'),
    ('object_type_changed', 'Object Reclassified',
     'Object type changed from {{old_type}} to {{new_type}}.',
     'notable'),
    ('rcs_size_changed', 'RCS Size Changed',
     'Radar cross-section size class changed from {{old_rcs_size}} to {{new_rcs_size}}.',
     'info')
ON CONFLICT (code) DO UPDATE SET
    display_name = EXCLUDED.display_name,
    description_template = EXCLUDED.description_template,
    severity = EXCLUDED.severity;
