#!/usr/bin/env bash
# Build a single-file AppImage for Kast Manager.
# Run from repo root: bash packaging/build-appimage.sh
# Output: KastManager-x86_64.AppImage
# Requires: meson, ninja, linuxdeploy (auto-downloaded), build deps:
#   Arch: sudo pacman -S meson ninja gettext appstream itstool glibmm-2.68 gtkmm-4.0 libgtop librsvg libadwaita systemd glib2-devel
#   Debian/Ubuntu: sudo apt install meson gettext appstream-util itstool libglibmm-2.68-dev libgtkmm-4.0-dev libgtop2-dev librsvg2-dev libadwaita-1-dev libsystemd-dev
#   Fedora: sudo dnf install meson gettext appstream itstool glibmm2.68-devel gtkmm4.0-devel libgtop2-devel librsvg2-devel libadwaita-devel systemd-devel
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

# Rootless workaround for dev machines missing the system glib-mkenums tool:
# redirect pkg-config to a user-local copy instead of requiring root.
if [ ! -x /usr/bin/glib-mkenums ] && [ -x "$HOME/.local/bin/glib-mkenums" ]; then
  mkdir -p "$ROOT/.tools/pkgconfig"
  sed "s|glib_mkenums=\${bindir}/glib-mkenums|glib_mkenums=$HOME/.local/bin/glib-mkenums|" \
    /usr/lib/pkgconfig/glib-2.0.pc > "$ROOT/.tools/pkgconfig/glib-2.0.pc" 2>/dev/null || true
  export PKG_CONFIG_PATH="$ROOT/.tools/pkgconfig:${PKG_CONFIG_PATH:-}"
fi

APP_ID="io.github.tejaskhanna989.KastManager"
BIN="kast-manager"
APPDIR="$ROOT/AppDir"
BUILD_DIR="$ROOT/build-appimage"
OUTPUT="$ROOT/KastManager-x86_64.AppImage"

echo "[1/5] Configuring (meson)..."
rm -rf "$BUILD_DIR" "$APPDIR"
meson setup "$BUILD_DIR" --prefix=/usr --buildtype=release

echo "[2/5] Building..."
ninja -C "$BUILD_DIR"

echo "[3/5] Installing into AppDir..."
DESTDIR="$APPDIR" ninja -C "$BUILD_DIR" install

# linuxdeploy expects usr/share/applications/*.desktop + icons + usr/bin/$BIN
mkdir -p "$APPDIR/usr/bin"
# meson installs to $APPDIR/usr/bin/$BIN already (project name). Verify:
if [ ! -x "$APPDIR/usr/bin/$BIN" ]; then
  echo "ERROR: $APPDIR/usr/bin/$BIN missing after install" >&2
  find "$APPDIR" -maxdepth 4 -type f -name "*kast*" | head
  exit 1
fi

# Ensure desktop file + icon exist for linuxdeploy
DESKTOP_SRC=$(find "$APPDIR" -name "*.desktop" | head -n 1)
echo "Desktop: $DESKTOP_SRC"
ICON_SRC="$ROOT/data/icons/public/hicolor/scalable/apps/io.github.tejaskhanna989.KastManager.svg"
mkdir -p "$APPDIR/usr/share/icons/hicolor/scalable/apps"
cp -f "$ICON_SRC" "$APPDIR/usr/share/icons/hicolor/scalable/apps/" 2>/dev/null || true

echo "[4/5] Fetching linuxdeploy..."
LINUXDEPLOY="$ROOT/.tools/linuxdeploy-x86_64.AppImage"
GTK_PLUGIN="$ROOT/.tools/linuxdeploy-plugin-gtk.sh"
mkdir -p "$ROOT/.tools"
if [ ! -x "$LINUXDEPLOY" ]; then
  curl -sL -o "$LINUXDEPLOY" "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage"
  chmod +x "$LINUXDEPLOY"
fi
if [ ! -x "$GTK_PLUGIN" ]; then
  curl -sL -o "$GTK_PLUGIN" "https://raw.githubusercontent.com/linuxdeploy/linuxdeploy-plugin-gtk/master/linuxdeploy-plugin-gtk.sh"
  chmod +x "$GTK_PLUGIN"
fi

echo "[5/5] Creating AppImage..."
# Gtk plugin bundles schemas, gio modules, pixbuf loaders, etc.
export LINUXDEPLOY_OUTPUT_VERSION="1.0.0"
export ARCH=x86_64
# linuxdeploy itself is an AppImage; allow running where FUSE is unavailable (containers, CI)
export APPIMAGE_EXTRACT_AND_RUN=1

PIXBUF_DIR="$(pkg-config --variable=gdk_pixbuf_moduledir gdk-pixbuf-2.0 2>/dev/null || true)"
if [ -d "${PIXBUF_DIR:-/nonexistent}" ] && [ -n "$(ls -A "$PIXBUF_DIR" 2>/dev/null)" ]; then
  PLUGIN_ARGS=(--plugin gtk)
else
  echo "WARNING: no gdk-pixbuf loaders found on this system (${PIXBUF_DIR:-unknown})."
  echo "         Building WITHOUT the gtk plugin; images/SVG icons may fall back to the host."
  echo "         For a fully portable AppImage, build on Ubuntu 22.04 or use the CI artifact."
  PLUGIN_ARGS=()
  # Minimal runtime env the gtk plugin would otherwise provide:
  glib-compile-schemas "$APPDIR/usr/share/glib-2.0/schemas" 2>/dev/null || true
  mkdir -p "$APPDIR/apprun-hooks"
  cat > "$APPDIR/apprun-hooks/linuxdeploy-kast-hook.sh" <<'HOOK'
export GSETTINGS_SCHEMA_DIR="$APPDIR/usr/share/glib-2.0/schemas${GSETTINGS_SCHEMA_DIR:+:$GSETTINGS_SCHEMA_DIR}"
export GI_TYPELIB_PATH="$APPDIR/usr/lib/girepository-1.0${GI_TYPELIB_PATH:+:$GI_TYPELIB_PATH}"
HOOK
fi
"$LINUXDEPLOY" --appdir "$APPDIR" "${PLUGIN_ARGS[@]}" \
  -d "${DESKTOP_SRC:-$APPDIR/usr/share/applications/$APP_ID.desktop}" \
  -i "$APPDIR/usr/share/icons/hicolor/scalable/apps/io.github.tejaskhanna989.KastManager.svg" \
  --output appimage

# linuxdeploy names output from desktop file; normalize to single stable name
PRODUCED=$(ls -t Kast_Manager*.AppImage KastManager*.AppImage kast-manager*.AppImage 2>/dev/null | head -n 1 || true)
if [ -n "${PRODUCED:-}" ] && [ "$PRODUCED" != "KastManager-x86_64.AppImage" ]; then
  mv -f "$PRODUCED" "$OUTPUT"
fi
chmod +x "$OUTPUT" 2>/dev/null || true
ls -lh "$OUTPUT"
echo "Done: $OUTPUT"
echo "Install with: bash install.sh \"$OUTPUT\""
