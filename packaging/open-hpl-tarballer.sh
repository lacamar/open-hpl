#!/usr/bin/env bash
# mx-rpm invokes: bash <this> <spec_path> <source_out_dir>
set -euo pipefail
spec="$1"; out="$2"
cd "$(dirname "${BASH_SOURCE[0]}")/.."
name=$(awk '/^Name:/{print $2;exit}' "$spec")
version=$(awk '/^Version:/{print $2;exit}' "$spec")
ref="v$version"; git rev-parse -q --verify "refs/tags/$ref" >/dev/null || ref=HEAD
git archive --prefix="$name-$version/" -o "$out/$name-$version.tar.gz" "$ref"
