#!/bin/sh
# Builds /srv/meta/manifest.json from /srv/content before nginx starts.
# Each file: its path, size and SHA-256, which the app checks after download.
# Restart the content service after changing files: docker compose restart content
set -eu
src=/srv/content
out=/srv/meta/manifest.json
mkdir -p /srv/meta
tmp="$out.tmp"
{
    printf '{\n  "version": 1,\n  "generated": "%s",\n  "files": [' "$(date -u +%Y-%m-%dT%H:%M:%SZ)"
    first=1
    cd "$src"
    # Skip hidden files and README.md; paths are limited to safe characters.
    find . -type f ! -path '*/.*' ! -name README.md | sed 's|^\./||' | sort | while IFS= read -r f; do
        case "$f" in
            *[!A-Za-z0-9_./-]*) echo "[content] skipped $f: use only letters, digits, _ - . /" >&2; continue ;;
        esac
        size=$(wc -c < "$f" | tr -d ' ')
        sha=$(sha256sum "$f" | cut -d' ' -f1)
        if [ "$first" = 1 ]; then first=0; else printf ','; fi
        printf '\n    { "path": "%s", "size": %s, "sha256": "%s" }' "$f" "$size" "$sha"
    done
    printf '\n  ]\n}\n'
} > "$tmp"
mv "$tmp" "$out"
echo "[content] manifest: $(grep -c '"path"' "$out" || true) files"
