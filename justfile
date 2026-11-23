build := "amnesia/src/build"

default:
    @just --list

test:
    [ -d {{build}} ] || cmake -S amnesia/src -B {{build}} -DCMAKE_BUILD_TYPE=RelWithDebInfo
    make -C {{build}} -j$(nproc) Soma PhysicsNewtonTests CStringTests PlatformXdgPathTests HpslTranspilerTests GlslToVulkanTests TraceRecorderTests >/dev/null
    ctest --test-dir {{build}} --output-on-failure -Q
    just test-release

# the RPM %check runs cargo test with these flags; NaN sign and FMA can differ from dev builds
test-release:
    CARGO_TARGET_DIR=rust/target/rpmcheck RUSTFLAGS='-Copt-level=3 -Ccodegen-units=1' cargo test -q --offline --release --manifest-path rust/Cargo.toml

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
    cargo fmt --all --check --manifest-path rust/Cargo.toml
    cargo clippy -q --offline --manifest-path rust/Cargo.toml --workspace --all-targets -- -D warnings
