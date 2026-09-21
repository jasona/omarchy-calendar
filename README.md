# Omarchy Calendar

An early native calendar for Omarchy, built with Qt 6 and QML. The current
prototype displays day, week, month, and agenda views from the offline
database, searches titles and locations, watches for sync updates, remembers
calendar visibility, and opens event details without requiring network access.

## Bootstrap build

The repository includes a qmake project so the prototype can build on a stock
Omarchy installation before the CMake development tools are installed.

```bash
mkdir -p build-qmake
cd build-qmake
qmake6 ../omarchy-calendar.pro
make -j"$(nproc)"
./omarchy-calendar
```

The prototype reads the active palette from
`~/.local/state/omarchy/current/theme/colors.toml` and typography overrides
from `~/.config/omarchy/shell.toml`. Changes reload while it is running.

Calendar events are served from the local SQLite database over DBus. The
existing `~/.local/state/omarchy/calendar-events.json` feed is imported as an
offline fallback and atomically refreshed from native Google data after every
successful synchronization for use by the Omarchy top bar.
Provider calendars can be paused or resumed from Settings. Paused calendars
remain cached locally, disappear from app queries and the top-bar feed, and
resume incremental synchronization without losing their stored data.

Use `1`, `2`, `3`, and `4` to switch between Day, Week, Month, and Agenda.
Press `T` to return the current calendar view to today, and `/` to search.
Arrow keys move through days and events, `J`/`K` select the next or previous
event or week row, and `Enter` opens the focused day or event. In Search,
press Down to move from the query field into the results.

Press `N` or choose **New event** to open the event composer. New events are
stored immediately in SQLite and placed in the durable mutation queue so they
survive restarts while Google write authorization and upload are completed.
Existing read-only connections can choose **Enable editing** in Settings to
grant the event-management scope without disconnecting or clearing cached data.
Once editing is enabled, queued creations upload automatically. Successful
responses replace the temporary local identity atomically, and transient
failures remain queued with bounded retry rather than losing the event. Stable
client-assigned Google IDs make retries idempotent across process restarts.

Open Settings with `Ctrl+,`. Google account onboarding uses Qt NetworkAuth's
desktop loopback flow and stores refresh tokens in Secret Service. Development
OAuth client setup is documented in
[`docs/google-oauth-setup.md`](docs/google-oauth-setup.md).

After connection, the service downloads the Google Calendar List and selected
calendar events, stores a sync token per calendar, refreshes incrementally every
five minutes, and repeats a full sync automatically when Google expires a token.
Transient failures use bounded exponential backoff and resume immediately when
network connectivity returns. Settings shows live calendar progress, the last
successful sync, actionable errors, and a guarded account-disconnect action.
The deterministic sync contract covers server outages, Google rate limiting,
expired incremental cursors, revoked authorization, cursor safety, and cached
calendar retention.
After the first successful Google sync, native provider data replaces the
legacy compatibility calendar in app queries; the legacy cache remains stored
as an automatic fallback if the Google account is disconnected.

## Local service

The service imports the compatibility feed transactionally into
`~/.local/share/omarchy-calendar/calendar.db`, watches for feed changes, and
owns `org.omarchy.Calendar` on the user DBus session. The app prefers this
service and falls back to reading the JSON feed if the service is unavailable.

Build and test it with:

```bash
mkdir -p build-service
cd build-service
qmake6 ../service/omarchy-calendar-service.pro
make -j"$(nproc)"
cd ..
./tests/test-service-import.sh
./tests/test-google-database.sh
./tests/test-dbus-contract.sh build-service/omarchy-calendar-service
./tests/test-google-auth-contract.sh build-service/omarchy-calendar-service
./tests/test-google-sync-retry.sh
./tests/test-google-mutation-upload.sh
```
