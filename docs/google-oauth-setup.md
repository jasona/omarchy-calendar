# Google OAuth development setup

Omarchy Calendar uses Google's installed desktop application flow with PKCE
and a temporary loopback listener on `127.0.0.1`. It requests the narrow
`openid`, `email`, `calendar.calendarlist.readonly`, and `calendar.events`
scopes. OpenID and email identify and label the connected account. Calendar
event access permits synchronization and user-directed creation, updates, and
deletion without granting broader calendar-settings access.
Refresh tokens are stored in Secret Service.

Install and unlock a Secret Service provider before connecting: `gnome-keyring`
is the usual Omarchy choice; KWallet and a configured KeePassXC can also provide
the service. Installing `libsecret` alone does not provide token storage.
Official release packages already include the publisher's Desktop app client
configuration. The following setup is for development or a personal client.

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
