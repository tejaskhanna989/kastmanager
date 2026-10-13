#!/usr/bin/env bash
# Bump Kast Manager version everywhere (except PKGBUILD sha256, which needs
# the published tag tarball: update it after `gh release create`).
# Usage: bash packaging/bump-version.sh 2.8.0
set -euo pipefail
VER="${1:?usage: bump-version.sh X.Y.Z}"
DATE="$(date +%Y-%m-%d)"
D="$(date '+%-d %B %Y')"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

OLD_MESON=$(grep -oP "version: '\K[0-9.]+" meson.build)
sed -i "s/version: '$OLD_MESON',/version: '$VER',/" meson.build

OLD_DEB=$(grep -oP '^kast-manager \(\K[0-9.]+' debian/changelog)
sed -i "s/kast-manager ($OLD_DEB-1)/kast-manager ($VER-1)/" debian/changelog

OLD_SPEC=$(grep -oP '^Version:\s+\K[0-9.]+' packaging/fedora/kast-manager.spec)
sed -i "s/^Version:        $OLD_SPEC$/Version:        $VER/" packaging/fedora/kast-manager.spec

OLD_PKG=$(grep -oP '^pkgver=\K[0-9.]+' packaging/arch/PKGBUILD)
sed -i "s/^pkgver=$OLD_PKG$/pkgver=$VER/" packaging/arch/PKGBUILD

OLD_SNAP=$(grep -oP "^version: '\K[0-9.]+'" snap/snapcraft.yaml)
sed -i "s/^version: '$OLD_SNAP'$/version: '$VER'/" snap/snapcraft.yaml

python3 - "$VER" "$DATE" "$D" <<'PY'
import pathlib, sys
ver, date, d = sys.argv[1], sys.argv[2], sys.argv[3]
p = pathlib.Path("data/io.github.tejaskhanna989.KastManager.metainfo.xml")
t = p.read_text()
t = t.replace("  <releases>\n",
f"""  <releases>
    <release date="{date}" version="{ver}" type="stable">
      <description>
        <ul>
          <li>TODO release notes</li>
        </ul>
      </description>
    </release>
""", 1)
p.write_text(t)
p = pathlib.Path("NEWS")
p.write_text(f"New in {ver} - {d}\n\n* TODO release notes\n\n" + p.read_text())
print("metainfo + NEWS bumped (fill in the TODO notes)")
PY

echo "Bumped $OLD_MESON -> $VER (meson/deb/spec/PKGBUILD/snap/metainfo/NEWS)."
echo "Remember: PKGBUILD sha256 needs the published tag tarball."
