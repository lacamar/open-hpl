# Amnesia: The Dark Descent (HPL2) — arm64 Linux port — session handover

Working tree: `/home/lm/.local/src/open-hpl` (was `AmnesiaTheDarkDescent` — renamed
as part of the project's rebrand to Open HPL; git history predates the rename).
The port itself is now committed; see `git log` for the breakdown.
Target system: Fedora Asahi Remix 44, aarch64, 16K page size. Real game data (Steam,
legitimately owned by the user) lives at
`/home/lm/.steam/steam/steamapps/common/Amnesia The Dark Descent` (also reachable via
`~/.local/share/Steam/steamapps/common/...`, same dir via symlink).

## Current status: **the game loads and renders real levels successfully.**

Confirmed via screenshot: after the main-menu/profile screens, `02_entrance_hall` (the
actual first level) loads and renders correctly — lit rooms, shadows, working script
execution. This took a long debugging chain (see "Bugs fixed" below) — the last blocker was
an AngelScript compatibility issue, now fixed.

## Build / deploy / test cycle (use this exact pattern)

```sh
cd /home/lm/.local/src/open-hpl
cmake --build amnesia/src/build --target Amnesia -j$(nproc)   # or --target Launcher, or no --target for all
GAMEDIR="/home/lm/.steam/steam/steamapps/common/Amnesia The Dark Descent"
\cp -f amnesia/src/build/Amnesia.bin.aarch64 "$GAMEDIR/Amnesia.bin.aarch64"   # \cp NOT cp — see gotcha below
cd "$GAMEDIR" && rm -f hpl.log
(setsid nohup ./Amnesia.bin.aarch64 > /tmp/amnesia_play.log 2>&1 < /dev/null &)
sleep 12
pgrep -af "Amnesia.bin.aarch64" | grep -v grep   # confirm still alive
cat "$GAMEDIR/hpl.log" 2>&1                       # only exists if a warning/error was logged
grim /tmp/check.png                                # screenshot (Wayland/niri desktop); Read it to view
```

**Gotchas:**
- `cp` is shell-aliased to `cp -i` on this system and **hangs forever** waiting for a
  y/n confirmation in non-interactive shells. Always use `\cp -f` (backslash bypasses the
  alias) when overwriting the deployed binary.
- `grep` is aliased to `ugrep`, which has different regex escaping rules (fails on some
  valid POSIX patterns, e.g. certain backslash escapes). If a `grep` call errors with
  `ugrep: error: ...`, simplify the pattern or redirect to a file and use `Read`/`sed -n`
  instead of piping through grep.
- The engine resolves resource paths (config/, maps/, etc.) **relative to the binary's own
  directory** (via its `binreloc` self-path mechanism), not the working directory or argv.
  The binary must physically live inside the Steam game folder (already deployed there as
  `Amnesia.bin.aarch64`, alongside the original `Amnesia.bin.x86_64` etc.) to find its data.
- A local RPM-build workflow also exists: `~/.local/rpm/specs/amnesia-dark-descent.spec`,
  built via `mx-rpm amnesia-dark-descent --release 44 --arch aarch64` (mock-based). This
  packages engine+launcher only (no editors). Not re-verified since the later engine fixes —
  low priority to re-check unless explicitly asked.
- Unit tests: `cmake --build amnesia/src/build --target PhysicsNewtonTests -j$(nproc) &&
  ctest --test-dir amnesia/src/build --output-on-failure` — currently passing. Only covers
  the Newton physics port (see below); nothing covers scripting, rendering, or GUI.
- An ASan-instrumented build also exists at `amnesia/src/build-asan` (configured with
  `-fsanitize=address`), useful for chasing memory bugs; slower and noisier but was
  essential for finding several of the bugs below. Rebuild it the same way
  (`cmake --build amnesia/src/build-asan --target Amnesia -j$(nproc)`) if a new crash needs
  root-causing; deploy as e.g. `Amnesia.asan.aarch64` alongside the release binary so both
  can coexist in the game dir.
- The user's real save games live at `~/.frictionalgames/Amnesia/Main/N/*.sav`. Two of them
  (`AutoSave_EntranceHall_2026_4_21_12_44_53_0.sav` and `..._12_46_33_0.sav`) contain
  **pre-existing data corruption** (their stored `msMapFolder`+`msFileName` fields already
  bake in a duplicated `maps/main/` path prefix, confirmed by inspecting the raw save file
  text) — this predates the port entirely and would fail on the original x86_64 binary too.
  They're currently sitn in `/tmp/amnesia-corrupted-saves/` (moved aside, not deleted) so
  auto-load doesn't hit them. Restore them (or not) is the user's call; not a port bug.

## Bugs found and fixed this session (in the order discovered)

All of these were real, reproducible bugs — none were "fixed" by weakening behavior or
suppressing warnings; each has a comment at the fix site explaining the root cause.

1. **Newton collision cache format incompatibility** — `HPL2/core/include/resources/WorldLoaderHplMap.h`,
   `MAP_CACHE_FORMAT_VERSION` bumped 10→11. Old `.map_cache` files bake in a Newton-2.x-serialized
   collision blob; our new Newton 3.14 deserializer misreads it as a near-infinite loop. The
   version bump forces stale caches to be rebuilt from source `.map` data instead.
2. **Compound-shape double-free** — `HPL2/core/sources/impl/CollideShapeNewton.cpp`,
   `~cCollideShapeNewton()`. Destructor used to call `mpWorld->DestroyShape()` on every
   compound/scene sub-shape, but those sub-shapes are *already* independently owned by the
   world's own `mlstShapes` list (registered by their own `CreateBoxShape()`/etc. call) and
   get deleted by `iPhysicsWorld::DestroyAll()`'s unconditional `STLDeleteAll` regardless.
   Whichever ran second used a freed pointer. Fix: removed the redundant sub-shape cleanup
   loop entirely. Has a regression test: `HPL2/tests/PhysicsNewtonTests.cpp` ::
   `TestShapeCreation` (builds a compound from independently-created shapes, then destroys
   the world — used to crash).
3. **~37 classes with non-virtual destructors deleted through base pointers** — real heap
   corruption (`new-delete-type-mismatch`, confirmed via ASan), spanning engine, game, and
   the OALWrapper dependency. Nearly all fixed (either added `virtual ~Foo(){}` where none
   existed, or added `virtual` to an existing non-virtual one). **7 classes from the
   original audit could not be located** and were never confirmed fixed or even proven to
   still exist under that name: `cLuxCombineItemsCallback`, `cLuxDiary`, `cLuxEventTimer`,
   `cLuxNote`, `cLuxProp_AttachedProp`, `cLuxQuestNote`, `cLuxUseItemCallback` (used
   extensively as concrete `hplNew()`-able types and in `kBeginSerializeBase(ClassName)`
   macro invocations, but their `class ClassName {...}` text doesn't appear literally
   anywhere — likely generated via macro token-pasting that wasn't traced). A rebuild with
   `-DCMAKE_CXX_FLAGS="-Wnon-virtual-dtor -Wdelete-non-virtual-dtor"` currently shows **zero**
   `-Wdelete-non-virtual-dtor` hits, so whatever these 7 are, they're not currently being
   deleted through a base pointer anywhere the compiler can see — likely fine, but flagging
   for awareness. (One more instance, `iLuxEffect` in `amnesia/src/game/LuxEffectHandler.h`,
   was found *after* that audit via a later ASan run and fixed separately.)
4. **The actual main blocker — `char` signedness on AArch64.** Two spots used plain `char`
   as a `-1`-sentinel index/counter, relying on `char` being signed (true on x86_64, the
   platform this code was always compiled for). **Plain `char` is unsigned by default on
   AArch64.** `-1` stored as `0xff` read back as `255`, defeating `if(x < 0)` guards.
   - `HPL2/core/include/impl/VertexBufferOpenGL.h`, `mvElementArrayIndex` → now `signed char`.
     This was the one directly causing a heap-buffer-overflow crash (confirmed via ASan +
     gdb) every time a real map loaded, in `iVertexBufferOpenGL::GetElementArray()`.
   - `HPL2/core/include/scene/SubMeshEntity.h`, `mlStaticNullMatrixCount` → now `signed char`.
     Found via a source sweep for the same anti-pattern, not yet proven to cause a visible
     symptom, but same root cause.
   - **Worth a broader sweep if more weirdness turns up**: search for other plain `char`
     members/locals used as a sentinel (`-1` assigned/compared) rather than for text. The
     sweep done this session was reasonably thorough but not exhaustive (see the
     `/tmp/plain_char_decls.txt`-style approach: `grep -rnE "^\s*char [a-zA-Z_]+(\[[^]]*\])?\s*;"`
     across `HPL2/core` and `amnesia/src`, then manually check which ones are ever compared
     `< 0` / assigned `-1` vs. just plain string/byte buffers).
5. **AngelScript: default array type not registered** — `HPL2/core/sources/impl/LowLevelSystemSDL.cpp`,
   `cLowLevelSystemSDL` constructor. AngelScript 2.19.2 had a built-in `T[]` array type;
   later AngelScript versions removed it in favor of the `array<T>` addon, which must opt in
   to also serving as the default `T[]` syntax via `RegisterScriptArray(engine, true)`. This
   was previously only called inside dead/unwired code (`RegisterScriptStringUtils`, never
   invoked anywhere). Fixed by calling it directly after `RegisterScriptString(mpScriptEngine)`.
6. **AngelScript: non-const `&in`/bare `&` string parameters** — `amnesia/src/game/LuxScriptHandler.cpp`.
   ~260 registered script-callable functions (`AddFunc("...", ...)`) declared string
   parameters as `string &in` or bare `string&`, never `const string &in`. AngelScript
   2.19.2 permitted binding a string literal to such a parameter; AngelScript 2.38 correctly
   rejects it (a literal is a temporary; a non-const reference implies the callee could
   mutate it — real safety tightening, not a regression to work around). This surfaced as
   `main (LINE, COL) : ERR : Not a valid reference` at every call site in a real game script
   that passed a string literal (e.g. `PreloadSound("react_scare")`,
   `CompleteQuest("02Web", "02Web")`). Fixed via two `sed` passes making every one
   `const string &in` (spot-checked several of the underlying C++ implementations first to
   confirm none of them actually mutate the referenced string — all were read-only, so this
   is behavior-preserving). **If new scripting errors show the same "Not a valid reference"
   pattern elsewhere** (other registration files, or other reference types besides
   `string`), the fix is the same: add `const` to the AngelScript declaration string.
7. Also fixed along the way, lower-stakes: a format-string vulnerability in the launcher
   (`fl_message(str.c_str())` → `fl_message("%s", str.c_str())`, caught by Fedora's hardened
   `-Werror=format-security` during RPM build) and a hardcoded `.bin.x86_64` executable
   suffix in `HPL2/core/cmake/BoilerPlate.cmake` (now derives from `CMAKE_SYSTEM_PROCESSOR`).

## Known-corrupted / unrelated findings (not port bugs, don't "fix" these in the engine)

- The two April-21 autosaves' path corruption (see above) — user's own data, pre-existing.
- A real shutdown-race in the OALWrapper dependency
  (`HPL2/dependencies/OALWrapper/sources/OAL_Device.cpp` `cOAL_Device::Close()` /
  `cOAL_SourceManager`'s updater thread) was found via ASan: `cOAL_SourceManager::Destroy()`
  correctly calls `SDL_WaitThread()` to join its updater thread before the object is deleted,
  yet ASan still caught the updater thread reading freed memory in `IsThreadAlive()`. This
  only manifested when `exit()` was called abruptly via `FatalError()` (itself triggered by
  the corrupted-save issue above) rather than through the engine's normal orderly shutdown
  sequence. **Not root-caused** — deprioritized because it doesn't block normal play (a
  normal quit-to-desktop wasn't confirmed to hit it) and the two issues that *did* trigger it
  are both explained. Worth a look if a similar shutdown-time crash resurfaces on a *normal*
  exit path (not one going through a save-corruption-triggered `FatalError`).

## GUI scaling (done this session)

Added an integer (or fractional) GUI scale factor, config key `GuiScale` under `[Graphics]`
in `main_settings.cfg` (default `1`, matches the original unscaled UI). Set e.g.
`GuiScale="2"` to make menus/HUD/dialogs/subtitles ~2x bigger.

**Root cause investigated**: HPL2's `cGuiSet` renders each GUI set's widgets in a "virtual"
coordinate space (`mvVirtualSize`/`mvVirtualSizeOffset`) that's mapped via an orthographic
projection onto the real screen (`HPL2/core/sources/gui/GuiSet.cpp::Render()`), and mouse
input is mapped back the same way (`HPL2/core/sources/gui/Gui.cpp::SendMousePos()`). Widget
positions/sizes throughout the game (`amnesia/src/game/Lux*.cpp`) are hardcoded in absolute
virtual units, not fractions of that space. Most in-game HUD/dialog GuiSets (Journal,
Inventory, Credits, PreMenu, LoadScreen, DemoEnd, the game HUD) call `SetVirtualSize()` with
an aspect-corrected ~800x600 base (see `LuxCalcGuiSetOffset`/`LuxCalcGuiSetScreenOffset` in
`LuxBase.cpp`), which is already resolution-independent (fixed *fraction* of the screen at
any resolution). But the **Main Menu and its sub-windows** (Options, Profile, LoadGame,
StartGame, KeyConfig, CustomStory — `cGuiSet::CreateSet("MainMenu", ...)` in
`LuxMainMenu.cpp`) never call `SetVirtualSize()`, so they fall back to `cGuiSet`'s
constructor default of `mvVirtualSize = GetScreenSizeFloat()` — i.e. 1 virtual unit = 1 real
screen pixel. Combined with widgets there using literal absolute-pixel sizes (e.g. `cVector2f
`(150,25)` combo boxes, 30px-tall buttons — sized for a ~1024x768-era design resolution),
those controls shrink to a much smaller *fraction* of the screen as real resolution goes up —
this is the concrete, literal "sized for a lower resolution" bug, confirmed by inspection of
`LuxMainMenu_Options.cpp` and friends.

**Mechanism chosen** (the "less invasive" option — no widget layout code touched): `cGuiSet`
now has a static, process-wide `mfGlobalGuiScale` (`SetGlobalGuiScale()`/`GetGlobalGuiScale()`
in `HPL2/core/include/gui/GuiSet.h`). `cGuiSet::SetVirtualSize()`
(`HPL2/core/sources/gui/GuiSet.cpp`) — which the constructor's default-size assignment now
also routes through — zooms the visible virtual-coordinate window in around its own centre by
this factor: at scale N, only the central `1/N` (linear) portion of a set's original virtual
space remains visible, stretched to fill the same real screen area, so everything drawn in it
(and every mouse hit-test against it, since both `Render()`'s ortho projection and
`Gui.cpp`'s `SendMousePos()` mouse-to-virtual conversion read the same
`GetVirtualSize()`/`GetVirtualSizeOffset()`) ends up N times bigger on screen, mouse
hit-testing included — verified in-game via screenshots that clicking still lines up with
the visually-scaled widgets.

`cLuxBase::InitEngine()` (`amnesia/src/game/LuxBase.cpp`) reads the new
`mpConfigHandler->mlGuiScale` (`LuxConfigHandler.h`/`.cpp`, loaded from/saved to
`main_settings.cfg` `[Graphics] GuiScale`) and calls `cGuiSet::SetGlobalGuiScale()` with it
**before** `CreateHPLEngine()` / any PreMenu/MainMenu/HUD GuiSet gets constructed, since each
set bakes the current global scale into its own virtual-to-screen mapping at construction (or
whenever `SetVirtualSize()` is called on it) — changing the value requires a restart to take
effect, matching how most other `[Graphics]` settings in this file already behave. No in-game
options-menu control was wired up (config-file-only, per the task's stated minimum bar) —
`LuxMainMenu_Options.cpp`'s settings wiring (creation + load + save + "needs restart"
tracking) spans ~8 touch points per setting and would have required careful vertical-layout
re-flow of the whole Graphics tab; not attempted this session.

**Verified**: built via a dedicated `amnesia/src/build-guiscale` dir, deployed as
`Amnesia.guiscale.aarch64` in the Steam game dir, ran against real game data at `GuiScale="1"`
(looked identical to the unscaled game — a `cGuiPopUpMessageBox` "Continue previously played
game?" Yes/No dialog rendered tiny, as before) and at `GuiScale="2"` (the same class of dialog
— a "Load Game" list window with visibly ~2x bigger buttons/text) — screenshots confirmed the
size difference. Centered dialogs (built via `iGuiPopUp`/`GuiPopUpMessageBox`, which position
themselves at `GetVirtualSize()/2 - windowSize/2`) scale cleanly with no clipping, since they
automatically stay centered in the now-smaller virtual space.

- **Known limitation, not fixed this session**: because the scale "zooms to centre" rather
  than rescaling each widget's own position relative to its actual anchor (top-left,
  bottom-right, etc.), content positioned close to the edge of a GuiSet's virtual space can
  end up outside the now-smaller visible window and get clipped at `GuiScale` > 1. Confirmed
  via screenshot: at `GuiScale="2"`, a "Load Game" list window (title bar + top of its
  scrollbar) got clipped off the top edge of the screen, while its (centered) buttons
  remained fully visible and correctly enlarged. A true fix would need each widget to know
  its own anchor point and rescale position + size together (e.g. top-left-anchored widgets
  scaling away from the top-left corner, bottom-right-anchored ones away from bottom-right) —
  significantly more invasive, touching widget layout code throughout
  `HPL2/core/sources/gui/` and `amnesia/src/game/Lux*.cpp`, not attempted this session.
- The Launcher (`amnesia/src/launcher/`, FLTK-based) is a separate, unrelated GUI system —
  not touched, and not investigated for whether it has the same "reads as tiny" issue.

## Pending work (this turn's requests — likely being handled by sub-agents, check for their reports)

2. **Resolution picker only enumerates one monitor (`eDP-1`, the laptop panel), not `DP-1`
   (external display)** — an SDL display-mode enumeration bug. Look at
   `HPL2/core/sources/impl/LowLevelGraphicsSDL.cpp` (or wherever `SDL_GetNumDisplayModes`/
   `SDL_GetDisplayMode`/equivalent SDL2 calls happen) and the launcher's resolution list
   population (`amnesia/src/launcher/LauncherHelper.cpp`, `PopulateResolutions` — already
   touched once this session for an unrelated warning fix, worth checking there first) to
   see if it's hardcoded to display index 0 rather than enumerating
   `SDL_GetNumVideoDisplays()`.
3. **Performance on max settings is below expectations** — asked for a **low-risk**
   optimization pass (i.e. don't restructure the renderer; look for cheap wins). Candidate
   areas nobody's checked yet this session: whether the build is actually using an optimized
   (`RelWithDebInfo`/`Release`) config with proper `-O2`/`-O3` and NEON codegen rather than
   accidentally still pointing at a debug or ASan build; whether the Newton physics
   `dgVectorScalar.h` fallback (chosen earlier this session because upstream's ARM NEON
   vector backend, `dgVectorArmNeon.h`, was bit-rotted/incomplete — see the very first
   background-fork report in this conversation if you have access to it, otherwise just
   re-verify from `HPL2/dependencies/newton-dynamics/sdk/dgCore/dgVector.h`) is a meaningful
   physics bottleneck worth revisiting; V-sync/framerate cap settings; texture/shadow quality
   defaults; whether `-march=native`/`-mcpu=native` or similar is worth adding to the release
   build flags for this specific CPU. Emphasis on *low-risk* — this is a working, fragile
   port; don't destabilize it chasing performance.
4. ~~**Skip splash screens**~~ — **Done.** The splash sequence is `cLuxPreMenu`
   (`amnesia/src/game/LuxPreMenu.cpp`/`.h`), driven by `config/pre_menu.cfg` (and
   `pre_menu_mac_linux.cfg`) which lists plain JPEG logo images (`startup_fg_logo.jpg`,
   `startup_ngp_logo.jpg`, `startup_ooo_logo.jpg`, shown 2s each) — no video/Theora
   playback is involved at all for this sequence; `VideoStreamTheora` is unused anywhere
   in `amnesia/src/game`. Turned out Escape/Enter-to-skip was **already implemented** in
   stock engine code: `cLuxPreMenu::ButtonPressed()` (respects a per-section
   `mbAllowSkipping` XML attribute, default true) is already wired to
   `eLuxAction_Exit`/`eLuxAction_UIPrimary` in
   `cLuxInputHandler::UpdatePreMenuInput()` (`amnesia/src/game/LuxInputHandler.cpp`).
   The actual gap was **mouse clicks did nothing** on the plain logo sections (no
   Continue button is shown for those, so nothing consumed the click). Fixed by adding
   a mouse-click branch to `UpdatePreMenuInput()` that calls the same `ButtonPressed()`,
   gated on `cLuxPreMenu::IsContinueButtonVisible()==false` (a getter that already
   existed in the header, unused anywhere — clearly intended for exactly this) so clicks
   don't hijack sections that have a real interactive widget (Continue button / gamma
   slider) to click on instead. Verified interactively: built to
   `amnesia/src/build-splash`, deployed as `Amnesia.splash.aarch64`, used `wtype` to send
   a Return keypress mid-splash — confirmed via `grim` screenshots (before/after) that it
   instantly advanced from the "Nordic Game Program" logo to the next one. No mouse-input
   simulation tool was available in this environment (no `ydotool`/`wlrctl`/`xdotool`)
   to interactively verify the new click path end-to-end; it was confirmed by code
   review to go through the identical `mpInput->BecameTriggerd(...)` →
   `cLuxPreMenu::ButtonPressed()` path already proven live for the keyboard case, using
   action IDs (`eLuxAction_LeftClick`/`MiddleClick`/`RightClick`) already used
   successfully elsewhere in the same file.

## AMFP and SOMA reverse-engineering support (this session)

Two more Frictional/HPL2-lineage games have real Linux data available locally
(Steam): `Amnesia: A Machine for Pigs` at
`/home/lm/.local/share/Steam/steamapps/common/Machine for Pigs` and `SOMA` at
`/home/lm/.local/share/Steam/steamapps/common/SOMA`. Neither ships open
source game-logic (only Dark Descent's was ever open-sourced) - the approach
for both is the same "Phase 0/1 scaffolding" pattern already used for `soma/`
(see its `src/game/` for the shape): a minimal `hplMain()` that boots the
shared HPL2 engine against the real game's own config/resource files, loads
one hardcoded map, and drives a debug free-fly camera - no player controller,
no scripts, no menus.

**AMFP: added this session, builds and boots, and the real blocker is now
root-caused and fixed.** (Earlier claims in this doc were wrong at each
intermediate stage - see git history if the old wording is wanted: first
"screenshot-confirmed working" mistook desktop wallpaper on another
workspace for the game's render; then "confirmed severe algorithmic
perf bug, not root-caused" was itself wrong about the *cause*, though
right that something serious was happening.)

`amfp/src/game/` (wired into `amnesia/src/CMakeLists.txt` as a third sibling
target next to `game` and `soma_game`). AMFP's shipped data is genuinely
HPL2-compatible (`core/shaders/deferred_base_vtx.glsl` and the rest of the
standard HPL2 deferred-renderer `.glsl` set, unlike SOMA - see below), and
its `main_init.cfg`/`resources.cfg` are the same shape as Dark Descent's.

**Root cause, found via `gdb -p <pid>` on the actually-stuck process**
(not guessing from logs/timing - attach and read the real backtrace): it
was wedged inside Newton's `dgWorld::CreateCollisionFromSerialization`
(`dgDeserializeMarker`), deserializing physics body #1 of 178, with the
`cBinaryBuffer` read cursor already sitting at the *exact end* of the 56MB
`.map_cache` file. `cBinaryBuffer::GetData()`
(`HPL2/core/sources/resources/BinaryBuffer.cpp`) silently no-ops past EOF -
clamps the cursor, returns `false`, leaves the destination untouched -
and nothing in Newton's C serialization-callback protocol checks that
return value, so once a corrupt/truncated cache runs past its real data,
Newton just keeps "reading" stale/uninitialized memory as valid stream
data forever instead of failing. The specific `.map_cache` that triggered
this was pre-existing (not written by this session) - almost certainly
truncated by an interrupted save from an earlier crashed run. Confirmed by
deleting it: rebuilding straight from `.map` source data took **3.9
seconds**, not 20 minutes - this was never an algorithmic/performance
problem in the load path itself.

**Fix** (`HPL2/core/{include,sources}/resources/WorldLoaderHplMap.{h,cpp}`,
`MAP_CACHE_FORMAT_VERSION` bumped 11->12): the cache format now stores its
own total payload byte count in the header; `LoadCacheFile` checks that
against the actually-loaded file size *before* ever reaching Newton's
deserializer, rejecting a truncated/corrupt cache in milliseconds with a
clean error instead of hanging. Verified end-to-end: fresh build saves a
v12 cache (3.1s), reloading from that cache is fast (663ms total, 491ms of
which is the physics-shape deserialization that used to hang), and
truncating a real cache to 70% is caught immediately
(`ERROR: File '...' is truncated or corrupt (expected N bytes, file is M)!
Rebuilding from source instead.`) with a clean fallback rebuild. This is
generic `cBinaryBuffer`/`WorldLoaderHplMap` code shared by all three
games, not AMFP-specific - benefits Dark Descent and SOMA's cache loading
too.

**Still open, not attempted**: with the load hang fixed, the world does
load and `cScene::Render()` runs, but a precisely-cropped screenshot of
the actual game window (via real geometry from `niri msg windows`, not a
full-desktop capture - that mistake from earlier this session is exactly
why precision here matters) showed a flat near-black frame with real but
minimal pixel variation (min=771/65535, not a totally blank clear color) -
consistent with the hardcoded debug-camera position/rotation guessed from
the map's `InitStart` Area being wrong for this specific map (embedded in
geometry, or just facing an unlit surface), *and* with there being no
lighting/scripts running at all in this Phase 0 scaffolding (no AMFP game
logic exists to reuse - unlike Dark Descent, AMFP's own game-logic source
was never open-sourced, only its data). **Getting an actually-playable
native AMFP is a much bigger undertaking than this fix** - it needs some
equivalent of Dark Descent's `Lux*` game-logic layer (player controller,
scripts, lighting/quest logic) written from scratch by reverse-engineering
AMFP's data/script format, not ported from existing open-source code.
**Do not swap `open-hpl.spec`'s `open-hpl-machine-for-pigs` launcher away
from box64 based on this fix alone** - box64-running-the-real-binary is
currently the only way to actually *play* AMFP; this session's native
Phase 0 build only proves the shared engine can load AMFP's real data
without crashing/hanging, which is a necessary foundation, not a
replacement.

**Headless testing pattern established this session** (per explicit
request - screenshots needing precise window geometry and an unlocked
screen are unreliable/slow to depend on): launch the binary
backgrounded, then verify success *without any screenshot* via (1)
`hpl.log` for `Total:`/`Game Running`/`ERROR` lines (already-instrumented,
see `gbLogTiming` in `WorldLoaderHplMap.cpp`), (2) `/proc/<pid>/stat`
field 14+15 (utime+stime) sampled a few seconds apart to distinguish
"genuinely still working" (ticks climbing near 1:1 with wall time) from
"truly hung" (ticks flat) from "crashed" (`pgrep` comes back empty +
`coredumpctl list`), and (3) `gdb -p <pid> -batch -ex bt -ex detach` to
get a real backtrace of a stuck process in seconds rather than guessing.
This is how the actual root cause above was found - not a screenshot.
Only reach for a screenshot (and then, only a precisely-cropped one via
`niri msg windows`/`niri msg outputs` real geometry, never a raw
full-desktop capture) to confirm *visual* correctness once the log/process
evidence already says loading succeeded. Worth formalizing into a small
reusable script (e.g. `scripts/headless-check.sh`) if this project keeps
needing it - not done yet, flagging for whoever picks this up next.

**SOMA: the confirmed NULL-program crash is now fixed - the engine boots,
loads real SOMA level data, and reaches "Game Running" without
crashing.** Root cause (found earlier via `coredumpctl` + `gdb`, debug info
present, confirmed by disassembly + core registers showing
`apProgram = 0x0`): `cMaterial::GetProgram()` never gets a compiled program
for any SOMA material because **SOMA ships `.hpsl` shaders under
`core/shaders/hpsl/*.hpsl`** (`deferred_base_vtx.hpsl`,
`deferred_gbuffer_solid_frag.hpsl`, `deferred_illumination_solid_frag.hpsl`,
plus tessellation-stage shaders like `deferred_terrain_tess_cs/es.hpsl` with
no HPL2 equivalent at all) instead of HPL2's plain `.glsl` - this is HPL3's
shader format/pipeline, and HPL2 has no HPSL compiler, so every
`hpl.log` line reads `Couldn't find file 'deferred_base_vtx.glsl' in
resources`, leaving `mpCurrentProgram` NULL for every material.
`iRenderer::DrawCurrentMaterial()`
(`HPL2/core/sources/graphics/Renderer.cpp:2188`, now ~2191) used to call
`pMatType->SetupObjectSpecificData(aRenderMode, mpCurrentProgram, ...)`
unconditionally, and `cMaterialType_SolidDiffuse::SetupObjectSpecificData()`
(`HPL2/core/sources/graphics/MaterialType_BasicSolid.cpp:586`) dereferenced
that NULL program (`apProgram->SetFloat(...)`) - guaranteed SIGSEGV on any
object using `HasObjectSpecificsSettings()`.

**Fix** (`HPL2/core/sources/graphics/Renderer.cpp`,
`iRenderer::DrawCurrentMaterial()`): added `&& mpCurrentProgram` to the
existing `if(pMaterial->HasObjectSpecificsSettings(aRenderMode))` guard, so
the `SetupObjectSpecificData()` call (and by extension every material-type
implementation that dereferences the program pointer it's handed) is simply
skipped when no program was loaded, instead of crashing. This is the only
unguarded `mpCurrentProgram` use in `Renderer.cpp` - the two other
call sites (`SetupTypeSpecificData`/`SetupMaterialSpecificData`, lines
~2133/2141) are already nested inside `if(mpCurrentProgram)` at line 2126
and were never reachable with a NULL program. No other crash surfaced during
testing - see below.

Verified end-to-end this session: rebuilt `Soma` target, deployed
`Soma.bin.aarch64` into the real SOMA install, launched headlessly (per the
established pattern below - `pgrep`/`/proc/<pid>/stat` tick sampling/
`coredumpctl`, no screenshots). The process loads `00_01_apartment.hpm`
(logged: "SOMA hpm: 145 static objects, 11 primitives, 0 entities, 70
lights, 0 areas, 0 sounds"), reaches `Game Running`, and stays alive and
CPU-active (utime climbing steadily - real render-loop work, not a hang)
for 40+ seconds with zero new crashes/coredumps (`coredumpctl list` shows
only pre-existing entries from Aug 28/30, nothing from this session's runs).
No further crash of the same shape needed fixing this session - one
guard was sufficient to reach a stable running state.

**Known-remaining limitations, expected and not attempted (still no HPSL
support exists)**:
- No real rendering: every SOMA material's Deferred/FogArea GLSL shaders are
  reported missing (`Couldn't find file 'deferred_base_vtx.glsl'`/
  `deferred_light_frag.glsl`/`deferred_fog_vtx.glsl`/`deferred_fog_frag.glsl`
  in resources) and never bind, so diffuse/illumination/fog shading is
  entirely absent - this is the direct, expected consequence of the guard
  above (skip instead of crash), not a new bug.
- Many SOMA entity types have no loader in this HPL2-based Phase 0
  scaffolding (`Couldn't find loader for type 'Prop_Grab'/'Prop_Lamp'/
  'Prop_Rigid'/'StaticProp'/...` etc.) - expected, matches the documented
  Phase 0 scope (no player/scripts, map geometry only).
- SOMA still needs either an HPSL→GLSL translation layer or a materially
  HPL3-shaped renderer to ever render correctly - that remains out of scope
  (multi-week effort, explicitly excluded from this session's goal of "stop
  the crash, boot, load real data").

## General guidance for whoever picks this up

- Always verify a fix against the *real* game data (see build/deploy/test cycle above), not
  just compilation success — this codebase has repeatedly compiled cleanly while still being
  behaviorally broken (that's the whole story of bugs #4 and #6 above).
- Prefer finding and fixing real root causes over silencing symptoms — every fix above has a
  concrete, understood mechanism, not a guess.
- This is a ~15-year-old x86-only codebase; assume more latent portability bugs of the same
  general shape (integer signedness/size assumptions, endianness — unlikely to matter since
  aarch64 is LE like x86_64 — alignment, or reference/calling-convention mismatches from the
  AngelScript upgrade) may still be lurking in code paths not yet exercised (most of the game
  has not been played through yet — only the very start of the first level is confirmed
  working).

## Next session priorities (handover, this session ending on token limit)

1. ~~**AMFP: fix root cause #2**~~ — **FIXED (2026-09-01), and it was a real bug - an
   earlier pass in this doc wrongly called it "intentional", which was wrong; corrected here.**
   Root cause: `cLuxPlayer::PlaceAtStartNode()` (`amnesia/src/game/LuxPlayer.cpp`, called by
   `TeleportPlayer()`) sets the character's position via `SetFeetPosition()` but never clears
   its existing velocity/move-speed. `maps/01_mansion_01.hps`'s `OnStart()` calls
   `TeleportPlayer("temple_intro_1")` as its very first action, before the character has ever
   had a safe initial position - by that point it had already been falling (from whatever
   default/uninitialized spot the engine placed it at on map load) long enough to hit terminal
   fall velocity (`mvVelocity.y = -30`, the engine's `mfMaxGravitySpeed` cap - confirmed via
   live `gdb -p <pid>` sampling from the first moment the player object existed). Teleporting
   to `temple_intro_1` (and later `MainStart`) carried that -30 velocity straight through both
   real, solid floors - confirmed via `gdb` that both floors are genuinely collidable when the
   character is placed there with velocity actually at zero. **Fix**: added
   `mpCharBody->StopMovement()` (`HPL2/core/sources/physics/CharacterBody.cpp` - an existing,
   already-used-elsewhere method that zeroes velocity/move-speed/acceleration state, e.g. used
   on ladder-exit and enemy-state-reset, just never wired into player teleportation) right
   before `SetFeetPosition()` in `PlaceAtStartNode()`. This is a general engine bugfix, not
   AMFP-specific - any teleport with residual motion (in either game) could have hit this;
   Dark Descent just never happened to exercise a teleport-while-falling until now. **Verified**:
   after the fix, sampling position/velocity across the real `TeleportPlayer("MainStart")` call
   (triggered via gdb by calling the exact same functions the in-game "New Game" confirmation
   popup calls - `gpBase->mpMainMenu->ExitMenu(eLuxMainMenuExit_StartGame)` etc, since no mouse
   input simulation tool exists in this environment) shows velocity `(0,0,0)` both immediately
   before and immediately after the teleport, with the player resting solid and stationary for
   6+ seconds post-teleport. Dark Descent re-verified unaffected (stable on ground,
   `mlOnGroundCount` 11-12, no crash) with the same fix in the shared binary.
2. **SOMA: real rendering needs an HPSL→GLSL transpiler** — scoped this session, not
   started. HPSL (`core/shaders/hpsl/*.hpsl`) is a genuine distinct shading language, not a
   GLSL variant: custom preprocessor (`@ifdef`/`@include`/`@else`/`@endif`), custom types
   (`cVector4f`, `cTexture2D`, `cTextureBuffer`), and shader I/O passed as explicit `main()`
   parameters instead of GLSL's global `in`/`out` declarations, plus a "constant buffer
   chosen by MaterialType" indirection layer not yet investigated. 75 `.hpsl` files total.
   This is realistically its own multi-session project — start by writing the `@ifdef`/
   `@include` preprocessor and the `main()`-parameter→GLSL-global rewrite for ONE simple
   shader (`clear_frag.hpsl`/`clear_vtx.hpsl` look like the smallest files, good first
   target) before attempting anything material-system-wide. SOMA currently boots and runs
   stably without crashing (fixed this session, commit `7e426e4`) but renders no real
   materials — that's the honest current ceiling until the transpiler exists.
3. Both AMFP and SOMA changes so far are in `amfp/src/game/`, `soma/src/game/`, and shared
   `HPL2/core/` — none of it ships in the `open-hpl` RPM except the experimental
   `open-hpl-machine-for-pigs` entry (see spec changelog 1.3.1-5). Re-run the release chain
   (commit → tag → `mx-rpm --copr arm64-misc -i`) after any further fixes, same pattern used
   throughout this session.

## "Attic staircase" physics/trigger bug — real location found in AMFP, NOT yet reproduced live

A user report described "a physics/event trigger bug with the attic staircase." First pass
this session wrongly ruled this out — see below for the correction. **Dark Descent has no
attic** (confirmed: no map/area named "attic" anywhere in `.../maps/main/ch01/ch02/ch03`'s
`.hps` scripts or compiled `.map` binary strings; `custom_stories/` is empty). The attic is
real, just in **AMFP** (`Amnesia: A Machine for Pigs`, `/home/lm/.steam/steam/steamapps/
common/Machine for Pigs`), confirmed by file search: `maps/02_mansion_02_child_mansion_
attic.nodes`, `entities/door/attic_floor_ladderhatch/` (a ladder hatch, not a conventional
staircase — the user may be describing this loosely), `static_objects/mansionbase/stairs/
stairs_attic_01/02/03`, matching ambience/sound assets (`attic_creak`, `attichatch_child`,
etc.). Also correcting an earlier claim in this file: **AMFP is not "Phase 0 only, not
playable"** — see commit `b2f51f4`, which found that Dark Descent's own real compiled
game-logic binary (`amnesia/src/game/`, not the separate `amfp/src/game/` free-fly
scaffold) can run AMFP's real level scripts and real player controller natively against
AMFP's actual game data, via two small AngelScript API-compat shims. `amfp/src/game/` (the
free-fly scaffold) genuinely has no player/scripts; the real `amnesia/src/game/` binary
does, when pointed at AMFP's data directory.

**The real attic sequence, found by grepping `maps/01_mansion_01.hps`** (not
`02_mansion_02.hps`, despite the nodes-file name above — that file's `.hps` has no "attic"
string at all; the actual attic hatch/ladder script lives in the first map):

- Two hatch prop entities, `attic_floor_ladderhatch_1` and `attic_floor_ladderhatch_2`.
- `AddEntityCollideCallback("attic_floor_ladderhatch_2", "PhysicsSimulationTrigger",
  "AtticHatchPhysicsSimulation", true, 1)` (line 94) — note the *parent* entity here is the
  hatch prop itself, not `"Player"`, unlike nearly every other callback in this file: this
  fires when the physics-simulated hatch prop collides with an area, not when the player
  does.
- `AtticHatchPhysicsSimulation()` (~line 1520): `SetEntityInteractionDisabled
  ("attic_floor_ladderhatch_1", true)`, `SetPropStaticPhysics("attic_floor_ladderhatch_2",
  true)`, then `CheckPoint("CP02_Attic", "CheckpointStart_1", ...)`. This is a
  dynamic-prop-settles-then-gets-pinned-static pattern — the hatch starts as a real physics
  body (so it can fall/swing open) and gets frozen static once it's done moving, driven by a
  collide-trigger on the prop itself.
- `SetEntityPlayerInteractCallback("attic_floor_ladderhatch_1", "DeactivateAtticHatchBlocker",
  true)` (line 145) and `AddEntityCollideCallback("attic_floor_ladderhatch_2",
  "LadderSoundCollision", "Ladder_Floor_Hit", true, 1)` (line 132) are two more
  collide/interact hooks on the same two hatch entities.
- Related but **distinct** — do not conflate: `02_mansion_02.hps` has its own real
  scripted collapsing-staircase event, `StairBreakEvent()`/`StairBreakEventTimerCalls()`
  (~line 1899), triggered by `AddEntityCollideCallback("Player", "StairBreakTrigger",
  "StairBreakEvent", true, 1)`. This is a "Transition Room" pipe/stair collapse (comment:
  "Transition Room Stair Sequence"), not the child's attic — a different area entirely, but
  worth knowing about since it's the *other* strong "stairs" hit in AMFP's data and a future
  session could easily conflate the two given both match on "stair".

**Leading hypothesis, not yet confirmed live**: this is very plausibly the same bug class
already documented in commit `b2f51f4` — "collision breaks specifically while moving,
likely a discrete (non-swept) per-frame collision step failing at seams between the ~20+
separate physics bodies AMFP's floor mesh got batched into." The attic hatch sequence above
is exactly the kind of place this would surface: a dynamic-to-static physics-prop
transition, driven by a prop-vs-area collide trigger, on/near a floor that's plausibly
built from the same kind of multi-body batched mesh. Not confirmed by live testing this
session (see below for why) — this is a strong, evidence-based lead for whoever picks this
up next, not a guess pulled from nothing, but still unverified.

**Attempted live reproduction this session, blocked by the environment, not by the game
logic**: built `amnesia/src/game`'s `Amnesia` target fresh in a dedicated worktree build
dir, deployed as a uniquely-suffixed test binary into the real AMFP install (mirroring the
already-deployed `AmnesiaOnAmfp.bin.aarch64` from the `b2f51f4` session), launched
headlessly. **The desktop screen was locked for this entire session** (password prompt
visible on screenshot) — per this project's own established testing pattern, screenshots
and any input-simulation are unreliable/unusable while locked, so all interaction here was
via `gdb -p <pid>` state inspection/manipulation only, no `wtype`/mouse/keyboard input sent
anywhere near the lock screen.

Sequence of real findings while trying to reach the attic checkpoint (`"CheckpointStart_1"`,
the start-node named in `AtticHatchPhysicsSimulation`'s `CheckPoint()` call, and the correct
target to `TeleportPlayer`/`ChangeMap` to once in-game):
1. The engine's main loop blocks in `cEngine::CheckIfAppInFocusElseWait()`
   (`HPL2/core/sources/engine/Engine.cpp:826`) whenever the window lacks real input focus
   (true for any locked-screen/backgrounded window) — an internal `while(GetWindowInputFocus
   ()==false)` loop, immune to flipping the outer `mbWaitIfAppOutOfFocus` gate via `gdb` once
   already inside it. Worked around via `gdb`'s `frame`/`return` commands to force that one
   call to return early (safe — it's a pure poll loop, no side effects to unwind) — this
   let the real engine loop run again.
2. `gpBase->mpMainMenu->ExitMenu(eLuxMainMenuExit_StartGame)` (called via `gdb`, same
   technique as the teleport-velocity fix's "New Game" verification elsewhere in this file)
   does trigger a real transition (confirmed: `mbExiting`/`mfMenuFadeAlpha` observed
   settling back to their idle values after the fade completes) but calls
   `cLuxBase::StartGame("","","")` with **empty arguments**, which falls through to Dark
   Descent's own default start-map config (`mpUserConfig`/`msStartMapFile`), not AMFP's
   `01_mansion_01` — the vanilla "New Game" menu path is the wrong tool for pointing this
   shared binary at a specific AMFP map/checkpoint.
3. Called `cLuxBase::StartGame("01_mansion_01", "", "CheckpointStart_1")` directly via
   `gdb` instead (constructing the `std::string` arguments by reusing already-live
   `std::string` lvalues in the inferior — `mMapChangeData.msMapFile`/`.msStartPos`/
   `.msSound`, `.assign()`-ed to the right values first — since `gdb` cannot construct a
   temporary `std::string` from a C string literal as a call argument on this build; a
   plain `set variable someStdString = "text"` also fails ("Invalid cast") but
   `someStdString.assign("text")` works reliably as a `gdb call`). This call **blocked for
   ~25+ minutes** at extremely low CPU (order of 2 CPU-seconds of real work per 10 minutes
   of wall time — not a tight spin, but not meaningful progress either).
4. Interrupted the stuck `gdb call` with `SIGINT` (safe: `gdb` abandons inferior calls
   cleanly on `^C`, printing "the function is done executing, GDB will silently stop", and
   detaches without corrupting the inferior). A fresh, non-`call`-blocking `gdb -p ... -ex
   bt -ex detach` attach then showed the **real** stall point: still inside
   `cLuxBase::StartGame()`, at its very first line, `mpLoadScreenHandler->
   DrawMenuScreen()` (`LuxBase.cpp:551`) → `Wayland_GLES_SwapWindow` →
   `wl_display_read_events` → blocked in `pthread_cond_wait`, i.e. **the loading-screen's
   buffer swap is waiting on a Wayland frame-done callback that the compositor never sends
   for a window it isn't compositing (obscured behind the lock screen).**

**This is a genuinely new, useful finding for this project's testing methodology, distinct
from the attic bug itself**: this file already knew screenshots were unreliable while
locked; what wasn't previously documented is that this is much more fundamental — *any*
code path that calls a real buffer-swap/present (loading screens, and presumably the normal
render loop's own swap once actually in-game) can block **indefinitely** while the screen
is locked, not just degrade or skip a frame. This fully explains why live reproduction
could not be completed this session, independent of anything about the attic hatch bug
itself. Killed the hung test process and removed the test binary afterward (`kill`, no
`--force`, no data loss — this was a scratch test binary, not the canonical deployed one).

**Next session**: retry the exact same repro path (steps above; the `StartGame("01_
mansion_01", "", "CheckpointStart_1")` `gdb` call is the fast-forward to the checkpoint,
already worked out) **with the screen unlocked** — the whole chain up to the
`DrawMenuScreen()` swap-wait was working correctly, this was purely an environment
precondition, not a bug to fix. Once at the checkpoint, sample `mpCharBody`'s
position/velocity across a few real frames the way the teleport-velocity fix did, focusing
on whatever happens right as `AtticHatchPhysicsSimulation()`'s `SetPropStaticPhysics` call
fires (that dynamic→static transition is the most likely single moment for a
seam/discontinuity bug to manifest, per the `b2f51f4` hypothesis above).
