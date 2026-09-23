# Preparing an Arch/AUR release

`packaging/arch/PKGBUILD.in` is a template, not an AUR submission. Release
preparation generates `PKGBUILD` and `.SRCINFO` with the checksum of the exact
source archive that users will download. There is no dependency on a private
file beside an AUR checkout or on a missing Git tag during local builds.

## Prepare locally

Install `base-devel`, `python`, `devtools`, and `namcap`. Commit the intended
source changes first: the generator archives a committed revision, not the
working tree. It never creates tags, pushes commits, or publishes artifacts.

Provide a Google **Desktop app** client in a file outside Git, containing only:

```ini
OMARCHY_CALENDAR_GOOGLE_CLIENT_ID=your-client-id.apps.googleusercontent.com
OMARCHY_CALENDAR_GOOGLE_CLIENT_SECRET=your-client-secret
```

The two values will be distributed in the release source archive and installed
package. Google treats installed applications as public clients that cannot
keep secrets; this is distinct from user tokens, which stay in Secret Service.
Do not use a web client, service-account key, access token, or refresh token.
The generator rejects additional fields rather than copying them into an artifact.
See [Google's installed-app documentation](https://developers.google.com/identity/protocols/oauth2/native-app).

From the repository root, as a regular user:

```bash
python3 packaging/prepare-release.py --ref HEAD \
  --oauth-env /absolute/path/to/google-oauth.env --output build-aur-release
cd build-aur-release/aur
extra-x86_64-build
cd ../..
./tests/check-arch-package.sh build-aur-release/aur
```

`extra-x86_64-build` uses sudo to create and run a clean Arch chroot. The archive
already present next to `PKGBUILD` lets it build before any release is public;
makepkg still verifies the pinned checksum. For a faster local build, use
`makepkg --cleanbuild` in that directory instead. Do not install the test package
over your daily application unless you intend to replace it.

Preparation outputs:

- `omarchy-calendar-VERSION.tar.gz`: source plus publisher Desktop app settings.
- `omarchy-calendar-VERSION-aur.tar.gz`: only `aur/PKGBUILD` and `aur/.SRCINFO`.
- `SOURCE_REVISION`: the archived Git commit.
- `SHA256SUMS`: checksums of those release files.
- `aur/`: a local build directory with the same source archive and AUR recipe.

Use a new output directory for each run. Repeating a revision with the same
client settings produces identical archives. CI uses an obviously fake desktop
client to exercise packaging without exposing publisher credentials. Never
publish CI's test artifacts as an official release.

## Publish after Google approval

1. Finish live authorization, synchronization, create/edit/RSVP/delete, restart,
   disconnect, and revoked-access checks using the production desktop client.
2. Update the version in `app/version.h`, both qmake projects, `CMakeLists.txt`,
   `packaging/arch/PKGBUILD.in`, AppStream metadata, README, and changelog. Run
   metadata, install, application contract, and clean-chroot checks.
3. Commit and push the intended source, then tag that commit `vVERSION` and push
   the tag. GitHub's release workflow takes the Desktop app settings from its
   existing OAuth secrets and creates a draft release with validated artifacts.
4. Review and publish the draft. Verify `SHA256SUMS` and test a fresh extraction
   of the AUR bundle with `makepkg --verifysource`; it must fetch the archive
   through the public release URL without your local source cache.
5. Commit only the generated `PKGBUILD` and `.SRCINFO` to the AUR package repo.
   Do not upload the source archive, binaries, or `PKGBUILD.in` to AUR.

Do not replace published source archives in place. Changed source or client
configuration needs a new upstream release and freshly generated checksums.
Keep the final publication/AUR submission separate from local preparation.
