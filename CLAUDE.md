# open-hpl

HPL2 engine port (aarch64 Linux) running Amnesia TDD, AMFP, SOMA, Rebirth, Bunker data.
SOMA work follows `SOMA_PLAN.md`.

## Hard rules

- Never write under `*/steamapps/common/*`. Test only from a scratch dir made by
  `scripts/setup-test-scratch.sh`; deploy binaries only with `scripts/deploy-test-binary.sh`.
- The engine never writes game data: caches go to `$XDG_CACHE_HOME/open-hpl/files/<abs path>`, and
  `main()` remounts every reachable `steamapps/common` read-only in a private namespace.
- Control socket paths must be short (<108 bytes): use `$XDG_RUNTIME_DIR/ohpl-*.sock`.
- Never `pkill -f` with a loose pattern; kill by PID.
- Headless instances serialize on a flock. A leftover `Soma.bin.aarch64` or an orphaned
  `soma-sweep.py` makes the next launch sit idle at 0% CPU: `pgrep -af Soma.bin` first.
  SIGTERM takes minutes on big maps; use `kill -9 <pid>`.
- `soma/conformance/expected.json` is generated from the game data. Never edit it by hand, and
  never add to `allowlist.json` without a reason taken from the data.
- Shared `HPL2/core` changes must keep `ctest` green and Amnesia building.
- Never run the official binary with its install directory writable.

## Build / test

```
make -C amnesia/src/build -j12 Soma        # targets: Amnesia Amfp Soma (Rebirth and Bunker run on Soma)
make -C amnesia/src/build -j12 PhysicsNewtonTests CStringTests PlatformXdgPathTests HpslTranspilerTests  # not in all
ctest --test-dir amnesia/src/build
eval "$(scripts/soma-init.sh)"             # build + scratch dir + deploy, exports XDG_*
export OHPL_GAME=rebirth; eval "$(scripts/soma-init.sh rebirth)"  # tools below, per game: soma rebirth bunker amnesia amfp
export OHPL_GAME=rebirth                   # later calls: tools set the scratch XDG_* per launch themselves
eval "$(scripts/soma-init.sh --env-only rebirth)"  # re-export XDG_* without rebuilding
scripts/soma-sweep.py --map 00_01_apartment  # one map, ~10 s
scripts/soma-sweep.py                      # all 29 maps -> soma/conformance/results.json
scripts/soma-sweep.py --only-failed --compare old.json
scripts/soma-sweep.py --play 10                    # script player instead of freecam -> play.json (stubs, player)
scripts/soma-census.py                     # regenerate expected.json from the Steam data
scripts/soma-run.sh <map.hpm> [socket]     # one headless instance, prints "<pid> <socket>"
scripts/soma-play.py start --map M          # script player: goto/look/interact/key/walk/wait/state/entities/log/shot
OHPL_PLAY_NAME=ohpl-x scripts/soma-play.py ...  # separate socket/pid for a second instance
scripts/soma-shader-check.py <dump-dir>    # glslang over OPENHPL_DUMP_HPSL_SHADERS_DIR dumps
scripts/soma-gen-bindings.py               # regenerate SomaScriptGenBindings.cpp (compiler-verified)
scripts/soma-re-struct-defaults.py         # recover script struct defaults from the official binary
scripts/soma-script-check.sh               # compile all game .hps against the recovered API (~2 s)
scripts/launcher-smoke.sh <game>           # installed open-hpl-<game> reaches the menu; run after every RPM install
git config core.hooksPath .githooks        # pre-push: build, ctest, script checks
scripts/soma-re-script-api.py <Soma_NoSteam.bin.x86_64> re.json api.json soma/data/script_api.txt
```

## Reference tools

`TOOLS.md`: official-game reference (`soma-ref.py`, `soma-compare.py`, `soma-audio.py`, script API,
script player gotchas) and the `hpl_control.py` command table, env vars, render-bug localisation.
Read it before driving the reference game or a running engine.

## State

- `soma/conformance/results.json`: per-map status, load report, render/frame/G-buffer stats,
  aggregated log errors, `failures`. Source of truth for what passes.
- `TASKS.md`: todo list. `PORTING_NOTES.md`: narrative/root-cause history.
- Session start: read `HANDOVER.md` first, then `SOMA_PLAN.md`, `results.json` failures and
  `git log -15`; run `soma-init.sh`; sweep one map as a smoke test.
