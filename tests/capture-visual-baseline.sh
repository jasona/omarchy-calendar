#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
app_binary="${1:-$project_dir/build-qmake/omarchy-calendar}"
service_binary="${2:-$project_dir/build-service/omarchy-calendar-service}"
output_dir="${3:-$project_dir/docs/screenshots/v1}"
work_dir="$(mktemp -d)"
trap 'rm -rf "$work_dir"' EXIT

mkdir -p "$output_dir" "$work_dir/dark/.local/state/omarchy/current/theme" \
  "$work_dir/dark/.config/Omarchy" "$work_dir/light/.local/state/omarchy/current/theme" \
  "$work_dir/light/.config/Omarchy"
cp "$project_dir/tests/fixtures/calendar-events-v1.json" "$work_dir/feed.json"

cat > "$work_dir/dark/.local/state/omarchy/current/theme/colors.toml" <<'EOF'
background = "#0b121a"
dark_background = "#080e14"
lighter_background = "#111b26"
foreground = "#d5efff"
dark_foreground = "#91adbd"
accent = "#8da7ef"
red = "#e98291"
green = "#75c497"
cyan = "#76d5ef"
EOF
cat > "$work_dir/light/.local/state/omarchy/current/theme/colors.toml" <<'EOF'
background = "#f4f1ec"
dark_background = "#e8e2d9"
lighter_background = "#fffdf9"
foreground = "#20242a"
dark_foreground = "#5c6670"
accent = "#4f64b8"
red = "#a53f55"
green = "#28764b"
cyan = "#146f86"
EOF
cat > "$work_dir/dark/.config/Omarchy/Omarchy Calendar.conf" <<'EOF'
[appearance]
density=compact
[onboarding]
completed=true
EOF
cat > "$work_dir/light/.config/Omarchy/Omarchy Calendar.conf" <<'EOF'
[appearance]
density=comfortable
[onboarding]
completed=true
EOF

export TEST_APP_BINARY="$app_binary"
export TEST_SERVICE_BINARY="$service_binary"
export TEST_OUTPUT_DIR="$output_dir"
export TEST_WORK_DIR="$work_dir"

dbus-run-session -- bash -c '
  set -euo pipefail
  OMARCHY_CALENDAR_DB_PATH="$TEST_WORK_DIR/calendar.db" \
  OMARCHY_CALENDAR_FEED_PATH="$TEST_WORK_DIR/feed.json" \
    "$TEST_SERVICE_BINARY" >"$TEST_WORK_DIR/service.log" 2>&1 &
  service_pid=$!
  trap "kill $service_pid 2>/dev/null || true" EXIT
  for _ in {1..40}; do
    gdbus introspect --session --dest org.omarchy.Calendar \
      --object-path /org/omarchy/Calendar >/dev/null 2>&1 && break
    sleep 0.05
  done

  for theme_name in dark light; do
    for view_name in day week month agenda search settings onboarding; do
      env -u QT_QPA_PLATFORMTHEME HOME="$TEST_WORK_DIR/$theme_name" \
        QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software \
        "$TEST_APP_BINARY" --view "$view_name" \
        --screenshot "$TEST_OUTPUT_DIR/$theme_name-$view_name.png"
      test -s "$TEST_OUTPUT_DIR/$theme_name-$view_name.png"
    done
  done

  for scale in 1.25 1.5 2; do
    env -u QT_QPA_PLATFORMTHEME HOME="$TEST_WORK_DIR/dark" \
      QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software QT_SCALE_FACTOR="$scale" \
      "$TEST_APP_BINARY" --view week \
      --screenshot "$TEST_WORK_DIR/scaled-$scale.png"
    test -s "$TEST_WORK_DIR/scaled-$scale.png"
  done
'

echo "visual baseline: $output_dir"
