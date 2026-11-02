#!/usr/bin/env bash
# Publishes server/content/ for the app (docs/CONTENT.md): writes
# content-meta/manifest.json (each file's path, size and SHA-256) and
# content-meta/manifest.sig, an ECDSA P-256 signature of it made with
# content-keys/manifest-key.pem. The app only accepts a manifest whose
# signature checks against the public key it was built with
# (app/content-pubkey.pem), so nobody between the server and a player can
# swap the files.
#
# The key is made on first run and never leaves this machine (keep a backup:
# a new key means a new app build). Show the public key with --pubkey.
# Env overrides (tests): KM_CONTENT_DIR, KM_CONTENT_META, KM_CONTENT_KEY.
set -euo pipefail
cd "$(dirname "$0")/.."

src="${KM_CONTENT_DIR:-content}"
meta="${KM_CONTENT_META:-content-meta}"
key="${KM_CONTENT_KEY:-content-keys/manifest-key.pem}"

command -v openssl >/dev/null || { echo "content-publish: openssl is needed (apt install openssl)" >&2; exit 1; }
if [ ! -f "$key" ]; then
    mkdir -p "$(dirname "$key")"
    chmod 700 "$(dirname "$key")"
    (umask 077 && openssl ecparam -name prime256v1 -genkey -noout -out "$key")
    echo "== made a new content signing key: $key (back it up; it never goes in git)"
    new_key=1
fi
if [ "${1:-}" = "--pubkey" ] || [ -n "${new_key:-}" ]; then
    echo "== content public key: put this in app/content-pubkey.pem and build the app"
    openssl ec -in "$key" -pubout 2>/dev/null
    [ "${1:-}" = "--pubkey" ] && exit 0
fi

mkdir -p "$meta"
python3 - "$src" "$meta/manifest.json.tmp" <<'PY'
import datetime, hashlib, json, os, re, sys
src, out = sys.argv[1], sys.argv[2]
files = []
for root, dirs, names in os.walk(src):
    dirs[:] = sorted(d for d in dirs if not d.startswith('.'))
    for n in sorted(names):
        if n.startswith('.') or (root == src and n == 'README.md'):
            continue
        rel = os.path.relpath(os.path.join(root, n), src).replace(os.sep, '/')
        if not re.fullmatch(r'[A-Za-z0-9_./-]+', rel):
            print(f"[content] skipped {rel}: use only letters, digits, _ - . /", file=sys.stderr)
            continue
        with open(os.path.join(root, n), 'rb') as f:
            data = f.read()
        files.append({"path": rel, "size": len(data), "sha256": hashlib.sha256(data).hexdigest()})
files.sort(key=lambda f: f["path"])
doc = {"version": 1,
       "generated": datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
       "files": files}
with open(out, "w") as f:
    json.dump(doc, f, indent=2)
    f.write("\n")
print(f"[content] manifest: {len(files)} files")
PY
openssl dgst -sha256 -sign "$key" "$meta/manifest.json.tmp" | base64 -w0 > "$meta/manifest.sig.tmp"
# The signature first, then the manifest: a reader never sees a new manifest
# with the old signature for long, and the app retries next launch anyway.
mv "$meta/manifest.sig.tmp" "$meta/manifest.sig"
mv "$meta/manifest.json.tmp" "$meta/manifest.json"
echo "[content] signed with $key"
