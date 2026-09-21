#!/usr/bin/env bash
set -euo pipefail
project_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="$(mktemp -d)"
trap 'rm -rf "$build_dir"' EXIT
cd "$build_dir"
qmake6 "$project_dir/tests/all-day-multiday-test.pro"
make -j2 >/dev/null
./all-day-multiday-test
echo "all-day and multi-day boundary contract: ok"
