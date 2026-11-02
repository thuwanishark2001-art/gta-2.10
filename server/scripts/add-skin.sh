#!/usr/bin/env bash
# Adds a custom skin (or vehicle) to the server's content (docs/CONTENT.md).
#   add-skin.sh <id> <model.dff> [<textures.txd> | <folder of .png textures>]
# Copies the model to content/models/<id>.dff. With a .txd, copies it to
# content/models/<id>.txd. With a folder of PNGs, reads which textures the
# model uses and copies each matching picture to content/models/<id>/<name>.png
# (it matches names loosely: "Body4 (1).png" is texture "body4"), saved
# under that loose name, which the app matches the same way. Then
# restarts the content service so players download it.
#
# Looks built from the player's own game (nothing of Rockstar's is uploaded):
#   add-skin.sh --base <id> <game id>       id <id> uses the game's model <game id>
#   add-skin.sh --tint <id> <RRGGBB> [texture]   colour the model (all textures,
#                                           or one); --tint <id> off removes tints
#   add-skin.sh --textures <id> <folder>    your own .png artwork on the game's
#                                           model (named after its textures)
#   add-skin.sh --inspect <id>              log the texture names model <id> uses
#                                           (the phone's log, "uses textures:")
#   add-skin.sh --list                      what's custom now
#   add-skin.sh --remove <id>               takes all of it away again
set -euo pipefail
# Paths the user typed are relative to where they ran it.
abs() { case "$1" in /*) printf '%s' "$1" ;; "") ;; *) printf '%s/%s' "$PWD" "$1" ;; esac; }
args=()
for a in "$@"; do args+=("$(abs "$a")"); done
cd "$(dirname "$0")/.."

restart() { docker compose restart content >/dev/null; echo "content service restarted; players get it the next time they open the app"; }
need_id() { [[ "${1:-}" =~ ^[0-9]{1,5}$ ]] && [ "$1" -ge 1 ] || { echo "The id must be a number, like 230." >&2; exit 2; }; }
cfg_set() {  # cfg_set <id> <key regex to drop> [new line]
    local f="content/models/$1.cfg" tmp
    mkdir -p content/models
    tmp=$(mktemp)
    [ -f "$f" ] && grep -viE "^[[:space:]]*($2)([[:space:]]|$)" "$f" > "$tmp" || true
    [ -n "${3:-}" ] && printf '%s\n' "$3" >> "$tmp"
    if [ -s "$tmp" ]; then mv "$tmp" "$f"; else rm -f "$tmp" "$f"; fi
}

case "${1:-}" in
--remove)
    need_id "${2:-}"
    rm -rf "content/models/$2.dff" "content/models/$2.txd" "content/models/$2.cfg" "content/models/$2"
    restart; echo "removed custom model $2"; exit 0 ;;
--base)
    need_id "${2:-}"; need_id "${3:-}"
    [ "$2" != "$3" ] || { echo "A model can't be its own base." >&2; exit 2; }
    cfg_set "$2" "base" "base $3"
    echo "model $2 now uses the game's model $3 (from each player's own game)"
    restart; exit 0 ;;
--tint)
    need_id "${2:-}"
    colour="${3:-}"; texture="${4:-*}"
    if [ "$colour" = "off" ]; then
        cfg_set "$2" "tint"
        echo "removed the tints on model $2"; restart; exit 0
    fi
    colour="${colour#\#}"
    [[ "$colour" =~ ^[0-9A-Fa-f]{6}$ ]] || { echo "The colour is 6 hex digits, like C0392B (red) or 1E40AF (blue)." >&2; exit 2; }
    [[ "$texture" == "*" || "$texture" =~ ^[A-Za-z0-9_.-]+$ ]] || { echo "Texture names: letters, digits, _ . -" >&2; exit 2; }
    if [ "$texture" = "*" ]; then cfg_set "$2" 'tint[[:space:]]+\*' "tint * $colour"
    else cfg_set "$2" "tint[[:space:]]+$texture" "tint $texture $colour"; fi
    echo "model $2: tint $texture $colour"
    restart; exit 0 ;;
--inspect)
    need_id "${2:-}"
    cfg_set "$2" "inspect" "inspect"
    echo "model $2: the phone's log will list its textures (\"CustomModels: model $2 uses textures: ...\")"
    echo "after you've read them: add-skin.sh --inspect-off $2"
    restart; exit 0 ;;
--inspect-off)
    need_id "${2:-}"; cfg_set "$2" "inspect"; restart; exit 0 ;;
--textures)
    need_id "${2:-}"
    src="${args[2]:-}"
    [ -d "$src" ] || { echo "usage: add-skin.sh --textures <id> <folder of .png pictures>" >&2; exit 2; }
    python3 - "$2" "$src" <<'PY'
import os, re, shutil, sys
model_id, src = sys.argv[1], sys.argv[2]
def key(s):
    s = os.path.splitext(s)[0]
    s = re.sub(r'\s*\(\d+\)$', '', s)
    return re.sub(r'[^a-z0-9]', '', s.lower())
out = f'content/models/{model_id}'
if os.path.isdir(out): shutil.rmtree(out)
os.makedirs(out)
n = 0
for root, _, files in os.walk(src):
    for f in sorted(files):
        if f.lower().endswith('.png') and key(f):
            shutil.copyfile(os.path.join(root, f), f'{out}/{key(f)}.png'); n += 1
            print(f"  texture {key(f)} <- {os.path.relpath(os.path.join(root, f), src)}")
print(f"model {model_id}: {n} pictures on the game's model (name them after its textures; see --inspect)")
PY
    restart; exit 0 ;;
--list)
    shopt -s nullglob
    ids=$(ls content/models 2>/dev/null | sed -nE 's/^([0-9]+)(\.(dff|txd|cfg))?$/\1/p' | sort -un)
    [ -n "$ids" ] || { echo "no custom models"; exit 0; }
    for id in $ids; do
        what=()
        [ -f "content/models/$id.dff" ] && what+=("own model")
        [ -f "content/models/$id.txd" ] && what+=(".txd")
        [ -d "content/models/$id" ] && what+=("$(ls "content/models/$id" | wc -l) pictures")
        [ -f "content/models/$id.cfg" ] && what+=("$(tr '\n' ';' < "content/models/$id.cfg")")
        echo "$id: ${what[*]}"
    done
    exit 0 ;;
--*)
    echo "unknown option $1 (see the top of this script)" >&2; exit 2 ;;
esac

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
    # Saved under the loose key (the client matches the same way), so names
    # with spaces or brackets still work: "23 1" -> 231.png.
    k = key(name)
    if not k:
        print(f"  ! texture \"{name}\" has no letters or digits; skipped")
        missing.append(name); continue
    shutil.copyfile(src, f'{out}/{model_id}/{k}.png')
    print(f"  texture {name} <- {os.path.relpath(src, tex)}")
if missing:
    print(f"  not found in {tex}: {', '.join(missing)}")
    print("  (the game's own texture is used if it has one by that name)")
PY
restart
