#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$project_dir"
version="$(sed -n 's/^#define OMARCHY_CALENDAR_VERSION "\([^"]*\)"/\1/p' app/version.h)"
[[ -n "$version" ]]

grep -q "project(omarchy-calendar VERSION $version " CMakeLists.txt
grep -q "OMARCHY_CALENDAR_VERSION=.*$version" omarchy-calendar.pro
grep -q "OMARCHY_CALENDAR_VERSION=.*$version" service/omarchy-calendar-service.pro
grep -q "^pkgver=$version$" packaging/arch/PKGBUILD
grep -q $'\tpkgver = '"$version" packaging/arch/.SRCINFO
grep -q "release version=\"$version\"" packaging/metainfo/org.omarchy.Calendar.metainfo.xml
grep -q "## \[$version\]" CHANGELOG.md

(cd packaging/arch && makepkg --printsrcinfo -p PKGBUILD | diff -u .SRCINFO -)
desktop-file-validate packaging/desktop/org.omarchy.Calendar.desktop
appstreamcli validate --no-net packaging/metainfo/org.omarchy.Calendar.metainfo.xml >/dev/null
grep -q 'privacy.html' docs/index.html
grep -q 'terms.html' docs/index.html
grep -q 'Google API Services User Data Policy' docs/privacy.html
grep -q 'jason@greatspark.com' docs/privacy.html
grep -q 'https://lastrefuge.ai/projects/omarchy-calendar' packaging/metainfo/org.omarchy.Calendar.metainfo.xml
grep -q 'GitHub Security Advisories' SECURITY.md
[[ -z "$(git ls-files packaging/google-oauth.env)" ]]
if [[ -n "${GITHUB_REF_NAME:-}" && "${GITHUB_REF_TYPE:-}" == "tag" ]]; then
  [[ "$GITHUB_REF_NAME" == "v$version" ]]
  if [[ "$version" == "1.0.0" ]]; then
    for public_url in "${OMARCHY_CALENDAR_HOMEPAGE_URL:-}" \
                      "${OMARCHY_CALENDAR_PRIVACY_URL:-}" \
                      "${OMARCHY_CALENDAR_TERMS_URL:-}"; do
      [[ "$public_url" == https://* ]]
      [[ "$public_url" != *github.io* ]]
    done
    [[ "${OMARCHY_CALENDAR_SUPPORT_EMAIL:-}" == *@* ]]
  fi
fi

echo "release metadata $version: ok"
