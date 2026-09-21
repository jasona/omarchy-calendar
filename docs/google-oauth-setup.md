# Google OAuth development setup

Omarchy Calendar uses Google's installed desktop application flow with PKCE
and a temporary loopback listener on `127.0.0.1`. It requests the narrow
`calendar.calendarlist.readonly` and `calendar.events.readonly` scopes.
Refresh tokens are stored in Secret Service.

Create an OAuth client whose application type is **Desktop app**, then create:

```ini
# ~/.config/omarchy-calendar/google-oauth.env
OMARCHY_CALENDAR_GOOGLE_CLIENT_ID=your-client-id.apps.googleusercontent.com
OMARCHY_CALENDAR_GOOGLE_CLIENT_SECRET=your-client-secret
```

Protect the file and restart the service:

```bash
chmod 600 ~/.config/omarchy-calendar/google-oauth.env
systemctl --user daemon-reload
systemctl --user restart omarchy-calendar.service
```

Open **Settings** with `Ctrl+,`. When the provider reports that it is ready,
select **Connect Google**. Authentication opens in the system browser and
returns to the local callback listener; the browser never passes the refresh
token through the application UI.
