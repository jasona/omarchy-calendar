#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
test_dir="$(mktemp -d)"
trap 'rm -rf "$test_dir"' EXIT

cd "$test_dir"
qmake6 "$project_dir/tests/google-sync-retry-test.pro"
make -j2 >/dev/null
./google-sync-retry-test

echo "google sync retry contract: ok"
