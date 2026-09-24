# open-hpl

HPL2 engine port (aarch64 Linux) running Amnesia TDD, AMFP, SOMA, Rebirth, Bunker data.
SOMA work follows `SOMA_PLAN.md`.

## Hard rules

- Never write under `*/steamapps/common/*`. Test only from a scratch dir made by
  `scripts/setup-test-scratch.sh`; deploy binaries only with `scripts/deploy-test-binary.sh`.
- Control socket paths must be short (<108 bytes): use `$XDG_RUNTIME_DIR/ohpl-*.sock`.
- Never `pkill -f` with a loose pattern; kill by PID.
- Headless instances serialize on a flock. A leftover `Soma.bin.aarch64` or an orphaned
  `soma-sweep.py` makes the next launch sit idle at 0% CPU: `pgrep -af Soma.bin` first.
  SIGTERM takes minutes on big maps; use `kill -9 <pid>`.
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
scripts/soma-run.sh <map.hpm> [socket]     # one headless instance, prints "<pid> <socket>"
scripts/soma-shader-check.py <dump-dir>    # glslang over OPENHPL_DUMP_HPSL_SHADERS_DIR dumps
```

## State

- `soma/conformance/results.json`: per-map status, load report, render/frame/G-buffer stats,
  aggregated log errors, `failures`. Source of truth for what passes.
- `TASKS.md`: todo list. `PORTING_NOTES.md`: narrative/root-cause history.
- Session start: read `HANDOVER.md` first, then `SOMA_PLAN.md`, `results.json` failures and
  `git log -15`; run `soma-init.sh`; sweep one map as a smoke test.

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
| `read_gbuffer_stats target=N` | per-channel min/max/mean/NaN/zero; 0-2 G-buffer, 4 light accumulation |
| `lights [n=8]` | nearest lights: type, distance, radius, colour, visible, shadows |
| `set_light name= visible=` | show/hide one light (per-light attribution) |
| `set_render_setting name= value=` | A/B `occlusion_culling`, `ssao`, `shadows`, `edge_smooth`, `fxaa`, `fog` |
| `pick x= y=` | raw targets 0, 1, 2, 4 (accumulation) under a pixel |
| `shader_report [failed_only=false]` | compile/link status + info log per shader |
| `entity_info name=` | transform, AABB, mesh, per-submesh material/visibility |
| `wait_frames n= [max_ms=]` | replies after n rendered frames or the time cap |
| `start_map map= [pos=]` | load a map (default: first PlayerStart), hides menus |
| `camera_state` / `set_camera` | camera pose |
| `input`, `screenshot`, `quit`, `resize`, `log_tail` | generic |

Screenshots carry the framebuffer's alpha; when converting an older `.bmp`, use
`magick <file> -alpha off` or the image is composited into convincing fake noise.

Localising a render bug: G-buffer targets 0-2 -> accumulation (4) -> `frame_stats`; the first
stage whose numbers go wrong is where the bug is. Prefer these over screenshots. Screenshots only for comparing against reference images.
Hangs: `gdb -p <pid> -batch -ex bt`. Crashes: `coredumpctl debug <pid>`.
