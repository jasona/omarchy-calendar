# DBus API

The initial read-only service owns `org.omarchy.Calendar` on the user bus and
exports `/org/omarchy/Calendar` with interface `org.omarchy.Calendar1`.

All payload methods return compact JSON. This keeps the first contract easy to
consume from QML, Quickshell, and command-line diagnostics while the underlying
SQLite schema can evolve independently.

| Method | Result |
| --- | --- |
| `GetEvents(firstDate, lastDate)` | Events whose local `dateKey` is in the inclusive ISO date range |
| `SearchEvents(queryText, limit)` | Case-insensitive title, location, and calendar-name matches |
| `GetCalendars()` | Calendar identity, color, provider metadata, access role, timezone, and sync-selection state |
| `GetAccounts()` | Provider accounts, enabled state, synchronization state, and errors |
| `GetNextEvent()` | First non-all-day event whose end time is in the future |
| `GetStatus()` | Schema version, database path, counts, feed timestamp, and last error |
| `GetProviderStatus()` | Google OAuth state plus synchronization progress, connectivity, last attempt/success, retry timing, and the latest error |
| `BeginGoogleAuthorization()` | Starts the loopback OAuth flow; returns false when client credentials or the callback listener are unavailable |
| `DisconnectGoogle()` | Removes the Google account, its cached calendars and events, and its refresh token from Secret Service |
| `SetCalendarSelected(calendarId, selected)` | Pauses or resumes a Google calendar, updates visible queries and the compatibility feed, and syncs when resumed |
| `CreateEvent(eventJson)` | Optimistically stores a new event and atomically queues its Google create mutation; returns its local event ID |
| `SyncNow()` | Starts Calendar List and Events synchronization when an authenticated Google access token is available |
| `Reload()` | Transactionally re-import the compatibility feed |

The service emits `EventsChanged` only after a successful event transaction.
`AccountsChanged` is reserved for successful account authorization, removal,
or account-state changes.

Google authorization emits `AuthorizationRequired(url)` after the local
callback listener is ready. A UI client opens that URL in the user's browser.
`ProviderStatusChanged` reports transitions through disconnected,
authorizing, connected, and error states. Refresh tokens are stored through
Secret Service and are never written to SQLite.
