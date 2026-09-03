Open HPL
========

An aarch64 Linux port of the HPL2 engine and *Amnesia: The Dark Descent*,
open-sourced by [Frictional Games](https://www.frictionalgames.com/) under
the GPLv3. Unofficial, not affiliated with or endorsed by Frictional Games.

Game data (maps, textures, audio) is not included — you need a legitimate
copy of the game to actually play.

What changed from upstream
---------------------------
The original codebase only targeted 32-bit x86 Linux, Windows, and
PowerPC-era macOS. This port:

- Replaces bundled 32-bit x86 third-party libraries with native aarch64
  system packages.
- Ports the Newton Dynamics physics bindings to Newton 3.14 (the original
  2.x line has no recoverable upstream history).
- Upgrades AngelScript to 2.38 for aarch64 calling-convention support.
- Drops the x86-only FBX mesh loader (unused — shipped content uses
  Collada/`.msh`).
- Fixes assorted portability/correctness bugs found while running the port
  against real game data — see [PORTING_NOTES.md](PORTING_NOTES.md).

Scope is the engine, game, and launcher. The FLTK-based content-creation
tools are not built by default.

Building
--------
CMake project files are in `amnesia/src/`. Newton Dynamics
(`HPL2/dependencies/newton-dynamics/`) must be built first, with its
static libs placed under `HPL2/dependencies/lib/linux/lib/` — see the RPM
spec's `%build` section for the exact steps.

License
-------
GPLv3 unless noted otherwise — see [LICENSE](LICENSE) and
[THIRD_PARTY_LICENSES.md](THIRD_PARTY_LICENSES.md) for vendored
third-party code.
