# SOMA handover (2026-09-24)

Read with `CLAUDE.md` (commands, hard rules) and `SOMA_PLAN.md` (the plan). `TASKS.md` item 1..N
is the ordered backlog; `soma/conformance/results.json` is what actually passes.

## State

- Branch `master`. **`c1f8aa9` (intro ambience) is committed but not pushed** - push it with the
  next release, or on its own.
- Tagged and shipped: `v1.3.26` = COPR `lacamar/arm64-misc`, installed locally as
  `open-hpl-1.3.26-1.fc44`. The spec lives at `~/.local/rpm/specs/open-hpl.spec`, outside git.
- Sweep: **8/29 maps pass**, every map boots and renders. Failure categories across the 29:
  `no_material` 15, `fps` 11, `gl_errors` 2, `status` (crash) 2, plus singletons.
- `ctest` 4/4. Dark Descent re-verified after every shared-core change (boots
  `02_entrance_hall.map`, healthy G-buffer, no shader failures).

## Fixed this session, with the evidence that proved it

1. **All meshes were 100x too large.** The Collada loader only honours `<unit meter="0.01">`
   when `<authoring_tool>` is `"FBX COLLADA exporter"`; SOMA's depot is centimetre data from
   OpenCOLLADA/modo/Maya. `cMeshLoaderCollada::SetConvertUnitFromAnyTool()`, SOMA only - Dark
   Descent's 99 centimetre files really are in metres, so the gate must stay per game.
   `bed_1` 311 m -> 3.1 m.
2. **Drawers and doors invisible.** `cEntityLoader_Object::CreateJoint()` reads the axis from
   `PinDir`; none of SOMA's 4928 joints have it (HPL3 stores `Rotation`). Zero-length axis ->
   Newton NaN -> body and meshes gone. Derived from Dark Descent's 295 joints carrying both:
   `PinDir == MatrixRotate(Rotation,XYZ)*(0,1,0)`, 295/295 exactly. nan_bounds 32 -> 0.
3. **That fix then cost 6x the frame rate**, so `cSomaBase::LoadMap()` now sets mass 0 on every
   joint-endpoint body. Clean A/B on `03_03_omicron_descent`: 59-60 fps zero-axis vs 8.8-16 fps
   with joints live, identical draw calls and body counts, all time in
   `cPhysicsWorldNewton::Simulate`; `swingdoor_airlock_2` drifted 2.1 m in 60 frames. Doors swing
   open on load because no script layer holds them shut.
4. **Normal maps were empty textures.** ATI2 (1780 files) mapped to GL format 0, BC5U (165) was
   rejected by DevIL, and a failed texture failed the whole material. Raw DDS path +
   `ePixelFormat_RGTC2_XY/YX` + swizzle to the DXT5nm layout the shaders unpack.
5. **Lights contributed nothing.** G-buffer was `GL_TEXTURE_RECTANGLE` while SOMA's shaders
   declare `sampler2D`, so the light pass read an unbound texture.
   `SetGBufferTextureType(eTextureType_2D)` + normalized UVs in `CopyToFrameBuffer()`.
6. Per-light `FalloffPow`/`SpotFalloffPow`/`Brightness`; CHC occlusion culling off for SOMA
   (0.1 fps and culled everything); FBX loader via assimp (121 fbx-only meshes); intro ambience
   from its FMOD bank; five crash fixes; two ASan-found out-of-bounds reads.

## Open, in the order I would take them

`TASKS.md` has the full list. Fixed on 2026-09-24 (see PORTING_NOTES): detached legs (inactive
map entities), lighting shifting while turning (fog read specular as depth), blue accumulation
clear, skinned-mesh bounds, silent phone ring, dead AA toggle; mesh cache back on. Still open:

- **Physics objects not interactable.** Expected - there is no general interact system, only a
  hardcoded point for the apartment phone. Jointed props are also pinned (item above).

Then: `no_material` (15 maps, use `world_stats.no_material_top`), fps on the big maps (physics
step dominates), window glass still opaque (translucent material reaches the render list; suspect
`mpRefractionTexture` is still rect-typed while the G-buffer is 2D), and the two crashers
(`04_01_tau_outside` intermittent heap corruption, bisected to per-object static collision
bodies; `02_05_theta_inside` crash on exit).

## How to work on this

```
eval "$(scripts/soma-init.sh)"          # build + scratch + deploy, exports XDG_*
scripts/soma-sweep.py --map 00_01_apartment
scripts/soma-sweep.py --compare <old.json>
scripts/soma-sweep.py --rejudge         # re-run checks over existing results, boots nothing
scripts/soma-run.sh <map.hpm>           # one headless instance -> "<pid> <socket>"
```

Diagnose with the headless text commands, not screenshots (`CLAUDE.md` lists all of them).
For a render bug walk G-buffer targets 0-2 -> accumulation (4) -> `frame_stats`; the first stage
whose numbers go wrong is where the bug is.

## Traps that cost me time

- **This machine's load gives ~2x fps noise** (load average 10+ on 12 cores). Three identical
  runs of one map gave 6.7 / 11.0 / 11.6 fps. Never attribute a performance change without an
  alternating A/B in the same conditions - one sweep-to-sweep diff showed one map 5x faster and
  another 10x slower from a single commit.
- **Screenshots carry the framebuffer's alpha.** Fixed in `CmdScreenshot`, but for any older
  `.bmp` use `magick <file> -alpha off` - ImageMagick composites the garbage alpha into
  convincing full-screen "block noise" that looks exactly like a rendering bug. I chased it.
- **Headless instances serialize on a flock.** A leftover `Soma.bin.aarch64` or an orphaned
  `soma-sweep.py` makes the next launch sit idle at 0% CPU. `pgrep -af Soma.bin` first, kill by
  PID, `-9` (SIGTERM takes minutes on big maps).
- **Never `pkill -f` with a loose pattern** - it matched my own shells twice.
- **COPR**: builds take 10-115 min (rawhide aarch64 straggles, and the repo only publishes once
  every chroot finishes). After it does, CloudFront can serve the pre-signing RPM for ~an hour,
  so `dnf` downloads it and rejects the checksum. Install from the origin instead:
  `--setopt=<repoid>.baseurl=https://copr-be.cloud.fedoraproject.org/results/lacamar/arm64-misc/fedora-44-aarch64/`.
- `soma/conformance/expected.json` is generated; never hand-edit it, and never add to
  `allowlist.json` without a reason taken from the data.
- The mesh cache lives in `$XDG_CACHE_HOME/open-hpl/soma/meshcache`; bump the `.v3` key in
  `GetExternalMeshCacheFile()` whenever mesh-loader output changes, or stale meshes load.
