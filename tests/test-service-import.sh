#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
service_binary="${1:-$project_dir/build-service/omarchy-calendar-service}"
test_dir="$(mktemp -d)"
trap 'rm -rf "$test_dir"' EXIT

cp "$project_dir/tests/fixtures/calendar-events-v1.json" "$test_dir/feed.json"

run_import() {
  OMARCHY_CALENDAR_DB_PATH="$test_dir/calendar.db" \
  OMARCHY_CALENDAR_FEED_PATH="$test_dir/feed.json" \
    "$service_binary" --import-only
}

run_import
run_import

result="$(sqlite3 "$test_dir/calendar.db" \
  "SELECT COUNT(*) || ':' || COUNT(DISTINCT provider_event_id) || ':' || (SELECT COUNT(*) FROM calendars) FROM events;")"
[[ "$result" == "3:2:1" ]]

schema_version="$(sqlite3 "$test_dir/calendar.db" "SELECT MAX(version) FROM schema_migrations;")"
[[ "$schema_version" == "6" ]]

account_tables="$(sqlite3 "$test_dir/calendar.db" \
  "SELECT COUNT(*) FROM sqlite_master WHERE type='table' AND name IN ('accounts','sync_cursors','pending_mutations');")"
[[ "$account_tables" == "3" ]]

event_identity="$(sqlite3 "$test_dir/calendar.db" \
  "SELECT group_concat(name, ',') FROM pragma_table_info('events') WHERE pk > 0 ORDER BY pk;")"
[[ "$event_identity" == "provider_event_id,date_key,calendar_id" || "$event_identity" == "calendar_id,provider_event_id,date_key" ]]

printf '{"version":2,"events":[]}' > "$test_dir/feed.json"
if run_import; then
  echo "Unsupported feed unexpectedly imported" >&2
  exit 1
fi

preserved="$(sqlite3 "$test_dir/calendar.db" "SELECT COUNT(*) FROM events;")"
[[ "$preserved" == "3" ]]

integrity="$(sqlite3 "$test_dir/calendar.db" "PRAGMA integrity_check;")"
[[ "$integrity" == "ok" ]]

echo "service import contract: ok"
