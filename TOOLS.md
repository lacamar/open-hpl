# open-hpl tools

## Reference: the official game

The Windows `Soma_NoSteam.exe` runs under wine-arm64ec (`gt`, test prefix `soma-test-claude`) in bwrap
with the install dir read-only (`soma/ref/wine-ro.sh`), on its own scale-1 headless output, as a mod
(`soma/ref/mod`) whose agent module runs AngelScript snippets inside the real engine.

```
scripts/soma-ref.py start --map 00_01_apartment   # real game, straight into a map
OHPL_REF_GFX="SSAOActive=false" scripts/soma-ref.py start ...   # override user_settings graphics keys
OHPL_REF_OVERRIDE=<dir> scripts/soma-ref.py start ...   # mod files from <dir> override game files (A/B a .mat)
scripts/soma-ref.py exec '__print("x=" + cLux_GetGameTime());'
scripts/soma-ref.py player | perf | ping | teleport X Y Z --yaw D | shot out.png | log [re] | stop
scripts/soma-compare.py start --map M             # ours (headless, script player) + ref
scripts/soma-compare.py exec 'code' [--diff]      # same snippet on both, mismatches marked
scripts/soma-compare.py player | entities | lights [--pattern] [--diff]
scripts/soma-compare.py view --pose X Y Z YAW PITCH   # screenshots + side-by-side + image metrics
scripts/soma-compare.py fps --secs 10
scripts/soma-compare.py report --map M            # all of the above -> report.json
scripts/soma-compare.py boot [--record-ref] [--first-launch]  # boot splash -> main menu, per-frame PSNR -> boot/timeline.png
scripts/soma-compare.py ui start|click FX FY|key K..|shot   # both main menus, driven in lockstep
scripts/soma-audio.py menu | map --map M [--out DIR] [--keep]  # record both games' audio (null sinks), levels/bands/spectrograms
scripts/soma-audio.py identify X.wav --bank 'Player*'  # which extracted event samples are in a recording
scripts/soma-audio.py playing                     # ours: live sound entries, FMOD events, recent starts
OPENHPL_SOMA_SCRIPT_CHECK_FILE=f.hps scripts/soma-script-check.sh   # compile one file, print errors
```

Extracted event samples:
`$XDG_CACHE_HOME/open-hpl/soma/events-v4` (bump the version in SomaSound.cpp and soma-audio.py when
extraction changes).

Snippets: function bodies printing `key=value` via `__print`; the real engine formats floats as `%f`.
Real compile errors are only "Couldn't build script"; compile locally first for messages. Exec slots
(500) are per launch. The Linux `Soma_NoSteam.bin.x86_64` (needed by the `soma-re-*` scripts) is gone
while Steam has the Windows depot installed.

Script player gotchas: terminals need `interact` within ~2 m, `click --button right` leaves them;
areas need look + click; interactive camera animations need W; MovingButton drag direction comes from
`InputDirectionToOn`; map paths are `maps/chapterNN/`. Ref `teleport` needs
`cLux_GetPlayer().ChangeState(ePlayerState_Normal);` first. Meshes: `GetDynamicMeshEntity`.
`translucents` indices shift every frame. SOMA screen targets are 2D (not rect); light accumulation is x8.

The script API (`soma/data/script_api.txt`: types, enum values, methods, properties with offsets,
globals, and the native C++ function each binds to) is recovered statically from the official
binary's registration code. The binary has a full symbol table; disassemble a bound native
(`objdump -d -C`) to learn what an API call really does. Never run the official binary with its
install directory writable.

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
| `read_gbuffer_stats target=N` | per-channel min/max/mean/NaN/zero; 0-2 G-buffer, 4 light accumulation (HDR), 10-12 shadow maps high/med/low |
| `lights [n=8]` | nearest lights: type, distance, radius, colour, visible, shadows |
| `set_light name= visible=` | show/hide one light (per-light attribution) |
| `set_entity name= visible=` | show/hide mesh entities by glob (restart to undo: hidden-by-design ones get shown) |
| `set_render_setting name= value=` | A/B `occlusion_culling`, `delayed_occlusion`, `ssao`, `shadows`, `edge_smooth`, `fxaa`, `fog`, `decals`, `illumination`, `skybox`, `translucent`, `image_trail`, `dof_pass`, `light_depth_cull`, `shadow_cull`, `shadow_depth_clamp`; `log` = renderer log to hpl log |
| `dump_target target=N path= [channel=C] [screen=ENT] [light=L]` | PFM of 0-2 G-buffer, 4 accumulation, 5 box-light weights, 6-8 SSAO (raw, blur, final), 9 SSAO depth mips, 10-12 shadow maps, 13 sun cascade atlas, a light's shadow map, or a prop's GUI screen render target; `channel` picks one (3 = alpha) |
| `translucents [skip=N]` | translucent render list; `skip` hides one entry |
| `pick x= y=` | raw targets 0, 1, 2, 4 (accumulation) under a pixel (ints) or screen point (0-1 with a dot) |
| `pick_entity x= y= [x2= y2= z2=]` | submeshes under a screen point (0-1), or along a world segment x,y,z -> x2,y2,z2; nearest first (skinned included) |
| `shader_report [failed_only=false]` | compile/link status + info log per shader |
| `entity_info name=` | transform, AABB, mesh, per-submesh material/visibility |
| `wait_frames n= [max_ms=]` | replies after n rendered frames or the time cap |
| `start_map map= [pos=]` | load a map (default: first PlayerStart), hides menus |
| `camera_state` / `set_camera` | camera pose |
| `player_state` | script player: state, move state, health, feet position, yaw |
| `lux_entity name=` | script entity: type, class, script class, active, callbacks, position |
| `script_exec code= [module=]` | compiles and runs AngelScript against the live API (or in the script file matching `module`); `__print(s)` returns output |
| `script_tick code= [module=] [steps=60]` | runs `code` once per map update with `int n` = step; without `code` returns step and output so far |
| `script_vars [name=]` | properties of a script object (e.g. `LuxPlayer`); no name lists objects |
| `stub_report [n=60]` | unimplemented API functions called so far, by count |
| `script_profile [on=1] [n=30]` | script calls by inclusive time; `on=1` starts/clears collection |
| `sound_stats` | playing sound entries by file (channel exhaustion) |
| `body_contacts name= [x= y= z=]` | contact points (depth, normal) of an entity's bodies; player shape at a feet position |
| `raycast x= y= z= x2= y2= z2=` | physics hits along a segment |
| `physics_stats` | static/dynamic/awake bodies, fastest awake |
| `input`, `screenshot`, `quit`, `resize`, `log_tail` | generic |

When a test run produces a good-looking frame (new feature rendering right, a nice vista), save it:
`scripts/ohpl-album.sh <shot> <map>-<desc>` (alpha stripped, into `~/Pictures/Screenshots/openhpl-screenshots`).

Screenshots carry the framebuffer's alpha; when converting an older `.bmp`, use
`magick <file> -alpha off` or the image is composited into convincing fake noise.

Localising a render bug: G-buffer targets 0-2 -> accumulation (4) -> `frame_stats`; the first
stage whose numbers go wrong is where the bug is. Prefer these over screenshots. Screenshots only for comparing against reference images.
Toggling `occlusion_culling` off and on leaves objects culled; restart before measuring.
NaN pixels: `read_gbuffer_stats` counts them, `dump_target ... channel=C` + numpy finds them, `pick x= y=` polled over
frames shows whether they are transient. The image trail keeps any NaN pixel forever: A/B with `image_trail=0`.
Rebirth/Bunker never write G-buffer 1 alpha; NaN there is noise.
GL call traces: `apitrace trace -o f.trace ./Soma.bin.aarch64` (no replay: context calls aren't captured).
Hangs: `gdb -p <pid> -batch -ex bt`. Crashes: `coredumpctl debug <pid>`.
