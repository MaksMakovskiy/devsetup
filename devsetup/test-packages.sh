#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
scratch=$(mktemp -d)
trap 'rm -rf -- "$scratch"' EXIT
dpkg-deb --extract dist/devsetup_0.1.0_amd64.deb "$scratch/deb"
"$scratch/deb/usr/bin/devsetup" --help
tar -xzf dist/devsetup-linux-x64.tar.gz -C "$scratch"
chmod +x "$scratch/devsetup-linux-x64"
"$scratch/devsetup-linux-x64" --help
cd dist
sha256sum --check SHA256SUMS.txt
