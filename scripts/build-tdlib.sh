#!/usr/bin/env bash
set -euo pipefail

root=$(cd "$(dirname "$0")/.." && pwd)
build_dir=$root/.deps/build/tdlib
install_dir=$root/.deps/tdlib

# TDLib translation units are huge; one job can take several gigabytes of RAM.
jobs=${JOBS:-6}

cmake -S "$root/third_party/tdlib" -B "$build_dir" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$install_dir"

cmake --build "$build_dir" --target install --parallel "$jobs"
