#!/bin/sh
# Builds and runs the .txd and .png decoder tests on this machine (needs g++, zlib, python3 and Pillow).
set -eu
cd "$(dirname "$0")"
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
python3 make_txd.py "$tmp/test.txd" "$tmp/expected.txt"
g++ -std=c++17 -O1 -Wall -fsanitize=address,undefined -o "$tmp/txdtest" txdtest.cpp
"$tmp/txdtest" "$tmp/test.txd" "$tmp/expected.txt"
# PNG textures (needs Pillow for the reference images).
mkdir "$tmp/png"
python3 make_png.py "$tmp/png"
g++ -std=c++17 -O1 -Wall -fsanitize=address,undefined -o "$tmp/pngtest" pngtest.cpp -lz
"$tmp/pngtest" "$tmp/png"
