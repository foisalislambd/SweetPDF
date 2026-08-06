#!/usr/bin/env bash
#
# Prints the version the next release should carry.
#
# The scheme is "every component stops at 9": 1.0.0 -> 1.0.1 -> ... -> 1.0.9 ->
# 1.1.0 -> ... -> 1.9.9 -> 2.0.0. The first release ever is 1.0.0.
#
# Usage:
#   scripts/next-version.sh              # next version after the newest v* tag
#   scripts/next-version.sh 1.0.9        # next version after an explicit one
#
set -euo pipefail

readonly LIMIT=9

current="${1:-}"

if [[ -z "$current" ]]; then
    # --sort=-v:refname orders tags numerically, so the newest release wins even
    # when a repository has both v1.0.9 and v1.1.0.
    current="$(git tag --list 'v[0-9]*.[0-9]*.[0-9]*' --sort=-v:refname | sed -n '1p')"
fi

if [[ -z "$current" ]]; then
    echo "1.0.0"
    exit 0
fi

current="${current#v}"

if [[ ! "$current" =~ ^([0-9]+)\.([0-9]+)\.([0-9]+)$ ]]; then
    echo "next-version: cannot parse '$current' as major.minor.patch" >&2
    exit 1
fi

major="${BASH_REMATCH[1]}"
minor="${BASH_REMATCH[2]}"
patch="${BASH_REMATCH[3]}"

if (( patch < LIMIT )); then
    patch=$(( patch + 1 ))
elif (( minor < LIMIT )); then
    minor=$(( minor + 1 ))
    patch=0
else
    major=$(( major + 1 ))
    minor=0
    patch=0
fi

echo "${major}.${minor}.${patch}"
