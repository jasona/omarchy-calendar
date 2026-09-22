# Google OAuth verification submission

This is the review package for the public Omarchy Calendar OAuth client in
Google Cloud project `omarchy-calendar-509223`. Keep this page beside the Google
Auth Platform verification form and update the status table as the review
advances.

## Submission identity

| Field | Production value | Status |
| --- | --- | --- |
| Application name | Omarchy Calendar | Configured |
| Application type | Desktop app | Configured |
| Publisher domain | `lastrefuge.ai` | Verified through Cloudflare DNS on 2026-09-21 |
| Homepage | `https://lastrefuge.ai/projects/omarchy-calendar` | Live and configured |
| Privacy policy | `https://lastrefuge.ai/privacy` | Live and configured |
| Terms | `https://lastrefuge.ai/terms` | Live and configured |
| Public support | `jason@greatspark.com` | Published on site and project Editor; Google consent screen uses account email |
| Developer contact | `jason@greatspark.com` | Configured |
| Application icon | `docs/omarchy-calendar-oauth-logo.png` | Uploaded; branding verified and published |
| Audience | External | In production |

## Requested scopes and form copy

Request only these four scopes:

| Scope | Why Omarchy Calendar needs it |
| --- | --- |
| `openid` | Establishes a stable identity for the Google account the user intentionally connects. |
| `https://www.googleapis.com/auth/userinfo.email` | Displays the connected email address so the user can identify and disconnect the correct account. It is not used for advertising, profiling, or sharing. |
| `https://www.googleapis.com/auth/calendar.calendarlist.readonly` | Discovers the user's calendars and reads their names, colors, visibility, access roles, and default reminder settings so the application can present an accurate calendar list. |
| `https://www.googleapis.com/auth/calendar.events` | Reads and manages the calendar events the user chooses to work with, including creation, editing, deletion, invitations, responses, recurrence, reminders, and meeting details. It also supports the local offline mutation queue and reconciliation after connectivity returns. |

Suggested sensitive-scope explanation:

> Omarchy Calendar is a native Linux calendar client. Users connect their own
> Google account and use the visible calendar interface to read and manage
> events. Calendar-list read access discovers and labels their calendars.
> Calendar-events access powers event display, creation, editing, deletion,
> invitations, RSVP, recurrence, reminders, meeting links, and an offline queue
> that uploads only the user's requested changes. OpenID and email identify and
> label the connected account. Data is stored locally on the user's computer;
> the developer does not operate a calendar-data backend and does not sell,
> advertise against, or share Google user data.

## Demo recording script

Record one continuous, readable video with the browser address bar and the full
application window visible where relevant. Use a dedicated test calendar and
avoid exposing unrelated personal events. Upload the result to YouTube as
**Unlisted** and keep its URL; Google's form requires a YouTube link. The
unverified-app warning is expected and must remain visible in the recording.
The project currently has one OAuth client, the Desktop app client, and the
recording must show that client in use.

1. Open the public product page, privacy policy, and terms on
   `lastrefuge.ai`. Show that they load without authentication and that the
   privacy policy identifies each category of Google data and its use.
2. Launch a clean Omarchy Calendar profile. Show the welcome screen and choose
   **Connect Google Calendar**.
3. Show Google's authorization screen, the Omarchy Calendar identity, and every
   requested permission. Continue with the test account.
4. Return to the application. Show the connected account label and synchronized
   calendar list, then briefly disable the network and show that previously
   synchronized events remain available locally.
5. Create a clearly named test event. Edit its time and details, add or respond
   to an invitation if the test setup permits, then show the same change in
   Google Calendar.
6. Delete the test event and show that deletion reaches Google Calendar.
7. Disconnect the account in Omarchy Calendar. Explain that the local account
   cache and Secret Service refresh token are removed, and show the returned
   disconnected state.

Keep the recording concise. Narrate which permission enables each visible step;
do not add marketing material or unrelated product features.

## Final console sequence

- [x] Verify `lastrefuge.ai` in Search Console through the Cloudflare DNS flow.
- [x] Save `jason@greatspark.com` as a project Editor.
- [ ] Select `jason@greatspark.com` as the OAuth user-support email if Google
      makes it eligible; otherwise use a Google Group at the public support
      address and document its ownership.
- [x] Upload the prepared 120×120 application icon in Branding and save.
- [x] Reopen Branding and Data Access to confirm the production URLs, authorized
      domain, support contact, icon, and exact four-scope set.
- [x] Verify and publish the OAuth branding.
- [x] Publish the app from Testing to Production.
- [x] Enter the sensitive-scope justification in Google's review form.
- [ ] Record the demonstration and provide its unlisted YouTube URL.
- [ ] Complete the brand and sensitive-scope verification form using the scope
      explanations above, attach the unlisted demo-video URL, and submit.
- [ ] Record the submission date, Google's case/reference number, and every
      follow-up request in this document.

## Approval and release record

| Milestone | Date | Evidence or reference |
| --- | --- | --- |
| Domain verified | 2026-09-21 | Google Search Console, Domain name provider method |
| Branding verified and published | 2026-09-21 | Google Auth Platform automated branding verification |
| App moved to production | 2026-09-21 | Google Auth Platform Audience |
| Verification submitted | — | — |
| Google follow-up answered | — | — |
| Verification approved | — | — |
| Production lifecycle retest passed | — | — |
| `v1.0.0` released | — | — |

After approval, run the real-account create, update, RSVP, delete, resync, and
revocation checks from the public-release checklist. Only then bump the release
metadata to `1.0.0` and create the `v1.0.0` tag.
