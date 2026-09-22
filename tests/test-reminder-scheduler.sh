#!/usr/bin/env bash
set -euo pipefail
project_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="$(mktemp -d)"
trap 'rm -rf "$build_dir"' EXIT
cd "$build_dir"
qmake6 "$project_dir/tests/reminder-scheduler-test.pro" >/dev/null
make -j2 >/dev/null
./reminder-scheduler-test
echo "reminder scheduler: ok"
