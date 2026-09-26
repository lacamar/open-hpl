# SOMA handover (2026-09-26)

Read with `CLAUDE.md` (commands, hard rules) and `SOMA_PLAN.md`. `TASKS.md` is the backlog.

## State

- SOMA now runs its own game scripts: `player/Player.hps` and its states drive movement and the
  camera, `base/InputHandler.hps` defines the input actions, map scripts run with timers,
  collide/interact/look-at callbacks, voice and dialog, fades, map changes and the HUD ImGui.
- Verified end to end (headless): `00_00_intro` slideshow and quote card; `00_01_apartment`
  wake-up camera animation -> answer the phone -> full voiced call with subtitles -> put the
  phone away -> get out of bed -> walk.
- `OPENHPL_SOMA_SCRIPT_PLAYER=0` falls back to the hand-written `cSomaPlayer` (and the
  hand-ported intro/phone call).
- `scripts/soma-script-check.sh`: 190/190 scripts compile; `stub_calls` in
  `soma/conformance/script_check.json` ranks the unimplemented API by how many scripts use it.

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
`__print()` returns output), `imgui_stats`. Script exceptions are logged with file:line.

## Next

See `TASKS.md` "Script layer". Biggest gaps: terminals drawn on in-world screens (3D gui sets),
the save system, main menu -> New Game via the script intro, grab/door interaction states.
