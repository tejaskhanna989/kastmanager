#!/usr/bin/env bash
# Attach release assets + set notes for a Kast Manager version.
# Usage: bash packaging/release.sh 2.7.0 [AppImage] [deb] [pkg] [rpm]
set -euo pipefail
VER="${1:?usage: release.sh VERSION [files...]}"
shift
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

NOTES="## Install (pick one)
- **AppImage**: download \`KastManager-x86_64.AppImage\` and \`install.sh\`,
  then \`bash install.sh KastManager-x86_64.AppImage\`.
- **Debian/Ubuntu**: \`sudo apt install ./kast-manager_${VER}-1_amd64.deb\`
- **Arch**: \`sudo pacman -U kast-manager-${VER}-1-x86_64.pkg.tar.zst\`
- **Fedora**: \`sudo dnf install ./kast-manager-${VER}-1.*.rpm\`
- **Snap**: \`sudo snap install kastmanager\`
"

if gh release view "v$VER" --repo tejaskhanna989/kastmanager >/dev/null 2>&1; then
  gh release edit "v$VER" --repo tejaskhanna989/kastmanager --notes "$NOTES"
else
  gh release create "v$VER" --repo tejaskhanna989/kastmanager \
    --title "Kast Manager v$VER" --notes "$NOTES" "$@"
  exit 0
fi

if [ "$#" -gt 0 ]; then
  gh release upload "v$VER" "$@" --repo tejaskhanna989/kastmanager --clobber
fi
gh release view "v$VER" --repo tejaskhanna989/kastmanager \
  --json assets --jq '.assets[] | "\(.name) \(.size)B"'
