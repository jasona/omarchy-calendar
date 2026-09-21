#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
service_binary="${1:-$project_dir/build-service/omarchy-calendar-service}"
test_dir="$(mktemp -d)"
trap 'rm -rf "$test_dir"' EXIT
cp "$project_dir/tests/fixtures/calendar-events-v1.json" "$test_dir/feed.json"

export TEST_SERVICE_BINARY="$service_binary"
export TEST_DATABASE_PATH="$test_dir/calendar.db"
export TEST_FEED_PATH="$test_dir/feed.json"

dbus-run-session -- bash -c '
  set -euo pipefail
  OMARCHY_CALENDAR_DB_PATH="$TEST_DATABASE_PATH" \
  OMARCHY_CALENDAR_FEED_PATH="$TEST_FEED_PATH" \
  OMARCHY_CALENDAR_GOOGLE_CLIENT_ID="test-client.apps.googleusercontent.com" \
  OMARCHY_CALENDAR_GOOGLE_CLIENT_SECRET="test-secret" \
    "$TEST_SERVICE_BINARY" >"$TEST_DATABASE_PATH.log" 2>&1 &
  service_pid=$!
  trap "kill $service_pid 2>/dev/null || true" EXIT

  ready=false
  for _ in {1..40}; do
    if gdbus introspect --session --dest org.omarchy.Calendar --object-path /org/omarchy/Calendar >/dev/null 2>&1; then
      ready=true
      break
    fi
    sleep 0.05
  done
  [[ "$ready" == true ]]

  before="$(gdbus call --session --dest org.omarchy.Calendar --object-path /org/omarchy/Calendar --method org.omarchy.Calendar1.GetProviderStatus)"
  begin="$(gdbus call --session --dest org.omarchy.Calendar --object-path /org/omarchy/Calendar --method org.omarchy.Calendar1.BeginGoogleAuthorization)"
  after="$(gdbus call --session --dest org.omarchy.Calendar --object-path /org/omarchy/Calendar --method org.omarchy.Calendar1.GetProviderStatus)"

  [[ "$before" == *"\"configured\":true"* ]]
  [[ "$before" == *"\"state\":\"disconnected\""* ]]
  [[ "$begin" == "(true,)" ]]
  [[ "$after" == *"\"state\":\"authorizing\""* ]]
  [[ "$after" == *"accounts.google.com/o/oauth2/v2/auth"* ]]
  [[ "$after" == *"127.0.0.1"* ]]
  [[ "$after" == *"calendar.calendarlist.readonly"* ]]
  [[ "$after" == *"calendar.events"* ]]
  [[ "$before" == *"\"writeAccessAvailable\":false"* ]]
'

echo "google auth contract: ok"
