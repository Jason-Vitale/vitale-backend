# vitale-backend

AUDT (Anomalous Underlying Debris/object Tracking — orbital event) tracker.
Polls Space-Track.org for current TLE/GP data on tracked orbital objects,
detects events (maneuvers, decay) against an in-process rule engine, and
serves the resulting history to a separate frontend.

## Architecture

Two independent services, one shared in-process library, one shared Postgres
instance:

- **`services/poller`** — write path. Fetches GP data from Space-Track,
  stores new snapshots (deduped), runs `rule_engine::RuleRegistry` over each
  `(prev, curr)` snapshot pair, and writes any resulting `audt_events`.
- **`services/api`** — read path. Serves `objects` and `audt_events` history
  to the frontend. Never writes. Does not link `rule_engine`.
- **`libs/rule_engine`** — static library, linked only into `poller`. Not a
  networked service; just a `Rule` interface + `RuleRegistry` + the concrete
  rules, so detection logic is unit-testable in isolation from HTTP/DB code.

Both services connect to the same Postgres instance as ordinary network
clients (via `DATABASE_URL`) — they are not colocated in-process and share no
memory. Postgres itself (Supabase/Neon) and compute (poller + api binaries on
a small cloud VM) are hosted separately. The frontend is a separate React
repo (Vercel), not part of this repository.

## Language / toolchain

- **C++20** is the project-wide baseline (`CMAKE_CXX_STANDARD 20`), chosen
  for stable, broad compiler support.
- **C++23** is adopted selectively, per-target, where a specific feature
  clearly helps rather than chasing "full C++23": `services/poller` opts
  into C++23 (`target_compile_features(poller PRIVATE cxx_std_23)`)
  specifically for `std::expected<T, std::string>` in
  `space_track_client.hpp`, modeling Space-Track HTTP calls as an explicit
  success/error return rather than exceptions or out-params.
- **CMake** (3.20+) as the build system, with `find_package(... CONFIG)`
  against Homebrew-installed dependencies.

## Dependencies

Installed via Homebrew (macOS):

```
brew install libpqxx nlohmann-json crow
```

(`curl` uses the system libcurl already present on macOS; `crow` pulls in
`asio` as its only transitive dependency.)

| Dependency | Used by | Purpose |
|---|---|---|
| libcurl | poller | HTTP client + in-memory cookie engine for Space-Track session auth |
| nlohmann/json | rule_engine | Building `detail_json` payloads for detected events |
| libpqxx | poller, api | Postgres C++ client |
| Crow | api | HTTP framework for the read-only API |

### Why Crow over Drogon

Crow was chosen over Drogon for the API service because this service is
small and deliberately read-only: it needs routing + JSON responses over a
handful of endpoints, nothing more. Crow is header-only-ish (one small
compiled unit via its Homebrew package), pulls in a single dependency
(`asio`), and stays out of the way of using `libpqxx` directly. Drogon is a
more complete framework — built-in ORM, its own async DB drivers, coroutine
support — but that ORM/DB layer would either go unused (since we already
have libpqxx + hand-written SQL matching a fixed schema) or compete with it.
For a small internal read API, Crow's minimalism is the better fit; Drogon
would be worth revisiting if the API service grows significantly (e.g. needs
WebSockets or heavy async fan-out).

## Building

```
mkdir build && cd build
cmake ..
cmake --build .
```

Produces three targets: `libs/rule_engine/librule_engine.a` (static lib),
`services/poller/poller`, `services/api/api`.

## Configuration (environment variables)

| Variable | Used by | Purpose |
|---|---|---|
| `SPACETRACK_IDENTITY` | poller | Space-Track.org account username |
| `SPACETRACK_PASSWORD` | poller | Space-Track.org account password |
| `DATABASE_URL` | poller, api | Postgres connection string (libpq keyword/value or URI form) |
| `API_PORT` | api | Port for the API service to listen on (default `8080`) |

## Space-Track integration notes

- **Auth**: `POST /ajaxauth/login` with form fields `identity`/`password`;
  the session cookie is captured by curl's cookie engine and reused for all
  later requests on the same `SpaceTrackClient` instance.
- **Rate limit**: the `gp` class allows at most 1 request/hour. Always batch
  every NORAD ID of interest into a single comma-delimited query — never
  issue one request per satellite.
- **Query filter**: GP queries append `/decay_date/null-val/epoch/%3Enow-10/`
  to restrict results to live, propagable objects.

## Database

Schema lives in `db/migrations/001_init.sql`: `objects` (static per-object
metadata), `snapshots` (per-poll orbital state, deduped on `gp_id`,
indexed on `(norad_cat_id, epoch DESC)`), `event_types` (rule metadata), and
`audt_events` (detected events, linking the `prev`/`new` snapshot pair that
triggered them).

**Dedup rule**: if a freshly fetched `gp_id` matches the last stored
snapshot's `gp_id` for that object, the insert is skipped entirely — no
"checked, nothing changed" rows are ever written.

## Rule engine

`libs/rule_engine` defines a small `Rule` interface
(`evaluate(prev, curr) -> optional<DetectedEvent>`) and a `RuleRegistry` that
runs every registered rule and collects all firings (more than one rule can
fire for the same snapshot pair). Built-in rules:

- **`ManeuverDetectedRule`** — fires when `inclination` and/or
  `semimajor_axis` change beyond a noise threshold between consecutive
  snapshots. Thresholds (`kInclinationDeltaThresholdDeg`,
  `kSemimajorAxisDeltaThresholdKm`) are placeholder constants in
  `maneuver_detected_rule.cpp` and need tuning against real GP history.
- **`DecayDetectedRule`** — fires when `decay_date` flips from null to
  non-null between consecutive snapshots.

## Status / next steps

Current milestone: CMake scaffolding for all three targets, dependencies
linking, and Space-Track login + a single GP query working end-to-end
(`services/poller/main.cpp` is currently a smoke test: login, fetch GP for
one NORAD ID, print the raw JSON response). Not yet wired up: parsing GP
JSON into `Snapshot` rows, the full poller loop (`db_writer` +
`RuleRegistry` integration), and rate-limit/scheduling logic around the
1 request/hour constraint.
