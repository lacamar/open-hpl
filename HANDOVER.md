# SOMA handover (2026-09-26, Rebirth notes 2026-10-14)

Read with `CLAUDE.md` (commands, hard rules) and `SOMA_PLAN.md`. `TASKS.md` is the backlog.

## State

- SOMA now runs its own game scripts: `player/Player.hps` and its states drive movement and the
  camera, `base/InputHandler.hps` defines the input actions, map scripts run with timers,
  collide/interact/look-at callbacks, voice and dialog, fades, map changes and the HUD ImGui.
- Verified end to end (headless): `00_00_intro` slideshow, narration; `00_01_apartment` wake-up,
  phone call, tracer fluid pickup and use, exit door -> `00_02_subway`; `00_03_laboratory`
  keypad code on the in-world terminal unlocks the door.
- Doors, drawers, grabbing, prop GUIs on in-world screens (laptops, keypads) work.
- Saves: autosave on map entry and at script checkpoints; mid-map saves restore in place.
  Menu CONTINUE loads the latest save; SAVE AND EXIT saves.
- `OPENHPL_SOMA_SCRIPT_PLAYER=0` falls back to the hand-written `cSomaPlayer` (and the
  hand-ported intro/phone call).
- `scripts/soma-script-check.sh`: 190/190 scripts compile; `stub_calls` in
  `soma/conformance/script_check.json` ranks the unimplemented API by how many scripts use it.

## Rebirth (2026-10-14)

- Runs on the SOMA module (`--game rebirth`). 01_01 passes (oversized allowlist), 01_02_caves playable to the exit,
  01_04 through the rift fall and crack squeeze. Recent work: PORTING_NOTES.md `Rebirth:` sections.
- Remaining stubs in 01_02 are save/achievement/gamepad no-ops; triage in PORTING_NOTES.md (voice time left section).
- Known gaps: rift white-out, dark caves exit area, `SetLiquidAmount`, no Rebirth reference images.

## How the scripting layer is built

- `soma/data/script_api.txt`: the API recovered from `Soma_NoSteam.bin.x86_64`
  (`scripts/soma-re-script-api.py`). Everything is registered; unimplemented calls are stubs.
- Natives: `SomaLux*` (map, entities, game, player, voice), `SomaImGui`, `SomaSound`,
  `SomaScript*` (math, strings, globals), `SomaScriptGenBindings.cpp` (generated, compiler
  checked, maps script methods onto same-named HPL2 methods).
- Struct defaults (`cImGui*Data`) come from the binary's default factories
  (`scripts/soma-re-struct-defaults.py`).
- FMOD events: `.fdp` project -> waveforms -> `.fsb` samples extracted to the cache;
  `cSoundEntityManager` resolver hook in HPL2 core.

## Debugging

`player_state`, `lux_entity name=`, `script_exec code=` (AngelScript against the live API,
`__print()` returns output), `imgui_stats`, `stub_report`, `sound_stats`, `body_contacts`.
Script exceptions are logged with file:line. Memory bugs: build with ASan
(`-fsanitize=address`) into the scratchpad; writes through wrongly typed pointers (recovered
offsets on our smaller objects) are invisible to ASan, use a gdb hardware watchpoint.

One-shot sessions (a game started by one shell call dies with it): `scripts/ohpl-session.sh`
(script player), `scripts/ohpl-freecam.sh` (freecam + `hpl_control.py` commands),
`scripts/ohpl-zfight.py --map M [--pos x,y,z]` (0.4 mm camera nudge, flags pixels that flip in
albedo / accumulation / final frame; thin hits along silhouettes and alpha-test edges are noise).

## Next

See `TASKS.md`: light intensity vs the ref, audio levels (item 11), Newton broadphase,
z-fighting repro (`open-hpl-a4i`).
