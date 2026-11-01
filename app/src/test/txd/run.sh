#!/bin/sh
# Builds and runs the .txd decoder test on this machine (needs g++ and python3).
set -eu
cd "$(dirname "$0")"
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
python3 make_txd.py "$tmp/test.txd" "$tmp/expected.txt"
g++ -std=c++17 -O1 -Wall -fsanitize=address,undefined -o "$tmp/txdtest" txdtest.cpp
"$tmp/txdtest" "$tmp/test.txd" "$tmp/expected.txt"
