# Review handover: Rust port (for Fable 5.2)

Careful, read-only review of the Rust port of open-hpl. When done, send the summary to the session that wrote this file (see "Report back").

## Rules

- READ-ONLY: no edits, commits, pushes, builds (cmake, make, cargo build/test/clippy) or game runs. The machine runs one heavy job at a time; another session owns builds.
- Reading files, `git log/show/diff`, `bd show/list` and grep are fine.
- Read `CLAUDE.md` first.

## Scope

Changes from `v1.3.77` to `master`, plus branch `rust-port-audit`. The worktree `.claude/worktrees/audit-merge` holds the newest crate.

1. Rust crate `rust/` (~18k lines; crate `ohpl`, staticlib linked into the C++ engine, `panic=abort`, edition 2024). Review `.claude/worktrees/audit-merge/rust`.
2. Every C++ shim calling it: `grep -rn ohpl_ HPL2 soma` (`Rust*.h` headers, String.cpp, BinaryBuffer.h, CRC.h, XmlDocument.cpp, ConfigFile.cpp, LanguageFile.cpp, PreprocessParser.cpp, GpuShaderManager.cpp, FileSearcher/ResourceManager, WorldLoaderHplMap*.cpp, WorldLoaderHpm*.cpp, EntityLoader_Object.cpp, HplMapBuilder.h, HpmBuilder.h, EntityObjectBuilder.h, SomaFsb.cpp, HpslTranspiler.h, PlatformUnix.cpp).
3. Zeppelin map-transfer fix: `git show 0b153f7` (`soma/src/game/SomaLux.cpp`).

## Contract

`bd show open-hpl-smj` and the commit messages. The port preserves the original C++ behaviour, bugs included; the kept quirks are in `bd list --label rust-port`. Where the C++ had undefined behaviour, Rust does something defined that doesn't crash. No panic can be reachable from FFI: clippy denies indexing, unwrap/expect, panic and integer division in non-test code.

## What to look for (priority order)

1. **FFI soundness:**
   - pointer and reference lifetimes across the boundary, e.g. Rust holding a pointer into a `std::string` or `std::vector` that can reallocate, or a callback running after its C++ object died
   - `&mut` aliasing created from raw pointers
   - `repr(C)` layout mismatches (compare field by field), and bool/enum ABI
   - buffer ownership and matching free functions; double free or leak
   - thread safety of statics and of libc `rand`/`srand`
2. **Panic paths the lints miss:** `split_at`, `chunks(0)`, `copy_from_slice` length mismatch, capacity overflow, `unwrap` inside macros, `from_utf8`.
3. **Differences from the C++ that the differential harnesses could miss:** rare branches, error paths, locale and float formatting, unsigned `char` on aarch64, integer wrap, iteration/sort order, float evaluation order and FMA.
4. **C++ shim bugs:** exception safety, missing null checks, wrong lengths, byte strings treated as UTF-8.
5. **Transfer fix logic:** `SameMap` semantics, when `gsPreloadMap` is cleared versus read, crouch/active size, saves made during a transfer.
6. Anything else that is clearly a bug. Skip style nits and anything clippy or rustfmt enforces.

For each finding give:
- severity: crash/UB, wrong behaviour, or minor
- file:line and what is wrong
- the input or scenario that triggers it
- the suggested fix
- confidence: confirmed, likely or speculative

Verify each one by reading the code path end to end, and don't pad. Also list what you reviewed and what you didn't get to.

## Output

Write the full report to `~/.cache/open-hpl/review/review-fable.md`.

## Report back

Send a message to the Claude session working on this repo's Rust port. It's another local session in `/home/lm/.local/src/open-hpl` (claude.ai session `session_01F3FjoGq5mMEkg6ZAjGEwPf`), and you can find it with `ListAgents`. Use `SendMessage`, with the first line "Fable 5.2 review of open-hpl Rust port done". Include the counts by severity, the top findings with file:line, and the report path. Keep it under 300 words.
