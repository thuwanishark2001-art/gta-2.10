#!/usr/bin/env bash
# Downloads the pinned third-party server dependencies into server/.deps.
# Nothing here is committed: every file is fetched from its upstream release
# and checked against the SHA-256 below.
set -euo pipefail

cd "$(dirname "$0")/.."
DEPS=.deps
mkdir -p "$DEPS/dl"

fetch() { # name url sha256
    local out="$DEPS/dl/$1"
    if [ ! -f "$out" ] || ! echo "$3  $out" | sha256sum -c --quiet - 2>/dev/null; then
        echo "fetch $1"
        curl -fsSL --retry 4 -o "$out" "$2"
    fi
    echo "$3  $out" | sha256sum -c --quiet -
}

fetch open.mp.tar.gz \
    https://github.com/openmultiplayer/open.mp/releases/download/v1.4.0.2779/open.mp-linux-x86.tar.gz \
    37ab402df078f6a276f75e1c3009c955188208821ffa647cf2d018533bb19bee
fetch pawnc.tar.gz \
    https://github.com/openmultiplayer/compiler/releases/download/v3.10.11/pawnc-3.10.11-linux.tar.gz \
    13977433acb1dfb23e3ec6354f622115d5073e97aced9395058a9db59d2564c2
fetch mysql.tar.gz \
    https://github.com/pBlueG/SA-MP-MySQL/releases/download/R41-4/mysql-R41-4-Debian-static.tar.gz \
    2e24aabfb7d674a454961e341a7df219f65069d273c44d4323e5199a59f135be
fetch bcrypt.zip \
    https://github.com/Sreyas-Sreelal/samp-bcrypt/releases/download/0.4.1/samp-bcrypt-linux-x86.zip \
    ebf76b12d799046a605e3063bb7b64b08e1fad9bdb3c299b357ec215a0793b5d
fetch sscanf.tar.gz \
    https://github.com/Y-Less/sscanf/releases/download/v2.13.8/sscanf-2.13.8-linux.tar.gz \
    7e407864dfed5203bcd2a1134fdab60fce233b7b5c89fca8ccd06d8b7b0f5889

OMP_STDLIB_COMMIT=17c1b0191195cb0b59a820135fbca622c84129dd

rm -rf "$DEPS/omp" "$DEPS/pawnc" "$DEPS/include" "$DEPS/tmp"
mkdir -p "$DEPS/omp/plugins" "$DEPS/omp/components" "$DEPS/include" "$DEPS/tmp"

tar xzf "$DEPS/dl/open.mp.tar.gz" -C "$DEPS/tmp"
cp -r "$DEPS/tmp/Server/omp-server" "$DEPS/tmp/Server/components" "$DEPS/omp/"
rm -f "$DEPS/omp/components/Fixes.so" # excluded in the stock config too

mkdir -p "$DEPS/tmp/pawnc" && tar xzf "$DEPS/dl/pawnc.tar.gz" -C "$DEPS/tmp/pawnc"
mv "$DEPS/tmp/pawnc/pawnc-3.10.11-linux" "$DEPS/pawnc"

tar xzf "$DEPS/dl/mysql.tar.gz" -C "$DEPS/tmp"
cp "$DEPS/tmp/mysql-R41-4-Debian/plugins/mysql.so" "$DEPS/omp/plugins/"
cp "$DEPS/tmp/mysql-R41-4-Debian/log-core.so" "$DEPS/omp/"
cp "$DEPS/tmp/mysql-R41-4-Debian/pawno/include/a_mysql.inc" "$DEPS/include/"

mkdir -p "$DEPS/tmp/bcrypt" && (cd "$DEPS/tmp/bcrypt" && unzip -q ../../dl/bcrypt.zip)
cp "$DEPS/tmp/bcrypt/plugins/samp_bcrypt.so" "$DEPS/omp/plugins/"
cp "$DEPS/tmp/bcrypt/includes/samp_bcrypt.inc" "$DEPS/include/"

mkdir -p "$DEPS/tmp/sscanf" && tar xzf "$DEPS/dl/sscanf.tar.gz" -C "$DEPS/tmp/sscanf"
cp "$DEPS/tmp/sscanf/components/sscanf.so" "$DEPS/omp/components/"
cp "$DEPS/tmp/sscanf/qawno/include/sscanf2.inc" "$DEPS/include/"

git clone -q https://github.com/openmultiplayer/omp-stdlib "$DEPS/tmp/omp-stdlib"
git -C "$DEPS/tmp/omp-stdlib" checkout -q "$OMP_STDLIB_COMMIT"
cp "$DEPS/tmp/omp-stdlib/"*.inc "$DEPS/include/"

rm -rf "$DEPS/tmp"
echo "deps ready in $DEPS"
