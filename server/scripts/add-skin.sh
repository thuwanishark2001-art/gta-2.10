#!/usr/bin/env bash
# Adds a custom skin (or vehicle) to the server's content (docs/CONTENT.md).
#   add-skin.sh <id> <model.dff> [<textures.txd> | <folder of .png textures>]
# Copies the model to content/models/<id>.dff. With a .txd, copies it to
# content/models/<id>.txd. With a folder of PNGs, reads which textures the
# model uses and copies each matching picture to content/models/<id>/<name>.png
# (it matches names loosely: "Body4 (1).png" is texture "body4"). Then
# restarts the content service so players download it.
#   add-skin.sh --remove <id>    takes a custom model away again
set -euo pipefail
# Paths the user typed are relative to where they ran it.
abs() { case "$1" in /*) printf '%s' "$1" ;; "") ;; *) printf '%s/%s' "$PWD" "$1" ;; esac; }
args=()
for a in "$@"; do args+=("$(abs "$a")"); done
cd "$(dirname "$0")/.."

if [ "${1:-}" = "--remove" ]; then
    id="${2:-}"
    [[ "$id" =~ ^[0-9]{1,5}$ ]] || { echo "usage: add-skin.sh --remove <id>" >&2; exit 2; }
    rm -rf "content/models/$id.dff" "content/models/$id.txd" "content/models/$id"
    docker compose restart content >/dev/null
    echo "removed custom model $id"
    exit 0
fi

if [ $# -lt 2 ]; then
    echo "usage: add-skin.sh <id> <model.dff> [<textures.txd> | <folder of .png textures>]" >&2
    exit 2
fi
id="$1"; dff="${args[1]}"; tex="${args[2]:-}"
[[ "$id" =~ ^[0-9]{1,5}$ ]] && [ "$id" -ge 1 ] || { echo "The id must be a number, like 230." >&2; exit 2; }
[ -f "$dff" ] || { echo "No such file: $dff" >&2; exit 2; }

mkdir -p content/models
python3 - "$id" "$dff" "$tex" <<'PY'
import os, re, shutil, struct, sys

model_id, dff, tex = sys.argv[1], sys.argv[2], sys.argv[3]
data = open(dff, 'rb').read()
if len(data) < 12 or struct.unpack_from('<I', data, 0)[0] != 0x10:
    sys.exit(f"{dff} isn't a .dff model (it doesn't start with a RenderWare clump).")

# Texture names the model uses: Clump > GeometryList > Geometry > MaterialList
# > Material > Texture > String.
CONTAINERS = {0x10, 0x1A, 0x0F, 0x08, 0x07, 0x03}
names = []
def walk(start, end):
    pos = start
    while pos + 12 <= end:
        ctype, size, _ = struct.unpack_from('<III', data, pos)
        body, nxt = pos + 12, pos + 12 + size
        if nxt > end:
            return
        if ctype == 0x06:  # Texture: Struct, then the name String
            p = body
            while p + 12 <= nxt:
                t, s, _ = struct.unpack_from('<III', data, p)
                if t == 0x02:
                    name = data[p + 12:p + 12 + s].split(b'\0')[0].decode('latin-1')
                    if name and name.lower() not in [n.lower() for n in names]:
                        names.append(name)
                    break
                p += 12 + s
        elif ctype in CONTAINERS:
            walk(body, nxt)
        pos = nxt
walk(0, len(data))

out = 'content/models'
for old in (f'{out}/{model_id}.txd',):
    if os.path.exists(old): os.remove(old)
if os.path.isdir(f'{out}/{model_id}'): shutil.rmtree(f'{out}/{model_id}')
shutil.copyfile(dff, f'{out}/{model_id}.dff')
print(f"model {model_id}: {os.path.basename(dff)} -> content/models/{model_id}.dff")
print(f"  it uses {len(names)} textures: {', '.join(names) or '(none)'}")

if not tex:
    print("  no textures given: it will use the game's textures by those names")
    sys.exit(0)
if os.path.isfile(tex):
    if not tex.lower().endswith('.txd'):
        sys.exit(f"{tex} isn't a .txd; give a .txd file or a folder of .png pictures.")
    shutil.copyfile(tex, f'{out}/{model_id}.txd')
    print(f"  textures: {os.path.basename(tex)} -> content/models/{model_id}.txd")
    sys.exit(0)
if not os.path.isdir(tex):
    sys.exit(f"No such file or folder: {tex}")

def key(s):
    s = os.path.splitext(s)[0] if s.lower().endswith('.png') else s
    s = re.sub(r'\s*\(\d+\)$', '', s)          # "name (1)" from repeat downloads
    return re.sub(r'[^a-z0-9]', '', s.lower())
pngs = {}
for root, _, files in os.walk(tex):
    for f in files:
        if f.lower().endswith('.png'):
            pngs.setdefault(key(f), os.path.join(root, f))
os.makedirs(f'{out}/{model_id}', exist_ok=True)
missing = []
for name in names:
    src = pngs.get(key(name))
    if not src:
        missing.append(name); continue
    if not re.fullmatch(r'[A-Za-z0-9_.-]+', name):
        print(f"  ! texture \"{name}\" has characters the content service can't serve; skipped")
        missing.append(name); continue
    shutil.copyfile(src, f'{out}/{model_id}/{name}.png')
    print(f"  texture {name} <- {os.path.relpath(src, tex)}")
if missing:
    print(f"  not found in {tex}: {', '.join(missing)}")
    print("  (the game's own texture is used if it has one by that name)")
PY
docker compose restart content >/dev/null
echo "content service restarted; players get it the next time they open the app"
