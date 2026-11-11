#!/bin/sh
# Builds MuPDF 1.28.6 into mupdf/install so SweetPDF links the current release
# instead of an older copy from the distribution. Uses the git checkout in
# mupdf/ when it is already there, and clones it otherwise. That directory is
# gitignored.
#
# 1.28.6's own change is the vendored brotli 1.2.1. This script builds with
# USE_SYSTEM_LIBS, so the brotli you link is the system one; the pin still
# matters because it is the release SweetPDF is tested against, and because
# the PKCS#7 helper below is taken from that tree.
set -eu

version=1.28.6
root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
repo="$root/mupdf"
src="$repo/release-${version}"
prefix="$repo/install"
jobs=$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)

if [ ! -d "$repo/.git" ]; then
    git clone --branch "$version" --depth 1 https://github.com/ArtifexSoftware/mupdf.git "$repo"
fi

if ! git -C "$repo" rev-parse -q --verify "refs/tags/${version}" >/dev/null; then
    git -C "$repo" fetch --depth 1 origin "tag" "$version"
fi

if [ ! -f "$src/Makefile" ]; then
    git -C "$repo" worktree add --detach "$src" "$version"
fi

if [ ! -f "$src/thirdparty/freetype/include/freetype.h" ] \
    && [ ! -f "$src/thirdparty/freetype/include/freetype/freetype.h" ]; then
    git -C "$src" submodule update --init --recursive
fi

echo "Building MuPDF ${version}..."
# System codecs, so the static archive does not ship its own libjpeg and fight
# the copy Leptonica was built against. libcrypto must be visible to pkg-config
# or the PKCS#7 helper is compiled as a stub that refuses to sign.
make -C "$src" -j"$jobs" build=release USE_SYSTEM_LIBS=yes libs
make -C "$src" -j"$jobs" build=release USE_SYSTEM_LIBS=yes build/release/libmupdf-pkcs7.a
make -C "$src" build=release USE_SYSTEM_LIBS=yes prefix="$prefix" install-libs

# install-libs does not ship the OpenSSL signature helper. Certificate signing
# links this archive plus the headers under include/mupdf/helpers.
pkcs7="$src/build/release/libmupdf-pkcs7.a"
if [ -f "$pkcs7" ]; then
    mkdir -p "$prefix/lib" "$prefix/include/mupdf/helpers"
    cp "$pkcs7" "$prefix/lib/libmupdf-pkcs7.a"
    cp "$src/include/mupdf/helpers/"*.h "$prefix/include/mupdf/helpers/"
fi
echo "Installed MuPDF ${version} to ${prefix}"
