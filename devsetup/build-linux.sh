#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
version=0.1.0
if [[ $(uname -s) != Linux || $(uname -m) != x86_64 ]]; then
  echo 'This release script requires Linux x86_64.' >&2
  exit 1
fi
mkdir -p dist
package=$(mktemp -d)
trap 'rm -rf -- "$package"' EXIT
mkdir -p "$package/DEBIAN" "$package/usr/bin"
g++ -std=c++17 -O2 -Wall -Wextra -Wpedantic -static -s src/main.cpp -o dist/devsetup-linux-x64
chmod 755 dist/devsetup-linux-x64
python3 test-linux.py dist/devsetup-linux-x64
tar -czf dist/devsetup-linux-x64.tar.gz -C dist devsetup-linux-x64 -C .. README.md
cp dist/devsetup-linux-x64 "$package/usr/bin/devsetup"
chmod 755 "$package" "$package/DEBIAN" "$package/usr" "$package/usr/bin" "$package/usr/bin/devsetup"
cat > "$package/DEBIAN/control" <<EOF
Package: devsetup
Version: ${version}
Architecture: amd64
Maintainer: DevSetup contributors
Section: devel
Priority: optional
Description: Interactive C++ and Visual Studio Code environment setup
 Detects toolchains, offers package-manager installation, and generates
 VS Code build and IntelliSense settings for a single C++17 source file.
EOF
chmod 644 "$package/DEBIAN/control"
dpkg-deb --root-owner-group --build "$package" "dist/devsetup_${version}_amd64.deb"
echo 'Built and tested Linux x64 archive and Debian/Ubuntu package.'
