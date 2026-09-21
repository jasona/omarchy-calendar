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

  status="$(gdbus call --session --dest org.omarchy.Calendar --object-path /org/omarchy/Calendar --method org.omarchy.Calendar1.GetStatus)"
  range="$(gdbus call --session --dest org.omarchy.Calendar --object-path /org/omarchy/Calendar --method org.omarchy.Calendar1.GetEvents 2026-09-20 2026-09-21)"
  search="$(gdbus call --session --dest org.omarchy.Calendar --object-path /org/omarchy/Calendar --method org.omarchy.Calendar1.SearchEvents Design 10)"
  accounts="$(gdbus call --session --dest org.omarchy.Calendar --object-path /org/omarchy/Calendar --method org.omarchy.Calendar1.GetAccounts)"
  provider="$(gdbus call --session --dest org.omarchy.Calendar --object-path /org/omarchy/Calendar --method org.omarchy.Calendar1.GetProviderStatus)"
  begin="$(gdbus call --session --dest org.omarchy.Calendar --object-path /org/omarchy/Calendar --method org.omarchy.Calendar1.BeginGoogleAuthorization)"
  sync="$(gdbus call --session --dest org.omarchy.Calendar --object-path /org/omarchy/Calendar --method org.omarchy.Calendar1.SyncNow)"
  disconnect="$(gdbus call --session --dest org.omarchy.Calendar --object-path /org/omarchy/Calendar --method org.omarchy.Calendar1.DisconnectGoogle)"
  select_missing="$(gdbus call --session --dest org.omarchy.Calendar --object-path /org/omarchy/Calendar --method org.omarchy.Calendar1.SetCalendarSelected missing true)"

  [[ "$status" == *"\"eventCount\":3"* ]]
  [[ "$status" == *"\"schemaVersion\":4"* ]]
  [[ "$range" == *"Design review"* ]]
  [[ "$range" == *"Conference"* ]]
  [[ "$search" == *"Design review"* ]]
  [[ "$search" != *"Conference"* ]]
  [[ "$accounts" == *"[]"* ]]
  [[ "$provider" == *"\"configured\":false"* ]]
  [[ "$provider" == *"\"tokenStorage\":\"secret-service\""* ]]
  [[ "$provider" == *"\"sync\":"* ]]
  [[ "$provider" == *"\"online\":"* ]]
  [[ "$provider" == *"\"state\":\"idle\""* ]]
  [[ "$begin" == "(false,)" ]]
  [[ "$sync" == "(false,)" ]]
  [[ "$disconnect" == "(false,)" ]]
  [[ "$select_missing" == "(false,)" ]]
'

echo "dbus contract: ok"
