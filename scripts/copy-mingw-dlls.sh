#!/usr/bin/env bash
#
# Makes a MinGW build folder self-contained.
#
# windeployqt only knows about Qt, so MuPDF, Tesseract, SQLite and the GCC
# runtime have to be copied in separately. Their import tables are walked
# recursively with objdump - reading the binaries is more reliable inside MSYS2
# than asking ldd, which resolves paths through its own mount table.
#
# Usage: scripts/copy-mingw-dlls.sh <folder-with-binaries>
#
set -euo pipefail

target="${1:?usage: copy-mingw-dlls.sh <folder-with-binaries>}"
# UCRT64 sets MINGW_PREFIX to /ucrt64. The fallback covers a shell that
# forgot to export it.
mingw_bin="${MINGW_PREFIX:-/ucrt64}/bin"

declare -A seen=()
queue=()

while IFS= read -r -d '' binary; do
    queue+=("$binary")
done < <(find "$target" -type f \( -name '*.exe' -o -name '*.dll' \) -print0)

while (( ${#queue[@]} > 0 )); do
    binary="${queue[0]}"
    queue=("${queue[@]:1}")

    while read -r dll; do
        [[ -n "$dll" ]] || continue
        [[ -z "${seen[$dll]:-}" ]] || continue
        seen["$dll"]=1

        # Anything not shipped by MinGW is a system DLL and must not be bundled.
        source="$mingw_bin/$dll"
        [[ -f "$source" ]] || continue

        if [[ ! -f "$target/$dll" ]]; then
            cp "$source" "$target/"
            echo "bundled $dll"
        fi
        queue+=("$target/$dll")
    done < <(objdump -p "$binary" | awk '/DLL Name:/ { print $3 }')
done
