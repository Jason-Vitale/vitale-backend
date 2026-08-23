CREATE EXTENSION IF NOT EXISTS pg_trgm;
CREATE INDEX IF NOT EXISTS idx_objects_name_trgm ON objects USING gin (object_name gin_trgm_ops);
