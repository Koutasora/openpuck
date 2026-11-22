#!/bin/sh
# Host test of the pure binds logic. Needs g++ on PATH or the portable w64devkit in %LOCALAPPDATA%/Programs.
set -e
cd "$(dirname "$0")/../.."
PATH="$LOCALAPPDATA/Programs/w64devkit/bin:$PATH"
OUT=build/tests
mkdir -p "$OUT"
g++ -std=c++17 -Wall -Wextra -o "$OUT/test_binds.exe" tests/binds/test_binds.cpp OpenPuck/binds.cpp
"$OUT/test_binds.exe"
