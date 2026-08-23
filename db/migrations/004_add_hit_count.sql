-- Tracks how many times each object's detail page (GET /objects/<id>) has
-- been looked up, backing a "most popular objects" endpoint. Only that
-- specific per-object lookup increments this -- list/search endpoints
-- return many rows per request and would otherwise inflate every returned
-- object's count just for appearing in a page of results.
ALTER TABLE objects ADD COLUMN hit_count BIGINT NOT NULL DEFAULT 0;
