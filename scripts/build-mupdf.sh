#!/bin/sh
# Builds MuPDF 1.28.5 into mupdf/install so SweetPDF links the current release
# instead of an older copy from the distribution. Uses the git checkout in
# mupdf/ when it is already there, and clones it otherwise. That directory is
# gitignored.
set -eu

version=1.28.5
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
# the copy Leptonica was built against.
make -C "$src" -j"$jobs" build=release USE_SYSTEM_LIBS=yes libs
make -C "$src" build=release USE_SYSTEM_LIBS=yes prefix="$prefix" install-libs
echo "Installed MuPDF ${version} to ${prefix}"
