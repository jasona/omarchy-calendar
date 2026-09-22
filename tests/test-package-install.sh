#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
work_dir="$(mktemp -d)"
trap 'rm -rf "$work_dir"' EXIT

cat > "$work_dir/google-oauth.env" <<'EOF'
OMARCHY_CALENDAR_GOOGLE_CLIENT_ID=package-test.apps.googleusercontent.com
OMARCHY_CALENDAR_GOOGLE_CLIENT_SECRET=package-test-secret
EOF

cmake -S "$project_dir" -B "$work_dir/build" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_LIBDIR=lib \
  -DOMARCHY_CALENDAR_OAUTH_ENV_FILE="$work_dir/google-oauth.env" \
  -DOMARCHY_CALENDAR_HOMEPAGE_URL="${OMARCHY_CALENDAR_HOMEPAGE_URL:-https://calendar.example.test/product}" \
  -DOMARCHY_CALENDAR_PRIVACY_URL="${OMARCHY_CALENDAR_PRIVACY_URL:-https://calendar.example.test/privacy}" \
  -DOMARCHY_CALENDAR_TERMS_URL="${OMARCHY_CALENDAR_TERMS_URL:-https://calendar.example.test/terms}" \
  -DOMARCHY_CALENDAR_SUPPORT_EMAIL="${OMARCHY_CALENDAR_SUPPORT_EMAIL:-support@example.test}"
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
  usr/share/doc/omarchy-calendar/LICENSE
  usr/share/doc/omarchy-calendar/SECURITY.md
)
for path in "${required[@]}"; do
  [[ -f "$work_dir/root/$path" ]] || { echo "missing installed file: $path" >&2; exit 1; }
done
grep -q 'package-test.apps.googleusercontent.com' \
  "$work_dir/root/usr/share/omarchy-calendar/google-oauth.env"

[[ "$("$work_dir/root/usr/bin/omarchy-calendar-service" --version)" == *"0.9.0"* ]]
env -u QT_QPA_PLATFORMTHEME QT_QPA_PLATFORM=offscreen \
  "$work_dir/root/usr/bin/omarchy-calendar" --version | grep -q '0.9.0'
release_info="$(env -u QT_QPA_PLATFORMTHEME QT_QPA_PLATFORM=offscreen \
  "$work_dir/root/usr/bin/omarchy-calendar" --release-info)"
grep -q 'https://calendar.example.test/product' <<<"$release_info"
grep -q 'https://calendar.example.test/privacy' <<<"$release_info"
grep -q 'https://calendar.example.test/terms' <<<"$release_info"
grep -q 'support@example.test' <<<"$release_info"

if command -v desktop-file-validate >/dev/null; then
  desktop-file-validate "$work_dir/root/usr/share/applications/org.omarchy.Calendar.desktop"
fi
if command -v appstreamcli >/dev/null; then
  appstreamcli validate --no-net "$work_dir/root/usr/share/metainfo/org.omarchy.Calendar.metainfo.xml"
fi

echo "package install contract: ok"
