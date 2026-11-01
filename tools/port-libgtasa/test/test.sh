#!/usr/bin/env bash
# Tests port.py on two small libraries built with the NDK: OLD, and NEW where
# every address moved and one function changed. Checks that each kind of
# address is moved, kept, or left for a human, as it should be.
set -euo pipefail
here=$(cd "$(dirname "$0")" && pwd)
ndk=${ANDROID_NDK_HOME:-$(ls -d /opt/android-sdk/ndk/* "${ANDROID_HOME:-/nonexistent}"/ndk/* 2>/dev/null | sort -V | tail -1 || true)}
bin=$ndk/toolchains/llvm/prebuilt/linux-x86_64/bin
export READELF=$bin/llvm-readelf
# Name the failing line: a silent non-zero exit is otherwise all CI shows.
trap 'echo "test.sh: line $LINENO failed (exit $?)" >&2' ERR
work=$(mktemp -d); trap 'rm -rf "$work"' EXIT

for v in old new; do
    def=; [ $v = new ] && def=-DNEW
    "$bin/clang" --target=aarch64-linux-android24 -O1 -fPIC -shared $def -Wl,--unresolved-symbols=ignore-all \
        -o "$work/${v}64.so" "$here/engine.c"
    "$bin/clang" --target=armv7a-linux-androideabi24 -mthumb -O1 -fPIC -shared $def -Wl,--unresolved-symbols=ignore-all \
        -o "$work/${v}32.so" "$here/engine.c"
done

# Addresses in OLD, as the client would write them.
sym() { "$READELF" -sW "$1" | awk -v n="$2" '$8 == n && !v { v = "0x" $2 } END { print v }'; }
got() { "$READELF" -rW "$1" | awk -v n="$2" '$3 ~ /GLOB_DAT/ && $5 == n && !v { v = "0x" $1 } END { print v }'; }
hex() { printf '0x%X' "$1"; }
plus() { printf '0x%X' $(( $1 + $2 )); }
str() { python3 -c "
import sys; d=open(sys.argv[1],'rb').read(); print(hex(d.index(b'engine/%s/%d.dat\0')))" "$1"; }

same32=$(sym "$work/old32.so" same); same64=$(sym "$work/old64.so" same)
chg32=$(sym "$work/old32.so" changed); chg64=$(sym "$work/old64.so" changed)
tab32=$(sym "$work/old32.so" table); tab64=$(sym "$work/old64.so" table)
mkdir -p "$work/src"
cat > "$work/src/sites.cpp" <<CPP
void a() { call(g_libGTASA + (VER_x32 ? $(hex $(( same32 - 1 ))) + 1 : $(hex $same64))); }
void b() { patch(g_libGTASA + (VER_x32 ? $(plus $(( same32 - 1 )) 4) + 1 : $(plus $same64 8))); }
void c() { patch(g_libGTASA + (VER_x32 ? $(plus $(( chg32 - 1 )) 2) + 1 : $(plus $chg64 4))); }
void d() { write(g_libGTASA + (VER_x32 ? $(plus $tab32 12) : $(plus $tab64 12))); }
void e() { write(g_libGTASA + (VER_x32 ? $(got "$work/old32.so" counter) : $(got "$work/old64.so" counter))); }
void f() { patch(g_libGTASA + (VER_x32 ? $(str "$work/old32.so") : $(str "$work/old64.so"))); }
static_assert(sizeof(T) == (VER_x32 ? 0x20 : 0x28));
CPP
cp "$work/src/sites.cpp" "$work/before.cpp"

export PORT_SRC=$work/src
port="python3 -I $here/../port.py"
$port check "$work/old64.so" "$work/old32.so"
out=$($port port "$work/old64.so" "$work/old32.so" "$work/new64.so" "$work/new32.so" --write || true)
echo "$out"

fail=0
expect() { if ! grep -qiF -- "$2" <<<"$1"; then echo "FAIL: expected: $2"; fail=1; fi; }
reject() { if grep -qiF -- "$2" <<<"$1"; then echo "FAIL: did not expect: $2"; fail=1; fi; }
after=$(cat "$work/src/sites.cpp")

nsame32=$(sym "$work/new32.so" same); nsame64=$(sym "$work/new64.so" same)
ntab32=$(sym "$work/new32.so" table); ntab64=$(sym "$work/new64.so" table)
expect "$after" "$(hex $(( nsame32 - 1 ))) + 1 : $(hex $nsame64)"                 # function start
expect "$after" "$(plus $(( nsame32 - 1 )) 4) + 1 : $(plus $nsame64 8)"            # inside an unchanged function
expect "$after" "$(plus $(( chg32 - 1 )) 2) + 1 : $(plus $chg64 4)"               # changed body: untouched
expect "$out"   "function body changed"
expect "$after" "$(plus $ntab32 12) : $(plus $ntab64 12)"                          # inside a data object
expect "$after" "$(got "$work/new32.so" counter) : $(got "$work/new64.so" counter)" # GOT slot
expect "$after" "$(str "$work/new32.so") : $(str "$work/new64.so")"                 # string
expect "$after" "0x20 : 0x28"                                                      # a size, not an address
reject "$out"   "unnamed"
[ "$after" != "$(cat "$work/before.cpp")" ] || { echo "FAIL: nothing was rewritten"; fail=1; }

# Porting a build to itself changes nothing.
cp "$work/before.cpp" "$work/src/sites.cpp"
$port port "$work/old64.so" "$work/old32.so" "$work/old64.so" "$work/old32.so" --write >/dev/null || true
cmp -s "$work/before.cpp" "$work/src/sites.cpp" || { echo "FAIL: identity port changed the source"; fail=1; }

[ $fail = 0 ] && echo "port.py: all checks passed"
exit $fail
