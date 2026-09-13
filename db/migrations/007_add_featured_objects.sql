-- Marks objects that should always be included in GpPoller's hourly "hot"
-- slice (see main.cpp's kGpMaxHotSlots) regardless of hit_count -- a
-- curated, hand-verified seed list of publicly recognizable objects (ISS
-- modules, famous collisions/ASAT debris, flagship telescopes, notable
-- GEO failures, espionage stories, etc.), chosen to bootstrap visible
-- orbital-event history before real site traffic accumulates enough
-- hit_count on its own. See DbWriter::top_gp_hot_targets, which reads
-- this column alongside hit_count.
ALTER TABLE objects ADD COLUMN featured BOOLEAN NOT NULL DEFAULT false;

UPDATE objects SET featured = true WHERE norad_cat_id IN (
    -- Historic firsts / milestones
    5,      -- VANGUARD 1 -- oldest human-made object still in orbit
    7530,   -- OSCAR 7 -- 1974 ham satellite, "died" 1981, revived itself in 2002
    8820,   -- LAGEOS 1 -- geodetic satellite, ~8.4M year orbital lifetime
    22195,  -- LAGEOS 2 -- companion to LAGEOS 1, actively laser-ranged worldwide

    -- Space telescopes
    20580,  -- HST -- Hubble Space Telescope
    50463,  -- JWST -- James Webb Space Telescope
    25867,  -- CXO -- Chandra X-ray Observatory
    28485,  -- SWIFT -- gamma-ray burst observatory
    33053,  -- GLAST -- Fermi Gamma-ray Space Telescope
    38358,  -- NUSTAR -- X-ray observatory
    43435,  -- TESS -- exoplanet-hunting telescope
    49954,  -- IXPE -- X-ray polarimetry mission

    -- Famous collisions / ASAT debris
    22675,  -- COSMOS 2251 -- 2009 Iridium/Cosmos collision
    38486,  -- COSMOS 2251 DEB -- tracked fragment from that collision
    24946,  -- IRIDIUM 33 -- 2009 Iridium/Cosmos collision
    38229,  -- IRIDIUM 33 DEB -- tracked fragment from that collision
    25730,  -- FENGYUN 1C -- 2007 Chinese ASAT test
    31378,  -- FENGYUN 1C DEB -- tracked fragment from the ASAT test
    22236,  -- COSMOS 2221 -- Feb 2024 near-miss with TIMED
    26998,  -- TIMED -- NASA's half of that same near-miss

    -- GEO failures / "zombie satellite" stories
    28884,  -- GALAXY 15 -- the original 2010 zombiesat
    22927,  -- TELSTAR 401 -- killed by a geomagnetic storm in 1997
    27820,  -- AMC-9 -- 2017 in-orbit breakup/loss
    41748,  -- INTELSAT 33E -- Oct 2024 GEO breakup, 16,000+ fragments

    -- Espionage / geopolitical
    37348,  -- USA 224 -- KH-11 spy satellite from the 2019 Trump Iran tweet
    65271,  -- USA 555 -- X-37B OTV-8, ongoing secretive spaceplane mission
    49330,  -- SJ-21 -- China's satellite-tug demo (towed a dead BeiDou sat)
    62485,  -- SJ-25 -- China's on-orbit refueling demo with SJ-21

    -- Space stations
    25544,  -- ISS (ZARYA)
    26400,  -- ISS (ZVEZDA)
    49044,  -- ISS (NAUKA)
    48274,  -- CSS (TIANHE-1)
    53239,  -- CSS (WENTIAN)
    54216,  -- CSS (MENGTIAN)

    -- Earth observation / weather
    25994,  -- TERRA
    27386,  -- ENVISAT
    49260,  -- LANDSAT 9
    41866,  -- GOES 16

    -- Navigation
    43873,  -- NAVSTAR 77 / USA 289 -- first GPS III, publicly named via contest

    -- Commercial constellations
    49187,  -- ONEWEB-0303
    58214,  -- STARLINK-30805
    68924   -- KUIPER-00408
);
