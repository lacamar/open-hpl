# Amnesia: The Dark Descent (HPL2) — arm64 Linux port — session handover

Working tree: `/home/lm/.local/src/AmnesiaTheDarkDescent` (uncommitted changes throughout —
nothing has been git-committed during this port; `git status`/`git diff` show everything).
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
cd /home/lm/.local/src/AmnesiaTheDarkDescent
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
