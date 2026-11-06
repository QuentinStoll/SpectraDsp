#!/usr/bin/env bash
# Build a macOS product archive with Installer.app checkboxes for
# Standalone, VST3, and Audio Unit. The package is not notarized.
set -euo pipefail

usage() {
    cat <<'EOF'
Usage: build-pkg.sh --output FILE [--version VER] [--standalone APP] [--vst3 BUNDLE] [--au BUNDLE]
EOF
}

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
VERSION="0.0.0"
OUTPUT=""
STANDALONE=""
VST3=""
AU=""

while [[ $# -gt 0 ]]; do
    case "$1" in
        --output)
            OUTPUT="${2:?}"
            shift 2
            ;;
        --version)
            VERSION="${2:?}"
            shift 2
            ;;
        --standalone)
            STANDALONE="${2:?}"
            shift 2
            ;;
        --vst3)
            VST3="${2:?}"
            shift 2
            ;;
        --au)
            AU="${2:?}"
            shift 2
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "Unknown option: $1" >&2
            usage >&2
            exit 1
            ;;
    esac
done

if [[ -z "$OUTPUT" ]]; then
    echo "Missing --output" >&2
    exit 1
fi
if [[ -z "$STANDALONE" && -z "$VST3" && -z "$AU" ]]; then
    echo "Need at least one of --standalone, --vst3, --au" >&2
    exit 1
fi

WORKDIR=$(mktemp -d)
trap 'rm -rf "$WORKDIR"' EXIT
PKGS="$WORKDIR/pkgs"
ROOTS="$WORKDIR/roots"
DIST="$WORKDIR/Distribution.xml"
mkdir -p "$PKGS" "$ROOTS"

copy_bundle() {
    local src="$1"
    local dest="$2"
    mkdir -p "$(dirname "$dest")"
    rm -rf "$dest"
    cp -R "$src" "$dest"
}

CHOICES=""
OUTLINE=""

if [[ -n "$STANDALONE" ]]; then
    copy_bundle "$STANDALONE" "$ROOTS/standalone/Profiler.app"
    pkgbuild \
        --root "$ROOTS/standalone" \
        --install-location /Applications \
        --identifier com.spectradsp.spectra.standalone \
        --version "$VERSION" \
        "$PKGS/standalone.pkg"
    OUTLINE="${OUTLINE}<line choice=\"standalone\"/>"
    CHOICES="${CHOICES}
    <choice id=\"standalone\" title=\"Standalone\" description=\"Profiler application in /Applications\" selected=\"true\" enabled=\"true\">
        <pkg-ref id=\"com.spectradsp.spectra.standalone\"/>
    </choice>
    <pkg-ref id=\"com.spectradsp.spectra.standalone\" version=\"${VERSION}\" onConclusion=\"none\">standalone.pkg</pkg-ref>"
fi

if [[ -n "$VST3" ]]; then
    copy_bundle "$VST3" "$ROOTS/vst3/Profiler.vst3"
    pkgbuild \
        --root "$ROOTS/vst3" \
        --install-location "/Library/Audio/Plug-Ins/VST3" \
        --identifier com.spectradsp.spectra.vst3 \
        --version "$VERSION" \
        "$PKGS/vst3.pkg"
    OUTLINE="${OUTLINE}<line choice=\"vst3\"/>"
    CHOICES="${CHOICES}
    <choice id=\"vst3\" title=\"VST3 plugin\" description=\"Profiler.vst3 in /Library/Audio/Plug-Ins/VST3\" selected=\"true\" enabled=\"true\">
        <pkg-ref id=\"com.spectradsp.spectra.vst3\"/>
    </choice>
    <pkg-ref id=\"com.spectradsp.spectra.vst3\" version=\"${VERSION}\" onConclusion=\"none\">vst3.pkg</pkg-ref>"
fi

if [[ -n "$AU" ]]; then
    copy_bundle "$AU" "$ROOTS/au/Profiler.component"
    pkgbuild \
        --root "$ROOTS/au" \
        --install-location "/Library/Audio/Plug-Ins/Components" \
        --identifier com.spectradsp.spectra.au \
        --version "$VERSION" \
        "$PKGS/au.pkg"
    OUTLINE="${OUTLINE}<line choice=\"au\"/>"
    CHOICES="${CHOICES}
    <choice id=\"au\" title=\"Audio Unit\" description=\"Profiler.component in /Library/Audio/Plug-Ins/Components\" selected=\"true\" enabled=\"true\">
        <pkg-ref id=\"com.spectradsp.spectra.au\"/>
    </choice>
    <pkg-ref id=\"com.spectradsp.spectra.au\" version=\"${VERSION}\" onConclusion=\"none\">au.pkg</pkg-ref>"
fi

cat > "$DIST" <<EOF
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="2">
    <title>Spectra</title>
    <organization>com.spectradsp</organization>
    <options customize="always" require-scripts="false" rootVolumeOnly="true"/>
    <domains enable_anywhere="false" enable_currentUserHome="false" enable_localSystem="true"/>
    <welcome file="welcome.html" mime-type="text/html"/>
    <readme file="readme.txt" mime-type="text/plain"/>
    <choices-outline>
        ${OUTLINE}
    </choices-outline>
    ${CHOICES}
</installer-gui-script>
EOF

mkdir -p "$(dirname "$OUTPUT")"
productbuild \
    --distribution "$DIST" \
    --package-path "$PKGS" \
    --resources "$SCRIPT_DIR/Resources" \
    "$OUTPUT"

echo "Wrote $OUTPUT"
