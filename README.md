# Omarchy Calendar

An open-source native calendar for Omarchy, built with Qt 6 and QML. The
application displays day, week, month, and agenda views from the offline
database, searches titles and locations, watches for sync updates, remembers
calendar visibility, and opens event details without requiring network access.

Version 0.9.0 is the v1.0 release candidate. All internal product, packaging,
performance, correctness, resilience, and visual gates pass. Public v1.0 is
waiting on the publisher-controlled Google OAuth domain and verification steps
in [`docs/google-oauth-release-checklist.md`](docs/google-oauth-release-checklist.md).

Security vulnerabilities should be reported privately as described in
[`SECURITY.md`](SECURITY.md).

## Install on Omarchy

The release package installs the application, background service, desktop
entry, icon, DBus activation metadata, AppStream metadata, and documentation.
From an AUR checkout, run:

```bash
cd packaging/arch
makepkg -si
systemctl --user enable --now omarchy-calendar.service
```

OAuth credentials remain a local administrator setting in
`~/.config/omarchy-calendar/google-oauth.env`; package upgrades do not replace
that file or the calendar database. Removing the package leaves user data in
`~/.local/share/omarchy-calendar` intact.

Official release builds can install publisher-managed production credentials
from an untracked file using CMake's `OMARCHY_CALENDAR_OAUTH_ENV_FILE` option.
The systemd unit loads those defaults first and a user's private configuration
second, so local development clients remain supported without editing package
files.

The public homepage, policies, and support contact are also release inputs. Set
`OMARCHY_CALENDAR_HOMEPAGE_URL`, `OMARCHY_CALENDAR_PRIVACY_URL`,
`OMARCHY_CALENDAR_TERMS_URL`, and `OMARCHY_CALENDAR_SUPPORT_EMAIL` as CMake
cache values; `omarchy-calendar --release-info` prints the exact values embedded
in a build. The official workflow pins the published Last Refuge URLs and
support address directly because they are public release metadata.

Pushing a version-matched `v*` tag runs the complete release workflow, builds
the credential-bearing Arch package and source archive, writes SHA-256
checksums, and publishes a GitHub release. The repository secrets
`GOOGLE_OAUTH_CLIENT_ID` and `GOOGLE_OAUTH_CLIENT_SECRET` must contain the
verified production desktop client. Changes under `docs/` deploy through the
separate GitHub Pages workflow.

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
Shared radius and spacing tokens keep controls and panels visually consistent,
and Settings offers comfortable and compact density modes. The main shell
adapts its navigation and inspector widths down to a 900×640 window while
retaining every calendar action; long event details scroll independently.

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
Press `F1` for an in-app shortcut reference. Primary views, event rows,
calendar visibility toggles, form fields, and icon-only controls expose
screen-reader names, roles, state, and press actions. Theme colors are adjusted
when necessary to preserve 7:1 primary-text contrast and 4.5:1 muted, accent,
status, and error contrast; typography overrides are bounded to keep layouts
usable at large font sizes.

Press `N` or choose **New event** to open the event composer. New events are
stored immediately in SQLite and placed in the durable mutation queue so they
survive restarts while Google write authorization and upload are completed.
Existing read-only connections can choose **Enable editing** in Settings to
grant the event-management scope without disconnecting or clearing cached data.
Once editing is enabled, queued creations upload automatically. Successful
responses replace the temporary local identity atomically, and transient
failures remain queued with bounded retry rather than losing the event. Stable
client-assigned Google IDs make retries idempotent across process restarts.

Select a writable event and choose **Edit event**, or press `E`, to reopen the
composer with its current title, time, location, and description. Changes are
applied locally in one transaction and queued for Google with the event ETag.
Edits made before a new event uploads are folded into its existing create
operation, while subsequent edits use an authenticated Google `PATCH`.
If Google reports that the event changed elsewhere, the service fetches the
current version and performs a three-way merge. Remote-only changes are kept
and the edit retries with the new ETag; overlapping field changes stop safely
and appear as a named conflict in Settings.

Timed events can be dragged in Day and Week to move them in 15-minute steps;
Week also moves across day columns, and Month events can be dragged between
date cells. Drag the lower edge of a timed card to resize it. For keyboard
control, use `Alt+↑/↓` to move by 15 minutes, `Alt+←/→` to move by a day, and
`Alt+Shift+↑/↓` to resize. Rapid adjustments are coalesced locally before the
latest ETag-protected update is sent to Google.

The composer supports all-day events and separate start and end dates. All-day
spans retain Google's date-only start and exclusive end values without passing
through UTC, while timed events that cross midnight appear on each covered day.
All-day events can be dragged across days in Week and Month, moved with the same
keyboard shortcuts, and converted to or from scheduled time in the composer.

Timed events can use any IANA timezone from the composer. Wall times are resolved
in the selected zone before they are saved, recent choices stay at the top of the
picker, and the preview shows the equivalent local date and time. Nonexistent
spring-forward times are rejected, repeated fall-back times offer first and
second occurrence choices, and the inspector preserves the event's original zone
while showing a local-time equivalent when it differs.

Google recurrence identity now survives synchronization, local edits, delete and
undo, and conflict rebasing. The inspector identifies recurring events and moved
exceptions, and the local RRULE engine expands daily, weekly, monthly, and yearly
rules with stable wall times across daylight-saving transitions. New events can
repeat daily, on weekdays, weekly, monthly, yearly, or at a custom interval, with
an optional occurrence count. The editor can apply a change to one occurrence,
this and following occurrences, or the entire series. Series edits retrieve the
parent event and use its current ETag. This-and-following changes split the rule
into an earlier series and a deterministic new future series, preserving bounded
occurrence counts without producing an exception for every instance.

Choose **Delete event** or press `Delete` to remove a writable event
immediately. A themed notification offers Undo for six seconds; Google does not
receive the deletion until that window closes. The durable delete queue treats
an already-absent remote event as success and restores the local event after a
permanent provider rejection. Deleting a new event before its first upload
cancels that creation without sending unnecessary provider traffic. Recurring
events offer the same three scopes; future cancellation truncates the parent rule
and whole-series cancellation targets the recurring parent.

The composer also manages Google guests, up to five popup reminders, busy/free
availability, event visibility, and guest permissions. Synced organizer and
attendee response state is available offline in the event inspector, which also
surfaces notes and prominent actions for Google Meet, Zoom, Microsoft Teams,
phone, and SIP entry points, including dial-in PINs and access codes. The
composer can create a Google Meet conference when the selected calendar reports
that capability; conference requests use a durable unique ID so retries cannot
create duplicate meetings. Guest changes
use Google's attendee notification flow, while three-way conflict handling keeps
remote guest, reminder, privacy, and availability changes from being overwritten
silently. Standard non-recurring events can move between writable calendars in
the same Google account; the composer disables that control when Google limits
the operation to the organizer or does not support the event type.

Press `Ctrl+K` for Quick Add. It understands relative dates, weekdays, ISO
dates, 12- or 24-hour times, and durations such as `for 45m` or `for 1h 30m`.
The popup shows the exact interpretation and opens the regular composer for a
final correction before anything is saved.

Search covers titles, notes, locations, calendar names, organizers, and guests.
It supports `calendar:`, `after:`, `before:`, `organizer:`, and `response:`
filters, including quoted calendar names. Matching title and detail text is
highlighted, and the result list supports arrow keys, J/K, Enter, edit, and
delete shortcuts.

Settings includes a durable queued-changes inspector. It shows each local
operation, state, attempt count, and provider error; failed or conflicted work
can be retried, edited, or discarded. Queue ordering stops later writes behind
an unresolved earlier change, retryable failures back off automatically, and
discarded remote edits are replaced by a fresh Google sync.

Invitations show the signed-in attendee’s response and provide Going, Maybe,
and No actions in the event inspector. RSVP mutations preserve the full guest
list, rebase safely when the invitation changes remotely, and propagate the
response through Google. Newly synchronized unanswered invitations produce one
deduplicated desktop notification.

The background service schedules popup reminders from event overrides or the
calendar defaults. Delivery state is persisted in SQLite, so service restarts
cannot repeat an acknowledged alert and snoozes survive until their new due
time. Notifications offer Open, Join when a meeting link is available,
Snooze 10 minutes, and Dismiss actions. Enabling the scheduler on an existing
database records a first-run boundary so old events do not produce an alert
flood.

Open Settings with `Ctrl+,`. Google account onboarding uses Qt NetworkAuth's
desktop loopback flow and stores refresh tokens in Secret Service. Development
OAuth client setup is documented in
[`docs/google-oauth-setup.md`](docs/google-oauth-setup.md).
First launch presents a themed welcome guide that explains local storage,
offline behavior, and Google authorization, then follows connection and initial
sync through to the calendar. The guide can be reopened from Settings. A
one-click redacted diagnostics export copies versions, counts, provider state,
and current errors while excluding tokens, event content, database paths, and
account identity.

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

The sync worker releases paginated Google response buffers as soon as an
attempt completes, fails, or is cancelled. The performance contract imports
20,000 events and repeatedly exercises visible-range and full-text queries;
the current reference run completes 250 range loads in about 2.8 seconds and
25 searches in under half a second. A live 7,000-event account returns a week
over DBus in about 7 ms and retains roughly 111 MB after synchronization.

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
./tests/test-release-metadata.sh
./tests/test-google-database.sh
./tests/test-dbus-contract.sh build-service/omarchy-calendar-service
./tests/test-google-auth-contract.sh build-service/omarchy-calendar-service
./tests/test-google-sync-retry.sh
./tests/test-google-mutation-upload.sh
./tests/test-google-event-move.sh
./tests/test-google-rsvp.sh
./tests/test-mutation-recovery.sh
./tests/test-secret-store-failure.sh
./tests/test-reminder-scheduler.sh
./tests/test-theme-accessibility.sh
./tests/test-performance-smoke.sh
./tests/test-google-series-update.sh
./tests/test-quick-entry-parser.sh
./tests/test-google-all-day-upload.sh
./tests/test-google-delete-undo.sh
./tests/test-event-adjustment.sh
./tests/test-all-day-multiday.sh
./tests/test-timezone-contract.sh
./tests/test-recurrence-contract.sh
./tests/test-package-install.sh
```

Run the complete contract suite with
`./tests/run-contract-tests.sh build-service/omarchy-calendar-service`.
Generate the sanitized light/dark visual matrix with
`./tests/capture-visual-baseline.sh`; the app's `--view` and `--screenshot`
options make the same render path available to CI without a display server.
