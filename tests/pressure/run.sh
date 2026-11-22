#!/bin/sh
# Host test of the analog pressure logic (pressure.cpp). Needs g++ on PATH or the portable w64devkit in %LOCALAPPDATA%/Programs.
set -e
cd "$(dirname "$0")/../.."
PATH="$LOCALAPPDATA/Programs/w64devkit/bin:$PATH"
OUT=build/tests
mkdir -p "$OUT"
g++ -std=c++17 -Wall -Wextra -o "$OUT/test_pressure.exe" tests/pressure/test_pressure.cpp OpenPuck/pressure.cpp
"$OUT/test_pressure.exe"
