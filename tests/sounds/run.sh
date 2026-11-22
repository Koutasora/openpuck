#!/bin/sh
# Host test of the melody logic (sounds). Needs g++ on PATH or the portable w64devkit in %LOCALAPPDATA%/Programs.
set -e
cd "$(dirname "$0")/../.."
PATH="$LOCALAPPDATA/Programs/w64devkit/bin:$PATH"
OUT=build/tests
mkdir -p "$OUT"
g++ -std=c++17 -Wall -Wextra -o "$OUT/test_sounds.exe" tests/sounds/test_sounds.cpp OpenPuck/sounds.cpp
"$OUT/test_sounds.exe"
