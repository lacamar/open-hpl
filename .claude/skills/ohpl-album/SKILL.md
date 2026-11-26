---
name: ohpl-album
description: Capture 4K Open-HPL dev-HUD screenshots across SOMA and Amnesia TDD (and probe AMFP/Rebirth/Bunker) via headless control, save keepers to the album, and report rendering bugs. Run intermittently after engine builds/iteration.
---

# ohpl-album

Run from /home/lm/.local/src/open-hpl. Obey repo CLAUDE.md hard rules. Never modify repo files, never touch `~/.cache/open-hpl/soma-scratch*` other than `soma-scratch-album`, never `pkill -f`/`kill -9`; quit with `hpl_control quit`.

## Setup (once)
- Scratches: `scripts/setup-test-scratch.sh <steamapps/common/GAME> ~/.cache/open-hpl/<name>-scratch-album` then `scripts/deploy-test-binary.sh <binary> <scratch>`. Don't run soma-init.sh.
- Binary: use the fresh build (`amnesia/src/build/*.bin.aarch64`) when testing a build, else `/usr/libexec/open-hpl/*`.
- 4K: SOMA `<Screen Width=.. Height=..>` in `$SCR/.xdg/config/open-hpl/soma/user_settings.cfg`; Amnesia TDD `.xdg/data/open-hpl/Amnesia/Main/main_settings.cfg`; AMFP `.../Amnesia/Pig/main_settings.cfg` (created on first quit; copy from another scratch). Runtime `resize` corrupts/no-ops, set before launch.
- HUD: `OPENHPL_DEV_HUD=1` (in alb.sh).

## Helpers
`. ~/.config/claude/skills/ohpl-album/alb.sh` (defines S, H, go, sweep, ss, sv, gl, am, turn, cap, amv). The `rm`/`cp` aliases are interactive/trash: use `command cp -f ... </dev/null`.
- SOMA: `go <map>` (no .hpm), `sweep tag x y z pitch yaws...`, `ss tag`; view `album-tmp/*-sheet.png` (Read tool can't read BMP).
- Amnesia: `gl <scratch> Amnesia.bin.aarch64`, then `amv <NN_name.map> 1200` (6 views + sheet). One map per call; relaunch with `gl` after a crash. Maps: maps/main/chNN/.
- Save keepers: `sv <shot-basename> <map>-<desc>` (-> scripts/ohpl-album.sh). Keep ~20-25, skip black/empty/dark frames.

## Notes
- Check logs: `$XDG_STATE_HOME/open-hpl/<game>/.../hpl-<pid>.log`; crashes: `coredumpctl info <pid>`.
- Avoid SOMA 03_03 (GPU hang). Headless instances serialise on a flock; check `pgrep -af '\.bin\.aarch64'` first.
- AMFP, Rebirth, Bunker repo builds are Phase-0 scaffolds (no start_map, 720p, hardcoded map); only camera_state/set_camera. Retest each after engine work.
- Report rendering bugs (map, pose, symptom) in the final message.
