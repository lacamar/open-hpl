build := "amnesia/src/build"

default:
    @just --list

test:
    [ -d {{build}} ] || cmake -S amnesia/src -B {{build}} -DCMAKE_BUILD_TYPE=RelWithDebInfo
    make -C {{build}} -j$(nproc) Soma PhysicsNewtonTests CStringTests PlatformXdgPathTests HpslTranspilerTests >/dev/null
    ctest --test-dir {{build}} --output-on-failure -Q

script-check:
    #!/usr/bin/env bash
    set -euo pipefail
    R=$(mktemp)
    trap 'rm -f "$R"' EXIT
    for g in soma rebirth bunker; do
        eval "$(scripts/ohpl_game.py $g)"
        [ -d "$SCRATCH" ] || continue
        OHPL_GAME=$g scripts/soma-script-check.sh "$R" 2>&1 | tail -1
    done

lint:
    git diff --check HEAD
