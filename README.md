# Kast Manager

[![Get it from the Snap Store](https://snapcraft.io/static/images/badges/en/snap-store-black.svg)](https://snapcraft.io/kastmanager)

> Fresh minimal system companion by **Tejas Khanna** — https://github.com/tejaskhanna989/kastmanager

Kast Manager 2.x is written from scratch (C, GTK4, libadwaita, libgtop):
no GNOME System Monitor code remains. Dark neon interface, sidebar layout.

Features: live **Dashboard** stat cards, sortable/searchable **Processes**
with End/Kill, hand-drawn **Graphs**, **File Systems** usage, and a minimal
**Files** browser (navigate, open, new folder, trash).

## Author

**Tejas Khanna** — https://github.com/tejaskhanna989/kastmanager

## License

GNU General Public License v3.0 or later. See `COPYING`.

## Donate

If Kast Manager helps you: https://github.com/sponsors/tejaskhanna989

## Install

```sh
sudo snap install kastmanager
```

Or `.deb` (Ubuntu 24.04+): download `kast-manager_2.6.0-1_amd64.deb` from
[releases](https://github.com/tejaskhanna989/kastmanager/releases),
then `sudo apt install ./kast-manager_2.6.0-1_amd64.deb`.

Or AppImage: download `KastManager-x86_64.AppImage` and `install.sh` from
[releases](https://github.com/tejaskhanna989/kastmanager/releases),
then `bash install.sh`.

## Building

```sh
# Arch
sudo pacman -S meson ninja gtk4 libadwaita libgtop librsvg
# Debian/Ubuntu
sudo apt install meson ninja-build libgtk-4-dev libadwaita-1-dev libgtop2-dev librsvg2-dev
meson setup build --prefix=/usr
ninja -C build
```

AppImage: `bash packaging/build-appimage.sh`, install with `bash install.sh`.
