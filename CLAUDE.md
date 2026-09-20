# open-hpl

HPL2 engine port (aarch64 Linux) running Amnesia TDD, AMFP, SOMA, Rebirth, Bunker data.
SOMA work follows `SOMA_PLAN.md`.

## Hard rules

- Never write under `*/steamapps/common/*`. Test only from a scratch dir made by
  `scripts/setup-test-scratch.sh`; deploy binaries only with `scripts/deploy-test-binary.sh`.
- Control socket paths must be short (<108 bytes): use `$XDG_RUNTIME_DIR/ohpl-*.sock`.
- Never `pkill -f` with a loose pattern; kill by PID.
- `soma/conformance/expected.json` is generated from the game data. Never edit it by hand, and
  never add to `allowlist.json` without a reason taken from the data.
- Shared `HPL2/core` changes must keep `ctest` green and Amnesia building.

## Build / test

```
make -C amnesia/src/build -j12 Soma        # targets: Amnesia Amfp Soma Rebirth Bunker
(cd amnesia/src/build && ctest)
eval "$(scripts/soma-init.sh)"             # build + scratch dir + deploy, exports XDG_*
scripts/soma-sweep.py --map 00_01_apartment  # one map, ~10 s
scripts/soma-sweep.py                      # all 29 maps -> soma/conformance/results.json
scripts/soma-sweep.py --only-failed --compare old.json
scripts/soma-census.py                     # regenerate expected.json from the Steam data
```

## State

- `soma/conformance/results.json`: per-map status, load report, render/frame/G-buffer stats,
  aggregated log errors, `failures`. Source of truth for what passes.
- `TASKS.md`: todo list. `PORTING_NOTES.md`: narrative/root-cause history.
- Session start: read `SOMA_PLAN.md`, `results.json` failures, `git log -15`; run
  `soma-init.sh`; sweep one map as a smoke test.

## Interrogating a running engine

Launch with `OPENHPL_HEADLESS_SOCKET=<sock>` (hidden window). Useful env:
`OPENHPL_SOMA_MAP=<file.hpm>`, `OPENHPL_SOMA_SKIP_BOOT=1`, `OPENHPL_SOMA_FREECAM=1`,
`OPENHPL_DUMP_HPSL_SHADERS_DIR=<dir>`. Log: `$XDG_STATE_HOME/open-hpl/soma/hpl-<pid>.log`.

`scripts/hpl_control.py --socket <sock> <cmd> [k=v ...]`:

| cmd | returns |
|---|---|
| `load_report` | per-track xml/created/skipped(reason) counts of the last `.hpm` load |
| `world_stats` | live counts by object type, world AABB, submeshes without material |
| `render_stats` | draw calls, render-list sizes, lights rendered, GL errors, fps |
| `frame_stats` | final-frame luminance mean/histogram, black/white/magenta fractions |
| `read_gbuffer_stats target=N` | per-channel min/max/mean/NaN/zero of a G-buffer target |
| `pick x= y=` | raw G-buffer values under a pixel |
| `shader_report [failed_only=false]` | compile/link status + info log per shader |
| `entity_info name=` | transform, AABB, mesh, per-submesh material/visibility |
| `wait_frames n=` | replies after n rendered frames |
| `start_map map= [pos=]` | load a map (default: first PlayerStart), hides menus |
| `camera_state` / `set_camera` | camera pose |
| `input`, `screenshot`, `quit`, `resize`, `log_tail` | generic |

Prefer these over screenshots. Screenshots only for comparing against reference images.
Hangs: `gdb -p <pid> -batch -ex bt`. Crashes: `coredumpctl debug <pid>`.
