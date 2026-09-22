#!/usr/bin/env bash
set -euo pipefail
project_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="$(mktemp -d)"
trap 'rm -rf "$build_dir"' EXIT
cd "$build_dir"
qmake6 "$project_dir/tests/secret-store-failure-test.pro" >/dev/null
make -j2 >/dev/null
./secret-store-failure-test
echo "secret service failure contract: ok"
