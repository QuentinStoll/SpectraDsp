#!/usr/bin/env bash
# Spectra Linux installer. Choose standalone and/or VST3.
# GUI checkboxes use zenity or kdialog when available; otherwise CLI flags.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WANT_STANDALONE=""
WANT_VST3=""
PREFIX=""
VST3_DIR=""
ASSUME_YES=0

usage() {
    cat <<'EOF'
Usage: install.sh [options]

  --standalone          Install the standalone application
  --vst3                Install the VST3 plugin
  --no-standalone       Skip the standalone application
  --no-vst3             Skip the VST3 plugin
  --prefix DIR          Standalone destination (default: /opt/spectra as root,
                        or ~/.local/opt/spectra as a user)
  --vst3-dir DIR        VST3 destination (default: /usr/lib/vst3 as root,
                        or ~/.vst3 as a user)
  --yes                 Do not open a GUI; default is both components
  -h, --help            Show this help

With no component flags, both components are selected. If zenity or kdialog
is available and stdout is a terminal, a checkbox dialog is shown instead.
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --standalone) WANT_STANDALONE=1; shift ;;
        --vst3) WANT_VST3=1; shift ;;
        --no-standalone) WANT_STANDALONE=0; shift ;;
        --no-vst3) WANT_VST3=0; shift ;;
        --prefix) PREFIX="${2:?}"; shift 2 ;;
        --vst3-dir) VST3_DIR="${2:?}"; shift 2 ;;
        --yes) ASSUME_YES=1; shift ;;
        -h|--help) usage; exit 0 ;;
        *)
            echo "Unknown option: $1" >&2
            usage >&2
            exit 1
            ;;
    esac
done

explicit_selection=0
if [[ -n "$WANT_STANDALONE" || -n "$WANT_VST3" ]]; then
    explicit_selection=1
fi
if [[ -z "$WANT_STANDALONE" ]]; then
    WANT_STANDALONE=1
fi
if [[ -z "$WANT_VST3" ]]; then
    WANT_VST3=1
fi

choose_gui() {
    local selected=""
    if command -v zenity >/dev/null 2>&1; then
        selected="$(zenity --list --checklist \
            --title "Spectra" \
            --text "Choose components to install" \
            --column "Install" --column "Component" \
            TRUE "Standalone" TRUE "VST3 plugin" \
            --separator '|' || true)"
        WANT_STANDALONE=0
        WANT_VST3=0
        [[ "$selected" == *"Standalone"* ]] && WANT_STANDALONE=1
        [[ "$selected" == *"VST3"* ]] && WANT_VST3=1
        return 0
    fi
    if command -v kdialog >/dev/null 2>&1; then
        selected="$(kdialog --checklist "Choose Spectra components" \
            standalone "Standalone" on \
            vst3 "VST3 plugin" on || true)"
        WANT_STANDALONE=0
        WANT_VST3=0
        [[ "$selected" == *"standalone"* ]] && WANT_STANDALONE=1
        [[ "$selected" == *"vst3"* ]] && WANT_VST3=1
        return 0
    fi
    return 1
}

if [[ "$explicit_selection" -eq 0 && "$ASSUME_YES" -eq 0 && -t 0 && -t 1 ]]; then
    choose_gui || true
fi

if [[ "$WANT_STANDALONE" -eq 0 && "$WANT_VST3" -eq 0 ]]; then
    echo "No components selected." >&2
    exit 1
fi

if [[ -z "$PREFIX" ]]; then
    if [[ "$(id -u)" -eq 0 ]]; then
        PREFIX=/opt/spectra
    else
        PREFIX="${HOME}/.local/opt/spectra"
    fi
fi

if [[ -z "$VST3_DIR" ]]; then
    if [[ "$(id -u)" -eq 0 ]]; then
        VST3_DIR=/usr/lib/vst3
    else
        VST3_DIR="${HOME}/.vst3"
    fi
fi

install_standalone() {
    local src="$SCRIPT_DIR/standalone"
    if [[ ! -d "$src" ]]; then
        echo "Standalone payload missing." >&2
        exit 1
    fi
    mkdir -p "$PREFIX"
    cp -a "$src"/. "$PREFIX/"
    if [[ -f "$SCRIPT_DIR/spectra.png" ]]; then
        cp -a "$SCRIPT_DIR/spectra.png" "$PREFIX/spectra.png"
    elif [[ -f "$SCRIPT_DIR/profiler.png" ]]; then
        cp -a "$SCRIPT_DIR/profiler.png" "$PREFIX/spectra.png"
    fi

    local app_name=""
    for candidate in Spectra Profiler; do
        if [[ -f "$PREFIX/$candidate" ]]; then
            app_name="$candidate"
            break
        fi
    done
    if [[ -n "$app_name" && ! -f "$PREFIX/${app_name}.bin" ]]; then
        if compgen -G "$PREFIX/*.so*" >/dev/null; then
            mv "$PREFIX/$app_name" "$PREFIX/${app_name}.bin"
            cat > "$PREFIX/$app_name" <<WRAP
#!/usr/bin/env bash
DIR="\$(cd "\$(dirname "\$(readlink -f "\$0")")" && pwd)"
export LD_LIBRARY_PATH="\$DIR\${LD_LIBRARY_PATH:+:\$LD_LIBRARY_PATH}"
exec "\$DIR/${app_name}.bin" "\$@"
WRAP
            chmod +x "$PREFIX/$app_name"
        else
            chmod +x "$PREFIX/$app_name"
        fi
    fi

    local desktop_dir
    if [[ "$(id -u)" -eq 0 ]]; then
        desktop_dir=/usr/share/applications
    else
        desktop_dir="${XDG_DATA_HOME:-$HOME/.local/share}/applications"
    fi
    mkdir -p "$desktop_dir"
    cat > "$desktop_dir/spectra.desktop" <<EOF
[Desktop Entry]
Version=1.0
Type=Application
Name=Spectra
Comment=Guitar amplifier simulator
Exec=${PREFIX}/${app_name:-Spectra}
Icon=${PREFIX}/spectra.png
Terminal=false
Categories=AudioVideo;Audio;
StartupNotify=true
EOF
    echo "Installed standalone to $PREFIX"
}

install_vst3() {
    local src=""
    for candidate in "$SCRIPT_DIR/vst3/Spectra.vst3" "$SCRIPT_DIR/vst3/Profiler.vst3"; do
        if [[ -d "$candidate" || -f "$candidate" ]]; then
            src="$candidate"
            break
        fi
    done
    if [[ -z "$src" ]]; then
        echo "VST3 payload missing." >&2
        exit 1
    fi
    local dest_name
    dest_name="$(basename "$src")"
    mkdir -p "$VST3_DIR"
    rm -rf "$VST3_DIR/$dest_name"
    cp -a "$src" "$VST3_DIR/$dest_name"
    echo "Installed VST3 to $VST3_DIR/$dest_name"
}

if [[ "$WANT_STANDALONE" -eq 1 ]]; then
    install_standalone
fi
if [[ "$WANT_VST3" -eq 1 ]]; then
    install_vst3
fi

echo "Spectra install complete."
