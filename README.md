# Omarchy Calendar

A native calendar for [Omarchy](https://omarchy.org/), built with Qt 6 and QML. Browse your schedule, manage Google Calendar events, and keep your calendar available offline in a desktop app that follows your Omarchy theme.

![Omarchy Calendar week view](screenshots/week.png)

## ✨ Features

- **Plan your way:** Day, week, month, and agenda views, with a live current-time indicator and all-day events.
- **Work offline:** Events live in a local SQLite database. Changes are queued and synchronized with Google Calendar when connectivity returns.
- **Edit directly:** Create, move, resize, and delete events; manage guests, reminders, recurrence, time zones, availability, and meeting links.
- **Find events quickly:** Search titles, notes, locations, calendars, organizers, and guests. Filters include `calendar:`, `after:`, `before:`, `organizer:`, and `response:`.
- **Stay informed:** Desktop reminders, invitation responses, sync status, and an inspector for queued or conflicted changes.
- **Feel at home on Omarchy:** Follows the active palette and typography settings, offers two density modes, and supports keyboard navigation and screen readers.

## 📸 Screenshots

| Day | Week |
| --- | --- |
| <img src="screenshots/day.png" alt="Day view with timed events and the current-time line" width="480"> | <img src="screenshots/week.png" alt="Week view with events across seven days" width="480"> |

| Month | Agenda |
| --- | --- |
| <img src="screenshots/month.png" alt="Month view with events in a calendar grid" width="480"> | <img src="screenshots/agenda.png" alt="Agenda view listing upcoming events" width="480"> |

| Search | Settings |
| --- | --- |
| <img src="screenshots/search.png" alt="Search screen" width="480"> | <img src="screenshots/settings.png" alt="Settings and Google Calendar account controls" width="480"> |

**Welcome screen**

<img src="screenshots/onboarding.png" alt="Welcome screen explaining local storage and Google Calendar connection" width="480">

## 📦 Install on Omarchy

From a clone of this repository, build the included Arch package:

```bash
cd packaging/arch
makepkg -si
systemctl --user enable --now omarchy-calendar.service
```

Launch **Omarchy Calendar** from the app launcher. The package installs the desktop app, background service, icon, desktop entry, and DBus activation metadata. Your database and local OAuth configuration remain in your home directory across package upgrades.

Google Calendar connection needs a desktop OAuth client. Official release packages may include publisher-provided credentials; for a local build, follow the [Google OAuth setup guide](docs/google-oauth-setup.md) and place your credentials in `~/.config/omarchy-calendar/google-oauth.env`. The app also works with locally cached events when offline.

## 🛠️ Build from source

The desktop app has a qmake project for a quick build on Omarchy:

```bash
mkdir -p build-qmake
cd build-qmake
qmake6 ../omarchy-calendar.pro
make -j"$(nproc)"
./omarchy-calendar
cd ..
```

To build the background service from the repository root:

```bash
mkdir -p build-service
cd build-service
qmake6 ../service/omarchy-calendar-service.pro
make -j"$(nproc)"
cd ..
```

The packaged build uses CMake. It requires Qt 6.5 or newer (Core, DBus, Gui, Network, NetworkAuth, Qml, Quick, QuickControls2, and Sql), `libsecret`, `pkg-config`, a C++20 compiler, and CMake 3.21 or newer:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

The app reads calendar data from the service over DBus and can fall back to the Omarchy JSON calendar feed if the service is unavailable. The service stores data in `~/.local/share/omarchy-calendar/calendar.db` and keeps Google calendars synchronized. The app follows `~/.local/state/omarchy/current/theme/colors.toml` and typography overrides in `~/.config/omarchy/shell.toml`.

## ⌨️ Everyday shortcuts

| Action | Shortcut |
| --- | --- |
| Day / Week / Month / Agenda | `1` / `2` / `3` / `4` |
| Today | `T` |
| Search | `/` |
| New event / Quick Add | `N` / `Ctrl+K` |
| Edit / delete selected event | `E` / `Delete` |
| Settings / all shortcuts | `Ctrl+,` / `F1` |

Arrow keys and `J` / `K` navigate events. In day and week views, use `Alt+↑/↓` to move a selected event by 15 minutes, `Alt+←/→` to move it by a day, and `Alt+Shift+↑/↓` to resize it. Quick Add understands phrases with relative dates, weekdays, times, and durations, and lets you review the result before saving.

## 🧪 Testing and releases

Build the service, then run the contract suite from the repository root:

```bash
./tests/run-contract-tests.sh build-service/omarchy-calendar-service
```

Run `./tests/capture-visual-baseline.sh` to generate the light and dark visual checks. The app also supports `--view <name>` and `--screenshot <path>` for automated captures.

Version **0.9.0** is the v1.0 release candidate. Public release depends on the publisher's Google OAuth domain and verification steps; see the [release checklist](docs/google-oauth-release-checklist.md). Release builds can supply OAuth credentials through CMake's `OMARCHY_CALENDAR_OAUTH_ENV_FILE` setting. `omarchy-calendar --release-info` prints the public metadata embedded in a build.

## 🤝 Contributing and security

Issues and pull requests are welcome. Please report vulnerabilities privately according to [SECURITY.md](SECURITY.md). This project is licensed under the [MIT License](LICENSE).
