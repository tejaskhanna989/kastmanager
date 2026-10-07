#!/usr/bin/env bash
# Kast Manager installer — moves the AppImage somewhere safe & hidden
# and creates a .desktop shortcut.
#
# Usage:
#   bash install.sh [path/to/KastManager-x86_64.AppImage] [--move|--copy] [--uninstall]
#
# Default behaviour: copy (use --move to move instead of copy).
# Safe hidden location: $HOME/.local/share/.kastmanager/KastManager-x86_64.AppImage
# Shortcut: $HOME/.local/share/applications/io.github.tejaskhanna989.KastManager.desktop
set -euo pipefail

APP_ID="io.github.tejaskhanna989.KastManager"
APP_NAME="Kast Manager"
BIN_NAME="KastManager-x86_64.AppImage"
DEST_DIR="$HOME/.local/share/.kastmanager"
DEST_FILE="$DEST_DIR/$BIN_NAME"
DESKTOP_DIR="$HOME/.local/share/applications"
DESKTOP_FILE="$DESKTOP_DIR/$APP_ID.desktop"
ICON_DIR="$HOME/.local/share/icons/hicolor/scalable/apps"
ICON_FILE="$ICON_DIR/$APP_ID.svg"

MODE="copy"
SRC=""
for arg in "$@"; do
  case "$arg" in
    --move) MODE="move" ;;
    --copy) MODE="copy" ;;
    --uninstall) MODE="uninstall" ;;
    --help|-h)
      sed -n '1,12p' "$0"
      exit 0 ;;
    *) SRC="$arg" ;;
  esac
done

uninstall() {
  rm -f "$DEST_FILE" "$DESKTOP_FILE"
  rmdir "$DEST_DIR" 2>/dev/null || true
  update-desktop-database "$DESKTOP_DIR" 2>/dev/null || true
  echo "Uninstalled $APP_NAME (removed $DEST_FILE and $DESKTOP_FILE)"
}

if [ "$MODE" = "uninstall" ]; then
  uninstall
  exit 0
fi

# Locate AppImage if not given
if [ -z "$SRC" ]; then
  for cand in \
    "./$BIN_NAME" \
    "./packaging/$BIN_NAME" \
    ./KastManager*.AppImage \
    ./kast-manager*.AppImage \
    "$(dirname "$0")/$BIN_NAME" \
    "$(dirname "$0")/packaging/$BIN_NAME"; do
    # glob may not expand; check literally + via compgen
    for f in $cand; do
      if [ -f "$f" ]; then SRC="$f"; break 2; fi
    done
  done
fi

if [ -z "${SRC:-}" ] || [ ! -f "$SRC" ]; then
  echo "ERROR: AppImage not found." >&2
  echo "Usage: bash install.sh [path/to/KastManager-x86_64.AppImage] [--move|--copy]" >&2
  exit 1
fi

SRC="$(cd "$(dirname "$SRC")" && pwd)/$(basename "$SRC")"
echo "Source: $SRC"
echo "Target (hidden): $DEST_FILE"

mkdir -p "$DEST_DIR" "$DESKTOP_DIR" "$ICON_DIR"
chmod 700 "$DEST_DIR"

if [ "$MODE" = "move" ]; then
  mv -f "$SRC" "$DEST_FILE"
else
  cp -f "$SRC" "$DEST_FILE"
fi
chmod +x "$DEST_FILE"

# Install icon if shipped alongside (repo checkout)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ICON_SRC="$SCRIPT_DIR/data/icons/public/hicolor/scalable/apps/$APP_ID.svg"
if [ ! -f "$ICON_SRC" ]; then
  ICON_SRC="$SCRIPT_DIR/packaging/icon.svg"
fi
if [ -f "$ICON_SRC" ]; then
  cp -f "$ICON_SRC" "$ICON_FILE"
  echo "Icon: $ICON_FILE"
else
  echo "Note: no icon found, shortcut will use $APP_ID (themed) fallback."
fi

cat > "$DESKTOP_FILE" <<EOF
[Desktop Entry]
Type=Application
Name=$APP_NAME
Comment=View current processes and monitor system state
Exec=$DEST_FILE
TryExec=$DEST_FILE
Icon=$APP_ID
Terminal=false
StartupNotify=true
StartupWMClass=kast-manager
Categories=GTK;System;Monitor;
Keywords=Monitor;System;Process;CPU;Memory;Network;History;Usage;Performance;Task;Manager;Activity;
X-AppImage-Location=$DEST_FILE
EOF

# If icon file installed, point Icon at absolute path for reliability
if [ -f "$ICON_FILE" ]; then
  sed -i "s|^Icon=.*|Icon=$ICON_FILE|" "$DESKTOP_FILE"
fi

chmod +x "$DEST_FILE"
update-desktop-database "$DESKTOP_DIR" 2>/dev/null || true
gtk-update-icon-cache -f -t "$HOME/.local/share/icons/hicolor" 2>/dev/null || true

echo ""
echo "Installed $APP_NAME"
echo "  AppImage : $DEST_FILE (hidden dir, mode 700)"
echo "  Shortcut : $DESKTOP_FILE"
echo "Launch from your app menu, or run: \"$DEST_FILE\""
echo "Uninstall: bash install.sh --uninstall"
