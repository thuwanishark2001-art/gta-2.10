#!/usr/bin/env bash
# Builds kmbot (a static Linux binary) from the mobile client's RakNet
# sources. Usage: build.sh <output file>
# The RakNet SA:MP files include "../../../main.h", so the sources are laid
# out as <work>/v/raknet with this directory's main.h at <work>/main.h.
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
repo="$(cd "$here/../../.." && pwd)"
out="$1"
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
mkdir -p "$work/v"
cp -r "$repo/app/src/main/cpp/samp/vendor/raknet" "$work/v/raknet"
cp "$here/main.h" "$work/main.h"
cd "$work/v/raknet"
objs=()
for f in *.cpp SAMP/*.cpp "$here/kmbot.cpp"; do
    o="$work/$(basename "${f%.cpp}").o"
    g++ -std=c++17 -O1 -w -c "$f" -I"$work" -I"$work/v/raknet" -o "$o"
    objs+=("$o")
done
g++ -static -pthread -o "$out" "${objs[@]}"
echo "built $out"
