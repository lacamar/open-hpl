# SOMA conformance plan

Goal: open-hpl's SOMA module loads and renders all 29 official maps the way the official build
does. Scope here is map loading + rendering. Scripts (AngelScript object-type layer), saves and
gameplay are separate tracks and listed only where they block rendering work.

Written 2026-09-20 from a review of `soma/src/game/`, `HPL2/core/.../WorldLoaderHpm.cpp`,
`HeadlessControl.cpp`, `scripts/`, TASKS.md, PORTING_NOTES.md and a census of the real Steam
data.

## Status (2026-09-21)

- P0, P1, P2: done. `CLAUDE.md` has the commands.
- P3: offline glslang check done (`soma-shader-check.py`, fed by shader dumps); material census
  done (missing: `projecteduv`, `terrain`, `terraindecal`). Bone attributes dropped: HPL2 skins
  on the CPU, and the HPSL GPU path needs texture buffers, so `UseSkeleton` stays off.
- P4: Decal, Billboard, ParticleSystem, FogArea, DetailMeshes loaded. All primitives in the
  depot are planes. Rest open.
- P5: started - the sweep's black frames were root-caused (rect vs 2D G-buffer samplers,
  ignored per-light falloff, CHC culling). Open items in TASKS.md.
- P6: not started.

## 1. Review findings

### Map loading

`cWorldLoaderHpm` reads 8 of the 18 sidecar tracks every official map ships. Approximate object
counts across all 29 maps (`grep -c ' ID="'`, includes section headers):

| Track | Objects | Loaded |
|---|---|---|
| StaticObject | 72.9k | yes |
| Entity | 39.1k | yes (generic loader, no per-type behaviour) |
| Compound | 21.7k | no - semantics unverified (grouping only, or carries transforms?) |
| Decal | 12.6k | **no** |
| Area | 12.1k | PlayerStart only, rest no-op |
| Light | 8.8k | yes |
| Terrain | active in 10/29 maps | **no** (warning only) |
| Primitive | 3.8k | plane only |
| Billboard | 3.1k | **no** |
| ParticleSystem | 2.8k | **no** |
| Sound | 2.0k | yes |
| StaticObjectBatches | 271 | no - must check it duplicates StaticObject before loading |
| LightMask | 236 | no |
| FogArea | 122 | **no** |
| LensFlare | 61 | no |
| ExposureArea | 35 | yes |
| StaticComboArea | 31 | no |
| DetailMeshes | 29 | no |

HPL2 already has decals, billboards, particle systems and fog areas (`cWorldLoaderHplMap`,
`cEngineFileLoading`), so four of the biggest gaps are attribute-mapping work, not new engine
features.

Only `main_menu`, `00_00_intro` and `00_01_apartment` have ever been booted. Every fix so far
exposed the next latent crash on the same map; the other 26 maps are unknown.

### Rendering

- HPSL corpus is 75 files; self-test covers ~10, and only a few combos. No corpus-wide test.
- `vtx_vBoneIndices`/`vtx_vBoneWeight` unbound: every skinned mesh renders wrong.
- Open from PORTING_NOTES: `CubeMap Type="Rect"` loaded as 2D; box-light/fog ×8 HDR boost;
  stuck fade overlay after `start_map` from the menu; level-bounds fall-through near the bed.
- No census of `.mat` material types/variables vs what `MaterialType_*` implements.
- Tessellation/terrain/undergrowth/water/SSAO/DoF/bloom/tonemapping shaders: status unknown.

### Observability

- Map load emits one summary `Log()` line; skipped tracks are silent; warnings are unstructured.
- `Log()` uses an unchecked 4096-byte `vsprintf` - a real stack overflow, already hit once.
- Headless commands exist for camera, input, G-buffer stats, screenshot. Nothing reports world
  contents, per-frame render counts, shader compile/link status, GL errors, or final-frame
  statistics. Diagnosis has leaned on screenshots, which the notes repeatedly record as flaky.
- `mouse_move` injection is a no-op for `GetRelPosition()`.
- No local build dir present; 495 lines uncommitted across 10 files.

### Process

PORTING_NOTES.md (5.6k lines) and TASKS.md (1.5k lines) are narrative, headed "this session",
and contain several retracted claims. Nothing machine-readable says what passes today, so each
session re-derives state by reading prose.

## 2. Conformance definition

The official x86_64 binary cannot run here, so the oracle is the data:

1. **Load conformance** - for every map, every object in every track is created, or is on an
   explicit allowlist with a reason. Expected counts come from an offline XML census that shares
   no code with the engine loader.
2. **Shader conformance** - every `.hpsl` × every combo the sweep actually requests transpiles,
   compiles and links with zero errors.
3. **Frame conformance** - per map at the PlayerStart pose: no NaN in any G-buffer target, no
   unwritten target, final-frame luminance inside a per-map band, magenta-pixel fraction ~0,
   zero GL errors.
4. **Reference conformance** - a small set of fixed poses compared against user-supplied
   official screenshots. Last resort, human-judged, not part of the automated gate.
5. **Stability** - 60 s per map with no crash, hang, or RSS growth.

## 3. Phases

Each phase ends with: ctest green, sweep results committed, no metric regressed.

### P0 - Baseline

- Commit the current working tree.
- Restore an out-of-tree build; `scripts/soma-init.sh`: build, `setup-test-scratch.sh`,
  `deploy-test-binary.sh`, short socket path (<108 bytes), print the exports.
- Project `CLAUDE.md`: never write under `steamapps/common`, scratch-dir rules, socket-path
  limit, how to run the sweep, where state lives.

### P1 - Text-first interrogation

Engine (`HeadlessControl.cpp`, shared) and `SomaBase.cpp`:

- `Log()` → `vsnprintf`. Prerequisite for everything below.
- `load_report`: JSON written at map load and returned by command. Per track: objects in XML,
  created, skipped (with reason), failed; list of missing resources; per-phase timings.
- `world_stats`: live counts by type (mesh entities, lights by kind, decals, billboards,
  particle systems, fog areas, bodies), world AABB.
- `render_stats`: last-frame draw calls, objects per pass, lights per sub-list, occlusion
  culled, `glGetError` accumulated since last call.
- `shader_report`: every program requested - source file, combo bits, compile/link status,
  info log. Replaces grepping hpl.log.
- `frame_stats`: final framebuffer mean/min/max luminance, 16-bin histogram, NaN count,
  magenta fraction, fraction-black. Generalise `read_gbuffer_stats` to share the code.
- `pick x= y=`: depth + world position + nearest renderable/material under a pixel.
- `entity_info name=`: transform, mesh, materials, body, visibility.
- `start_map` resets menu/fade state so it is reliable from any state.
- `wait_frames n=` so scripts stop sleeping.
- Fix `mouse_move` relative injection.

`scripts/hpl_control.py` needs no change; commands are generic JSON.

### P2 - Sweep harness

- `scripts/soma-census.py`: offline XML census → `soma/conformance/expected.json`.
- `scripts/soma-sweep.py`: one process per map; `start_map`, `wait_frames`, collect
  `load_report` + `shader_report` + `render_stats` + `frame_stats` + G-buffer stats; on death
  attach coredumpctl/gdb backtrace; on stall use the `/proc/<pid>/stat` tick check from
  `headless-check.sh`. Output `soma/conformance/results.json` plus a one-screen text table.
- `--map`, `--only-failed`, `--compare <old.json>` for regression diffs.
- First full run is the real bug list. Triage crashes/hangs first, across all 29 maps,
  before any feature work.

### P3 - Shader corpus

- ctest: transpile all 75 `.hpsl`, validate with `glslangValidator` offline (no GPU). Combos
  come from the sweep's `shader_report`, stored as a fixture.
- Bind bone index/weight attributes (generic attribs or spare texcoord units, matching what
  Dark Descent's skeleton shader does).
- Material census: every `.mat` type + variable in the depot vs implemented. Feed P5.

### P4 - Loader gaps

Order by visual impact over cost. Each item: read the real XML, map to the existing HPL2
object, extend `load_report`, move the track off the allowlist.

1. Decal
2. Billboard
3. ParticleSystem (check `.ps` format drift from HPL2 first)
4. FogArea
5. Primitive types beyond plane
6. StaticObjectBatches / StaticComboArea / Compound - determine semantics before loading;
   risk is double-drawing or missing parent transforms
7. LightMask, LensFlare
8. DetailMeshes
9. Terrain - heightmap + blend layers as a plain mesh first, no tessellation; 10 maps

### P5 - Rendering conformance

Driven by sweep metrics, one root cause at a time:

- `CubeMap Type="Rect"`, box-light and fog HDR scale, translucent pass leftovers.
- Material types found missing in P3.
- Post chain: tonemapping, bloom, FXAA, then SSAO/DoF. Each behind a toggle so
  `frame_stats` can A/B it.
- Water, undergrowth, tessellation: last.
- Reference poses (criterion 4) only after 1-3 pass for that map.

### P6 - Soak

New Game click-through → intro → apartment, then every map 60 s with scripted movement; RSS
and fps sampled via `render_stats`.

## 4. Working method (Fable 5.1 guidance)

From Anthropic's current docs (links at end), applied to this repo:

- **Structured state, not prose.** `soma/conformance/results.json` is the source of truth
  for what passes. TASKS.md becomes a short todo list; PORTING_NOTES.md keeps narrative.
  Never delete or loosen an expected count or allowlist entry to make a run pass - change
  it only with a cited reason from the data.
- **First session builds the framework** (P0-P2); later sessions iterate on the failing list.
- **Fresh context over compaction.** Each session starts: read `CLAUDE.md`, `SOMA_PLAN.md`,
  `results.json`, `git log -15`; run `soma-init.sh`; run the sweep on one known-good map as
  a smoke test before touching anything.
- **Git as checkpoints.** One commit per root cause, sweep diff in the body. No long-lived
  dirty tree.
- **Verify with numbers.** A fix is done when a sweep metric moves and nothing else
  regresses. Screenshots only for criterion 4. No "verified" claims from code reading alone.
- **Competing hypotheses for rendering bugs.** Keep a short hypothesis list with confidence
  in the scratch notes; pick the measurement that splits them (the `read_gbuffer_stats`
  zero/NaN result is the model case).
- **General fixes only.** No per-map or per-material special cases to move a metric; if an
  expected value looks wrong, say so rather than work around it.
- **Scope discipline.** Bugs found outside the current item go to TASKS.md as follow-ups
  unless they block it. No new tests beyond what the phase calls for.
- **Subagents only for independent work** (e.g. per-track loaders in P4 in separate
  worktrees, each with its own scratch dir and socket). Lead keeps working while they run.
  Not for searches a grep answers.
- **Batch independent tool calls**; don't stop to ask about reversible steps the plan already
  covers; stop for anything touching Steam dirs, pushes, releases.
- **Effort:** default `high`; `xhigh` for renderer root-causing; `medium` is enough for P4
  attribute-mapping work.

Session prompt:

```
Continue SOMA conformance per SOMA_PLAN.md. Read CLAUDE.md, SOMA_PLAN.md,
soma/conformance/results.json and git log first, run scripts/soma-init.sh, smoke-test one
map with the sweep, then take the highest-priority failing item. Verify with sweep metrics,
commit per root cause, update results.json and TASKS.md before ending.
```

Docs: <https://platform.claude.com/docs/en/build-with-claude/prompt-engineering/prompting-claude-fable-5-1>,
<https://platform.claude.com/docs/en/build-with-claude/prompt-engineering/claude-prompting-best-practices>
