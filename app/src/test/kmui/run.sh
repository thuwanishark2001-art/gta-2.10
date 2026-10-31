#!/usr/bin/env bash
# Builds and runs the host test for the client's KMUI parser.
set -euo pipefail
cd "$(dirname "$0")"
out=$(mktemp)
c++ -std=c++17 -Wall -Wextra -o "$out" parse_test.cpp ../../main/cpp/samp/kmui/kmui_parse.cpp
"$out"
rm -f "$out"
