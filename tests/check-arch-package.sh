#!/usr/bin/env bash
set -euo pipefail

package_dir="$(realpath "${1:?usage: check-arch-package.sh PACKAGE_DIRECTORY}")"
shopt -s nullglob
packages=("$package_dir"/omarchy-calendar-[0-9]*-*.pkg.tar.zst)
[[ ${#packages[@]} == 1 ]] || { echo 'Expected one application package' >&2; exit 1; }
package="${packages[0]}"
work_dir="$(mktemp -d)"
trap 'rm -rf "$work_dir"' EXIT

# namcap can report errors with exit status zero; inspect its diagnostics as well.
namcap "$package_dir/PKGBUILD" "$package" | tee "$work_dir/namcap.log"
if grep -q ' E: ' "$work_dir/namcap.log"; then
  echo 'namcap reported packaging errors' >&2
  exit 1
fi
bsdtar -xf "$package" -C "$work_dir"
for path in usr/bin/omarchy-calendar usr/bin/omarchy-calendar-service \
            usr/share/licenses/omarchy-calendar/LICENSE \
            usr/share/omarchy-calendar/google-oauth.env \
            usr/share/applications/org.omarchy.Calendar.desktop \
            usr/share/metainfo/org.omarchy.Calendar.metainfo.xml \
            usr/share/dbus-1/services/org.omarchy.Calendar.service \
            usr/lib/systemd/user/omarchy-calendar.service; do
  [[ -s "$work_dir/$path" ]] || { echo "Missing package file: $path" >&2; exit 1; }
done
[[ "$(stat -c %a "$work_dir/usr/share/omarchy-calendar/google-oauth.env")" == 644 ]]
for binary in omarchy-calendar omarchy-calendar-service; do
  if readelf -d "$work_dir/usr/bin/$binary" | grep -Eq '\((RPATH|RUNPATH)\)'; then
    echo "Unexpected runtime library path in $binary" >&2
    exit 1
  fi
done
echo 'Arch package validation: ok'
