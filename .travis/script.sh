#!/bin/bash
set -e

# ---------------------------------------------------------------------------------------------------------------------

cat >>/etc/pacman.conf <<EOF
[multilib]
Include = /etc/pacman.d/mirrorlist

[mingw-w64]
SigLevel = Optional TrustAll
Server = https://github.com/jpcima/arch-mingw-w64/releases/download/repo.\$arch/
EOF

pacman -Sqy --noconfirm
pacman -Sq --noconfirm base-devel mingw-w64-gcc mingw-w64-pkg-config mingw-w64-zlib mingw-w64-libsndfile

# ---------------------------------------------------------------------------------------------------------------------

make CC="${_HOST}"-gcc CXX="${_HOST}"-g++ PKG_CONFIG="${_HOST}"-pkg-config
mkdir -p release
cp -f bin/sforzando-helper.exe release/sforzando-helper-"${_BUILD}".exe
