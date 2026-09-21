#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="$(mktemp -d)"
trap 'rm -rf "$build_dir"' EXIT

qmake6 "$project_dir/tests/google-database-test.pro" -o "$build_dir/Makefile"
make -C "$build_dir" -j"$(nproc)" >/dev/null
"$build_dir/google-database-test" "$project_dir/tests/fixtures"

echo "google database contract: ok"
