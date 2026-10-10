# Kast Manager

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
