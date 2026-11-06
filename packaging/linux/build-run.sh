#!/usr/bin/env bash
# Pack a Spectra Linux self-extracting installer (.run).
# Uses makeself when available; otherwise a tar+stub archive.
set -euo pipefail

usage() {
    cat <<'EOF'
Usage: build-run.sh --payload DIR --output FILE [--label TEXT]
EOF
}

PAYLOAD=""
OUTPUT=""
LABEL="Spectra installer"

while [[ $# -gt 0 ]]; do
    case "$1" in
        --payload) PAYLOAD="${2:?}"; shift 2 ;;
        --output) OUTPUT="${2:?}"; shift 2 ;;
        --label) LABEL="${2:?}"; shift 2 ;;
        -h|--help) usage; exit 0 ;;
        *)
            echo "Unknown option: $1" >&2
            usage >&2
            exit 1
            ;;
    esac
done

if [[ -z "$PAYLOAD" || -z "$OUTPUT" ]]; then
    echo "Need --payload and --output" >&2
    exit 1
fi
if [[ ! -x "$PAYLOAD/install.sh" ]]; then
    echo "Payload must contain an executable install.sh" >&2
    exit 1
fi

mkdir -p "$(dirname "$OUTPUT")"

if command -v makeself >/dev/null 2>&1; then
    makeself --sha256 "$PAYLOAD" "$OUTPUT" "$LABEL" ./install.sh
    echo "Wrote $OUTPUT"
    exit 0
fi

WORKDIR=$(mktemp -d)
trap 'rm -rf "$WORKDIR"' EXIT
TAR="$WORKDIR/payload.tar.gz"
tar -czf "$TAR" -C "$PAYLOAD" .

STUB="$WORKDIR/stub.sh"
cat > "$STUB" <<'STUB'
#!/usr/bin/env bash
set -euo pipefail
ARCHIVE_MARKER="__SPECTRA_ARCHIVE__"
SELF="$(readlink -f "$0" 2>/dev/null || python3 -c "import os,sys; print(os.path.realpath(sys.argv[1]))" "$0")"
ARCHIVE_LINE="$(awk "/^${ARCHIVE_MARKER}$/ {print NR + 1; exit}" "$SELF")"
WORKDIR="$(mktemp -d)"
trap 'rm -rf "$WORKDIR"' EXIT
tail -n +"$ARCHIVE_LINE" "$SELF" | tar -xzf - -C "$WORKDIR"
chmod +x "$WORKDIR/install.sh"
exec "$WORKDIR/install.sh" "$@"
STUB

cat "$STUB" > "$OUTPUT"
printf '__SPECTRA_ARCHIVE__\n' >> "$OUTPUT"
cat "$TAR" >> "$OUTPUT"
chmod +x "$OUTPUT"
echo "Wrote $OUTPUT (tar stub; install makeself for the standard wrapper)"
