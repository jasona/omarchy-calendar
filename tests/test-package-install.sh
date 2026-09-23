#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
work_dir="$(mktemp -d)"
trap 'rm -rf "$work_dir"' EXIT
version="$(sed -n 's/^#define OMARCHY_CALENDAR_VERSION "\([^"]*\)"/\1/p' "$project_dir/app/version.h")"
[[ -n "$version" ]]
homepage="${OMARCHY_CALENDAR_HOMEPAGE_URL:-https://calendar.example.test/product}"
privacy="${OMARCHY_CALENDAR_PRIVACY_URL:-https://calendar.example.test/privacy}"
terms="${OMARCHY_CALENDAR_TERMS_URL:-https://calendar.example.test/terms}"
support="${OMARCHY_CALENDAR_SUPPORT_EMAIL:-support@example.test}"

cat > "$work_dir/google-oauth.env" <<'EOF'
OMARCHY_CALENDAR_GOOGLE_CLIENT_ID=package-test.apps.googleusercontent.com
OMARCHY_CALENDAR_GOOGLE_CLIENT_SECRET=package-test-secret
EOF

cmake -S "$project_dir" -B "$work_dir/build" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_LIBDIR=lib \
  -DOMARCHY_CALENDAR_OAUTH_ENV_FILE="$work_dir/google-oauth.env" \
  -DOMARCHY_CALENDAR_HOMEPAGE_URL="$homepage" \
  -DOMARCHY_CALENDAR_PRIVACY_URL="$privacy" \
  -DOMARCHY_CALENDAR_TERMS_URL="$terms" \
  -DOMARCHY_CALENDAR_SUPPORT_EMAIL="$support"
cmake --build "$work_dir/build"
DESTDIR="$work_dir/root" cmake --install "$work_dir/build"

required=(
  usr/bin/omarchy-calendar
  usr/bin/omarchy-calendar-service
  usr/share/applications/org.omarchy.Calendar.desktop
  usr/share/icons/hicolor/scalable/apps/org.omarchy.Calendar.svg
  usr/share/metainfo/org.omarchy.Calendar.metainfo.xml
  usr/share/dbus-1/services/org.omarchy.Calendar.service
  usr/lib/systemd/user/omarchy-calendar.service
  usr/share/omarchy-calendar/google-oauth.env
  usr/share/doc/omarchy-calendar/README.md
  usr/share/doc/omarchy-calendar/CHANGELOG.md
  usr/share/licenses/omarchy-calendar/LICENSE
  usr/share/doc/omarchy-calendar/SECURITY.md
)
for path in "${required[@]}"; do
  [[ -f "$work_dir/root/$path" ]] || { echo "missing installed file: $path" >&2; exit 1; }
done
grep -q 'package-test.apps.googleusercontent.com' \
  "$work_dir/root/usr/share/omarchy-calendar/google-oauth.env"

[[ "$("$work_dir/root/usr/bin/omarchy-calendar-service" --version)" == "omarchy-calendar-service $version" ]]
[[ "$(env -u QT_QPA_PLATFORMTHEME QT_QPA_PLATFORM=offscreen \
  "$work_dir/root/usr/bin/omarchy-calendar" --version)" == "Omarchy Calendar $version" ]]
release_info="$(env -u QT_QPA_PLATFORMTHEME QT_QPA_PLATFORM=offscreen \
  "$work_dir/root/usr/bin/omarchy-calendar" --release-info)"
jq -e --arg version "$version" --arg homepage "$homepage" --arg privacy "$privacy" \
  --arg terms "$terms" --arg support "$support" \
  '.version == $version and .homepageUrl == $homepage and .privacyUrl == $privacy
   and .termsUrl == $terms and .supportEmail == $support' <<<"$release_info" >/dev/null
cmp "$project_dir/LICENSE" "$work_dir/root/usr/share/licenses/omarchy-calendar/LICENSE"

if command -v desktop-file-validate >/dev/null; then
  desktop-file-validate "$work_dir/root/usr/share/applications/org.omarchy.Calendar.desktop"
fi
if command -v appstreamcli >/dev/null; then
  appstreamcli validate --no-net "$work_dir/root/usr/share/metainfo/org.omarchy.Calendar.metainfo.xml"
fi

echo "package install contract: ok"
