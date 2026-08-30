Open HPL
========

An aarch64 Linux port of the HPL2 engine and the *Amnesia: The Dark Descent*
game source, as open-sourced by [Frictional Games](https://www.frictionalgames.com/)
under the GNU GPLv3. This is an unofficial community fork/port — it is not
affiliated with, endorsed by, or supported by Frictional Games, and "Amnesia:
The Dark Descent" remains their trademark.

No game data (maps, textures, audio, scripts) is included here. Frictional
Games only open-sourced the engine and game *code* — the game's data files
are still commercial, copyrighted content. You need a legitimate copy of the
game (e.g. via Steam or GOG) to actually play; this repo only gets you a
working aarch64 binary to point at that data.

What changed from upstream
---------------------------
The original codebase only ever targeted 32-bit x86 Linux, Windows, and
PowerPC-era macOS. This port:

- Replaces the bundled 32-bit x86 prebuilt third-party libraries with native
  aarch64 system packages (SDL2, DevIL, GLEW, libtheora, libvorbis/ogg,
  OpenAL, libjpeg-turbo, AngelScript 2.38).
- Vendors and builds [Newton Dynamics 3.14](https://github.com/JulioJerez/newton-dynamics)
  from source for the physics bindings — the original Newton 2.00 the engine
  shipped with is unrecoverable upstream (no 1.x/2.x history exists in the
  current canonical repo).
- Upgrades the AngelScript scripting integration from the vendored 2.19.2 to
  system AngelScript 2.38, which has proper AArch64 calling-convention
  support.
- Removes the proprietary, x86-only FBX SDK mesh loader entirely (unused at
  runtime — shipped content uses Collada/`.msh`, not FBX).
- Fixes a long tail of real portability and correctness bugs found by
  actually running the port against real game data — see
  [PORTING_NOTES.md](PORTING_NOTES.md) for the full list (a Newton
  collision-cache format bump, a compound-shape double-free, dozens of
  non-virtual-destructor bugs, `char`-signedness bugs on AArch64, AngelScript
  2.38's stricter const-reference checking, and more).

Scope is the engine, game, and launcher. The FLTK-based content-creation
tools (level/model/material/particle editors) are not built by default.

Building
--------
CMake project files for Linux and macOS are included (`amnesia/src/`); a
Newton Dynamics build (`HPL2/dependencies/newton-dynamics/`) must be built
first and its static libs placed under
`HPL2/dependencies/lib/linux/lib/` — see the RPM spec's `%build` section for
the exact invocation, or `PORTING_NOTES.md` for a local dev build/deploy/test
cycle. A regression test for the Newton port lives at
`HPL2/tests/PhysicsNewtonTests.cpp`.

Contributing Code
-----------------
We encourage everyone to contribute code to this project — sign up for a
GitHub account, fork, and hack away at the codebase.

License Information
--------------------
All code in this repository is licensed under the GNU GPLv3 unless noted
otherwise. See [LICENSE](LICENSE) for the full text, and
[THIRD_PARTY_LICENSES.md](THIRD_PARTY_LICENSES.md) for the licenses of
vendored third-party code (Newton Dynamics, the AngelScript SDK addons, and
OALWrapper) that ship inside this repository under their own, GPL-compatible
terms.
