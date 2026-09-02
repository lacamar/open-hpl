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
effect, matching how most other `[Graphics]` settings in this file already behave.

**In-game Options control (later session)**: added a "UI Scale" combo box (1x/2x/3x/4x —
integer only, matching `mlGuiScale`'s `int` storage) to the Graphics tab's Screen group in
`amnesia/src/game/LuxMainMenu_Options.cpp`/`.h`, following the exact wiring pattern already
used by `mpCBTextureSizeLevel` (a restart-required combo box): `SetUpInput(..., true, ...)`,
population + selection in `SetInputValues()`, snapshot round-trip in
`DumpInitialValues()`/`DumpCurrentValues()`, and the actual config write in `ApplyChanges()`
(`pCfgHdr->mlGuiScale = ...`). The Screen group's box grew from height 70 to 115 to fit the
new row below Fullscreen/VSync; focus-nav chain updated (`VSync` → `GuiScale` →
`TextureSizeLevel`, was `VSync` → `TextureSizeLevel` directly).

Two new translation keys are needed for the label/tooltip to actually render text —
`OptionsMenu/GuiScale` ("UI Scale") and `OptionsMenu/GuiScaleTip` — in each `config/base_*.lang`
file (game-shipped data, not part of this repo, so not added here; missing keys just log a
`cLanguageFile::Translate()` warning and render blank text, no crash — confirmed by reading
that function's fallback path). Whoever ships this should add those two entries alongside the
existing `TexQuality`/`TexQualityTip` pair as a template.

**Verified this pass**: built (`amnesia/src/build-guiscale2`), deployed as
`Amnesia.guiscale2.aarch64`, booted successfully against real game data with no warnings in
`hpl.log` and the process stable while running. Could not interactively click through to the
Graphics tab this pass — the desktop session was screen-locked for the whole test window (no
input simulation tool is available in this environment regardless, per the existing "Skip
splash screens" note) — so the new control's correctness rests on code review against the
proven `TextureSizeLevel` pattern, not a live screenshot. Re-verify visually (select each of
1x/2x/3x/4x, confirm it persists to `main_settings.cfg`'s `[Graphics] GuiScale` and that a
restart actually changes UI size, same as the original `GuiScale="1"`/`"2"` screenshots above)
next time an interactive desktop session is available.

**Verified**: built via a dedicated `amnesia/src/build-guiscale` dir, deployed as
`Amnesia.guiscale.aarch64` in the Steam game dir, ran against real game data at `GuiScale="1"`
(looked identical to the unscaled game — a `cGuiPopUpMessageBox` "Continue previously played
game?" Yes/No dialog rendered tiny, as before) and at `GuiScale="2"` (the same class of dialog
— a "Load Game" list window with visibly ~2x bigger buttons/text) — screenshots confirmed the
size difference. Centered dialogs (built via `iGuiPopUp`/`GuiPopUpMessageBox`, which position
themselves at `GetVirtualSize()/2 - windowSize/2`) scale cleanly with no clipping, since they
automatically stay centered in the now-smaller virtual space.

- The Launcher (`amnesia/src/launcher/`, FLTK-based) is a separate, unrelated GUI system —
  not touched, and not investigated for whether it has the same "reads as tiny" issue.

### Follow-up: clipping at `GuiScale > 1` actually fixed (later session)

User report: at `GuiScale="2"`, "lots of elements are offscreen so I can't interact with
menus", the splash screen was offscreen, and splash still couldn't be skipped by click or
key. Root-caused and fixed all three, without the full per-widget-anchor rewrite the note
above assumed was required:

1. **`cGuiSet::SetVirtualSize()`** (`HPL2/core/sources/gui/GuiSet.cpp`) zoomed around the
   *centre* of a set's full `avSize`. Since MainMenu-category sets default their virtual size
   to the raw screen size and most game code positions widgets from `(0,0)`, centre-zoom
   pushed the *majority* of a set's content outside the shrunk visible window - not just
   edge-adjacent content as the original note assumed. Changed the anchor to `avOffset` (the
   set's own origin - `(0,0)` for sets like MainMenu that don't aspect-correct, or whatever
   `LuxCalcGuiSetScreenOffset` already centered for sets that do) instead of the screen's
   geometric centre. `GetVirtualSize()/2`-based centering (popups, `CenterGlobalPositionInSet`)
   is unaffected either way, since that math is expressed in the already-scaled
   `GetVirtualSize()` and doesn't depend on where the zoom is anchored.
2. **`cLuxMainMenu`** (`amnesia/src/game/LuxMainMenu.cpp`) computed its top-level layout
   (button list, logo) as `menu.cfg`'s `*RelativePos`/`*RelativeSize` fractions multiplied by
   the raw screen size (`mvScreenSize`), then fed the result into the same (now scaled)
   coordinate space as everything else - so e.g. the button list's `TopMenuStartRelativePos`
   of `(0.8, 0.325)` landed at 80% across the *original* screen, which at `GuiScale="2"` is
   already past the shrunk visible window's right edge, regardless of anchor choice. Changed
   these calculations to multiply by `mpGuiSet->GetVirtualSize()` (the current, already-scaled
   space) instead, so the fraction is always relative to what's actually visible. Its
   full-screen background/overlay `DrawGfx` calls (captured game screenshot + blur behind the
   in-game pause menu, black fade, top-menu backdrop) had the same bug in reverse - sized via
   raw `mvScreenSize` instead of `GetVirtualSize()`, so at `GuiScale > 1` they only covered
   their own origin corner of the visible window instead of all of it. Symptom, live: pausing
   in-game showed the frozen background "zoomed into just the top-left corner". Fixed the same
   way, size parameter switched to `GetVirtualSize()`.
3. **`iWidget::CenterGlobalPositionInSet()`** (`HPL2/core/sources/gui/Widget.cpp`) and
   `cGuiPopUpMessageBox`'s equivalent inline centering (`HPL2/core/sources/gui/
   GuiPopUpMessageBox.cpp`) computed `GetVirtualSize()/2 - windowSize/2` with no floor - a
   window bigger than the (scale-shrunk) visible canvas in either dimension centers into
   *negative* coordinates, overflowing past the top/left edge instead of just the
   bottom/right. This is what the original note's "Load Game clipped off the top" screenshot
   actually was, not edge-of-canvas positioning - Load Game's fixed pixel size doesn't fit
   inside a `GuiScale="2"`-shrunk canvas whose scale was derived from real screen resolution,
   not the layout's own design resolution. Clamped both to `Max(0, ...)` in each axis, so an
   oversized window anchors to the set's origin (still fully reachable) instead of spilling
   off both edges. Confirmed live: the Profiles window (same oversized-dialog situation as
   Load Game, encountered via the first-run profile-creation flow) went from partially
   off-screen (`Delete`/`Create` buttons cut off at the left edge) to fully visible and
   correctly centered.
4. **Splash/pre-menu** (`amnesia/src/game/LuxPreMenu.cpp`): added an `abIgnoreGlobalScale`
   parameter to `cGuiSet::SetVirtualSize()` (defaults `false`, every other call site
   unaffected) and set it for PreMenu's call specifically - a full-bleed splash/logo image
   already fills the screen at scale 1 and has nothing to gain from `GuiScale`, only edges to
   lose to it. Confirmed fix #1-3 above didn't independently need this (they fix
   *any*-size-canvas clipping generally), but leaving splash unscaled is strictly simpler and
   guarantees it never regresses no matter what else changes here.
5. **Splash skip** (click/key): re-tested live against current `HEAD` (not just reviewed) -
   `eLuxAction_Exit`/`UIPrimary` keyboard skip (`cLuxInputHandler::UpdatePreMenuInput()`,
   `amnesia/src/game/LuxInputHandler.cpp`) fired correctly via `wtype`, advancing straight
   through the splash sequence to the Profiles window in one keypress. The behavior the user
   originally hit was against a stale installed binary predating this session's fixes, not a
   regression in the skip logic itself - once the *fixed* binary was actually running, skip
   worked as already documented above. Mouse-click skip still not independently verified live
   (no click-simulation tool in this environment - see the note above) - only keyboard was
   confirmed this pass.

**Verified live** (real Steam game data, `amnesia/src/build`, deployed over the Steam-side
`Amnesia.bin.aarch64`, `GuiScale="2"` from the user's own `main_settings.cfg`): splash logos
render centered and unclipped; a single keypress skips through them; the Profiles dialog is
fully visible and centered; user confirmed the main menu button list and in-game pause view
both look correct after these fixes.

**Not fixed / still a known limitation**: a dialog wider or taller than the shrunk visible
canvas in either dimension (i.e. bigger than `screen_size / GuiScale`) still can't fully fit
on screen - the clamp (#3) keeps it *reachable* (anchored on-screen at the origin) but its
far edge still gets cut off by the viewport. This becomes likelier at `GuiScale="3"`/`"4"` or
on lower real resolutions. The genuinely complete fix would need each widget to know its own
anchor point and rescale position + size together (e.g. top-left-anchored widgets scaling
away from the top-left corner, bottom-right-anchored ones away from bottom-right) -
significantly more invasive, touching widget layout code throughout
`HPL2/core/sources/gui/` and `amnesia/src/game/Lux*.cpp` - not attempted here either.

## Pending work (this turn's requests — likely being handled by sub-agents, check for their reports)

2. ~~**Resolution picker only enumerates one monitor (`eDP-1`, the laptop panel), not `DP-1`
   (external display)**~~ — **Done, in two places.** `cPlatform::GetAvailableVideoModes`
   (`HPL2/core/sources/impl/PlatformSDL.cpp`) itself was never the problem — it already loops
   over `SDL_GetNumVideoDisplays()` and tags every `cVideoMode` with its real `mlDisplay`. The
   bug was in the two *consumers* of that list, both dropping/merging entries that share a
   `mvScreenSize` across displays (very common — e.g. a laptop panel and an external monitor
   both supporting 1920x1080):
   - **Launcher** (`amnesia/src/launcher/LauncherHelper.cpp::PopulateResolutions`, FLTK-based):
     `Fl_Menu_::add()` treats identical label text as the *same* menu entry, so two displays'
     same-size modes collapsed into one dropdown item bound to whichever display was enumerated
     last. Fixed by appending `" (<display name>)"` to every label whenever more than one
     display is present.
   - **In-game Options menu** (`amnesia/src/game/LuxMainMenu_Options.cpp`, the "Resolution"
     block, ~line 1096): a hand-rolled "remove duplicates" pass that merges adjacent
     same-`mvScreenSize` entries in the (display-then-size sorted) `vVidModes` vector to
     collapse refresh-rate variants — but it never checked `mlDisplay`, so at every
     display-boundary crossing where two displays shared a resolution, one display's entry got
     silently merged away and lost, exactly like the Launcher bug. There's a telling
     commented-out block right after it (`// Since the same resolution on display 0 will have
     the same text as display 1, this won't work`) — the original Frictional dev noticed the
     label-collision symptom but left the flawed size-only dedup untouched. Fixed the same way:
     the dedup loop now also requires matching `mlDisplay` before merging two entries, and
     labels get a `" (<display name>)"` suffix whenever more than one display is present.
   Verified: built clean (`amnesia/src/build-resfix2`, deployed as a uniquely-named
   `Amnesia.resfix2.aarch64` test binary, never overwriting the canonical one), boots and runs
   stably headlessly with no new `hpl.log` warnings. This dev machine only had `eDP-1` active
   at fix time (`DP-1`, the LG 4K external, was present in `niri msg outputs` but disabled) —
   **no live two-display repro was available**; correctness was confirmed by hand-tracing the
   dedup loop against a two-display example (both listing 1920x1080) and by symmetry with the
   already-verified Launcher fix. Worth a real screenshot check next time a second monitor is
   actually plugged in.
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
2. **SOMA: HPSL→GLSL transpiler** — **proof-of-concept done (2026-09-01), real work
   remains.** Scoping correction from the previous pass of this doc: HPSL's `@ifdef`/
   `@else`/`@endif`/`@include`/`@define` directives are **not** a custom preprocessor -
   they're the *exact same* directive set implemented by HPL2's own
   `cPreprocessParser` (`HPL2/core/include/system/PreprocessParser.h`), already used to
   preprocess HPL2's own hand-written GLSL shaders (see the `@ifdef UseDiffuse` etc.
   blocks in a real install's `core/shaders/deferred_base_frag.glsl` - same syntax,
   same engine class, wired in at `cGpuShaderManager::CreateShader()`,
   `HPL2/core/sources/resources/GpuShaderManager.cpp:132`). Nothing needed writing
   there - just reuse it. That leaves a narrower, syntax-only gap: HPSL's
   `cVectorNf`/`cTextureX` type names and its `main()`-parameter I/O convention, vs.
   GLSL 120's `vec4`/`sampler2D`/global `attribute`/`varying`/`gl_FragData[N]`
   convention (confirmed by inspecting a real install's `deferred_base_vtx.glsl`/
   `deferred_base_frag.glsl`, which use `gl_Vertex`/`gl_Color`/`gl_MultiTexCoord0`
   fixed-function built-ins and `varying`, GLSL 120 + `#extension GL_ARB_draw_buffers`
   for MRT output - not custom attributes).

   Added `soma/src/game/HpslTranspiler.{h,cpp}`: a best-effort, regex/string-based
   (not a real parser) transpiler doing exactly that rewrite, plus
   `soma/src/game/HpslTranspilerSelfTest.{h,cpp}`, a one-shot self-test wired into
   `cSomaBase::Init()` (right after `InitEngine()`) that loads the real
   `clear_vtx.hpsl`/`clear_frag.hpsl` from a real SOMA install via the engine's own
   file searcher, runs them through `cPreprocessParser::Parse()` (the real, shared
   preprocessing step) then `TranspileHpslToGlsl()`, and attempts a **real GL compile**
   via `iGpuShader::CreateFromString()` against the live GL context - not a syntax
   check, an actual `glCompileShader()` call. **Verified**: built to
   `amnesia/src/build-hpsl`, deployed as `Soma.hpsltest.aarch64` into the real SOMA
   install, ran headlessly; `hpl.log` shows both
   `HpslTranspilerSelfTest: 'clear_vtx.hpsl' PASSED (compiled as real GLSL)` and the
   same for `clear_frag.hpsl`, `overall result: PASS`. Caught and fixed one real bug
   in the process: the transpiler's header-prefix slice (`sSrc.substr(0, lMainPos)`,
   text before the literal string `"main"`) left the `void` return-type keyword of
   HPSL's `void main(...)` dangling in front of the generated `varying` declarations
   (`void varying vec4 px_vColor;` - a genuine GLSL syntax error, `0:13(6): error:
   syntax error, unexpected VARYING`), caught by the real-compile self-test exactly as
   intended; fixed by trimming a trailing `void` token off the header-prefix slice.

   **Known limitations, not fixed**: the vertex-input semantic name → GLSL built-in
   table (`vtx_vPosition`→`gl_Vertex`, `vtx_vColor`→`gl_Color`, plus three more
   guessed-by-analogy entries) has exactly two verified entries - only what
   `clear_vtx.hpsl` exercises; no `cTextureBuffer`/constant-buffer support at all
   (unexplored - the `HPL2/core/include/system/PreprocessParser.h` comment about a
   "constant buffer chosen by MaterialType" indirection layer, referenced in the
   previous pass of this doc, is still uninvestigated); assumes a vtx/frag pair uses
   matching parameter names for interpolated values (true for `clear_vtx`/`clear_frag`,
   unverified elsewhere); simple regex/brace-counting, not a real HPSL grammar, so it
   will not survive constructs the two clear-pass shaders don't use. It is also
   **not wired into the real shader-loading path**
   (`cGpuShaderManager::CreateShader()`) at all - `RunHpslTranspilerSelfTest()` is a
   standalone diagnostic, deliberately kept off the shared HPL2/core code path other
   in-flight work touches this session.

   **Next step for whoever continues this**: try transpiling
   `deferred_base_vtx.hpsl`/`deferred_base_frag.hpsl` next (the shaders actually
   blocking real rendering, per the `Couldn't find file 'deferred_base_vtx.glsl'`
   errors below) rather than another toy shader - expect it to fail on unmapped
   vertex builtins (`gl_Normal`/`gl_MultiTexCoord1` equivalents beyond the five
   guessed here) and possibly on `@ifdef`-combo interaction with the transpiler (the
   combo path in `ProgramComboManager.cpp` calls `cGpuShaderManager::CreateShader()`
   *with* a variable container - the self-test here always passes an *empty* one,
   untested against real `@ifdef UseDiffuse`-style combo variables). Once a
   representative real material's pair transpiles and compiles, wire the fallback
   into `cGpuShaderManager::CreateShader()`: when `mpFileSearcher->GetFilePath()`
   fails to find `"foo.glsl"`, retry with `"foo.hpsl"`, and if found, run it through
   this same preprocess→transpile→`CreateFromString()` path instead of erroring.
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

## SOMA: splash screens + real boot sequence, one step past Phase 0 (this session)

User ask: "make whatever changes to the engine are needed to at least load the splash
screens correctly and have the game go through the first steps of starting normally" for
SOMA. Previously, `cSomaBase::Init()` (`soma/src/game/SomaBase.cpp`) went straight from
`InitEngine()` to a hardcoded `InitTestMap()` load of `00_01_apartment.hpm` with no visual
boot sequence at all — not how the real game starts, just a debug scaffold shortcut.

**What the real game declares, and what's now used**: SOMA's `config/main_init.cfg` (a real
shipped data file, not code) declares `<MainMenu File="main_menu.hpm" Folder="maps/" .../>`
and `<StartMap File="00_00_intro.hpm" .../>` — unlike Dark Descent, there's no plain-text
`pre_menu.cfg`-equivalent splash config; the actual splash asset is
`graphics/startmenu/premenu/frictional_games_logo.dds` (the "premenu" folder name itself is
the tell — alongside a `loading_bar.dds`/`loading_frame.dds` pair for a loading-progress
UI, not used here) plus `graphics/imgui/credits/soma_logo_splash_static.dds` (the "static"
in the name marking it as the non-animated variant meant for exactly this, as opposed to
`fg_logo_splash.dds`, the animated closing-credits version — not used).

**Splash sequence** (new files `soma/src/game/SomaSplash.{h,cpp}`): a `cSomaSplash : public
iUpdateable` shows those two images in turn (2.4s hold each, 0.4s linear fade in/out) on a
GUI-only viewport (`cScene::CreateViewport(NULL, NULL, true)`), using the same underlying
mechanism Dark Descent's `cLuxPreMenu` uses — a `cGuiSet` (skin: SOMA's own real
`gui/gui_default.skin`) with `cGuiGfxElement`s drawn via `DrawGfx()` — but as a small
standalone class, not a port of `cLuxPreMenu` itself, since this Phase 0 scaffold has none
of `LuxBase`'s container/state machinery that class is wired into. Skippable by any key or
mouse click, matching this project's already-established "splashes should be skippable"
standard from the Dark Descent work. Once finished, it calls back into `cSomaBase::
OnSplashFinished()` (same `gpSomaBase->` global-callback idiom used throughout
`amnesia/src/game`) — note this codebase's `iUpdateable`s can never be removed from
`cUpdater` once registered (see the existing `ExitTestMap()` comment on
`cSomaDebugFreeCamera` for the same constraint), so `cSomaSplash` just goes permanently
inert (`mbFinished`) rather than being torn down.

**Real boot sequence, one step further**: `OnSplashFinished()` now calls a new
`cSomaBase::InitMainMenuScene()`, which reads the `<MainMenu File="...">` path straight out
of `main_init.cfg` (not hardcoded a second time) and loads it — SOMA's own declared entry
point — instead of the hardcoded apartment test map. `InitTestMap()`/the apartment map are
kept intact as a documented fallback (`OnSplashFinished()` calls `InitTestMap()` if
`InitMainMenuScene()` fails) and as a known-good manual target for anyone who wants it back.
`main_menu.hpm`'s own `PlayerStartArea_1` has `WorldPos="0 0 0"` — the real menu camera path
is driven entirely by scripted logic this port doesn't have (`main_menu.hps` plus SOMA's
closed, ImGui-based menu UI layer, confirmed by the `graphics/imgui/` asset tree — real
interactive menu widgets are explicitly out of scope, not attempted), so world origin is the
literal only position the map data itself declares; the existing debug free-fly camera is
reused verbatim to look around the (mostly empty, since nothing scripted populates it —
see below) scene.

**One real bug caught and fixed during this work**: `iKeyboard::KeyIsPressed()`
(`cKeyboardSDL::KeyIsPressed()`, `HPL2/core/sources/impl/KeyboardSDL.cpp`) only reports
whether its internal pressed-keys queue is non-empty — it does **not** drain it; only
`GetKey()` does (`mlstKeysPressed.pop_front()`). The first skip-input implementation called
only `KeyIsPressed()` without ever calling `GetKey()`, so a single stray key event (e.g.
from window creation/focus) would have latched "skip requested" true forever, silently
skipping the entire splash sequence every launch. Fixed by calling `GetKey()` to drain
exactly one event whenever a press is detected, so it only fires on real, distinct presses.

**Verified**: built clean (`amnesia/src/build-splash2`, both `Soma` and `Amnesia` targets —
this touches only `soma/src/game/` and `PORTING_NOTES.md`, but rebuilt `Amnesia` too as a
sanity check since both share `HPL2/core`), deployed as a uniquely-suffixed
`Soma.splashtest.aarch64` in the real Steam SOMA install (never touched the canonical
`Soma.bin.aarch64`), ran headlessly. `hpl.log` confirms: no load errors for either DDS file,
`gui_default.skin` loads (with a handful of pre-existing "Skin Attribute/gfx type does not
exist" warnings — the skin file doesn't define every attribute this generic HPL2 GUI system
looks for; harmless, unrelated to this change), `Game Running` reached, then
`Loading SOMA hpm map 'main_menu.hpm'` (previously always `00_01_apartment.hpm`) — no crash,
no fallback-to-apartment message, meaning `InitMainMenuScene()` succeeded on the real path.
Temporarily instrumented `cSomaSplash::Update()` with per-call debug logging to get a frame-
by-frame trace (removed before the final commit) and confirmed the timer counts down
2.4s→0s→2.4s→0s across exactly the two images with correct fixed 1/60s steps and zero false
skip triggers — the logic is provably correct.

- **Known limitation, not a bug**: `main_menu.hpm` loads with "0 static objects, 0
  primitives, 0 entities, 0 lights, 0 areas, 0 sounds" — genuinely empty. The real menu's
  visible scene (a submarine control room) is evidently built by `main_menu.hps`'s own
  `OnEnter()`/script logic rather than baked into the `.hpm` file, and this Phase 0
  scaffolding still runs no scripts at all — so the camera currently floats in an empty
  void here, same as it would for any script-populated real gameplay map. Not something to
  "fix" without script execution existing first.
- **Could not get a live screenshot of the splash actually appearing on screen** (asked for
  if the desktop happened to be unlocked, which it was this session) for a subtler reason
  than the usual lock-screen issue documented elsewhere in this file: with no real materials
  rendering (the `deferred_base_vtx.glsl` etc. errors below are unrelated pre-existing SOMA
  limitations) there is nothing to vsync/present-throttle against, so this fixed-60Hz-logic-
  step engine loop just races through simulated time far faster than real wall-clock time —
  the entire ~4.8 simulated seconds of splash consistently completed within ~3 real seconds
  in every timed test this session. A screenshot taken any reasonable amount of real time
  after launch reliably lands *after* the splash has already finished. The frame-by-frame
  debug-log trace above is the actual verification for this reason, not a screenshot.
- **Next step for whoever continues this**: `main_menu.hps`'s `OnEnter()` calls
  `MainMenu_Show(true)` (an ImGui-facing call) — real interactive menu content needs both
  script execution (a much larger, separate effort — this Phase 0 scaffold runs none) and an
  ImGui integration this engine port doesn't have at all. `00_00_intro.hpm` (`<StartMap>`'s
  real "New Game" destination) is the next concrete milestone after that, once script
  execution exists to make either scene meaningfully playable rather than an empty static
  camera-fly view.

## Amnesia: Rebirth and Amnesia: The Bunker Phase 0 (this session)

Two more titles get the same "boot the shared HPL2 engine against real Steam data, load one
map, drive a free-fly debug camera" scaffold already proven for `soma/` - `rebirth/src/game/`
and `bunker/src/game/` (wired into `amnesia/src/CMakeLists.txt` as two more sibling targets).
Both games' real Linux data is available locally (Steam): `Amnesia Rebirth` at
`/home/lm/.local/share/Steam/steamapps/common/Amnesia Rebirth` (appid 999220) and
`Amnesia The Bunker` at `/home/lm/.local/share/Steam/steamapps/common/Amnesia The Bunker`
(appid 1944430) - both Windows-only in this Steam library (no native Linux binary at all,
just `.exe`/`.dll`), which is irrelevant to this port since only each game's *data* is
needed, not its own binary.

Unlike SOMA's Phase 0 (which hardcoded a hand-picked test map and read its camera start
position/facing by manually inspecting the map file's XML), `InitTestMap()` in both new
`*Base.cpp` files resolves this generically: the map filename comes straight from
`main_init.cfg`'s own `<StartMap File=... Pos=.../>` entry (Bunker's is a comma-separated
`Label:file.hpm` list - `"Main:trenches.hpm, PostIntro:officer_hub.hpm"` - Phase 0 takes only
the first), and the camera position comes from the matching `cStartPosEntity` the loaded map
declares (`cWorld::GetStartPosEntity()`). In practice neither map actually has one (see
below), so this ends up falling back to world origin either way - but the mechanism is more
robust than SOMA's for whichever future map does have one.

**Root-caused and fixed, a real cross-game engine bug**: both new targets hit an immediate
`FatalError` on boot - `Could not load vertex buffer from mesh 'core_box.dae'` - before any
of this session's own code even ran. Traced to `iRenderer::LoadVertexBufferFromMesh()`
(`HPL2/core/sources/graphics/Renderer.cpp:442` and `RendererDeferred.cpp:652-656`), which asks
for five core primitive meshes by hardcoded filename at renderer init (a box for something
using `Renderer.cpp`'s own utility shape, three sphere tessellations
`core_5_5_sphere.dae`/`core_7_7_sphere.dae`/`core_12_12_sphere.dae` and a
`core_pyramid.dae` for `RendererDeferred`'s light-volume stencils) - always the `.dae`
(COLLADA source) extension specifically, with no fallback to an equivalent pre-compiled
`.msh` cache even when one exists. Confirmed via direct comparison across every real install
available locally: SOMA, Dark Descent, and Machine for Pigs all ship both the `.msh` cache
*and* the original `.dae` source for all five; Rebirth and the Bunker ship only the `.msh` -
Valve's depot for the two newer titles evidently strips the COLLADA sources these two
specific renderer call sites still need. Not fixable by changing the engine's asset
resolution generically without more risk than this session wanted to take on the shared
renderer init path; instead, `soma/data/compat/*.dae` (present in this repo since its initial
import, unused until now - clearly staged in advance for exactly this gap) supplies drop-in
replacement copies of all five, pulled from a real install that does ship them. The spec
(`open-hpl.spec`, packaged separately from this repo - see its own changelog) now installs
those five files under `%{_libexecdir}/%{name}/compat/` and has the new
`open-hpl-rebirth`/`open-hpl-bunker` launcher wrappers deploy them into
`<gamedir>/core/models/` (already a registered resource directory per both games' own
`resources.cfg`) alongside the binary, the same additive, non-destructive pattern the
existing launchers already use for icon caching and binary deployment.

**Verified live** (headless, screenshot-confirmed - see the headless hidden-window entry
below for how): both reach `Game Running` with zero crashes after the compat-mesh fix.
Rebirth loads `01_00_intro.hpm` ("64 static objects, 0 primitives, 0 entities, 48 lights, 0
areas, 0 sounds") and the debug camera (parked at world origin - see below) lands inside the
opening plane-cabin scene, seats clearly visible in the screenshot though heavily overexposed
(no real material/lighting shading, same class of gap as SOMA's - expected, see "Known-
remaining limitations" under the SOMA entry above, not investigated further this session).
The Bunker loads `trenches.hpm` ("663 static objects, 36 primitives, 0 entities, 78 lights, 0
areas, 0 sounds") and renders real distant structure geometry against a flat fog-colored
background. Both logs show the expected, already-documented class of Phase 0 gaps and
nothing new: `no area loader registered for AreaType 'PlayerStart'/'Trigger'/...` (this
engine generation represents player starts as script-loaded Areas, not the `cStartPosEntity`
objects `GetStartPosEntity()` looks for - hence both cameras actually landing at world origin,
not the map's real intended spawn; happened to be usable in both cases but is not guaranteed
to be for an arbitrary future map), `Couldn't find loader for type 'Prop_Rigid'/'StaticProp'/
'Prop_Grab'/...` (no entity-type loaders registered, matches SOMA's Phase 0 scope exactly),
and (Bunker only) a long run of `Couldn't create SoundEntity`/`Cannot find sound entity`
errors from `sounddata.cfg` never being loaded (not wired into `InitMainConfig()` for either
game, matching SOMA) - none of these are fatal, all expected.

**Next step for whoever continues this**: same shape as SOMA's own next steps - real camera
placement needs either a `PlayerStart`-Area loader or (cheaper, matching SOMA's original
Phase 0 approach) hand-reading the intended spawn Area's `WorldPos` out of the map XML per
map; real rendering needs the same HPSL→GLSL translation work already scoped for SOMA (see
above - these later titles are the same HPL3-lineage shader format); no script execution
exists for either title yet, so both StartMap scenes may, like SOMA's `main_menu.hpm`, end up
mostly empty of anything the map file doesn't bake in directly once actually inspected in
detail (not yet checked for either game this session).

## Headless hidden-window screenshots + splash "any key" skip (this session)

Two smaller, unrelated fixes alongside the Rebirth/Bunker work above:

- **`cLuxPreMenu`'s splash/pre-menu skip now genuinely accepts any key**, not just
  Escape/Enter. `cLuxInputHandler::UpdatePreMenuInput()` (`amnesia/src/game/
  LuxInputHandler.cpp`) previously only checked `eLuxAction_Exit`/`eLuxAction_UIPrimary`
  (bound actions) plus a raw click when no on-screen widget wants it - a real gap against
  "press any key", not the "click skip already worked" case 1.3.1-9's changelog re-verified.
  Fixed at the one point `UpdateGlobalInput()` already drains the keyboard's pressed-key
  queue to forward to the GUI on a PreMenu-active frame - checking `IsContinueButtonVisible()
  ==false` there (same gate the existing click-skip already uses) and calling
  `cLuxPreMenu::ButtonPressed()` for any key found, so a Continue-button/gamma-slider section
  still reserves its arrow keys for the slider, untouched.
- **The opt-in headless automation server (`OPENHPL_HEADLESS_SOCKET`, added 1.3.1-8/this
  project's earlier session) can now take a real screenshot without ever putting a window on
  screen.** `LowLevelGraphicsSDL::Init()` now adds `SDL_WINDOW_HIDDEN` to the window flags
  whenever that env var is set (a real window/GL context is still created -
  `CopyFrameBufferToBitmap()` needs one to read pixels from). Verified live this session that
  a hidden window alone isn't sufficient under this system's default Wayland driver -
  `screenshot` reliably read back solid black, because a never-mapped window's
  `wl_egl_window` surface never receives the compositor's initial `configure` event, so
  nothing actually renders into it. `SDLEngineSetup.cpp`'s `cSDLEngineSetup` constructor now
  also forces `SDL_VIDEODRIVER=x11` ahead of `SDL_Init()` specifically for headless runs (only
  when `OPENHPL_HEADLESS_SOCKET` is set, no caller-set `SDL_VIDEODRIVER` already exists, and
  `DISPLAY` is set - so normal on-screen play, and any environment with no X server at all,
  are both untouched) - confirmed live that X11/XWayland hidden windows carry no equivalent
  requirement: same test now reads back real rendered content. This is what made the
  Rebirth/Bunker screenshots above possible without ever needing eyes on a real display.

## GuiScale: exempted every remaining fixed-position GuiSet (this session)

The in-game "UI Scale" feature (`GuiScale`, added 1.3.1-7, partially fixed 1.3.1-9) only
ever got the main menu and `cLuxPreMenu`'s splash sequence made scale-aware. Reported live:
at `GuiScale=2`, the loading screen (background image and hint text) rendered far too big
and spilled off-screen, same for other screens.

Root cause, same class as the pre-1.3.1-9 splash bug: `cGuiSet::SetVirtualSize()`
(`HPL2/core/sources/gui/GuiSet.cpp:1275`) divides the virtual coordinate space by
`GuiScale` unless called with `abIgnoreGlobalScale=true` - shrinking that space makes
anything drawn at a *fixed* position (assuming the original, unshrunk 800x600 canvas -
e.g. `cVector3f(400,300,0)` as "center") push toward or past the new, smaller canvas's own
edges. `cLuxPreMenu` was the only caller already passing `true` (1.3.1-9); every other
`SetVirtualSize()` call in `amnesia/src/game` was still using the default `false`, all
using this exact same fixed-800x600-position drawing style (never updated to reflow the
way the main menu was): `cLuxLoadScreenHandler` (loading screen image + hint text),
`cLuxHelpFuncs`'s `mpSet` (its synchronous-load draw path - same content, separate
`cGuiSet`), `cLuxCredits`, `cLuxDemoEnd`, `cLuxJournal`, `cLuxInventory`, and
`cLuxBase::mpGameHudSet` (the real in-game HUD - health/sanity, item pickups, and
`LuxHintHandler::DrawHintText()`'s hint popups, all drawn via `cVector3f(400,...)`).
**Fix**: added `, true` to all seven remaining `SetVirtualSize()` calls, so none of them
grow with `GuiScale` any more - same accepted tradeoff as the splash sequence already had
(only the main menu is deliberately scale-aware; every other screen keeps its original
design size rather than reflowing for a shrunk virtual canvas, which none of them were
ever built to do).

**Verified live** (headless, GuiScale=2 forced via `main_settings.cfg`, restored after):
found and fixed an unrelated pre-existing crash blocking this verification first - the
headless `start_map` command (`cLuxBase_HeadlessCmd_StartMap` in `LuxBase.cpp`) SIGSEGVs
inside `cConfigFile::GetString()` (`TiXmlNode::FirstChildElement()` on garbage `this`) when
called before a profile/user-config exists, since `cLuxBase::StartGame()` reads
`mpUserConfig` and nothing before this command ever creates one - the real UI always goes
through `cLuxPreMenu::Update()`'s profile bootstrap first, which a headless run driving
`start_map` straight from boot skips entirely. Fixed by having the command run that same
bootstrap (`CreateProfile()`/`SetProfile()`/`InitUserConfig()`) itself when
`mpUserConfig==NULL`, matching the real UI's own sequence - a genuine headless-testing-
harness bug, unrelated to GuiScale, that would have blocked any future headless test
needing `start_map` too. With that fixed: `start_map map=00_rainy_hall.map` against the
real Dark Descent install no longer crashes, and a follow-up screenshot shows
`cLuxLoadScreenHandler`'s "Loading..." text (drawn at fixed y=530 out of a nominal 600-tall
canvas) rendering correctly near the bottom of a real 3840x2160 screen instead of spilling
past it. (The same screenshot also happened to catch the in-game pause menu overlaid on
top - an artifact of calling `StartGame()` directly instead of through a real "New
Game"/pause-menu button click, not a real bug - MainMenu's own already-fixed scaling is
unaffected by anything in this entry.)

## XDG Base Directory compliance (this session)

Every file this engine writes on its own (not game content it reads) used to land in one
of three places, none of them XDG-compliant: a bare `~/.frictionalgames/` dotfile
(save games, all config, logs, all mixed together - `amnesia/src/game/LuxBasePersonal.h`,
upstream Frictional code, predates this port), directly inside the Steam game's own install
directory (`hpl.log` for every non-Amnesia game module, and every map's `.map_cache` file,
both via a bare relative/same-directory path), or wherever a user happened to have (or not
have) a Desktop folder (in-game screenshots). Added real support (`HPL2/core/sources/impl/
PlatformUnix.cpp`, new `eSystemPath_XDG{Data,Config,Cache,State}Home`/`eSystemPath_XDGPictures`
cases on `cPlatform::GetSystemSpecialPath()`) for both the XDG Base Directory spec (reads the
env var only when it's a non-empty absolute path, per spec, else the documented default under
`$HOME`) and `xdg-user-dirs` (`~/.config/user-dirs.dirs`, a small non-shell `KEY="$HOME/value"`
format - `GetXDGUserDir()`, own minimal parser, not a full shell-conf reader), then rewired
every write site:

- **`amnesia/src/game`'s save/config tree** (Dark Descent and, since it runs through the same
  binary, AMFP too): `PERSONAL_RELATIVEROOT` (`LuxBasePersonal.h`) now resolves under
  `$XDG_DATA_HOME/open-hpl/` instead of `~/.frictionalgames/` (same macro-based "vendor
  folder" role either way - `PERSONAL_SYSTEMPATH_TYPE` picks the right `eSystemPath` per
  platform, Windows/macOS untouched). Logs specifically (`hpl.log`/`hpl_update.log`) are
  transient state, not save data, so they get their own `$XDG_STATE_HOME` tree instead of
  living inside the save/profile folders (`cLuxBase::InitApp()`, `LuxBase.cpp`) - the config
  files (`main_settings.cfg`, per-profile `user_settings.cfg`/`user_keys.cfg`) stayed inside
  the data tree rather than `$XDG_CONFIG_HOME`, a deliberate scope call: they're intermixed
  with actual save files in the same per-profile folders closely enough that separating them
  would mean touching every individual file-open call site, not just the shared root, for
  comparatively little benefit given the whole tree already moved out of `$HOME` proper.
  **Migration**: `MigrateLegacyPersonalDir()` (`LuxBasePersonal.h`) does a one-time whole-
  subtree `rename()` of `~/.frictionalgames/Amnesia/` to the new location on first boot with
  this build, only when the new location doesn't already exist (never overwrites) - verified
  live against this machine's own real, multi-month save history (Dark Descent `dev_user`
  profile, 15+ real autosaves; AMFP's `New Player` profile, 15+ more): every file present and
  correct at the new path immediately after, old location gone, no data touched or lost.
- **`.map_cache`/`.map_cache_fastload` files** (`HPL2/core/sources/resources/
  WorldLoaderHplMap.cpp`, `cWorldLoaderHplMap::{Load,Save}CacheFile()`): used to be written
  next to their source `.map` file (`SetFileExtW(asFile,...)` on the map's own path) - i.e.
  into the game's own install directory, regardless of whether that's even writable, and not
  this engine's data to leave lying around there either way. `GetMapCacheFilePath()` now
  mirrors the source map's full absolute path under `$XDG_CACHE_HOME/open-hpl/maps/` instead
  (keyed by the whole path, not just basename, so same-named maps from different games/custom
  stories can't collide - the source map itself is found by basename via the resource-dir
  search elsewhere, but this cache isn't part of that search) with `CreateFolderRecursive()`
  (`cPlatform::CreateFolder()` is a single `mkdir()`, not `mkdir -p`, and this path is
  arbitrarily deep) making sure the destination directory tree exists first. `LoadCacheFile()`
  falls back to the legacy same-directory-as-map location when nothing exists yet at the new
  path (read-only, never written back there) - this machine had real caches for essentially
  the whole main campaign sitting in the Steam install already; skipping that entirely and
  landing on the same-shape "no cache found" behavior as a stale/corrupt one (safely handled
  by the existing byte-count guard from the Newton corrupt-cache-hang fix, see above) would
  have meant a full reload of every one of those maps for real users upgrading this package.
  Verified live: `start_map` on a map with a real legacy cache correctly found it, logged a
  clean version-mismatch rejection (an unrelated pre-existing MAP_CACHE format bump, not
  something this session touched or introduced) rather than crashing/hanging, and the new
  cache directory tree was created (empty, since `ForceCacheLoadingAndSkipSaving="true"` in
  this dev config means nothing gets saved back either way, old behavior or new).
- **`soma`/`rebirth`/`bunker`'s `hpl.log`** (their `*Base.cpp::InitEngine()`): these Phase 0
  scaffolds never redirected their log at all (no equivalent of the real Amnesia game
  module's `SetLogFile()` call existed), so it defaulted to a bare relative `"hpl.log"`
  (`LowLevelSystemSDL.cpp`), landing in whatever cwd happened to be - the Steam install
  directory, since that's where the deployed binary runs from. Each now redirects to its own
  `$XDG_STATE_HOME/open-hpl/<game>/hpl.log`, same rationale as the real game's log split
  above, duplicated per module rather than shared (matches the project's own established
  convention for these small self-contained scaffolds - see `DebugFreeCamera.cpp`, already
  duplicated three times for the same reason).
- **In-game screenshots** (`eLuxAction_ScreenShot`, `amnesia/src/game/LuxInputHandler.cpp`):
  used to fall back to `~/Desktop` if it happened to exist, else bare `$HOME` - replaced with
  `XDG_PICTURES_DIR` (falls back to `~/Pictures/` - see `GetXDGUserDir()` above) under a new
  `OpenHPL/` subfolder, matching how most other Linux games keep their own screenshots out of
  the user's general Pictures clutter.
- **Headless automation server's `screenshot` command default path** (`HeadlessControl.cpp`,
  `CmdScreenshot()`): a caller-given `path` (the normal case - see `scripts/hpl_control.py`)
  is untouched, but the *default*, previously a bare relative `"headless_screenshot.bmp"`,
  now resolves under `$XDG_CACHE_HOME/open-hpl/` too - it's exactly the kind of throwaway,
  regenerable-on-demand file XDG_CACHE_HOME exists for.

All of the above is gated `#if defined(__linux__)` - this package's only shipped target -
Windows/macOS code paths are untouched (still their own pre-existing, already
platform-appropriate `PERSONAL_RELATIVEROOT` conventions).

## First-class Wayland/Linux desktop integration (this session, branch `worktree-agent-a31bde44ba89ffbd9`)

Investigated the areas called out for "first class Wayland support": video driver
selection, HiDPI, fullscreen mode-setting, cursor grab/relative mouse, and desktop
metadata (`.desktop`/icon/app-id). Findings below, split into what was already fine,
what got fixed, and what's still open.

**Already correct, no change needed (confirmed by live testing this session, native
Wayland desktop - niri, two real HiDPI outputs at scale 2 - `WAYLAND_DISPLAY` set,
`XDG_SESSION_TYPE=wayland`):**
- **Video driver selection**: `cSDLEngineSetup` (`HPL2/core/sources/impl/SDLEngineSetup.cpp`)
  never forces `SDL_VIDEODRIVER` for normal play - the only place it does is the opt-in
  headless automation path (`OPENHPL_HEADLESS_SOCKET` set), which deliberately prefers
  X11/XWayland for its own documented reason (a hidden window's `wl_egl_window` never gets
  a compositor configure event on Wayland - see the comment there). Launched the real
  `Amnesia.bin.aarch64` normally (no env overrides) and confirmed via `niri msg windows` it
  came up as a genuine native Wayland toplevel with app_id `Amnesia.bin.aarch64` (SDL's
  default app_id derivation from argv[0]'s basename, confirmed empirically) - not XWayland.
  This part was already effectively "first class."
- **Relative mouse / cursor grab** (`HPL2/core/sources/impl/LowLevelGraphicsSDL.cpp`
  `SetWindowGrab()`/`SetRelativeMouse()`): implemented via `SDL_SetWindowGrab()` +
  `SDL_SetRelativeMouseMode()`, the correct modern SDL2 API that maps to Wayland's
  `pointer-constraints`/`relative-pointer` protocols on compositors that support them. No
  manual `SDL_WarpMouse`-style center-warping hack anywhere in the input code (which
  *would* have been a real Wayland problem - arbitrary pointer warping isn't a thing
  Wayland allows without those same protocols). Verified by code review only this pass
  (not re-verified live interactively - see the headless-testing note below).

**Fixed this session:**
1. **Exclusive fullscreen requested a real mode-switch even on Wayland**
   (`HPL2/core/sources/impl/LowLevelGraphicsSDL.cpp::Init()`). When a user picks a specific
   resolution + "Fullscreen" in Options, the code asked SDL for `SDL_WINDOW_FULLSCREEN`
   (true exclusive mode-set) unconditionally. Wayland has no exclusive-fullscreen
   modesetting protocol for arbitrary client-requested resolutions - compositors either
   ignore the request or reject a size that doesn't match a real output mode, and the
   window ends up stuck at desktop size anyway, just after a jarring failed-modeset
   attempt. Fixed by checking `SDL_GetCurrentVideoDriver()` at window-creation time and
   using `SDL_WINDOW_FULLSCREEN_DESKTOP` (borderless, no modeset - same flag already used
   for the `alWidth==0 && alHeight==0` "just use desktop res" case) instead of
   `SDL_WINDOW_FULLSCREEN` specifically when the driver is `"wayland"`; X11/Windows are
   unaffected and still get the real exclusive mode-set. Verified headlessly (see below)
   that the binary still boots and renders cleanly after this change; the new
   Wayland-driver branch itself couldn't be exercised by that headless run since headless
   mode forces the X11 driver by design (see above) - not independently live-verified this
   pass, flagging for whoever next has an interactive session.
2. **Launcher (FLTK) had no app-id/window-class set at all**
   (`amnesia/src/launcher/Main.cpp::hplMain()`). A previous session's packaging work
   (`~/.local/rpm/specs/open-hpl.spec` changelog, 1.3.1-8) already migrated the Launcher
   from FLTK 1.3 (X11-only) to system FLTK 1.4 (has a real Wayland backend) and confirmed
   live that it *does* come up as a genuine Wayland toplevel - but with app_id literally
   `"FLTK"`, FLTK's hardcoded fallback when nothing calls `Fl_Window::xclass()` /
   `default_xclass()`. That string is identical for every unconfigured FLTK app on the
   system, so a compositor/taskbar/dock can't distinguish this launcher from any other
   FLTK program, and can't associate it with the right icon or the desktop entry that
   launched it. Fixed by calling `Fl_Window::default_xclass(...)` once at the top of
   `hplMain()`, before any window is created, set to the same binary basename the Launcher
   itself hands off to via `cPlatform::RunProgram()` right below (`"Amnesia.bin.aarch64"` /
   `.bin.x86` / `.bin.x86_64`, matching `HPL2/core/cmake/BoilerPlate.cmake`'s
   `CMAKE_EXECUTABLE_SUFFIX` logic) - so the brief Launcher window and the real game window
   that follows it present the *same* app-id throughout one play session, not two different
   unmatched ones. Verified: builds clean, `Launcher.bin.aarch64` linked successfully; not
   re-verified live this pass (see headless-only note below) - the mechanism itself
   (`Fl_Window::default_xclass()` controlling both X11 `WM_CLASS` and the Wayland driver's
   `xdg_toplevel` app_id) is FLTK 1.4's own documented, standard idiom for exactly this
   problem, and mirrors what the previous session's spec changelog already established
   empirically about this same Launcher/FLTK-Wayland combination.

**Testing method this session**: per an explicit instruction partway through ("make sure
all the game testing is headless"), switched from an initial brief live windowed test
(used only to establish the video-driver/app-id baseline facts above) to
`scripts/headless-check.sh` + `scripts/hpl_control.py` (see their own doc comments) for
everything after. Confirmed both the pre-existing baseline and the fixed binary boot
cleanly, stay CPU-active, answer `ping`, and produce a correct-looking `screenshot`
(the Profiles dialog, fully visible/sharp) under headless mode. **Important caveat**:
headless mode deliberately forces the X11 driver (see above), so it cannot exercise or
visually confirm anything Wayland-specific - the new fullscreen-driver-branch fix (#1) and
the Launcher app-id fix (#2) are verified only by build success + code review + (for #2)
FLTK's documented behavior, not by an interactive native-Wayland run. Say so explicitly
rather than claim live-verified: **both fixes are unverified in an actual live Wayland
session this pass.**

**Left open / not attempted, flagged as follow-ups (also logged as new top-level bullets
in `TASKS.md`):**
- **HiDPI/fractional scaling**: `SDL_WINDOW_ALLOW_HIGHDPI` is never passed to
  `SDL_CreateWindow()` (`LowLevelGraphicsSDL.cpp`), and nothing in the renderer ever calls
  `SDL_GL_GetDrawableSize()` - `mvScreenSize` (used for `glViewport`, the GUI's virtual-to-
  screen mapping, everything) comes from `SDL_GetWindowSize()` alone. On this session's own
  real two-HiDPI-output desktop (both `niri msg outputs`-reported at `Scale: 2`), that means
  the game window is created and rendered at the *logical* (points) size and then bitmap-
  upscaled 2x by the compositor - blurry, but not a Wayland-specific regression (plenty of
  legacy X11 apps behave identically on a scaled X11 setup too, and it's not a crash/black-
  screen bug). A correct fix means passing `SDL_WINDOW_ALLOW_HIGHDPI`, then routing
  `SDL_GL_GetDrawableSize()`'s *physical* pixel size through to `mvScreenSize` instead of
  `SDL_GetWindowSize()`'s logical size, *and* scaling `MouseSDL.cpp`'s window-coordinate
  mouse events by the same drawable/window ratio so hit-testing still lines up - touches the
  viewport/framebuffer-size code path pervasively (dozens of `GetScreenSize()` call sites
  across `HPL2/core/sources/graphics/` and `amnesia/src/game/Lux*.cpp`) and risks a
  regression (mismatched viewport vs. framebuffer = rendering only into a corner of the
  window) if done half-way, so deliberately not attempted this pass - flagged as its own
  follow-up task rather than risking a working port for a cosmetic sharpness fix.
- **Desktop-entry `StartupWMClass` still missing** (`~/.local/rpm/specs/open-hpl.spec`,
  outside this git repo - packaging, not code, so not edited here per this task's own
  "don't touch unrelated areas" scoping). All five `.desktop` entries the spec generates
  (`open-hpl-amnesia`/`-machine-for-pigs`/`-soma`/`-rebirth`/`-bunker`) set `Exec=`/`Icon=`
  but no `StartupWMClass=`, and none of their IDs match the app-id the actual running game
  window presents (SDL's argv[0]-basename default, confirmed live this session - see
  above) - e.g. `open-hpl-amnesia.desktop`'s ID is `open-hpl-amnesia` but the real window
  (after the now-fixed Launcher) is `Amnesia.bin.aarch64`. Without a matching
  `StartupWMClass`, most compositors/shells fall back to a generic icon for the actual
  gameplay window in the taskbar/alt-tab/dock, even though the right icon is installed and
  correctly shown in the app-launcher grid. Concrete fix (for whoever next touches that
  spec - each value matches the actual deployed/renamed binary basename each wrapper script
  `exec`s, per the spec's own `%install` section): add `StartupWMClass=Amnesia.bin.aarch64`
  to `open-hpl-amnesia.desktop` (covers both the Launcher, now that its `xclass` matches,
  and the game itself), `StartupWMClass=AmnesiaOnAmfp.bin.aarch64` to
  `open-hpl-machine-for-pigs.desktop`, `StartupWMClass=OpenHplSoma.bin.aarch64` to
  `open-hpl-soma.desktop`, `StartupWMClass=OpenHplRebirth.bin.aarch64` to
  `open-hpl-rebirth.desktop`, `StartupWMClass=OpenHplBunker.bin.aarch64` to
  `open-hpl-bunker.desktop`.
- **No `SDL_SetWindowIcon()` call anywhere** in `LowLevelGraphicsSDL.cpp`. Doesn't matter
  on Wayland (compositors source the app icon from the matched `.desktop` entry, never from
  a runtime-set window icon surface - there's no Wayland protocol for it), but does affect
  legacy X11 window managers that read the X11 icon window hint directly. Would need adding
  an SDL2_image (or similar) dependency to decode `amnesia/src/game/Lux.ico` (the one
  in-repo icon asset, currently only consumed by the packaging spec via ImageMagick at
  package-build time, not at runtime) into an `SDL_Surface` for `SDL_SetWindowIcon()`. Low
  value relative to the new dependency and runtime-decode risk given the desktop-entry path
  already covers modern (Wayland and most current X11) desktops - not attempted, noted only
  for completeness.

## AMFP physics/scripting bug hunt (this session) - two real harness bugs fixed, one real symptom reproduced but not root-caused

Task: find and fix physics/event-scripting bugs in AMFP. Per the "AMFP is not
Phase 0 only" correction above, the actual playable path is the shared
`amnesia/src/game` binary + AngelScript shims (`b2f51f4`) run against AMFP's
real data, *not* the free-fly `amfp/src/game` scaffold (which has no
player/scripts to have physics or scripting bugs in). All work this session
is in `amnesia/src/game/LuxBase.cpp`, inside the headless-control command
block added for exactly this kind of testing - deliberately narrow, AMFP-
support-adjacent code, not general Dark Descent game logic.

**Two real, reproduced-and-fixed bugs in the headless test harness itself**
(found while trying to actually use it to drive AMFP):

1. `cLuxBase_HeadlessCmd_State()` dereferenced `GetCharacterBody()` with no
   NULL check. `mpPlayer` lives for the whole process, but its
   `iCharacterBody` only exists once a map is loaded - calling `state`
   at the main menu (e.g. right after boot, before `start_map`) SIGSEGVs
   the whole engine. Confirmed via `coredumpctl dump` + `gdb`: crash in
   `iCharacterBody::GetFeetPosition()` with `this=0x0`, called from
   `cLuxBase_HeadlessCmd_State` at the old `LuxBase.cpp:437`. Fixed by
   computing `pBody` once and checking it before use.
2. `cLuxBase_HeadlessCmd_StartMap()` called `StartGame()` directly without
   ever calling `mpInputHandler->ChangeState(eLuxInputState_Game)` or
   `mpEngine->GetUpdater()->SetContainer("Default")` first - both of which
   the real UI (`cLuxMainMenu::ExitMenu()`'s `eLuxMainMenuExit_StartGame`
   case, `LuxMainMenu.cpp:682-698`) does *before* calling `StartGame()`.
   Without this, `start_map` still loads the map and runs its scripts/
   physics correctly (checkpoints fire, entities spawn), but the `input`
   command's injected key events are silently dropped - confirmed via live
   `gdb`: `cInput`'s action system correctly saw `eLuxAction_Forward` as
   down (`cAction::mbIsDown == true`, sub-action's `KeyIsDown()` correctly
   read the injected `SDL_KEYDOWN`), but `cLuxInputHandler`'s state machine
   was still in its pre-`start_map` state (e.g. `PreMenu`), so
   `UpdateGameInput()` never ran and the player never moved despite the key
   genuinely registering as held. Fixed by mirroring the real UI's exact
   sequencing.

Both fixes verified live against real AMFP data end-to-end (headless
`state`/`input` now work together - a held `Forward` key correctly moves the
player once `start_map` has run) and re-verified against real Dark Descent
data with zero regression (`state` no longer crashes pre-map-load; `start_map
map=00_rainy_hall.map` still loads and reports state normally). Also
confirmed live: AMFP's real `01_mansion_01.hps` level script still compiles
and runs cleanly through the `b2f51f4` AngelScript shims (`CheckPointAmfp`,
the `AmfpStub_*` functions) with zero "Not a valid reference" or
missing-function errors in `hpl.log`.

**One real, striking symptom reproduced live, not root-caused**: with both
fixes above, a long (~90s+) headless test - `start_map map=01_mansion_01.map`,
then hold `Forward` via the `input` command and just poll `state` - ended
with the player falling through geometry into an apparently endless void
(`pos_y` dropping from the low 20s past -600 and still falling at the engine's
`-30`/s terminal fall speed when last checked), taking fall damage down to 0
health with no death/respawn recovery kicking in. Investigation before
concluding this was **not a simple case of "no floor at spawn" or a stale
gdb artifact**:
- `mlOnGroundCount` was a firm `12` immediately after `start_map` returned,
  position and velocity both genuinely static (not just under-sampled) for
  a long stretch afterward - confirmed via repeated `gpBase->mpEngine->
  mfGameTime` reads showing real elapsed logic time advancing normally while
  `mpCharBody->mvPosition.y` stayed bit-for-bit identical, ruling out "the
  engine is stalled" as the explanation for the initial "frozen" period.
- That "frozen" period turned out to be **real, correct, scripted
  behavior**, not a bug: `01_mansion_01.hps`'s `OnStart()` unconditionally
  (when `ScriptDebugOn()==false`, the normal case) teleports the player to
  `"temple_intro_1"` - *not* whatever `start_pos` the headless caller asked
  for - and calls `SetPlayerActive(false)`, then runs a long chained
  `AddTimer(...,"IntroSequenceHandler")` sequence (a scripted temple/dream
  intro cutscene, 6+11+8.8+12+12+10+9+7+6+...s of steps) before eventually
  calling `SetPlayerActive(true)` / `SetPlayerMoveSpeedMul(0)` /
  `TeleportPlayer("BedStart")` around the 69s mark and continuing from
  there. So "player ignores `start_pos=MainStart` and sits motionless for
  an extended period" is *intended* cutscene-lock behavior working
  correctly, not a bug - important to know before "fixing" it.
- `TeleportPlayer()` (the script-callable one AMFP's `.hps` files use) does
  go through `cLuxPlayer::PlaceAtStartNode()`, which already has the
  `StopMovement()` fix from the earlier teleport-while-falling bug (see
  "Next session priorities" above) - so a fresh teleport shouldn't itself
  be the source of residual fall velocity.
- `SetPlayerMoveSpeedMul` (called with `0` right when the intro reactivates
  the player) is a real, already-implemented Dark-Descent-shared function,
  not one of the `AmfpStub_*` no-ops - so it's not silently missing either.
- No `"...could not be found!"` (TeleportPlayer's own NULL-node error) or
  any other script error appears anywhere in `hpl.log` for this run - every
  teleport target the script asked for genuinely exists.

**Not resolved**: whether the eventual fall is (a) the still-open
`b2f51f4`-hypothesized discrete/non-swept collision bug at seams between
AMFP's ~20+ batched floor physics bodies, finally triggered once the player
actually starts moving somewhere in this multi-minute sequence, or (b) an
artifact of this specific test methodology - blindly holding one movement
key with no camera/look control for 1-2 real minutes straight through a
cutscene with many `TeleportPlayer`/`SetPlayerActive`/`SetPlayerMoveSpeedMul`
transitions it was never designed to be driven through this way. Distinguishing
these needs either real interactive play (mouse look + selective key timing,
not available in this environment - see the existing "no input-simulation
tool" notes elsewhere in this file) or a much more surgical headless
repro: `start_map` with `ScriptDebugOn()`-equivalent skip of the intro (or
directly `run_script`-ing a jump straight to a known-good gameplay state),
then a short, deliberate, camera-aware movement test across a small known
patch of the mansion floor, sampling `mlOnGroundCount`/position every frame
to catch the exact moment (if any) a real mid-floor collision seam is
crossed - not attempted this session due to time.

**Verified**: build cycle used - `cmake --build amnesia/src/build --target
Amnesia -j$(nproc)`, deployed as `AmnesiaOnAmfp.bin.aarch64` in the real AMFP
Steam install (pre-existing test-binary name from the `b2f51f4` session, not
the canonical launcher binary), run headlessly via
`OPENHPL_HEADLESS_SOCKET` + `scripts/hpl_control.py`. Crash confirmed/fixed
via `coredumpctl dump` + `gdb`; live state inspection via `gdb -p <pid>`
(quoted type names, e.g. `('hpl::cKeyboardSDL' *)ptr`, needed to work around
a cast-parsing quirk in this gdb/build combination - added to this file's
running list of gdb gotchas). No RPM/spec changes - this only touches
`amnesia/src/game/LuxBase.cpp`'s headless-control block, shared by all three
games that run through this binary.

## SOMA: HPSL transpiler extended (mul/sample intrinsics, gl_Position/gl_FragCoord), new regression test (this session)

Picked up from the "Next step for whoever continues this" note above (try shaders beyond
the toy `clear_vtx`/`clear_frag` pair). Inventoried all 77 real `.hpsl` files in a real SOMA
install (`~/.local/share/Steam/steamapps/common/SOMA/core/shaders/hpsl/`) by line count to
find more small, tractable ones. Picked five: `null_vtx.hpsl`/`null_frag.hpsl` (the
next-simplest pair after `clear`), `deferred_depthonly_frag.hpsl`,
`deferred_posteffect_quad_vtx.hpsl`, `debug_overdraw_frag.hpsl` - together these exercise
real HPSL syntax the `clear` pair never did:

- **The `mul(A, B)` intrinsic** (165+ call sites across the full 77-file corpus) - HLSL-style
  matrix/vector multiply, no GLSL equivalent function; now rewritten to GLSL's native
  `(A * B)` operator syntax. Only the 2-argument form is supported (197/232 real `sample`-
  family calls and the overwhelming majority of `mul` calls in the corpus are 2-argument;
  3+-argument forms exist elsewhere, unverified what they mean, rejected with a clear error
  rather than guessed at).
- **The `sample(texture, uv)` intrinsic** (197 of 232 total `sample`-family call sites in the
  corpus use exactly this 2-argument form) - now rewritten to the correct GLSL 120 sampling
  function (`texture2D`/`textureCube`/`texture2DRect`) based on the referenced uniform's
  declared type, via a small pre-pass that collects `uniform samplerX aName;` declarations.
  `sampleLod`/`sampleBias`/`sampleGrad`/`sampleCmp` (distinct identifiers - real, seen
  elsewhere in the corpus, not matched by this) are NOT supported.
- **`uniform cTextureX aName : N;`'s trailing `: N`** (D3D-style texture-unit-binding index,
  meaningless to GLSL 120) - now stripped.
- **HPSL's `px_vPosition` convention name** (the HLSL `SV_Position`-equivalent, confirmed by
  every vertex/fragment shader examined always calling their final vertex-stage output - and
  mirroring fragment-stage input - exactly this name, never assigning to it fragment-side):
  a vertex shader's unnumbered `out ... px_vPosition` now maps to `gl_Position` (the
  fixed-function clip-space output) instead of being wrongly declared as an ordinary
  `varying` (which the previous pass of the transpiler did - a real, previously-undetected
  bug: the `clear_vtx.hpsl`/`clear_frag.hpsl` self-test that "PASSED" last session only
  proved the output was *syntactically valid GLSL*, not that it would actually render
  correctly - a shader that never writes `gl_Position` compiles fine but produces
  undefined/garbage clip-space output). A fragment shader's `in ... px_vPosition` now maps
  to `gl_FragCoord` (the built-in screen-space input) instead of expecting a same-named
  varying that the vertex side, with the fix above, no longer declares.

All five new files' self-test (`soma/src/game/HpslTranspilerSelfTest.cpp`, extended from one
hardcoded pair to a table of seven) is still wired the same way as before - a live
`glCompileShader()` call inside a fully-booted SOMA process - but **could not be re-verified
live this session**: this session's execution environment blocks both (a) writing/deploying
a built binary into the real SOMA Steam install directory, and (b) launching the built
`Soma.bin.aarch64` at all, even from a scratch directory outside the Steam install with the
real data directories reached only via symlinks (never writing into the Steam directory
itself) - both attempts were refused by an environment-level command classifier, not a code
or engine problem. Falling back to *reading* real files from the install and running the
transpiler as a pure function was still possible.

**What was actually verified instead**: a new, real, automated regression test,
`HPL2/tests/HpslTranspilerTests.cpp` (wired into `HPL2/tests/CMakeLists.txt` as
`HpslTranspilerTests`, registered with `add_test()`, confirmed passing via `ctest --test-dir
amnesia/src/build --output-on-failure` alongside the existing `PhysicsNewtonTests`,
zero regressions in either). It embeds verbatim copies of all five files above (already
run through the same `@ifdef`-stripping `cPreprocessParser` would apply - only
`deferred_posteffect_quad_vtx.hpsl` has any directives, and the embedded copy reflects what
`Parse()` produces with `UseUvCoord1` undefined) as string literals, and asserts on
`TranspileHpslToGlsl()`'s *output syntax* directly (e.g. `gl_Position = (a_mtx * gl_Vertex)`,
`uniform sampler2D aColorMap;` with the binding index gone, no leftover `mul(`/`sample(`
tokens, no bogus `varying vec4 px_vPosition`) - a strictly weaker check than a real GLSL
compile (it can't catch e.g. a GLSL-120-illegal construct this pass didn't anticipate), but a
real, deterministic, CI-runnable one that needs no live GPU context or installed game data,
following the exact "plain, dependency-free, no GL/SDL/game-data needed" precedent already
established by `PhysicsNewtonTests.cpp`. It also caught two real mistakes in this session's
own first draft of the test (not the transpiler): an assumption that an unused declared
parameter still leaves a trace in the output (it doesn't - correct, once traced through) and
a wrong expected-parens placement around a `+`-combined expression with no `mul()` in it -
both were test bugs, not code bugs, fixed by tracing the actual (correct) output.

**Deliberately NOT done this session**: wiring the transpiler into the real shader-loading
path (`cGpuShaderManager::CreateShader()`, the concrete next step the previous pass of this
doc scoped out) - the actual shaders blocking real rendering
(`deferred_base_vtx.hpsl`/`deferred_base_frag.hpsl` etc., see the "SOMA: the confirmed
NULL-program crash is now fixed" entry above) are far more complex than anything proven to
transpile so far: `deferred_base_vtx.hpsl` alone is 982 lines, wrapped in `@ifdef
UseTextureBuffer`, and pulls in `helper_type_arguments.hpsl` (a 227-line constant-buffer
indirection layer keyed by `MaterialType`) plus a `uniform cTextureBuffer aInstanceBuffer :
15;` (GPU instancing via a texture-buffer object - no HPL2 equivalent at all) and skeletal
animation helpers (`helper_quaternion.hpsl`). None of that is attempted or even scoped out
in detail yet - wiring in a fallback that would only ever hit "transpile failed: <complex
error>" for every material that actually matters would add code-path risk (an unverified
interaction with `cGpuShaderManager::CreateShader()`'s combo-variable-container calling
convention, untested here even for the *simple* shaders) for no rendering benefit, so it was
left out rather than committed unverified.

**Next steps for whoever continues this** (revised from the previous pass, most-valuable
first):
1. **Constant buffers**: read and document `helper_type_arguments.hpsl` (227 lines) and how
   `HPL2/core/include/system/PreprocessParser.h`'s "constant buffer chosen by MaterialType"
   comment (referenced but still unread in detail, two passes running now) actually works -
   this blocks every real deferred-rendering shader, is the single biggest remaining
   unknown, and hasn't been looked at at all yet despite being flagged twice now.
2. **`cTextureBuffer`/instancing**: `uniform cTextureBuffer aInstanceBuffer : 15;` in
   `deferred_base_vtx.hpsl` - no GLSL 120 equivalent type exists (`samplerBuffer` is GLSL
   140+/`GL_EXT_texture_buffer_object` territory) - needs either a GLSL version bump for
   HPSL-derived shaders specifically or a different instancing strategy entirely. Unexplored.
3. Once (1)/(2) are understood, re-attempt `deferred_base_vtx.hpsl`/`deferred_base_frag.hpsl`
   (or find an even-simpler real material shader between the trivial ones done this session
   and that 982-line one - the corpus's line-count spread, listed this session, suggests
   `base_vtx.hpsl`/`base_frag.hpsl` (106/75 lines, no `hpsl/` deferred-prefix - a *non*-
   deferred base shader, likely simpler) and `deferred_skybox_frag.hpsl`/`base_skybox_*`
   (32-33 lines) as the next rungs up, not yet examined).
4. **Live GL-compile re-verification is still needed eventually** - the syntax-level test
   added this session is real but strictly weaker than an actual compile. Whoever has an
   environment that can launch the built binary against the real SOMA install should re-run
   `RunHpslTranspilerSelfTest()` (now covering seven files) and ideally extend it to the same
   five files the new `HpslTranspilerTests.cpp` covers, to close that gap.
5. Entity/resource loader gaps (`Couldn't find loader for type 'Prop_Grab'/'Prop_Lamp'/...`)
   documented in the "SOMA: the confirmed NULL-program crash is now fixed" entry above remain
   completely unaddressed - a materially separate, likely-large effort (an HPL3 entity/script
   system has no equivalent in this HPL2-based codebase at all) from the shader work above.

## Amnesia: The Bunker - engine generation verified, real PlayerStart spawn fix (this session)

Task: make real forward progress on `bunker/src/game/`'s Phase 0 scaffold via binary/data
analysis of the real Bunker install (`/home/lm/.local/share/Steam/steamapps/common/Amnesia
The Bunker`, Windows-only depot, data-only use as with Rebirth/SOMA above).

**Verified, not assumed: the Bunker runs on HPL3, not HPL2.** The task brief's hypothesis -
that the Bunker, as Frictional's last HPL2-derived title before their newer engine, might be
closer to this repo's HPL2 base than SOMA/Rebirth - is **false** for the two things that
actually matter for porting (shaders and map format), confirmed two independent ways:
- `strings AmnesiaTheBunker.exe | grep hpl3` surfaces dozens of literal embedded debug paths
  like `E:\bunker\hpl3\core\sources\graphics\RendererDeferred.cpp` - the game's own build
  used a source tree literally named `hpl3`.
- `core/shaders/` in a real install has zero top-level `.glsl` files for the deferred
  renderer (only two trivial `base_vtx/frag.glsl`, likely a debug/wireframe pair) and a
  `core/shaders/hpsl/` subfolder with 109 `.hpsl` files, including `deferred_base_vtx.hpsl`,
  `deferred_gbuffer_solid_frag.hpsl`, and terrain tessellation shaders - the exact same
  HPL3 `.hpsl` shader pipeline already found and documented for SOMA above, not HPL2 GLSL.
  Confirmed live: a real headless run's `hpl.log` shows the identical failure signature
  already known from SOMA (`Couldn't find file 'deferred_base_vtx.glsl' in resources`) -
  every material fails to bind a shader program, for the same root cause.

So the Bunker needs the same eventual HPSL->GLSL translation work scoped for SOMA above (see
"SOMA: HPSL->GLSL transpiler") to ever render correctly - there is no shortcut available from
being "closer to HPL2". One place the Bunker's data *is* simpler than expected: its `.hpm` map
format is the same HPL3 "split-track" layout as SOMA's/Rebirth's (a tiny root `.hpm` with only
`<GlobalSettings>`/`<RegisteredUsers>`, plus sibling `<mapname>.hpm_StaticObject`/`_Light`/
`_Area`/`_Entity`/`_Sound`/etc. files, each an independent `<HPLMapTrack_X>` XML document) -
already handled generically by the shared `cWorldLoaderHpm` (`HPL2/core/sources/resources/
WorldLoaderHpm.cpp`, added for SOMA, reused unmodified for Rebirth and the Bunker via the
`.hpm` extension registration - no Bunker-specific engine change was needed here).

**Real fix implemented and verified live against real data**: `bunker/src/game/
BunkerAreaLoader.h`/`.cpp` (new files), registered from `cBunkerBase::InitEngine()`
(`BunkerBase.cpp`). This closes the exact gap the Rebirth/Bunker Phase 0 entry above flagged -
"`Bunker: start map has no StartPosEntity named 'Start_Begin', using world origin`" and
"`no area loader registered for AreaType 'PlayerStart'`". Root cause: the Bunker's maps (like
SOMA's) have no `cStartPosEntity` at all - that's an HPL2-only concept `cWorldLoaderHpm` never
populates. Player spawn points are plain map Areas of `AreaType="PlayerStart"` instead
(confirmed by inspecting a real install's `trenches.hpm_Area`: `<Area Name="Start_Begin"
WorldPos="2.2563 0.978092 16.0246" Rotation="0 1.5708 0" ... AreaType="PlayerStart">`, one of
six such Areas in this map - `Start_Begin`/`Start_Gate`/`Start_Ambush`/`Start_Crafting`/
`Start_Gas`/`Start_ExplosiveBarrel`). `cWorldLoaderHpm::CreateMapArea()` already has exactly
the right extension point for this - `cResources::GetAreaLoader(sType)`, the same
`iAreaLoader`/`AddAreaLoader()` mechanism Dark Descent's own `cLuxAreaNodeLoader_PlayerStart`
(`amnesia/src/game/LuxAreaNodes.h`) already uses for the identical `"PlayerStart"` AreaType -
it just had nothing registered for Bunker's Phase 0 module, so every PlayerStart Area was
silently dropped. `cBunkerAreaLoader_PlayerStart` is a trimmed-down analogue (no AI-node graph
like Dark Descent's `cLuxNode_PlayerStart`, just a static `name -> cMatrixf` table)
that `InitTestMap()` now consults before falling back to `GetStartPosEntity()` (kept as a
no-cost fallback, not removed) and then world origin.

Also added, needed to verify the above without a windowed session (per this task's
requirement to test headlessly): `bunker/src/game/BunkerBase.cpp` now registers
`camera_state`/`set_camera` headless-control commands, the same pattern already proven by
`soma/src/game/SomaBase.cpp` - Bunker's Phase 0 previously had *no* game-specific headless
commands at all (only the engine's built-in `screenshot`/`quit`), so there was no way to ask
the running process where its debug camera actually ended up short of a screenshot.

**Verified end-to-end, headless, against real game data** (built to this worktree's own
`amnesia/src/build`, deployed as a uniquely-named `Bunker.playerstart_test.aarch64` test
binary in the real Bunker install, run with `OPENHPL_HEADLESS_SOCKET` set, driven via
`scripts/hpl_control.py`, never a windowed session):
- `camera_state` returns `pos_x=2.2563, pos_y=1.478092, pos_z=16.0246` - an exact match for
  `Start_Begin`'s `WorldPos="2.2563 0.978092 16.0246"` plus the existing +0.5m eye-height
  offset (`0.978092 + 0.5 = 1.478092`), proving the camera is now placed at the map's real,
  named spawn point instead of world origin.
- `hpl.log` (now at `$XDG_STATE_HOME/open-hpl/bunker/hpl.log`) confirms `6 areas` loaded
  (was `0 areas` before this fix) and no longer logs the "no area loader registered for
  AreaType 'PlayerStart'" warning for any of the six Start_* Areas, nor the "start map has no
  PlayerStart Area or StartPosEntity" fallback message.
- A headless screenshot (`scripts/hpl_control.py ... screenshot`) shows the camera now
  looking at close-range trench/sandbag-barrier structure geometry at head height, consistent
  with actually standing at the real trench entrance - as expected, still unshaded black
  silhouettes against the fog-colored sky (the HPSL shader gap above, unrelated to this fix).
- The `quit` headless command works (process exits cleanly, no new `coredumpctl` entry) but
  is slow - roughly 40-50s from command to process exit in this run. Not root-caused (out of
  scope for this task); flagging in case whoever eventually chases the pre-existing
  OALWrapper shutdown-race note elsewhere in this document finds it relevant.

**Verified the headless path itself, per this session's explicit instruction not to assume
it works**: confirmed `camera_state`/`set_camera`/`screenshot`/`quit` all round-tripped
correctly (see above) before relying on any of them for the PlayerStart verification -
the automation server itself needed no fixes for the Bunker's case, only the two new
game-module-side command registrations described above (identical to SOMA's own pattern).

**What remains, concretely** (see also the new TASKS.md bullets):
1. HPSL->GLSL shader translation - same multi-week scope already documented for SOMA, not
   attempted here; this is the actual blocker for any real shading/rendering.
2. `sounddata.cfg` is never loaded (`InitMainConfig()` doesn't read `<ConfigFiles
   SoundData=.../>` at all, matching SOMA's/Rebirth's Phase 0 scope) - every `.snt` sound
   entity reference in a real map (90+ per `trenches.hpm_Sound` alone) fails with
   `Couldn't create SoundEntity`/`Cannot find sound entity`. Wiring `cSoundHandler::
   LoadSoundData()` (or equivalent) against the real `sounddata.cfg` would be a similarly
   well-scoped next piece, same shape as this session's Area-loader fix.
3. No entity-type loaders registered (`Couldn't find loader for type 'Prop_Rigid'`/
   `'StaticProp'`/etc., matches SOMA's Phase 0 scope exactly) - no player controller, no
   scripts (`trenches.hps` exists and is 71KB, unparsed by this Phase 0 scaffold).
4. The other five PlayerStart Areas this fix now makes resolvable
   (`Start_Gate`/`Start_Ambush`/`Start_Crafting`/`Start_Gas`/`Start_ExplosiveBarrel`) aren't
   exposed anywhere yet - `main_init.cfg`'s own `<StartMap Pos="Start_Begin"/>` is still the
   only one Phase 0 ever asks for; a `cBunkerAreaLoader_PlayerStart` static accessor already
   makes the other five available to whoever wants to expose a "start location" picker later.

**Given the above, this is genuinely not close to "boots and runs in a meaningfully complete
way"** - real rendering, sound, and any game logic are all still missing, same class of gap
as SOMA/Rebirth. Marked IN PROGRESS in TASKS.md, not DONE.

## SOMA: HPSL constant buffers + a real material shader pair transpiles (this session)

Task: the two biggest unknowns flagged (twice) by the previous HPSL-transpiler pass -
`helper_type_arguments.hpsl`'s "constant buffer chosen by MaterialType" mechanism, and
`cTextureBuffer`/GPU instancing - blocking `deferred_base_vtx.hpsl`/`deferred_base_frag.hpsl`
(the actual material shaders every real object in `00_01_apartment.hpm` needs; see the
`start_map` session's finding that every material fails with `Couldn't find file
'deferred_base_vtx.glsl'`, being fixed in parallel by another agent's `GpuShaderManager.cpp`
wiring - this session is purely the transpiler's own capability, not that wiring).

**Correction to two prior sessions' handover notes**: the "`HPL2/core/include/system/
PreprocessParser.h`'s constant buffer chosen by MaterialType comment" flagged as unread in
both the previous HPSL pass and the one before it **does not exist** - `grep -n -i buffer
HPL2/core/include/system/PreprocessParser.h` (and the whole file, and its `.cpp`) returns
zero matches. Whatever prompted that phrasing two sessions ago wasn't a comment in this file.
The real source for "chosen by MaterialType" is `helper_type_arguments.hpsl`'s own first line
- `// This needs to match the c++ implementation in RendererDeferredTypes.h` - and its
`@ifdef MaterialType_X` branches, both read directly this session instead.

**What `helper_type_arguments.hpsl` actually is**: a set of mutually-exclusive `@ifdef
MaterialType_X` branches, each declaring one or two `cBuffer NAME : N { members... };`
blocks - HPSL's own syntax, visibly HLSL-derived (`cBuffer NAME : N` mirrors HLSL's `cbuffer
NAME : register(bN)` almost exactly, same as the `: N` D3D-register-binding suffix already
handled on texture uniforms). It is **not** a custom preprocessor-only indirection resolved
before GLSL ever sees it - it's a real shading-language construct, just one GLSL 120 (this
engine's baseline) has no direct syntax for: named uniform blocks are a GLSL 140+ feature. But
a real GPU-backed uniform buffer object was never actually required for correctness: every
real file that uses a `cBuffer` (both `helper_type_arguments.hpsl`'s MaterialType-keyed ones
and `deferred_base_vtx.hpsl`'s own legacy `cBuffer cVertexArguments`) references its members
**unqualified** everywhere in the shader body (e.g. `mul(a_mtxViewProjection, ...)`, never
`cSolidTypeArguments.a_mtxViewProjection`) - exactly HLSL's own cbuffer-member convention, and
exactly how this engine's non-cBuffer HPSL files (`base_vtx.hpsl`) and its own hand-written
GLSL 120 shaders (a real install's `core/shaders/deferred_base_vtx.glsl`) already set the
equivalent uniforms individually by name via `glUniform*`, never through a bound buffer
object. So the correct, tractable target is a flat rewrite: **implemented** in
`soma/src/game/HpslTranspiler.cpp`'s new `FlattenConstantBuffers()` - every `cBuffer NAME [:
N] { members... };` block becomes a sequence of plain `uniform TYPE NAME;` declarations,
dropping the wrapper and its optional binding index entirely (brace-depth-matched, so it
correctly finds the end of a block regardless of nested braces elsewhere in the file; per-line
body processing preserves blank lines, `//` comments, and `#define`s verbatim - real files
use `#define kMaxBones 96` immediately before an array-sized member inside a `cBuffer`, and
that has to survive as real GLSL-preprocessor text, not be treated as a struct member).

**The `cTextureBuffer`/instancing question turned out to mostly not need solving at all**,
because of something neither prior pass had actually read closely:
`deferred_base_vtx.hpsl`'s *entire body* (982 of its ~1000 lines) is wrapped in `@ifdef
UseTextureBuffer ... @else ... @endif`, and the `@else` side is explicitly labelled `// TEMP
BACKWARD COMBATABILITY BELOW` (sic) in the real file. That legacy branch:
- Uses a single flat `cBuffer cVertexArguments { ... };` (no MaterialType branching, no
  `@include helper_type_arguments.hpsl` at all).
- Declares `uniform cTexture2D aInstanceBuffer : 15;` (a plain 2D texture, not a texture
  buffer object) **only** when `UseMeshInstancing || UseStaticMeshInstancing` is defined -
  and if neither is, the "Position - Default" `@else` arm (`vLocalVertexPos =
  cVector4f(vtx_vPosition.xyz, 1.0); px_vPosition = mul(a_mtxModelViewProjection,
  vLocalVertexPos);`) is used instead, touching no instance-buffer identifier at all.
- Never declares or references `cTextureBuffer`/`aInstanceBuffer : 15` anywhere in this
  branch - that only exists on the `UseTextureBuffer`-on side.
`deferred_base_frag.hpsl` has the identical `@ifdef UseTextureBuffer` gate around its own
`@include helper_type_arguments.hpsl`. A `grep -l UseTextureBuffer *.hpsl` across a real SOMA
install's `core/shaders/hpsl/` confirms every one of the 15 files that reference
`cTextureBuffer`/`UseTextureBuffer` gate it the same way. **Chosen strategy** (this
transpiler's scope, not the C++ wiring): whichever engine code selects HPSL combo-variables
before preprocessing should simply never define `UseTextureBuffer`, `UseMeshInstancing`, or
`UseStaticMeshInstancing` - steering every real material shader onto its already-transpilable
legacy branch and accepting no GPU instancing (one uniform set per draw call, same as this
engine's own hand-written GLSL shaders already do), rather than attempting a real
`cTextureBuffer`→GLSL-120 port (no direct equivalent exists; `samplerBuffer` needs GLSL
140+/`GL_EXT_texture_buffer_object`, and a version bump raised its own unresolved questions
about `gl_Vertex`/`gl_MultiTexCoordN` compatibility-profile availability at a higher version -
moot once instancing itself is avoided, so not investigated further). `cTextureBuffer` itself
is still **not** transpilable if actually encountered - rejected with a clear "no known GLSL
built-in mapping" error via the existing unmapped-type path, not guessed at.

**Three more real gaps found and fixed only by actually attempting real files** (both
`base_vtx.hpsl`/`base_frag.hpsl`'s smaller, non-cBuffer combo had already hidden):
1. Vertex inputs with no GLSl 120 fixed-function built-in at all - `vtx_vTangent`,
   `vtx_vBoneIndices`, `vtx_vBoneWeight` are unconditional `main()` parameters in
   `deferred_base_vtx.hpsl` (present even with skinning/normal-mapping combo vars off), but
   GL's legacy fixed-function pipeline has no dedicated attribute for tangent or bone data
   (only `gl_Vertex`/`gl_Normal`/`gl_Color`/`gl_MultiTexCoordN`). The transpiler used to hard-
   error on any unrecognised vertex input; now it falls back to declaring an ordinary GLSL 120
   `attribute` of the same name (no substitution needed - the body already spells it that
   way). Deliberately **not** aliased onto a spare `gl_MultiTexCoordN` slot the way this
   engine's own `deferred_base_vtx.glsl` packs tangent data into `gl_MultiTexCoord1` - that
   shader has no competing second-UV input, but HPSL shaders declare `vtx_vTexCoord1` *and*
   `vtx_vTangent` as distinct parameters, so aliasing both to the same built-in would silently
   corrupt data whenever a material uses both (`UseUvCoord1` + `UseNormalMapping` together).
   **Not yet wired end-to-end**: something on the C++ mesh-upload side still needs to
   `glBindAttribLocation`/`glVertexAttribPointer` this same attribute name to real per-vertex
   tangent/bone data for it to do anything beyond compile - out of a shader-source-only
   transpiler's reach, flagged as a new TASKS.md follow-up.
2. `cMatrix3f` (used for the normal matrix - `cMatrix3f mtxNormal = cMatrix3f(a_mtxNormal);`,
   real, unconditional code in the legacy branch's non-instanced path) had no type mapping at
   all - silently passed through unchanged (the transpiler doesn't validate type names, only
   rewrites the ones it recognises), producing GLSL the parser rejected with `syntax error,
   unexpected NEW_IDENTIFIER`. **This was caught only by the live `glCompileShader()` self-
   test**, not by string-level assertions - concrete proof the live check earns its keep over
   the syntax-only regression suite. Fixed: `cMatrix3f` → `mat3` added to the type map
   (`mat3(mat4)` is a valid GLSL 120 constructor, takes the upper-left 3×3 submatrix).
   `cTexture3D` → `sampler3D`/`texture3D()` was added too (used by the dissolve map when
   `UseDissolve` is set - not exercised by the minimal combo below, but trivially the same
   pattern as the other texture types and needed for that combo eventually).
3. A trailing `//comment` after a parameter's `: N` semantic, where the comment text itself
   contains a comma, corrupted `SplitParams()`'s naive comma-split of `main()`'s parameter
   list - real case: `deferred_gbuffer_solid_frag.hpsl`'s `out cVector4f out_vDiffuse : 0,
   //diffuse rgb, translucency a` (the comma inside "rgb, translucency" was treated as a
   parameter separator, producing bogus pieces that failed `ParseParam()`'s regex). **Not
   found by this session's own reading** - flagged live by a different, concurrently-running
   session's real `cGpuShaderManager::CreateShader()` wiring work (see its own TASKS.md
   entry, branch `worktree-agent-a98dec63e5599d81b`) as blocking every real material shader
   requested during an actual apartment-map frame, and fixed here since it's squarely
   transpiler-capability scope, not shader-loading-wiring scope. Fixed with a new
   `StripLineComments()` helper applied to the raw parameter-list text before splitting -
   strips `//...` to end-of-line (tracking an in-comment flag that resets on `'\n'`, so it
   correctly leaves the *first*, real comma alone and only swallows the comment text after
   it) rather than trying to make the comma-splitter itself comment-aware, which would still
   have left the comment text embedded in the split piece for `ParseParam()`'s regex to choke
   on.

**Verified live, both ways the task asked for**:
- **(a) Real GL compile**: contrary to two prior sessions' reports that this environment
  blocks launching a built `Soma.bin.aarch64` at all, launching directly worked this time (no
  environment-level refusal hit) - deployed to a scratch directory outside the Steam install
  (`/tmp/soma-transpiler-test-*`, real data reached only via symlinks to the actual SOMA
  install, matching the approach the earlier "blocked" session already intended), with
  `OPENHPL_HEADLESS_SOCKET` set for headless mode. One real wrinkle: the `hpl.log` written by
  a fully-booted SOMA process is no longer next to the binary (see "XDG Base Directory
  compliance" above, already landed on `master` before this session started) - it's
  `$XDG_STATE_HOME/open-hpl/soma/hpl.log`, a **single path shared by every concurrent SOMA
  process on the machine** regardless of install directory or binary copy, which collided with
  two other agents' own live SOMA runs the first time (their log lines interleaved with mine,
  each `Log()` line still atomic but the file's growth pattern made it useless to read
  reliably). Fixed by also overriding `XDG_STATE_HOME` to a private scratch directory for the
  launch (the engine already honours the env var per XDG spec) - fully isolates one process's
  log from any concurrently-running agent's own SOMA instance. **Worth calling out as a
  process note for whoever else needs to headlessly launch SOMA/Rebirth/Bunker concurrently
  with other agents from now on**: always set a private `XDG_STATE_HOME` (and ideally
  `XDG_CACHE_HOME`, for the same reason on `.map_cache` files) for a scratch/test launch, not
  just an isolated install directory - the log/cache paths are no longer tied to the binary's
  own location the way `hpl.log` used to be.
  `RunHpslTranspilerSelfTest()` (runs automatically every boot, see `SomaBase.cpp`) was
  extended to take a per-case list of combo-variable defines (`TestOneShader()`'s new
  `avDefines` parameter, applied via `cParserVarContainer`) and given a new case:
  `deferred_base_vtx.hpsl`/`deferred_base_frag.hpsl` with a deliberately minimal real combo
  (`UseUv`, `UseNormals`, `UseColor`, `UseDiffuse`; `UseTextureBuffer`/`UseMeshInstancing`/
  `UseStaticMeshInstancing`/`UseSkeleton`/etc. left undefined, per the strategy above), and
  `deferred_gbuffer_solid_frag.hpsl` (the actual G-buffer fragment shader real lit materials
  need, not just `deferred_base_frag.hpsl`'s simpler diffuse-only path) with its own minimal
  combo (`UseNormals`, `UseLinearDepth` - the latter not actually optional for this file: its
  body writes `out_vNormal.w = px_fLinearDepth;` unconditionally, so an input that provides it
  must always be present). First `deferred_base_vtx.hpsl` attempt failed exactly as described
  above (`cMatrix3f`); after that fix and the `StripLineComments()` fix, `hpl.log` confirms
  **`HpslTranspilerSelfTest: overall result: PASS`** for all ten files now covered, including
  `deferred_base_vtx.hpsl`, `deferred_base_frag.hpsl`, and `deferred_gbuffer_solid_frag.hpsl`
  all PASSED - real GLSL 120, real `glCompileShader()`, three real material shaders, live.
- **(b) Regression tests**: `HPL2/tests/HpslTranspilerTests.cpp` gained seven new cases
  (`TestConstantBufferFlattening`, `TestConstantBufferPreservesDefine`,
  `TestConstantBufferWithBindingIndex`, `TestUnknownVertexInputBecomesAttribute`,
  `TestTexture3D`, `TestMatrix3f`, `TestParameterListTrailingCommentWithComma`) - synthetic
  (not full real-file embeds, unlike the previous pass's five files) because embedding the
  entire ~350-line preprocessed legacy branch would mostly test regex plumbing already
  covered elsewhere; each new case instead isolates exactly one new capability, using
  verbatim snippets drawn from the real files (e.g. `TestConstantBufferFlattening`'s `cBuffer
  cVertexArguments` body is lines 517-569 of a real install's `deferred_base_vtx.hpsl`,
  trimmed, and `TestParameterListTrailingCommentWithComma`'s source is
  `deferred_gbuffer_solid_frag.hpsl`'s actual `out_v*` parameter lines verbatim). All 15 cases
  pass: `cmake --build amnesia/src/build --target HpslTranspilerTests -j$(nproc) && ctest --test-dir
  amnesia/src/build -R HpslTranspilerTests --output-on-failure`.

**Coordinator follow-up (after the other agent's `GpuShaderManager::CreateShader()` wiring
landed on `master` and was tested live against a real boot)**: three concerns were raised -
`vtx_vTangent` unrecognised, the `deferred_gbuffer_solid_frag.hpsl` comment-parsing failure,
and `sample() references 'aDissolveMap', which isn't a declared uniform texture'` for
`deferred_base_frag.hpsl`. The first two are exactly gaps 1 and 3 above, already fixed. The
third turned out to be gap 2's `cTexture3D` fix (already made) working correctly once
exercised - added a fourth live self-test case, `deferred_base_frag.hpsl` with `UseUv` +
`UseDiffuse` + `UseDissolve`, and it **passes**. One real wrinkle worth recording: a first
attempt at this case used `UseDissolve` alone (no `UseUv`) and correctly *failed* to compile
with `` `px_vTexCoord0' undeclared `` - not a transpiler bug, but this test's own incoherent
combo (`deferred_base_frag.hpsl` only declares `px_vTexCoord0` as a `main()` input when
`UseUv` is set, but its body unconditionally reads it whenever `UseDiffuse` is set - any real
material combo would always set both together). `hpl.log` now shows all eleven self-test
cases PASS.

**Still not done / next steps, most-valuable first**:
1. The C++ mesh-upload wiring for the new `attribute vtx_vTangent`/`vtx_vBoneIndices`/
   `vtx_vBoneWeight` declarations (see gap 1 above) - needed before normal-mapped or skinned
   materials can render correctly, even once shader *compilation* succeeds.
2. This session's minimal combo (`UseUv`/`UseNormals`/`UseColor`/`UseDiffuse` only) doesn't
   exercise `UseNormalMapping`, `UseSkeleton`, `UseSway`, `UseColorMul`, `UseDissolve`,
   `MaterialType_Translucent`, or any of `deferred_base_vtx.hpsl`'s other real combos - each
   may surface its own unmapped type or construct the same way `cMatrix3f` did here (e.g.
   `cVector4l`/`cVector2l`, seen but not needed by this combo, used by the `UseSkeleton`/
   `UseTextureBuffer` paths - likely `ivec4`/`ivec2`, unverified). Widening
   `vMinimalMaterialCombo` (or adding more cases) and re-running the live self-test is the
   fastest way to find the next one.
3. This is still only proof that the transpiler's *output* is valid, compilable GLSL - it is
   not wired into `cGpuShaderManager::CreateShader()` (the real shader-loading path), which is
   the other agent's concurrent work per this session's task brief. Whoever finishes that
   wiring will need to pick concrete combo-variable defaults for real materials, and should
   reuse this session's `UseTextureBuffer`/`UseMeshInstancing`/`UseStaticMeshInstancing`-
   undefined strategy rather than rediscovering it.
