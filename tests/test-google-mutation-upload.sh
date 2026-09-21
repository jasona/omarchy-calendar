#!/usr/bin/env bash
set -euo pipefail
project_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="$(mktemp -d)"
trap 'rm -rf "$build_dir"' EXIT
cd "$build_dir"
qmake6 "$project_dir/tests/google-mutation-upload-test.pro"
make -j2 >/dev/null
./google-mutation-upload-test
echo "google mutation upload contract: ok"
