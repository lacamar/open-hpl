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

## SOMA: entity/area loaders, and the real sounddata.cfg story

This session's task: SOMA had the same "0 entities, 0 areas" gap as Rebirth/Bunker had
before their own loader fixes (see those sections above) - real static geometry/lights load
fine via `cWorldLoaderHpm`'s type-agnostic code paths, but every `<Entity>`/`<Area>` element
was silently dropped since nothing recognized SOMA's own Type/AreaType names. Also asked to
chase the "sounddata.cfg never loaded" gap documented elsewhere in this file (see the Bunker
section above and TASKS.md).

### Entity/area loaders (soma/src/game/SomaLoaders.{h,cpp})

Same shape as `rebirth/src/game/RebirthLoaders.{h,cpp}` - a generic
`cSomaGenericEntityLoader : public cEntityLoader_Object` (all real work already
type-agnostic in the base class; only `BeforeLoad`/`AfterLoad` hooks needed, and `AfterLoad`
only carries over the `CastShadows` instance-var override, same as Rebirth's/Dark Descent's
own `cLuxStaticPropLoader`), a `cSomaAreaLoader_PlayerStart` creating a real
`cStartPosEntity` via `cWorld::CreateStartPos()`, and a `cSomaAreaLoader_Noop` for
gameplay-only AreaTypes with no engine-side meaning yet. Registered from
`cSomaBase::InitEngine()` (`SomaBase.cpp`), same call site pattern as
`RegisterRebirthLoaders()` in `RebirthBase.cpp`.

**One real difference from Rebirth worth flagging for whoever ports Bunker's own loader
list next**: SOMA's per-map `.hpm_Entity` data carries no `Type` attribute of its own - each
`<Entity>` instance only references a `FileIndex` into a per-map file list of `.ent` paths
(`<File Id="87" Path="entities/urban/desk/desk_paper_crumbled/desk_paper_crumbled.ent"/>`),
and the actual entity-type name lives inside *that* `.ent` file's own
`<UserDefinedVariables EntityType="Prop_Grab">` element - confirmed by inspecting a real
`desk_paper_crumbled.ent`. So the Type list here was collected two ways, not just one:

1. A live headless boot of two real maps (`00_01_apartment.hpm`, `00_02_subway.hpm`),
   reading hpl.log's `"No loader for type 'X' found!"` / `"No loader for area type 'X'
   found!"` / `"SOMA hpm: no area loader registered for AreaType 'X'"` warnings directly.
2. A full census across the *entire* real SOMA install, which is more complete than any
   handful of sampled maps could ever be on its own (a per-map boot only exercises what that
   specific map happens to reference):
   ```
   grep -rohE 'EntityType="[^"]+"' entities/ | sort -u
   grep -ohE 'AreaType="[^"]+"' maps/*/*/*.hpm_Area | sort -u
   ```

The census found **46 entity types** (the two live-boot maps only exercised 16 of them) -
`StaticProp`, `Prop_Rigid`, `Prop_Grab`, `Prop_SwingDoor`, `Prop_Readable`, `Prop_Lamp`,
`Prop_LevelDoor`, `Prop_Lever`, `Prop_MoveObject`, `Prop_MovingButton`, `Prop_Terminal`,
`Prop_HandheldTerminal`, `Prop_Tool`, `Prop_Slide`, `StaticCollider` plus the two live maps'
`Critter_FishSmall`, and (census-only, not seen in either sampled map) `Prop_Button`,
`Prop_ConstructLure`, `Prop_Datamine`, `Prop_EnergySource`, `Prop_HudObject`, `Prop_Meter`,
`Prop_OmniSlot`, `Prop_PhysicsSlideDoor`, `Prop_PlayerHands`, `Prop_Push`, `Prop_SlideDoor`,
`Prop_Tear`, `Prop_Wheel`, plus the monster/NPC `Agent_*`/`Critter_*`/`critter_wau_swarm_
agent_fish` set (`Agent_Anglerfish`, `Agent_Construct_Crawler`, `Agent_Construct_Worker`,
`Agent_DeepseaSuit`, `Agent_Flesher`, `Agent_Humanoid`, `Agent_Humanoid_NPC`, `Agent_
Infected_Robot`, `Agent_Puppet`, `Agent_Remade`, `Agent_Roomba`, `Agent_Swarm`, `Agent_
SwimBot`, `Agent_Viperfish`, `Critter_CaveSpider`, `Critter_CrabSmall`, `Critter_CrabSpider`,
`Critter_Nautilus`). All 46 are registered with the same generic loader - an `Agent_*`
mesh with no AI/scripting layer to drive it (none exists in this Phase 0 scaffold) just
renders as inert static geometry instead of being silently dropped, exactly as inert as
every other entity type this scaffold loads; nothing gameplay-shaped should be inferred from
a monster's mesh being present in the scene.

The census found **24 AreaTypes** (again, more than the ~10 either sampled map alone
exercised): `PlayerStart` (real loader), plus no-op `AgentRepel`, `AmbientLight`,
`CameraAnimation`, `Climb`, `Crawl`, `Datamine`, `Description`, `Distortion`,
`DoorwayTrigger`, `EyeTrackingZoom`, `Hide`, `InteractAux`, `Ladder`, `Liquid`,
`MapTransfer`, `PathNode`, `Sit`, `Soundscape`, `Sticky`, `Tool`, `Trigger`,
`VisibilityArea`, `VisibilityPortal`, `Zoom`.

**Nice-to-have from the task brief, done**: `cSomaBase::LoadMap()` (and the `start_map`
headless command) now takes an optional `pos=` field - if given and the loaded map has a
`PlayerStart` Area of that name (populated by `cSomaAreaLoader_PlayerStart` while the map
was loading, same mechanism as Rebirth's `InitTestMap()`), the debug camera spawns there via
`cWorld::GetStartPosEntity()` instead of the caller-supplied `x`/`y`/`z` fallback.

**Verified end-to-end** (headless, real game data, branch `worktree-agent-a385d484e71b61dce`):

| Map | Before (entities, areas) | After (entities, areas) |
|---|---|---|
| `00_01_apartment.hpm` | 0, 0 | 425, 80 |
| `00_02_subway.hpm` | (not sampled before) | 214, 37 |

(both maps' static-object/light counts are unchanged by this fix, as expected - 145
static/70 lights for the apartment map, matching the pre-existing `PORTING_NOTES.md` SOMA
entry above). Zero `"no loader"`/`"no area loader"` warnings remain in `hpl.log` for either
map post-fix (the only remaining `"No loader for..."` lines are the pre-existing,
unrelated `"No loader for file extension 'fbx' found!"` gap already documented for Rebirth -
confirmed SOMA hits this too, ~11 instances, not investigated further this session).
`start_map map=00_01_apartment.hpm pos=PlayerStartArea_1 x=-10.75 y=1.7 z=8.25` followed by
`camera_state` reported `pos_y: 1.51415` - exactly the map's own authored
`WorldPos="-10.75 1.01415 8.25"` for `PlayerStartArea_1` plus the code's `+0.5` eye-height
nudge, confirming the real Area lookup path (not the `x`/`y`/`z` fallback) was actually used.

A headless screenshot after the fix (`00_01_apartment.hpm`) shows the scene rendering flat
magenta/black geometric shapes rather than a lit room - **this is the pre-existing SOMA
material-shader gap** (every `deferred_*.glsl` fails to load, `ERROR: Couldn't find file
'deferred_base_vtx.glsl' in resources` etc. - see the SOMA section above and the two
shader-focused sessions' TASKS.md entries), not a regression from this fix: the same broken
shading already applied to the 145 static objects before this session touched anything.
Confirming entity geometry specifically became visible (as opposed to just static
architecture) wasn't possible to eyeball from this particular screenshot's angle/shading -
the object counts in `hpl.log` are the more reliable evidence here.

### The real sounddata.cfg story (correcting an earlier assumption in this document)

TASKS.md previously suggested (based on the Bunker session's notes, itself echoing an
earlier session) that the "every `.hpm_Sound` reference fails" gap across all three Phase 0
modules was a small, self-contained fix - "wire whatever `cSoundHandler` API loads
`sounddata.cfg`, mirroring how Dark Descent's own `LuxBase.cpp` does it". Investigated for
real this session; **both halves of that premise are wrong**:

1. **`sounddata.cfg` does not exist as a file anywhere in the real SOMA, Rebirth, or Bunker
   Steam installs**, despite all three declaring `<ConfigFiles SoundData="sounddata.cfg"/>`
   in their `main_init.cfg`. Confirmed by an exhaustive `find` across all three real install
   trees - zero hits. Whatever a loader for it would have read, there was never anything to
   read.
2. **Dark Descent's own `LuxBase.cpp` never reads any `SoundData` config key at all** -
   confirmed by grepping it directly. Its `main_init.cfg` has no `SoundData` entry in the
   first place (only `Resources`/`Materials`/`Menu`/`PreMenu`/`Demo`/etc.) - it's an
   HPL3-era-only config key, not something Dark Descent's own code has ever had a reason to
   read. The "mirror `LuxBase.cpp`" suggestion had no real target to mirror.

**What's actually happening**: traced `cWorld::CreateSoundEntity()`
(`HPL2/core/sources/scene/World.cpp:960`) -> `cSoundEntityManager::CreateSoundEntity()`
(`HPL2/core/sources/resources/SoundEntityManager.cpp:74`), and confirmed it does exactly one
thing: resolve `<name>.snt` via the engine's ordinary resource-dir file searcher (the same
mechanism meshes/textures/everything else uses) - no `sounddata.cfg`-shaped config file is
involved in this path at all, at any level. So the real question became "why can't the file
searcher find these `.snt` files", and the answer is architectural, not a missing
registration: a real install's `.hpm_Sound` track references sound entities by names like
`"Entities_Urban/kitchen/fridge/hum_loop"` or `"physics/wood/robust/roll"` - and **no `.snt`
file by any of these names, or almost any name, exists anywhere in the real SOMA install**.
What *does* exist under `sounds/` is a set of **FMOD Studio/Designer banks**:
`sounds/entities/Entities_Urban.fev`/`.fdp`, `sounds/physics/...fsb`, etc. - `.fev`
(FMOD event project), `.fsb` (FMOD sound bank), `.fdp` (FMOD event project data) are
FMOD's own proprietary binary formats, completely unrelated to HPL2's OpenAL-backed,
individually-file-per-sound `.snt`-XML convention. Confirmed the same is true of Rebirth
(`sounds/creatures/creatures.fev`/`.fsb`) and Bunker (`sounds/creatures/creature.fsb`,
`sounds/physics/physics.fev`, etc.) - all three HPL3-generation games moved their sound
pipeline to FMOD entirely, matching the pattern already established for shaders (HPSL
instead of GLSL) and entity/area taxonomy (renamed types) - this is a third instance of the
same "HPL3 quietly replaced this HPL2 subsystem wholesale" shape, not a coincidence.

**Consequence**: there is no small fix here. `cSound`/`cSoundHandler`/`cSoundEntityManager`
as they exist in this codebase have no FMOD-awareness whatsoever - getting real sound
working in SOMA/Rebirth/Bunker needs either (a) a real FMOD bank reader wired into a new
`iSoundData`-equivalent backend (FMOD's runtime SDK is itself proprietary and non-free,
which may rule this out entirely for a GPLv3 project depending on licensing), or (b) an
offline re-encode of each bank's contained sounds into individual `.ogg` files plus
hand/script-generated `.snt` XML wrappers naming them the way `.hpm_Sound`/`.ent` files
expect - itself a real reverse-engineering task (FMOD bank internals aren't documented for
free extraction, though third-party bank-extraction tools exist). Either path is a
genuinely multi-week undertaking, not attempted this session - corrected the misleading
TASKS.md bullet rather than writing a `sounddata.cfg` loader that would have compiled,
looked plausible, and done precisely nothing against real game data (the file it would
load doesn't exist). Flagging this specifically so nobody re-derives the same wrong lead a
third time - if a future session has bandwidth for real sound in any of these three games,
start from "FMOD banks need a backend/re-encode", not "sounddata.cfg needs loading".

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

## SOMA: HPSL->GLSL transpiler wired into real shader loading (this session)

Prior sessions built the HPSL->GLSL transpiler proof-of-concept
(`soma/src/game/HpslTranspiler.{h,cpp}`) and extended its real-syntax coverage
(`mul()`/`sample()`, `px_vPosition`->`gl_Position`/`gl_FragCoord`, etc.), but it
only ever ran as a one-shot startup self-test
(`soma/src/game/HpslTranspilerSelfTest.cpp`, called from `cSomaBase::Init()`)
against a fixed list of seven small non-material `.hpsl` files - it was never
connected to the real shader-loading path, so every real material still hit
`ERROR: Couldn't find file 'deferred_base_vtx.glsl' in resources` and the
apartment map rendered fully black (confirmed via `start_map`, see the
"SOMA support" TASKS.md entries). This session's task was specifically the
*wiring*, not extending the transpiler's own feature coverage (that's a
separate concurrent agent's work, same session, extending
`soma/src/game/HpslTranspiler.cpp` - not touched here beyond reading its
header for the exact function signature).

**Where the fallback lives**: `HPL2/core/sources/resources/GpuShaderManager.cpp`,
`cGpuShaderManager::CreateShader()`. This one function has two independent
code paths depending on whether a `cParserVarContainer` is passed:
- The `apVarContainer != NULL` branch is the one real materials actually use
  (`cProgramComboManager::CreateShader()` in
  `HPL2/core/sources/graphics/ProgramComboManager.cpp` always passes one) -
  reads the file, runs it through `cPreprocessParser::Parse()` (the same
  `@ifdef`/`@include` preprocessor HPSL and GLSL both use), then compiles the
  parsed string.
- The `apVarContainer == NULL` branch is a plain resource-cache path (used by
  a handful of fixed shaders with no variable substitution) that skips
  preprocessing entirely and compiles straight from file.

Both branches previously did the same thing on a failed `mpFileSearcher->
GetFilePath(asName)` lookup: `Error("Couldn't find file '%s' in resources\n",
...)` and bail. Both now, on that same failure, try the same basename with a
`.hpsl` extension (`cString::SetFileExt(asName, "hpsl")`) via a second
`GetFilePath()` call before giving up. If found: read it, preprocess it with
`cPreprocessParser::Parse()` (using the caller's real `apVarContainer` in the
first branch, an empty one in the second - matching
`HpslTranspilerSelfTest.cpp`'s own convention), then hand the preprocessed
string to a registered transpiler callback. On transpile success, compile the
returned GLSL exactly like the pre-existing path did; on failure, a new,
distinctly-worded error (`"Couldn't transpile HPSL shader '%s' (from '%s'):
%s\n"`) reports why, with the transpiler's own reason text.

**The HPL2/core <-> soma/ dependency problem, and how it's resolved**:
`HPL2/core` is linked by every game module (Dark Descent, AMFP, SOMA, Rebirth,
Bunker) - it cannot `#include "HpslTranspiler.h"` or link
`soma/src/game/HpslTranspiler.cpp` directly, since that file is game-module
code (`using namespace hpl;`, SOMA-specific comments) and, more importantly,
only SOMA (for now) needs it at all. Resolved with a static function-pointer
hook: `HPL2/core/include/resources/GpuShaderManager.h` declares
`typedef bool (*tHpslTranspileCallback)(const tString& asHpslSource,
eGpuShaderType aType, tString& asGlslOut, tString& asErrorOut);` - deliberately
matching `TranspileHpslToGlsl()`'s exact signature - plus
`cGpuShaderManager::SetHpslTranspileCallback()` (static setter, NULL by
default) and a static `mpHpslTranspileCallback` member.
`cSomaBase::Init()` (`soma/src/game/SomaBase.cpp`) calls
`cGpuShaderManager::SetHpslTranspileCallback(TranspileHpslToGlsl)` right after
`InitEngine()` succeeds (window/GL context exists, before any real shader
load). Dark Descent/AMFP/Rebirth/Bunker never call this setter, so the
callback stays NULL for them and every new code path in `CreateShader()` is
gated behind an `if(mpHpslTranspileCallback)` check that's provably false -
dead code for every non-SOMA game, structurally, not just by convention.

**Zero-regression verification for Dark Descent** (this touches shared
`HPL2/core` code used by every game, so this was treated as the highest-risk
part of the task): built both `Amnesia` and `Soma` from one CMake configure
(`amnesia/src/CMakeLists.txt` pulls in both via `add_subdirectory`). Used
`git stash` to get a clean pre-change `Amnesia.bin.aarch64`, deployed it as
`Amnesia.baseline.aarch64` into the real Dark Descent Steam install, launched
headlessly (`OPENHPL_HEADLESS_SOCKET`), took a screenshot, `git stash pop` to
restore the change, rebuilt, deployed as
`Amnesia.worktree-agent-a98dec63e5599d81b.aarch64`, launched, took a matching
screenshot. Both runs produced **zero `hpl.log` output** (the file doesn't
even get created - `SetLogFile()`/`ReopenFile()` only ever happens on first
`Log()`/`Error()` call, per `LowLevelSystemSDL.cpp`, and neither run logged
anything). Raw byte-diffing the two BMP screenshots showed a large
difference (expected - Dark Descent's pre-menu scene has an animated
torch/particle flicker, so no two screenshots of it are ever byte-identical
even from the *same* binary run twice, confirmed by capturing two screenshots
of the unmodified baseline binary back-to-back: ~11% of raw bytes differ
between those two baseline-only runs too). The meaningful comparison is
ImageMagick `compare`'s perceptual pixel-difference count: baseline-vs-baseline
(same binary, two separate runs) = 6435 differing pixels (0.098%);
baseline-vs-wired (different binaries) = 6497 differing pixels (0.099%) - the
same order of magnitude, i.e. statistically indistinguishable from the
inherent animation noise. Screenshots of both diffs visually show the same
flickering-torch-glow pattern in the same screen region either way. Concluded:
no measurable rendering change for Dark Descent from this wiring.

**Verified live against SOMA's real `00_01_apartment.hpm`** (`start_map`
headless command, per the existing "SOMA: splash screens + real boot
sequence" section above): deployed `Soma.worktree-agent-a98dec63e5599d81b.
aarch64`, booted headlessly, called `start_map` with the apartment map's real
`PlayerStartArea_1` coordinates. `hpl.log` (shared path caveat below) now
shows, for every real material's `deferred_base_vtx.glsl` request:

```
ERROR: Couldn't transpile HPSL shader 'deferred_base_vtx.glsl' (from 'deferred_base_vtx.hpsl'): Unrecognised vertex input 'vtx_vTangent' - no known GLSL built-in mapping (see HpslTranspiler.h)
ERROR: Could not load material 'soliddiffuse' shader 'deferred_base_vtx.glsl'
```

instead of the old, terminal `Couldn't find file 'deferred_base_vtx.glsl' in
resources` - direct proof the `.hpsl` fallback is genuinely triggering:
`deferred_base_vtx.hpsl` is found, preprocessed, and handed to
`TranspileHpslToGlsl()`, which then rejects it for a specific, legible reason.
Also surfaced a second, different transpiler gap on
`deferred_gbuffer_solid_frag.hpsl`: `Couldn't parse parameter '		//diffuse
rgb' (expected '(in|out) TYPE NAME [: N]')` - a `//`-comment line inside a
parameter list is apparently mis-parsed as if it were itself a parameter.
Both are new findings for whoever's extending `HpslTranspiler.cpp`
(constant-buffer/instancing support was already known-blocking per TASKS.md;
these are two more, smaller, and probably easy relative to that).

**Still fully black** - as expected, and worth being precise about: a
before/after screenshot pair of the real apartment map at the same camera
position (before = this SOMA build with the wiring reverted via
`git stash`, after = with it restored) is **pixel-identical**
(`PIL.ImageChops.difference(a, b).getbbox() is None`). The wiring makes real
material shaders fail differently (a specific, actionable transpile error
instead of a dead end) but not yet *succeed* - every real deferred-rendering
material still needs the constant-buffer/instancing support and the two
smaller gaps above before anything actually compiles and renders. Checked
whether any of the *already-working* simpler shaders
(`deferred_depthonly_frag`/`deferred_posteffect_quad_vtx`/
`debug_overdraw_frag`/`null_vtx`/`null_frag`/`clear_vtx`/`clear_frag` - the
seven the self-test already proves compile) get requested through
`CreateShader()` during a real apartment-map frame, as a way to get
additional live-GL-compile proof of the wiring beyond the self-test; none of
them were requested this pass (grepped `hpl.log` for all seven basenames
across the whole `start_map` load+frame - zero hits), so the self-test remains
the only *actual* live-GL-compile evidence for those specific files; the
wiring's live-compile proof for real material shaders is blocked entirely on
the transpiler gaps above.

**New problem found and worth flagging explicitly**: SOMA's `hpl.log` path
(`~/.local/state/open-hpl/soma/hpl.log`, set in `cSomaBase::InitEngine()`) is
the same fixed path for every launched SOMA process regardless of which
build/worktree/agent started it, and `cLogWriter::ReopenFile()`
(`HPL2/core/sources/impl/LowLevelSystemSDL.cpp`) opens it with `fopen(...,
"w")` - full truncation. Multiple concurrent agents each headlessly testing
their own `Soma.<branch>.aarch64` build (a normal occurrence this session -
observed `Soma.agenta.aarch64`, `Soma.review.aarch64`, and another
worktree's build all running simultaneously) silently stomp on each other's
log content with no error to either side. Discovered this firsthand when
launching a SOMA test instance truncated a concurrent agent's in-progress
log mid-session. Added as a new top-level TASKS.md bullet (not fixed here -
out of this task's scope) - a real fix should make the log path unique per
process, e.g. keyed off `OPENHPL_HEADLESS_SOCKET`'s own path or the PID.
(Fixed later this session - see the "Suffix hpl.log with PID under headless
mode" commit.)

## SOMA: real geometry finally renders - matrix-uniform bug found and fixed; lighting still missing (this session, continued after the wiring above merged)

Task: the coordinator merged this session's constant-buffer work (`73fcb19`) and the
wiring above onto `master`, then ran `start_map` against real `00_01_apartment.hpm` with
real `.mat`-driven combo variables (not the self-test's manually-chosen ones) and found
`deferred_base_vtx.hpsl`/`deferred_base_frag.hpsl` now compile, but
`deferred_gbuffer_solid_frag.hpsl` failed at real GL compile time with `'px_fLinearDepth'
undeclared`. Asked to keep iterating directly against the live pipeline (`start_map` +
screenshots), fixing whatever real gaps it surfaces, until actual lit/textured rendering
works or a genuine wall is hit.

**Fixed three more real compile gaps, each found by actually running the live pipeline**
(not guessed at, not from the self-test's own chosen combos):

1. **`UseDepth`/`UseLinearDepth` combo-variable naming mismatch** (not a transpiler bug -
   fixed in `HPL2/core/sources/resources/GpuShaderManager.cpp`, the HPSL-fallback branch of
   `cGpuShaderManager::CreateShader()`). Root cause, confirmed by reading the actual combo
   variables reaching the transpiler (temporarily logged them - see history, not left in the
   final diff): this engine's own `cMaterialType_SolidDiffuse::LoadSpecificData()`
   (`HPL2/core/sources/graphics/MaterialType_BasicSolid.cpp`) unconditionally sets
   `"UseDepth"` - a name written for Dark Descent's own hand-written GLSL `@ifdef`
   vocabulary - but `deferred_base_vtx.hpsl`/`deferred_gbuffer_solid_frag.hpsl` gate their
   own (also unconditionally-*written*) linear-depth output behind `"UseLinearDepth"`
   instead, so the fragment body read an identifier nothing had declared. Aliased in the
   HPSL-fallback path only (`if(apVarContainer->Get("UseDepth")!=NULL)
   apVarContainer->Add("UseLinearDepth");` right before `Parse()`) - Dark Descent's own
   compiles never reach this branch, so this can't affect them. Turning that alias on then
   newly exercised `deferred_base_vtx.hpsl`'s `afInvFarPlane`/`afT`/sway/scrolling-noise/
   soft-particle/instancing-offset uniforms, all gated behind a *second* HPSL-only name,
   `"UseExtendedArgs"` (confirmed via `grep` to be the *only* place that name appears
   anywhere in the real `.hpsl` corpus, for exactly this purpose) - so that's now always
   added too for any HPSL compile (harmless when unused: every member it gates is a plain
   declaration, read only by code separately gated behind its own specific flag).
2. **`cTexture2DCmp`/`sampleCmp()`** (`soma/src/game/HpslTranspiler.cpp`) -
   `deferred_light_frag.hpsl`'s shadow-map lookup. Maps to `sampler2DShadow`/
   `shadow2D(tex, vec3(uv, refZ)).x`. Deliberately the *non-projective* `shadow2D` - this
   engine's own hand-written `deferred_light_frag.glsl` uses the projective `shadow2DProj`
   with a `vec4` (including a `.w` for the perspective divide), but every real `sampleCmp()`
   call site only ever passes 3 arguments and never reads a `.w` component at all, so there's
   no perspective-divide term in the HPSL source to preserve - inventing one would be
   unfaithful, not just unnecessary.
3. **`cVector2l`/`cVector3l`/`cVector4l` + `load()`/`texelFetch`** (same file) -
   `deferred_light_frag.hpsl`'s G-buffer readback,
   `load(aNormalDepthMap, vMapCoords, 0)`. The `cVector*l` types map to the same `ivecN` as
   the existing `cVector*i` (HPSL's other integer-vector spelling - both exist in the real
   corpus). `load(tex, coords, mip)` maps 1:1 to `texelFetch(tex, coords, mip)` - HLSL's
   `Texture.Load()` 3-argument shape matches GLSL's `texelFetch()` exactly, no argument
   reshuffling needed (unlike `sampleCmp` above). The catch: `texelFetch` needs GLSL 130,
   unavailable in this engine's GLSL 120 baseline - so `TranspileHpslToGlsl()` now bumps the
   emitted `#version` to 130, but **only** for the specific shader files where `load()`
   actually fires (tracked via a `bool& abFired` out-parameter on the new
   `RewriteLoadIntrinsic()`), never globally. GLSL 130 is still a compatibility-profile
   version (core profiles didn't exist until 150), so `gl_FragData`/`gl_FragCoord`/
   `varying`/`gl_Vertex`/`gl_MultiTexCoordN` all keep working unchanged in every shader that
   needs the bump - the version-conflict concern flagged in earlier passes (`gl_Vertex`
   removed in 150+ *core* profile) simply doesn't apply here, since nothing is requesting a
   core profile and this bump is fragment-shader-only in every real case found so far.

**Verified live after these three**: `hpl.log` for a real `start_map` run against
`00_01_apartment.hpm` shows **zero** `"Couldn't transpile"`/`"Failed to compile GLSL
shader"`/`"Couldn't create program"` errors for any currently-requested material shader -
`deferred_base_vtx.hpsl`, `deferred_base_frag.hpsl`, `deferred_gbuffer_solid_frag.hpsl`, and
`deferred_light_frag.hpsl` (once it's actually requested - see below) all compile clean.
6 new dependency-free regression cases added to `HPL2/tests/HpslTranspilerTests.cpp` (20
total at this point).

**But the screenshot told a different story than "compiling == working"** - exactly the
caution the coordinator's own instruction anticipated. At `PlayerStartArea_1`'s real
coordinates (`-10.75 1.7 8.25`, the same position a previous session verified matches the
map's authored `WorldPos`), every direction and every nearby camera position (including
directly beside a real light source, `WorldPos="-8.5 2.05 6"` from the map's own
`.hpm_Light` data) showed **only the skybox** - identical stormy clouds regardless of yaw/
pitch/position, meaning real solid geometry (145 static objects, confirmed loaded and
confirmed compiling clean) was never actually appearing on screen at all.

**Root-caused, not guessed at.** Added a temporary diagnostic
(`Log("...SolidObjectNum=%d", mpCurrentRenderList->GetSolidObjectNum())` in
`cRendererDeferred::RenderObjects()`, `HPL2/core/sources/graphics/RendererDeferred.cpp` -
removed again once done, not left in the final diff) and confirmed **~427-430 solid objects
were genuinely being submitted for rendering every frame** - ruling out an
occlusion-culling/frustum-culling explanation immediately. That pointed at the shaders
themselves producing degenerate output despite compiling. Reading this engine's own
`HPL2/core/sources/graphics/RenderFunctions.cpp` (`iRenderFunctions::SetMatrix()`, called
for every solid-object draw) confirmed the real cause: **this engine feeds every per-object
transform exclusively through the legacy OpenGL fixed-function matrix stack**
(`mpLowLevelGraphics->SetMatrix(eMatrix_ModelView, viewMatrix * modelMatrix)` and a separate
`eMatrix_Projection` call) - `SetMatrix()` computes `eMatrix_ModelView` as exactly `(view *
per-object model)`, matching this engine's own hand-written `deferred_base_vtx.glsl`
(a real Dark Descent install's copy), which does `gl_Position = ftransform();` - the GLSL
120 fixed-function built-in, using `gl_ModelViewMatrix` implicitly, **no custom `a_mtx*`
uniform anywhere**. There is no by-name uniform-setting call in this engine's C++ for
`"a_mtxModelViewProjection"`/`"a_mtxModelView"`/`"a_mtxProjection"`/`"a_mtxNormal"`
specifically (a real, working by-name mechanism *does* exist and *is* used for one other
matrix, confirmed: `MaterialType_BasicSolid.cpp`'s
`apProgram->SetMatrixf(kVar_a_mtxUV, apMaterial->GetUvMatrix())` for UV animation - that one
genuinely receives real data). Left as flattened plain uniforms (this session's earlier,
now-superseded behavior), `a_mtxModelViewProjection` etc. would sit at GLSL's default
all-zero value forever, transforming every real vertex to the origin - degenerate, invisible
geometry that **compiles perfectly fine**, which is exactly why nothing before an actual
render/screenshot check could have caught it, and exactly the gap the coordinator's
instruction to screenshot (not just check compile logs) was written to catch.

**Fixed**: new `SubstituteFixedFunctionMatrixUniforms()` in `HpslTranspiler.cpp` maps those
four specific, well-known HPSL uniform names to their exact GLSL 120 fixed-function
built-in equivalents (`gl_ModelViewProjectionMatrix`/`gl_ModelViewMatrix`/
`gl_ProjectionMatrix`/`gl_NormalMatrix`) and removes their now-redundant `uniform mat4
NAME;` declaration (whether that declaration came from `FlattenConstantBuffers()` or was
already a plain uniform in the source, e.g. the simpler `base_vtx.hpsl`'s own `uniform
cMatrixf a_mtxModelViewProjection;` - the same bug, independently confirmed to exist there
too, now also fixed by the same substitution). Deliberately **not** substituted:
`"a_mtxModel"` (a model-only, not model-view, matrix - no fixed-function equivalent exists,
since the legacy matrix stack only ever exposes the *combined* modelview state, never model
alone - a real, still-open gap, see below) and `"a_mtxUV"` (already correctly fed real data
through the different, working `SetMatrixf(kVar_a_mtxUV, ...)` mechanism mentioned above -
touching it would be both unnecessary and wrong).

**Verified live, dramatic before/after**: same camera positions as the "only skybox"
screenshots above, rebuilt and redeployed. **Before this fix**: pure skybox in every
direction, confirmed via four separate screenshots at three different positions/angles
(rotating yaw and pitch did rotate the visible sky correctly - ruling out a stale-screenshot
artifact - but never revealed anything but sky). **After**: the skybox is now largely gone,
replaced by real geometry silhouettes (rendered black/unlit - see below) that correctly
*occlude* the sky the way real solid geometry should - i.e. solid objects are now genuinely
being drawn in their correct world-space positions for the first time this port has ever
confirmed. A few bright magenta triangles remain visible at some angles - almost certainly
an unrelated, separate "missing shader" fallback-color artifact (magenta is the
conventional broken-material color in this engine's family), most likely the skybox mesh
itself or a light-volume debug shape hitting one of the filename-mismatch gaps noted below,
not investigated further this pass.

**Existing tests updated, not just added to**: `TestNullPair`/`TestConstantBufferFlattening`/
`TestMatrix3f` all happened to use one of the four now-substituted names as an incidental
placeholder in their own fixture text (testing unrelated things - uniform pass-through,
cBuffer flattening, the `cMatrix3f`→`mat3` mapping) - each updated to either assert the new
(correct) substituted output or renamed its fixture uniform to something outside the
substituted set (e.g. `TestMatrix3f`'s uniform renamed `a_mtxCustomNormal`) so it keeps
testing only its own original, narrower point. One new dedicated test,
`TestFixedFunctionMatrixSubstitution`, added. 21 cases total, all passing.

**Still not lit - next, concretely narrowed-down blocker, NOT a transpiler-syntax problem**:
geometry now renders, but as flat black silhouettes, not diffuse-lit. Investigated with the
same "add a temporary counter, read hpl.log, don't guess" discipline used above:
- A completely fresh process (ruling out any shader-cache pollution from an earlier scene),
  `start_map` against `00_01_apartment.hpm`, camera placed directly beside a real light
  source: `deferred_light_frag.hpsl` is **never requested at all** - confirmed via `grep -oE
  "[A-Za-z_0-9]+\.hpsl"` over the fresh run's log, waited 11+ seconds (many frames) to rule
  out a query/warm-up-frame timing artifact.
- This is **not** because zero lights are visible/culled: a second temporary diagnostic
  (`Log("...GetLightNum=%d", mpCurrentRenderList->GetLightNum())` at the top of
  `cRendererDeferred::SetupLightsAndRenderQueries()`, also removed again, not left in the
  final diff) showed **37 lights** genuinely reaching the render list every single frame at
  this exact camera position. So lights are found and considered visible, but something
  between that point and the actual per-light draw call (which would request
  `deferred_light_frag.hpsl`) drops all 37 of them before any of `RenderLights()`'s five
  dispatch functions (`RenderLights_Box_StencilFront_RenderBack`/
  `RenderLights_Box_RenderBack`/`RenderLights_StencilBack_ScreenQuad`/
  `RenderLights_StencilFront_RenderBack`/`RenderLights_RenderBack` - each apparently handling
  a different light-type/size/shadow bucket) ever calls `SetProgram()`/`CreateShader()` for
  one. **Deliberately not chased further this pass**: `InitLightRendering()` (the function
  that sorts `mvTempDeferredLights` into `mvSortedLights[eDeferredLightList_*]` buckets
  consumed by those five dispatch functions,
  `HPL2/core/sources/graphics/RendererDeferred.cpp:1829`) is the concrete next place to
  look - but it's shared HPL2 core rendering code used by *every* module including Dark
  Descent, a materially higher-risk piece of code to touch speculatively than the SOMA-only
  HPSL transpiler work above, and diagnosing it properly (is it a screen-space-area
  computation returning 0, a near-plane test, a shadow-quality gate, something else
  entirely) needs more instrumentation/time than remained this pass. A real, well-scoped,
  concrete next task for whoever continues - not a vague "lighting is broken."
- Also found, **not yet fixed, a separate class of gap** (filename mismatches, not shader
  syntax): several shader requests in this engine's C++ ask for exact `.glsl` filenames that
  don't exist among the real `.hpsl` corpus's actual names at all (so neither the direct
  `.glsl` lookup nor the `.hpsl`-fallback lookup finds anything) -
  `MaterialType_BasicSolid.cpp:330` requests `"deferred_illumination_frag.glsl"` (real file:
  `deferred_illumination_solid_frag.hpsl`, likely only affects self-illuminating/emissive
  materials specifically, not general lighting), `RendererDeferred.cpp:413` requests
  `"deferred_gbuffer_skybox_frag.glsl"` (real file: `deferred_skybox_frag.hpsl`),
  `MaterialType_Decal.cpp:100` requests `"deferred_decal_frag.glsl"` (real file:
  `deferred_gbuffer_decal_frag.hpsl`), and `PostEffect_Bloom.cpp:59` requests
  `"posteffect_bloom_blur_vtx.glsl"` (real file, if any, not confirmed -
  `posteffect_bloomhdr_blur_frag.hpsl` is the closest real name, note also `_frag` not
  `_vtx`). Same shape of fix
  as the `UseDepth`→`UseLinearDepth` alias above (a small filename-alias table somewhere in
  the HPSL-fallback path), but for filenames instead of combo-variable names - not attempted
  this pass, added as a new TASKS.md bullet.

**Build/verify**: `cmake --build amnesia/src/build --target HpslTranspilerTests Soma
-j$(nproc) && ctest --test-dir amnesia/src/build -R HpslTranspilerTests
--output-on-failure` (21/21 passing). Live re-verification used a scratch directory with
real SOMA data reached via symlinks (`/tmp/soma-live` this session, not committed/left
behind), `OPENHPL_HEADLESS_SOCKET` for headless mode, and `XDG_STATE_HOME` overridden to a
private scratch path each launch (still necessary - see the "hpl.log path collides" note
above, now fixed on `master` via PID-suffixing but a private `XDG_STATE_HOME` is still the
simplest way to get a clean, predictable log path per test run without needing to discover
the PID first). Both real fix commits (`636560f` "fix real compile gaps", `0ec59d7`
"substitute fixed-function matrix built-ins") are on branch
`worktree-agent-a616b92d063d83a8f`, not merged/pushed - left for the user to review per this
session's standing instruction.

**Concrete next steps, most-valuable first**:
1. `InitLightRendering()`/`RenderLights_*` dispatch - why 37 correctly-identified visible
   lights never reach a single `CreateShader()` call. The single most valuable remaining
   step toward real lit rendering; needs care since it's shared HPL2 core code.
2. The filename-mismatch table (illumination/skybox/decal/bloom-blur) - smaller in scope,
   same shape of fix as the `UseDepth` alias, affects secondary rendering (emissive glow,
   sky, decals, bloom) rather than base lighting.
3. `a_mtxModel` still has no working uniform-setting path (no fixed-function equivalent,
   and no by-name `SetMatrixf` call for it found) - currently only matters for the
   `UseSway`/`UseDetailMesh`/`UseStaticMeshInstancing` combos this session's testing never
   exercised (the "Position - Default" non-instanced path this session verified doesn't
   reference it), but will need solving before those combos work.
4. The C++ mesh-upload wiring for `vtx_vTangent`/`vtx_vBoneIndices`/`vtx_vBoneWeight` (flagged
   in the earlier "SOMA: HPSL constant buffers" section) remains unaddressed - now more
   clearly the next thing standing between "flat-lit geometry" and "normal-mapped geometry",
   once (1) above is solved.

## SOMA: real-time lights now render lit geometry - three real uniform/binding gaps found and fixed, not a dispatch bug (this session)

Task: root-cause and fix the "SOMA: real-time light rendering never invokes
`deferred_light_frag.hpsl` at all" bullet from TASKS.md/the previous session's PORTING_NOTES
entry above - the hypothesis being that `InitLightRendering()`
(`RendererDeferred.cpp:1829`) was dropping all 37 real lights into no bucket before any of
`RenderLights()`'s five dispatch functions ever called `SetProgram()`/`CreateShader()`.

**First finding: that premise no longer holds on current `master`.** Before touching
anything, added temporary `Log()` instrumentation (removed again before the final commit,
same discipline as prior sessions) to `InitLightRendering()` and both
`RenderLights_StencilFront_RenderBack()`/`RenderLights_RenderBack()`, then ran a fresh
headless process against real `00_01_apartment.hpm` (`start_map ... pos=PlayerStartArea_1`).
Result: `InitLightRendering()` genuinely buckets 22 of the 37 lights every frame
(`RenderBack=12 StencilFront_RenderBack=2 Box_RenderBack=6 Box_StencilFront_RenderBack=2`,
the rest correctly culled by the existing large-light occlusion-query mechanism), and
`SetupProgramAndTextures()` (`RendererDeferred.cpp:1413`) *does* call
`mpProgramManager->GenerateProgram(eDefferredProgramMode_Lights, lFlags)` every frame,
returning real non-NULL program pointers for several distinct combo-flag values (61, 109,
33, 5, 37 observed) - `deferred_light_frag.hpsl` transpiles and compiles clean, zero
"Couldn't transpile"/"Couldn't create program" errors anywhere in the log. Whatever produced
the original "never invokes it at all" symptom (most likely a transpile-time failure in an
earlier, less-complete state of `HpslTranspiler.cpp` that has since been fixed by later
commits, e.g. the matrix-uniform-substitution fix two sessions ago) is no longer present -
this is a real, empirically-checked finding, not a guess, and the stale TASKS.md bullet is
corrected below rather than left to mislead the next reader.

**So the dispatch bug is resolved already; the actual remaining cause of flat-black/wrong
rendering was three separate, real uniform/binding gaps between HPSL's actual shader source
and this engine's C++ uniform-feeding code** - found by dumping the transpiled
`deferred_light_frag.glsl` output to a scratch file (temporary `fopen`/`fwrite` in
`GpuShaderManager.cpp`, removed again) and reading it against the real
`deferred_light_frag.hpsl` source and the real (working, Dark-Descent-native)
`deferred_light_frag.glsl` side by side:

1. **`avScreenToFarPlane`/`avInvScreenSize` never set at all (affects every light type).**
   HPSL's `GetPos(vec2 avUV, float afDepth)` reconstructs view-space position purely from
   `gl_FragCoord.xy` and these two uniforms - a completely different technique from Dark
   Descent's own `deferred_light_frag.glsl`, which instead interpolates a per-vertex
   far-plane ray (`gvFarPlanePos`, a `varying` computed in the vertex shader from
   `afNegFarPlane` and a tangent term). Since HPL2's C++ was written for the varying-based
   technique, it never had a reason to set either of these two uniform names anywhere -
   confirmed via `grep -rn "ScreenToFarPlane\|InvScreenSize" HPL2/core/`, zero hits before
   this session's fix. Left at GLSL's default zero, `GetPos()` collapsed to a degenerate
   on-axis position independent of screen location. Fixed in `SetupProgramAndTextures()`
   (`RendererDeferred.cpp`): registered two new `kVar_*` ids
   (`avScreenToFarPlane`/`avInvScreenSize`) for `eDefferredProgramMode_Lights` and set them
   every light draw from the same `mfFarLeft`/`mfFarRight`/`mfFarTop`/`mfFarBottom`/
   `mvRenderTargetSize` fields `SetupRenderVariables()` already computes once a frame for
   the (already-working) full-screen light quad path - mapping raw pixel coordinates
   directly onto the far-plane rectangle, mirroring the exact quad `UpdateqQuadVertexPostion`
   already builds from the same fields elsewhere in this file.
2. **Spotlight view-projection matrix registered under the wrong uniform name.** HPL2's C++
   registers/sets `a_mtxSpotViewProj` (matching Dark Descent's own real
   `deferred_light_frag.glsl`, confirmed via `grep` on the real installed file) - but SOMA's
   real `deferred_light_frag.hpsl` declares this uniform as `a_mtxLightViewProj` (confirmed
   via `grep -n "a_mtxLightViewProj" .../core/shaders/hpsl/deferred_light_frag.hpsl`, real
   SOMA install), and the transpiler passes uniform names through verbatim - no alias exists,
   same shape of gap as the earlier `UseDepth`→`UseLinearDepth` combo-variable alias, just for
   a uniform name instead of a preprocessor variable. `GetVariableAsId()`
   (`GLSLProgram.cpp:200`) fails closed (returns `false`, no crash) for a name that doesn't
   exist in the compiled program, so this was a silent no-op, not an error - the real
   `a_mtxLightViewProj` uniform stayed at its GLSL zero-matrix default forever. Every spot
   light's `vProjectedUv = mul(px_mtxLightViewProj, vec4(vPos,1))` therefore divided by a
   zero `.w`, producing NaN, which propagated through `fAttenuatuion *= max(0,
   vProjectedUv.z)` and onward into every spot light's whole contribution (max-with-NaN is
   driver-defined in GLSL, but a NaN pixel value reliably shows as black or garbage). Also
   found while fixing this: HPL2's C++ only ever set the (any-name-spelled) matrix `if
   (pLight->GetGoboTexture() || apLightData->mbCastShadows)`, but HPSL's real
   `@ifdef LightType_Spot` branch (both its `UseGobo` and no-gobo halves) reads this matrix
   *unconditionally* just to derive the near-clip/cone-edge attenuation term - Dark Descent's
   own shader apparently doesn't need it outside gobo/shadow, HPSL's does for every spot
   light. Fixed: `SetupLightProgramVariables()` now always computes and sets
   `kVar_a_mtxLightViewProj` for any spot light (the pre-existing `kVar_a_mtxSpotViewProj`
   call, still gated behind gobo/shadow, is left untouched for Dark Descent's own shader).
3. **Every fragment-shader sampler silently bound to texture unit 0 (the most severe of the
   three - this produced genuinely garbled/rainbow output, not just wrong brightness).**
   HPSL declares each texture uniform's binding as a D3D-style `uniform cTextureX NAME : N;`
   suffix, which `HpslTranspiler.cpp`'s `StripUniformBindingIndices()` discards entirely (by
   design - GLSL 120 has no such syntax) with nothing downstream to consume the index instead.
   Dark Descent's own hand-written `.glsl` shaders instead rely on a `@define sampler_NAME N`
   preprocessor line (`PreprocessParser.cpp:698`'s `@define` handling), parsed into
   `cPreprocessParser`'s own variable container and consumed right after compile in
   `GpuShaderManager.cpp`'s "Sampler to texture units setup" block
   (`pShader->AddSamplerUnit(name, unit)`, consumed at link time by
   `cGLSLProgram::Compile()`, `GLSLProgram.cpp:111-130`, via `glUniform1i`). HPSL source has
   no `@define` lines at all, so that block found nothing for every HPSL-derived fragment
   shader and silently skipped calling `AddSamplerUnit()` for any sampler - meaning
   `aDiffuseMap`/`aNormalDepthMap`/`aSpecMap`/`aShadowMap`/`aShadowOffsetMap` (5 distinct
   samplers in `deferred_light_frag.hpsl` alone) all defaulted to GLSL's implicit unit 0,
   each reading whatever `SetTexture(0, ...)` had most recently bound there - visually,
   diffuse color, normal/depth data, specular data and shadow data all sampling the *same*
   single texture, producing exactly the rainbow/garbage noise pattern seen mid-session
   (screenshot evidence: a colorful noisy band along solid-geometry edges, completely
   different in *kind* from the flat-black/flat-white symptoms of gaps 1-2, not just degree).
   Fixed with a new `ApplyHpslTextureBindings()` helper in `GpuShaderManager.cpp`: regexes
   `uniform\s+cTexture\w*\s+(\w+)\s*:\s*(\d+)\s*;` out of the *pre-transpile* HPSL text
   (saved off right before the transpile callback overwrites it - the `: N` suffix is still
   present there, only `HpslTranspiler.cpp`'s output has it stripped) and calls the exact
   same `iGpuShader::AddSamplerUnit()` the `@define` path already uses, in both HPSL-fallback
   branches of `CreateShader()` (the `apVarContainer` branch materials/lights use via
   `cProgramComboManager`, and the plain-resource branch). Lands on the same consumer as the
   pre-existing mechanism, so no new code path for `cGLSLProgram::Compile()` to trust.

**Also found, and given a fixed non-authored default rather than left broken**: HPSL's
`afFalloffPow`/`afSpotFalloffPow` (exponents on the linear "1 - dist/radius"/cone-edge
attenuation terms) have no HPL2-native equivalent to read - HPL2's own `cLight`/`cLightSpot`
classes have no per-light falloff-exponent field at all (the original engine did radial
falloff via an authored 1D texture curve, `iLight::GetFalloffMap()`, which this HPSL shader
doesn't even sample). Left unset, both default to GLSL's `0`, and `pow(x, 0) == 1` for any
`x` - i.e. *no* falloff shaping at all within a light's radius/cone, full unattenuated color
everywhere it reaches. With the position-reconstruction and sampler-binding fixes above
actually landing correctly, this alone was enough to saturate the accumulation buffer to
solid white in the same `00_01_apartment.hpm` scene (all three fixes were needed together to
get *any* meaningfully different image from flat black; this one was needed on top to get a
non-blown-out one). No authored per-light value exists anywhere in this port's data model to
read instead, so `SetupProgramAndTextures()` now sets both to a fixed `2.0` - a reasonable,
clearly-commented placeholder, not a derived-from-data value.

**Verified live, headless, real SOMA install** (`00_01_apartment.hpm`,
`scripts/headless-check.sh` + `scripts/hpl_control.py`, same pattern as prior sessions):
- **Before this session's fixes** (current `master`, i.e. after the matrix-uniform fix but
  before this session's three fixes): screenshot near a real light shows solid black
  background, flat-shaded magenta triangles (separate, already-delegated bug, not touched),
  faint white dots (debug gizmos) - matches the "still broken" baseline described in this
  session's task brief exactly.
- **After fix 1+2 only** (position reconstruction + spotlight matrix, sampler binding not
  yet fixed): visibly different from flat black - a colorful noisy/rainbow band appears along
  real geometry edges - but clearly wrong (garbled, not lit).
- **After all three fixes + the falloff-exponent default**: real per-pixel lighting/shading
  gradient visible and tied to actual geometry - moving the camera away from the
  camera-beside-a-light test position (`set_camera` to a spot further into the room) shows a
  visible warm-toned vertical-stripe gradient across a wall/paneling surface, not a flat
  color - genuine per-pixel variation, not just "not black". The scene is still overexposed
  (large areas saturate to white) close to the many real lights in this small apartment
  room with the placeholder `2.0` falloff exponent and no tone-mapping/exposure step in this
  fallback pipeline - a real, separate follow-up (see below), but squarely a *tuning*
  problem now, not a "the pipeline doesn't work" one.
- **Dark Descent regression check**: built `Amnesia` from the same tree, deployed as
  `Amnesia.lightfix.aarch64` into the real Dark Descent Steam install, headless-booted to the
  Profiles menu screen (the same known-good scene with the real torch spotlight used by
  earlier sessions) - `hpl.log` has zero lines (this game only writes the file on a
  warning/error at all, so an absent file is the strongest possible zero-regression signal),
  and the screenshot shows the torch's warm point-light falloff rendering exactly as before -
  correct gradient, correct shadowing, no white blowout, no black. Expected: every new
  `kVar_*`/uniform name this session added is either only read behind SOMA's own HPSL
  transpile path, or (for the `a_mtxSpotViewProj`→also-`a_mtxLightViewProj` and sampler-unit
  changes) a harmless no-op against Dark Descent's own real shader/HPSL text, confirmed by
  `GetVariableAsId()`/the new binding-regex both failing closed for names/patterns that don't
  exist in DD's own file - Dark Descent never takes the HPSL-fallback branch at all
  (`mpHpslTranspileCallback` stays `NULL` for it), so its own light rendering path is
  provably untouched by any of this beyond the harmless new registrations.
- **Regression tests**: `ctest --test-dir amnesia/src/build --output-on-failure` - all 4
  tests pass (`PhysicsNewtonTests`, `CStringTests`, `PlatformXdgPathTests`,
  `HpslTranspilerTests`), no changes needed to any of them (this session's fixes are all in
  `RendererDeferred.cpp`/`GpuShaderManager.cpp`, not `HpslTranspiler.cpp` itself).

**Files changed**: `HPL2/core/sources/graphics/RendererDeferred.cpp` (new `kVar_*` ids;
`avScreenToFarPlane`/`avInvScreenSize`/`afFalloffPow`/`afSpotFalloffPow` registration and
per-light-draw `Set*()` calls in `SetupProgramAndTextures()`; `a_mtxLightViewProj`
registration and always-on set in `SetupLightProgramVariables()`'s spot-light branch),
`HPL2/core/sources/resources/GpuShaderManager.cpp` (new `ApplyHpslTextureBindings()` helper,
called from both HPSL-fallback branches of `CreateShader()`).

**Concrete next steps**:
1. Real exposure/tone-mapping for the HPSL light-accumulation path, or a data-driven
   per-light falloff exponent, to fix the remaining overexposure - cosmetic/tuning now, not
   a "pipeline broken" bug. `deferred_light_frag.hpsl`'s `gl_FragData[0].xyz = vDiffuse *
   8.0;` "increase precision" scale-up implies a matching `/8.0` (or a real tonemap curve)
   is expected downstream in whatever composites the accumulation buffer into the final
   image - worth checking whether that composite step is HPSL-derived too (another gap of
   the same shape as this session's) or genuinely missing entirely.
2. The already-known filename-mismatch table (illumination/skybox/decal/bloom-blur, see the
   previous session's PORTING_NOTES entry above) and the mesh-upload wiring for
   `vtx_vTangent`/`vtx_vBoneIndices`/`vtx_vBoneWeight` are both still open, independent of
   this session's work.
3. `ApplyHpslTextureBindings()`'s regex only matches `uniform cTextureX NAME : N;` -
   confirmed sufficient for every sampler `deferred_light_frag.hpsl` declares, but not
   audited against the rest of the 77-file real `.hpsl` corpus; worth a quick grep sweep
   (`grep -rn "uniform cTexture" core/shaders/hpsl/`) if a not-yet-exercised material shader
   turns out to have samplers in an unexpected declaration shape (e.g. split across multiple
   lines) the regex doesn't match - it would fail closed (no binding set, back to the unit-0
   bug for that one sampler) rather than crash, same as before this fix existed.

## SOMA: shader-filename/sampler-binding fixes, and the magenta skybox artifact finally root-caused (this session)

Two pieces of work, the second finishing what the first's investigation started.

### Filename aliases, HPSL fallback resource-dir gap, real sampler-binding bug (commit `dea9ad5`)

- Added a small filename-alias table to `GpuShaderManager.cpp` (same shape as the existing
  `UseDepth`->`UseLinearDepth` combo-variable alias) for three real Dark-Descent-vs-HPSL
  naming mismatches documented in TASKS.md: `deferred_illumination_frag.glsl` ->
  `deferred_illumination_solid_frag.hpsl`, `deferred_gbuffer_skybox_frag.glsl` ->
  `deferred_skybox_frag.hpsl`, `deferred_decal_frag.glsl` -> `deferred_gbuffer_decal_frag.hpsl`.
  `posteffect_bloom_blur_vtx/frag`/`posteffect_bloom_add_frag` deliberately left unaliased -
  no real HPSL equivalent exists anywhere in the corpus (confirmed by searching it in full).
- Found the real cause of the "residual `deferred_base_vtx.glsl` lookup failures" bullet in
  TASKS.md: `cGraphics::Init()`'s hardcoded `core/shaders` resource-dir registration (needed
  before a game module's own `resources.cfg` loads, since several renderers/material
  types/post-effect types build GPU programs directly in their own constructors) registered
  the directory non-recursively. Harmless for Dark Descent/AMFP (no subdirectories under
  `core/shaders/` in their real installs), but SOMA/Rebirth/Bunker's real HPSL source lives one
  level down at `core/shaders/hpsl/` - so every shader built during this early bootstrap
  window failed to find its `.hpsl` fallback file at all. Fixed by registering recursively.
- `PostEffect_Bloom.cpp`: bail out gracefully (return the input texture untouched) when any of
  its three GPU programs failed to build, true on the HPSL path since bloom has no real HPSL
  equivalent. Without this, `SetProgram(NULL)` on a missing program just unbinds whatever
  shader was last active rather than skipping the draw - full-screen quads still got submitted
  through the fixed-function pipeline with stale GL state, contributing to the magenta/white
  artifact investigated below.
- Two more real HPSL vertex/fragment combo-variable mismatches found once the resource-dir fix
  let these shaders actually get requested: `deferred_skybox_frag.hpsl` and
  `posteffect_radial_blur_frag.hpsl` both unconditionally read varyings (`px_vColor`,
  `px_vTexCoord0`) that `deferred_base_vtx.hpsl` only emits when `UseColor`/`UseUv` are set,
  which these call sites never set. Fixed at the call site; confirmed harmless for Dark Descent
  by reading its own real `.glsl` pair for each (neither references the corresponding built-in).
- The real systemic bug: HPSL's `uniform cTextureX aName : N;` D3D-style texture-unit-binding
  suffix was stripped and discarded during transpile with no replacement - GLSL 120 has no
  in-shader way to express it, so it has to reach a real `glUniform1i()` call after linking,
  which `GpuShaderManager.cpp` already does via `iGpuShader::AddSamplerUnit()`, but only by
  scanning the preprocessor's own `sampler_NAME=N` parsed-variable map - a convention driven by
  `@define sampler_NAME N` lines in Dark Descent's own hand-written `.glsl`, which HPSL's `: N`
  syntax has no way to produce. Every HPSL fragment shader's samplers were silently defaulting
  to texture unit 0. `TranspileHpslToGlsl()` now takes an `asSamplerBindingsOut` parameter that
  `GpuShaderManager.cpp` applies directly via `AddSamplerUnit()`, bypassing the
  preprocessor-var scan for the HPSL path. All 15 `HpslTranspilerTests.cpp` call sites updated
  for the new signature; `ctest` stays green.
- Verified live against a real headless boot of `00_01_apartment.hpm`: decal/skybox/
  illumination filename-alias errors gone, `deferred_base_vtx.glsl` "Couldn't find file" errors
  gone (22 -> 0, only the three genuinely-missing bloom files remain), zero "Failed to link"
  errors remain.

### The magenta full-screen artifact, root-caused: fixed-function skybox + missing fog uniforms

The investigation above left a real, separate symptom open: a headless screenshot of
`00_01_apartment.hpm` showed two large, flat-shaded magenta triangles, and - discovered by a
parallel coordinating session doing its own verification pass - the **entire frame was
overexposed near-white and completely unresponsive to camera position/orientation**: repeated
screenshots from independently-launched processes at different camera poses were
byte-for-byte pixel-identical, which is not possible for real camera-dependent 3D rendering.
That second symptom was the real clue - it meant a screen-space-independent, full-frame draw
was swamping everything else.

Root cause, found by bisecting which render pass produces full-frame, camera-independent
output: `iRenderer::RenderBasicSkyBox()` (`Renderer.cpp`, shared base class, called from every
renderer's `RenderObjects()`) draws the world's skybox cubemap through the fixed-function
pipeline - `SetProgram(NULL)`. That's correct-enough behavior on the mature desktop OpenGL
drivers this engine originally shipped against, but on this project's real test platform
(Mesa's AGX driver, Asahi Linux/Apple Silicon), fixed-function rendering of a bound
`GL_TEXTURE_CUBE_MAP` with no shader produced solid, wrong, saturated color instead of actually
sampling the cubemap - and, being a skybox, it's drawn at infinite depth behind everything,
filling the entire frame regardless of camera orientation (explaining both the magenta
triangles - the visible facets of the skybox mesh - and the complete camera-independence, since
a skybox's world-space geometry is deliberately camera-rotation-invariant by design, it just
should be *sampling a real texture*, not flat-shading). Confirmed this is SOMA-specific in
practice, not a latent Dark Descent bug: no real Dark Descent map ships a non-empty
`SkyBoxTexture` (`SkyBoxActive` is false everywhere in the real Dark Descent install), while
`00_01_apartment.hpm` has both `SkyBoxActive=true` and a real cubemap set.

Fix: added `iRenderer::GetSkyBoxProgram()` (`Renderer.h`, defaults to `NULL` - unchanged
fixed-function behavior for any renderer that doesn't override it) and
`cRendererDeferred::GetSkyBoxProgram()` (`RendererDeferred.h`, returns the real, working
`mpSkyBoxProgram` this renderer already builds in `LoadData()` for its own now-dead
`RenderDeferredSkyBox()` pass - see the existing comment on that function, it's been commented
out since this engine shipped, so `mpSkyBoxProgram` was unused dead weight until now).
`RenderBasicSkyBox()` now calls `SetProgram(GetSkyBoxProgram())` instead of
`SetProgram(NULL)`.

Fixing the skybox surfaced a second, same-shape bug one layer down: `deferred_fog_frag.hpsl`
(SOMA's real fog shader, both the full-screen and per-area variants) unconditionally
reconstructs view-space position from `gl_FragCoord` via `avScreenToFarPlane`/`avInvScreenSize`
- the exact same uniforms and formula as `deferred_light_frag.hpsl`'s already-fixed case (see
the "real-time lights now render lit geometry" section above) - but neither was ever
registered/set for the fog program. Fixed identically: registered both variable ids on
`mpFogProgramManager` and set them from the same `mfFarLeft/mfFarRight/mfFarTop/mfFarBottom`/
`mvRenderTargetSize` values already computed once per frame, in both `RenderFullScreenFog()`
and the per-`cFogArea` variant.

**Verified live, headless, real `00_01_apartment.hpm`, both fixes together**: the magenta
triangles are completely gone. Camera-independence is gone too - confirmed by diffing two
screenshots taken from the same process at the same position but different yaw
(`ImageChops.difference(...).getbbox()` now reports a real, full-frame difference, not `None`)
- proof the renderer is genuinely responding to the camera again, not just "no more magenta by
coincidence." FPS also recovered to ~22 (previously observed as low as ~3-8 FPS in the same
scene before this fix, likely the fixed-function skybox path forcing extra state
changes/driver fallback work every frame on this GPU). The scene is still overexposed
near-white - the same real, separate tone-mapping/exposure follow-up already flagged in the
"real-time lights" section above, not a new regression from this fix; a few faint vertical
streaky texture patterns and small debug-gizmo dots are visible through the overexposure,
consistent with real (if blown-out) geometry rendering underneath rather than a solid clear
color.

### Follow-ups

- Tone-mapping/exposure: SOMA's real lit geometry is now visible but overexposed white in most
  of the frame at the real `PlayerStartArea_1` spawn (close to multiple real lights). Not yet
  investigated - likely needs whatever real HDR tonemap/exposure pass HPSL's pipeline expects
  that this scaffold doesn't yet perform, or a real per-light intensity/attenuation tuning gap
  beyond the `afFalloffPow`/`afSpotFalloffPow` fixed defaults already added.
- `posteffect_bloom_*`: still genuinely absent from the real HPSL corpus (confirmed by a full
  corpus search this session) - bloom just doesn't run for SOMA at all right now (graceful
  no-op via the `PostEffect_Bloom.cpp` fix above), which is fine for correctness but means
  SOMA's real bloom look is missing. Would need either a hand-written GLSL bloom shader pair
  bypassing the HPSL fallback entirely, or confirmation from a real disassembly of SOMA's own
  shipped shader binaries (if any exist beyond the `.hpsl` source tree) that no such pass ships.
- `RenderDeferredSkyBox()` in `RendererDeferred.cpp` remains genuinely dead code (its call site
  has been commented out since this engine shipped) - `mpSkyBoxProgram` is now used by the
  fix above via `GetSkyBoxProgram()`, but the dedicated duplicate pass itself is still
  unreachable and could be deleted as cleanup, not attempted this session to keep the diff
  minimal.

## SOMA: real global exposure applied (ExposureArea), and a correction about the skybox fix (this session, continued)

### Global exposure from real .hpm_ExposureArea data

Once the skybox fix above landed and camera-responsiveness was restored, a fresh headless
screenshot of `00_01_apartment.hpm` at its real `PlayerStartArea_1` was still severely
overexposed near-white - the "real, separate tone-mapping/exposure follow-up" already flagged
by the lighting-fix session. Investigated properly rather than left as a guess: SOMA's real map
data ships a `.hpm_ExposureArea` sidecar file (`HPLMapTrack_ExposureArea`) this engine never had
any loader for at all - `00_01_apartment.hpm_ExposureArea` declares one real `ExposureArea_1`
entity with `Exposure="-1.2"` (a real photographic EV compensation) and `WhiteCut="2.2"`, neither
read anywhere in `soma/src/game/` or `HPL2/core/`.

Added `cWorldLoaderHpm::LoadExposureAreaTrack()` (`HPL2/core/{include,sources}/resources/
WorldLoaderHpm.{h,cpp}`) as a new sidecar track loader, following the exact same
`OpenSidecar()`/`GetTrackRoot()`/per-`Section` pattern every other track in this file already
uses (`LoadAreasTrack()`/`LoadSoundsTrack()` etc. right above it) - this is `cWorldLoaderHpm`,
the SOMA/HPL3-specific loader, entirely separate from `cWorldLoaderHplMap` (Dark Descent's own),
so zero risk to Dark Descent. Deliberately simplified: applies only the first `ExposureArea`
entity found as one flat global scale via a new `cWorld::SetGlobalExposure()`/
`GetGlobalExposure()` pair (`HPL2/core/include/scene/World.h`, defaults to `1.0` - a true no-op
for every world that never calls the setter, i.e. every existing Dark Descent/AMFP/no-
ExposureArea-SOMA-map world), not the real system's spatial blend/fade between multiple
overlapping areas as the camera moves through them (`WorldPos`/`Scale`/`TransitionTime` are
read from the file but unused) - a real, honest first step, not the full system; noted as a
follow-up below.

Applying it needed a real value, not just a stored one: `cRendererDeferred::CopyToFrameBuffer()`
(the final accumulation-buffer-to-screen blit) now draws one additional untextured fullscreen
quad in `eMaterialBlendMode_Mul` blend mode, colored `(exposure,exposure,exposure,1)`, right
after the existing blit - skipped entirely when `GetGlobalExposure()==1.0f` (the common case, so
truly zero extra draw call for every world that doesn't set one). No new shader file needed
(this repo carries no `.glsl`/`.hpsl` of its own - every game's shaders come from its own real,
separately-installed data - so a fixed-function multiply-blend quad is the only way to add a new
visual effect from engine code alone, matching how `SetTexture(unit, NULL)` + `glDisable()`
already correctly falls back to plain vertex-color modulation, confirmed by reading
`cLowLevelGraphicsSDL::SetTexture()`).

**Verified live, headless, real `00_01_apartment.hpm`**: hpl.log shows `applying global exposure
-1.200000 EV (0.435275 linear) from 'ExposureArea_1'`; the real `PlayerStartArea_1` screenshot
went from solid overexposed white to a properly-exposed gray-toned scene with visible wall
texture/streak detail - a dramatic, clearly real improvement, not a subtle one.

### Correction: the skybox fix's "magenta triangles gone" claim was wrong

The previous PORTING_NOTES section ("SOMA: shader-filename/sampler-binding fixes...") claimed
the skybox fix made the magenta triangles "completely gone," verified by comparing a yaw=0
screenshot (still showing magenta) against a *different*, yaw=1.57 screenshot (genuinely
magenta-free) - comparing screenshots at two different camera angles and treating the second as
representative of the fix was a mistake, caught this session while re-verifying with the
exposure fix layered on top (the same two triangular patches are still visible at yaw=0, just
darkened to purple by the new exposure multiply - confirmed by the color relationship: pure
magenta `(255,0,255)` times the `0.435` exposure scale is almost exactly the dark purple/maroon
actually seen). What the skybox fix *did* genuinely verify and fix, real and still holding:
camera-responsiveness (screenshots now differ across camera moves - confirmed again this
session, still true) and the FPS recovery (~3-8 -> ~19-22, still holding). What it did NOT fix:
whatever specifically produces these two magenta/purple triangular patches at this particular
camera pose.

Investigated further this session, real findings (not yet a fix):
- The real map data (`00_01_apartment.hpm`'s `<GlobalSettings><SkyBox .../>`) declares
  `Active="true" Color="0.67 0.79 1 1" Texture="D:/work/depth/redist/textures/environment/
  lab_env.dds"` - a light sky-blue tint, not magenta at all, so this is not the skybox's
  *intended* appearance.
- `iRenderer::RenderBasicSkyBox()` (`Renderer.cpp`) only checks `GetSkyBoxActive()`, never
  `GetSkyBoxTexture()!=NULL` - if the texture failed to load, the skybox mesh still draws every
  frame with the fixed skybox program (now real, post-skyfix) sampling an unbound/disabled
  texture unit 0 (`SetTexture(0, mpCurrentWorld->GetSkyBoxTexture())` with a NULL texture calls
  `SetTexture`'s NULL path, which `glDisable()`s that unit rather than binding anything) - a
  plausible source of undefined/driver-specific "error" color on Mesa's AGX driver, which is
  known to return a distinctive magenta-ish pattern for incomplete/unbound texture reads on some
  paths, matching what's seen.
- However: no `Warning("SOMA hpm: could not load skybox texture ...")` (the exact line
  `cWorldLoaderHpm::LoadGlobalSettings()` already logs on a real load failure - see its own
  comment, written by a prior session, calling the "D:/work/..." authored path "a known Phase 1
  data-path gap") appears anywhere in a real boot's hpl.log, and every texture-resolution layer
  between there and the actual file load (`cTextureManager::CreateCubeMap()` ->
  `CreateSimpleTexture()` -> `FindTexture2D()` -> `iResourceManager::FindLoadedResource()` ->
  `cFileSearcher::GetFilePath()`) resolves purely by basename (`cString::GetFileName()` strips
  everything up to the last `/`, same mechanism every other real texture in this map already
  loads through successfully) - meaning the prior session's "will not resolve here" comment on
  that code is itself an unverified assumption, not confirmed by reading the actual resolution
  code, and the real `lab_env.dds` file does exist in the install
  (`textures/environment/lab_env.dds`). So the texture most likely *does* load successfully by
  basename, same as everything else - meaning the missing-texture theory above is probably
  wrong too, and the real cause is something else entirely (a DDS cubemap face-order/format
  quirk this old DevIL 1.7.8 loader mishandles, or something unrelated to the skybox at all that
  just happens to render as two triangles near where the skybox mesh's corners would be).
- Also confirmed harmless, not the cause: a burst of unrelated `ERROR: Sampler X does not
  exist, could not bind it to unit N` lines (`aSpecularMap`/`aHeightMap`/`aDiffuseMap`/
  `aNoiseMap`/`aSkyboxMap` - note "aSkyboxMap", not the real HPSL name "aSkyMap") appear for
  many different shaders throughout a real boot - this is `GLSLProgram.cpp`'s sampler-binding
  loop (`SetupProgramAndTextures()`/link-time setup) trying a fixed set of Dark-Descent-
  convention sampler names against every linked program regardless of whether that particular
  program declares them; `glGetUniformLocation()` correctly returns -1 for names a given
  program doesn't have and the loop `continue`s past it - confirmed non-fatal, doesn't affect
  which unit any *real* sampler ends up bound to (which is separately, correctly handled by this
  session's earlier `ApplyHpslTextureBindings()` fix for HPSL shaders, or GLSL's own unit-0
  default for a sampler nothing ever explicitly binds).

**Not fixed this session** - correctly flagging this as still open rather than repeating the
earlier overclaim. Next step for whoever picks this up: instrument `CreateCubeMap()`/
`CreateSimpleTexture()` directly (a temporary log line printing `sPath`/whether `pTexture` came
back non-NULL) to settle whether the skybox texture load actually succeeds or not, before
guessing further at what happens after.

## SOMA: the magenta/gray artifact root-caused for real - it's the translucent/forward render pass, not the skybox (this session, continued)

Definitively isolated via bisection (disable one render pass at a time, rebuild, re-screenshot
the exact same real `00_01_apartment.hpm` PlayerStartArea_1 pose, compare) rather than more
guessing, after the skybox theory above turned out to be a dead end (confirmed: temporarily
forcing `iRenderer::RenderBasicSkyBox()` to return immediately produces a byte-identical
screenshot - the skybox pass, real cubemap and all, contributes *nothing* visible at this camera
pose. Also re-checked and ruled out the "incomplete cubemap" theory from earlier the same
session: that was a bug in this session's own throwaway DDS-header-parsing script - `caps2` is
at byte offset 108 into the header, not 112 - the real `lab_env.dds` file has all 6 cubemap
faces properly flagged and `cBitmapLoaderDevilDDS::LoadBitmap()` correctly reports 6 images,
confirmed by adding a temporary diagnostic Log() to `cTextureManager::CreateSimpleTexture()`
and reading real hpl.log output, not by re-parsing the file a second time.):

- Disabling `iRenderer::RenderBasicSkyBox()` entirely: **no change** - rules out skybox.
- Disabling `cRendererDeferred::RenderDecals()` entirely: **no change** - rules out decals.
- Disabling `cRendererDeferred::RenderTranslucent()` entirely: **the entire image goes flat
  black** (except the small debug-gizmo dots in the corner, unrelated) - every gray "wall"
  surface, every purple/magenta triangle, all of it, is being drawn by this one pass.

This means at this specific camera pose, **all real visible content is classified as
translucent**, not opaque/deferred geometry - plausible on its own terms (real Frictional
apartment interiors have real glass: windows, partitions, maybe a glass table - and
`lab_env.dds`, the same texture referenced by `<SkyBox Texture>`, is *also* referenced by
several real glass/reflective entity materials found earlier this session, e.g.
`entities/station/lab/beaker/lab_env.dds` - a shared environment-reflection map convention).

The critical implication: **none of this session's HPSL fixes touch this code path at all.**
`RenderTranslucent()` (`RendererDeferred.cpp`) uses `eMaterialRenderMode_Diffuse`/
`_DiffuseFog` - a forward-rendering technique completely separate from the deferred G-buffer
pipeline (`SetupProgramAndTextures()`/`RenderLightObject()`/the light-frag/fog-frag uniform
fixes, the skybox fix, the sampler-binding fix) every earlier fix this session targeted. It
almost certainly requests its own, different real HPSL shader file(s) via `cMaterial`/
`iMaterialType`'s own program-lookup path (not yet identified which ones) - the same class of
"C++ hardcodes a Dark-Descent-era filename/uniform-name that doesn't match HPSL's real name"
bug found and fixed repeatedly today, just never yet investigated for this specific rendering
technique. Given real forward-rendered glass needs at minimum a working diffuse/alpha-blend
shader and (for anything reflective) a real environment-map sample, this is likely multiple
real gaps of the same shape as everything already fixed today, not a single small fix.

**Not fixed this session - this is the concrete, well-isolated next step**, much stronger than
where this investigation started (a vague "two magenta triangles"). Next session should:
1. Find what real shader filename(s)/uniform names `eMaterialRenderMode_Diffuse` materials
   request (grep `MaterialType_BasicSolid.cpp`/`MaterialType_Translucent.cpp`/wherever
   `cMaterial::GetProgram(eMaterialRenderMode_Diffuse, ...)` actually builds a GPU program) and
   check them the same way every deferred-path filename was checked - real `.hpsl` corpus
   search, filename-alias table if needed.
2. Check whether this path even goes through the same `bIsHpslFallback`/`ApplyHpslTextureBindings()`
   machinery in `GpuShaderManager.cpp` at all, or bypasses it entirely.
3. Re-verify with the same bisection method (a temporary early-return, not guessing) once a fix
   is in place - it's fast and decisive, much more effective this session than reasoning about
   driver behavior from screenshots alone.

## SOMA/HPSL: the translucent/forward-pass artifact, precisely split into two independent real bugs (this session)

Picked up exactly where the previous session left off: `cRendererDeferred::RenderTranslucent()`
(`eMaterialRenderMode_Diffuse`, `MaterialType_BasicTranslucent.cpp`) does go through the same
`GpuShaderManager.cpp` HPSL-fallback + `ApplyHpslTextureBindings()` machinery as the deferred
G-buffer path - confirmed live (`deferred_transparent_frag.hpsl` compiles and links with zero
errors in hpl.log, real non-NULL `iGpuProgram*` for every translucent material at the test pose)
- so the earlier hypothesis of a bypassed/separate code path was wrong. Re-verified the whole
setup first: fast-forwarded this worktree onto master's current tip (`82a75a1`, a clean
ancestor - no merge needed) to pick up the lighting/skybox/sampler-binding/exposure fixes landed
since this bug was first isolated, then re-confirmed the exact same gray/magenta artifact at the
documented `00_01_apartment.hpm` `PlayerStartArea_1` pose before touching anything.

Added a temporary per-material diagnostic `Log()` in `RenderTranslucent()`'s object loop
(reverted before committing - not shipped) to print every real translucent object/material/blend-
mode/program hitting this pass. This immediately turned the abstract "gray and magenta shapes"
into concrete, real content: `static_objects/urban/walls/plain_glass_livingroom.mat` (a living-
room window pane - the large gray arrow-shaped area, `MulX2` blend, `Refraction`+`EnvMap`
enabled) and `entities/technical/block_box/block_box.mat` (a `technical/`-folder "blocking
volume" prop, object names `block_box_char_1_pCube1`/`BedCollider_Crouch_pCube1` - a
gameplay/physics helper mesh, not real player-visible art - `Add` blend, no refraction/envmap).
Bisected which contributes which visible shape by temporarily `continue`-skipping objects by
name/material in the render loop, rebuilding, and re-screenshotting each time (same discipline
the prior session used for render passes, just one level more granular - per-object instead of
per-pass) - confirmed live: excluding objects with a `CubeMap` texture makes the gray shape
disappear entirely (magenta unaffected); further excluding `block_box`-named objects then makes
the magenta shape disappear too (gray unaffected). Two independent bugs, not one:

### Fixed and verified: `avInvScreenSize` was never registered/set on `MaterialType_BasicTranslucent`

`deferred_transparent_frag.hpsl`'s `cTransparentFragArguments` cBuffer declares
`cVector2f avInvScreenSize`, used to convert the fragment's device pixel coordinates into a
screen-space UV for refraction's distorted sample position: `vDistortedScreenPos =
(px_vPosition.xy * avInvScreenSize + vRefractOffset)`. `MaterialType_BasicTranslucent.cpp` had
no `kVar_avInvScreenSize` slot at all - Dark Descent's own hand-written
`deferred_transparent_frag.glsl` never declares this uniform (confirmed by grepping the real
Dark Descent install's shader file - zero matches), so nothing in this file's history ever needed
one. Left unset, it silently defaulted to `(0,0)`: every refracting translucent surface's sampled
UV collapsed to near the `mpRefractionTexture`'s `(0,0)` corner (whatever stale content lives
there, since `CopyFrameBufferToTexure()` only ever updates the object's own on-screen clip rect,
not the whole texture) instead of the fragment's real screen position - producing a flat,
mostly-uniform color across the entire surface, independent of what's actually behind it.

Fixed the same way every other per-frame screen-size constant is wired for this material type
(`kVar_afInvFarPlane` in `MaterialType_BasicSolid.cpp`, `kVar_avInvScreenSize` in
`RendererDeferred.cpp`'s light/fog passes): added `kVar_avInvScreenSize` (id 10), registered via
`AddGenerateProgramVariableId("avInvScreenSize", ...)` in `LoadData()` for all 5 blend-mode
program managers, and set it in the previously-empty `cMaterialType_Translucent::
SetupTypeSpecificData()` from `apRenderer->GetRenderTargetSize()` (called once per program bind,
same convention as the existing `afInvFarPlane` setup).

**Verified live**: before the fix, the living-room window rendered as a completely flat, textureless
gray shape. After, real background detail bleeds through via refraction - visible wall/paper
texture streaks and a curved line detail, matching the "properly-exposed gray-toned scene with
visible wall texture/streak detail" already established as correct for this exposure-corrected
scene in an earlier session. Confirmed via pixel diff (`ImageChops.difference().getbbox()` is a
real, large, non-`None` region between before/after screenshots at the identical camera pose) -
not a placebo. `ctest --test-dir amnesia/src/build` stays green (4/4), and a headless Dark
Descent regression boot (`Amnesia.bin.aarch64`, real Dark Descent install, Profiles menu) shows
zero hpl.log output and an unchanged, correctly torch-lit menu screenshot - this file is shared
HPL2 core code, and `SetupTypeSpecificData()`'s new branch is gated on
`eMaterialRenderMode_Diffuse`/`_DiffuseFog`, both already reachable for Dark Descent's own real
translucent materials (glass, water uses a different material type) - genuinely exercised, not
dead code, and produces byte-for-byte-equivalent output since Dark Descent's own
`deferred_transparent_frag.glsl` simply doesn't read this new uniform at all.

### Root-caused but NOT fixed: `block_box.mat`'s magenta is a real "×8 HDR precision boost with no compensating downstream divide" gap

Excluding `block_box`-named objects removes the magenta shape entirely, unaffected by the
`avInvScreenSize` fix or by disabling `UseEnvMap`/`UseRefraction` (also bisected, temporarily, to
rule out reflection/refraction as the cause for this specific object - reverted). This object is
real content, not garbage: `entities/technical/block_box/block_box.mat` is a `technical/`-folder
prop referenced by names like `BedCollider_Crouch_pCube1` - a gameplay collision-blocker/marker
mesh, `Add` blend, no refraction, no cubemap, `AffectedByLightLevel=false`.

Root cause, read directly from `deferred_transparent_frag.hpsl`'s `main()`: every blend-mode
branch that includes `BlendMode_Add` (also `BlendMode_Alpha`/`BlendMode_PremulAlpha`, and
`UseEnvMap`/`UseRefraction` regardless of blend mode) ends with an unconditional
`vFinalColor *= cVector4f(8.0, 8.0, 8.0, 1.0);` - the shader's own comment calls this "Multiply
with 8.0 to increase precision." This is a real HPL3/HPSL-era HDR-precision convention with **no
equivalent anywhere in Dark Descent's own hand-written `deferred_transparent_frag.glsl`**
(confirmed by grep - zero `8.0`/`* 8` occurrences in the real Dark Descent shader file), and this
port's `RenderTranslucent()` draws straight into the real framebuffer via ordinary GL blending
(`SetBlendMode()` → real `GL_ONE`/`GL_ONE`-style fixed-function blend state) with no subsequent
tonemap/composite pass that could apply a matching `/8`. A moderately-saturated diffuse texture
sample, boosted ×8 and additively blended, clips straight into a fully-saturated flat color -
consistent with the solid, hard-edged magenta triangle actually seen (not textured/gradiented,
since the source texture's own detail gets crushed by the ×8 clip).

**Why not fixed this session**: the only local (non-shader-source, since the real `.hpsl` is
proprietary SOMA data outside this repo) place to compensate is either (a) scale
`afLightLevel`/`afAlpha` by `1/8` before it reaches the shader, or (b) use a `GL_CONSTANT_COLOR`
source blend factor with `glBlendColor(0.125,...)` at the GL level for the affected blend modes.
(a) doesn't cleanly cancel the boost when `UseFog` is also compiled in (true for this exact
material at this camera pose - `progName` includes `UseFog`): the fog contribution is mixed in
via `ApplyFogColor()` (a `mix()`, not a pure scale) *between* the `afLightLevel` multiply and the
final `×8`, so pre-scaling `afLightLevel` alone would under-boost the object's own color relative
to fog and change the visual balance, not just its brightness - a different, not obviously
correct, artifact. (b) is a real fix (GL blending operates on the shader's final output, so an
exact `1/8` GL-level scale is source-value-agnostic and correctly cancels the boost regardless of
what internal math produced it) but changes `SetBlendMode()`/blend-state handling in shared
`iRenderFunctions`/`LowLevelGraphicsSDL.cpp` code used by every blend mode across every module
(Dark Descent's particles/decals/GUI included) - real risk of a silent, hard-to-notice regression
elsewhere if not scoped very carefully (e.g. only for the SOMA-only HPSL-fallback path, which
would need a new "is this an HPSL-sourced program" flag threaded through to blend-state setup,
not a small change). Flagging precisely rather than attempting a rushed fix.

**Concrete next step**: implement (b), scoped narrowly - e.g. a new `iGpuProgram`/`cMaterial`-side
flag set only when a program came from the HPSL-fallback path (already known at `CreateShader()`
time in `GpuShaderManager.cpp` via `bIsHpslFallback`) that `RenderTranslucent()` checks to apply
a `GL_CONSTANT_COLOR`/`glBlendColor(0.125,0.125,0.125,1)` source factor instead of the normal
`GL_ONE` for `BlendMode_Add`/`_Alpha`/`_PremulAlpha`, and investigate whether `UseEnvMap`/
`UseRefraction`-only cases (e.g. `MulX2` glass) need the same treatment for their reflection
term specifically (likely yes, unverified - the reflection addition happens before the same `×8`
line for those modes too).

### Investigated, real, but deliberately not touched: `CubeMap Type="Rect"` is silently loaded as a plain 2D texture

While tracing the window material, noticed `static_objects/urban/plain_glass_livingroom.mat`
declares its `CubeMap` texture unit as `Type="Rect"` (`textures/environment/skyline_day_env.dds`),
not `Type="Cube"` like every other real cubemap-using material checked this session (e.g.
`entities/urban/kitchen/glass/Glass.mat`, `entities/urban/grocery/botte_of_juice/
botte_of_juice_empty.mat`, both `Type="Cube"`). `cMaterialManager::GetType()`
(`MaterialManager.cpp:496`) only recognizes `"cube"`/`"1d"`/`"2d"`/`"3d"` (case-insensitive) and
silently falls back to `eTextureType_2D` for anything else, including `"rect"` - so this specific
material's env-reflection texture loads as an ordinary `GL_TEXTURE_2D`, then gets bound to a
texture unit the shader samples via `samplerCube`/`textureCube()` whenever `UseEnvMap` is set - a
genuine type mismatch. Confirmed the DDS file itself is real and not a mis-saved cubemap: its
header's `caps2` field (byte offset 108, matching the correct offset - a previous session's own
DDS-parsing bug used the wrong offset here, see the "root-caused for real" section above) has
`DDSCAPS2_CUBEMAP` (`0x200`) unset but `DDSCAPS2_VOLUME` (`0x400000`) set - `skyline_day_env.dds`
is a 512×512 non-cubemap texture, so `Type="Rect"` is *not* a data-entry mistake for `"Cube"`,
it's real, deliberate HPL3 authoring for a different (currently unidentified) env-map technique
this shader/material system doesn't have a code path for. Left uninvestigated further - understood
just well enough to know it's a real content-format question (what does `Type="Rect"` actually
mean in HPL3's real renderer - a spherical/equirectangular panorama sampled with computed UVs
instead of `textureCube()`? a `GL_TEXTURE_RECTANGLE`?) rather than a "just add another case to
GetType()" one-liner; guessing wrong here risks a differently-wrong render for no better reason
than the current gap. This currently contributes to `plain_glass_livingroom`'s residual
imperfection (window still isn't a *fully* correct-looking reflective glass pane, even after the
`avInvScreenSize` fix - it's now recognizably showing real refracted background detail, which is
the main win this session, but its reflection component is still wrong) but is independent of,
and additional to, the `avInvScreenSize` fix and the `×8` gap above.

### Summary for next session

- Landed and verified: `avInvScreenSize` fix (commit-ready, see diff in
  `MaterialType_BasicTranslucent.cpp`).
- Precisely root-caused, not fixed: the `×8` HDR-precision-boost-with-no-divide gap (affects
  `block_box.mat`'s magenta directly, and very likely also affects the window/bottle glass
  materials' `UseEnvMap`/`UseRefraction` reflection terms to some lesser degree - unverified how
  much, since the `Type="Rect"` gap below already prevents `plain_glass_livingroom`'s reflection
  from being checked in isolation).
- Precisely root-caused, not fixed: `CubeMap Type="Rect"` loads as a plain 2D texture
  (`MaterialManager.cpp:496`'s `GetType()`) - real content, unclear what HPL3's real renderer
  does with it, needs understanding before attempting a fix.
- Both remaining gaps are scoped narrowly enough (one function each) that a focused follow-up
  session should be able to land both without another multi-hour bisection - the hard part (
  finding *which* real objects/materials are responsible for each remaining visible artifact) is
  already done.

## SOMA/HPSL: the ×8 HDR-precision-boost gap, actually fixed this time (this session, continued)

Picked up the "root-caused but NOT fixed" gap from the section above: `deferred_transparent_frag.hpsl`
ends `main()` with an unconditional `vFinalColor *= cVector4f(8.0, 8.0, 8.0, 1.0);` under
`@ifdef UseRefraction || UseEnvMap || BlendMode_Add || BlendMode_Alpha || BlendMode_PremulAlpha`
that Dark Descent's own real `deferred_transparent_frag.glsl` has no equivalent of, and this port
has no compensating downstream divide for anywhere.

Rejected the two GL-blend-state approaches considered previously (a `GL_CONSTANT_COLOR`/
`glBlendColor()` source factor, or pre-scaling `afLightLevel`) as too invasive (new shared
`eBlendFunc` enum value, new low-level graphics method, global GL state to manage) and not exact
for `Alpha`/`PremulAlpha` blend modes (GL only allows one source blend factor - can't combine
`SRC_ALPHA` and a constant scale). Simpler, more complete fix: this repo carries no `.glsl`/`.hpsl`
of its own, but it does fully own the HPSL->GLSL *transpiler* (`soma/src/game/HpslTranspiler.cpp`)
that produces the actual GLSL text fed to the driver - so the boost can be removed at its source,
in the transpiled shader text itself, correctly handling every blend mode uniformly (the multiply
line disappears entirely, regardless of which `@ifdef` branch was active) instead of trying to
compensate for it downstream per blend mode.

Added `RemoveUncompensatedHdrPrecisionBoost()` (a `std::regex`-based line removal, same shape as
the existing `StripUniformBindingIndices()` in the same file) to the `TranspileHpslToGlsl()`
pipeline. Confirmed via a full corpus search that the exact `"vFinalColor *= cVector4f(8.0, ...)"`
pattern exists in exactly one file among the entire real HPSL corpus, so this can't affect
anything else. One real gotcha found the hard way: the transform must run *after*
`ReplaceTypeNames()` (which rewrites `cVector4f` -> `vec4` earlier in the same pipeline) - an
initial attempt placed it before that step and matched nothing, silently. A temporary diagnostic
`Log()` (added, confirmed the match fires correctly for multiple real combo variants during a real
boot, then removed before committing) caught this.

**Verified live, for real this time**: `entities/technical/block_box/block_box.mat`'s pixel color
at the real `00_01_apartment.hpm` `PlayerStartArea_1` pose went from a clipped, saturated bright
magenta/purple to `(58,2,24)` - matching `raw_texture_color(119,2,49) * this_scene's_own_exposure_
multiplier(~0.435)` almost exactly, confirming the fix is both real and correctly scoped, not a
coincidental color shift. `ctest` stays 4/4 green. Change is 100% contained to
`soma/src/game/HpslTranspiler.cpp` (SOMA-only, never compiled into or reachable from Dark
Descent/AMFP/Rebirth/Bunker's own `.glsl`-only shader path) - zero regression risk by construction,
no live Dark Descent re-verification needed for this one.

**A real false start worth recording for calibration**: the first two attempts at verifying this
fix (a wrong-regex build, then a supposedly-fixed rebuild) both showed a byte-for-byte identical
screenshot before/after, and were wrongly written up as "the fix compiles and matches during
transpile but doesn't visibly affect this material" - complete with a plausible-sounding
alternative theory (that `block_box.mat` is an intentionally-invisible debug/collision-marker prop
real SOMA hides via game-script logic this scaffold doesn't run, so no fix should be expected to
change it). That specific reasoning about the material's *nature* (debug placeholder, physics-only
prop) is still true and worth keeping in mind - but the conclusion "so this fix can't have worked"
was wrong. Root cause of the false negative: a `cp` deploy step hit an interactive "overwrite?"
prompt and timed out after 2 minutes without actually copying the new binary, so both "verification"
runs were silently re-testing a stale build. Caught by adding a temporary diagnostic and seeing it
fire, then re-deploying cleanly (`rm` first, then `cp`, avoiding the interactive prompt entirely)
and re-testing - the corrected result is unambiguous. Lesson: **a `cp` onto an existing file in
this environment can prompt interactively and silently hang/timeout rather than error** - always
`rm -f` the destination first when redeploying a test binary under the same name, don't rely on
`cp`'s overwrite behavior.

### Remaining open item

- `CubeMap Type="Rect"` silently loading as a plain 2D texture (`MaterialManager.cpp:496`) is
  still unfixed - unrelated to the HDR-boost gap, see the previous section for full detail. Still
  the one remaining known, understood-but-unfixed gap from this investigation thread.

## SOMA: a real interactive main menu (this session)

Previously, after the splash sequence, SOMA loaded `main_menu.hpm` (a real but legitimately
empty scene) with a free-fly debug camera and no GUI at all - nothing to click, no way to
actually start playing without the headless control-socket workflow.

Added `soma/src/game/SomaMainMenu.{h,cpp}` - a plain native `cGuiSet`/`cWidgetButton` menu built
the same way `amnesia/src/game/LuxMainMenu.{h,cpp}` builds Dark Descent's real menu, trimmed to
what this scaffold needs: a title label, a "New Game" button (calls `cSomaBase::LoadMap()` with
`00_01_apartment.hpm`/`PlayerStartArea_1` - the same real, extensively-tested entry point the
headless `start_map` command already uses), and a "Quit" button (`cEngine::Exit()`). Not a
recreation of SOMA's real menu - the actual game uses ImGui + an AngelScript-driven
`main_menu.hps`, a completely different UI toolkit this engine has no integration for at all;
this is an honest, functional native replacement appropriate for Phase 0/1 scope. Created by
`cSomaBase::InitMainMenuScene()`, attached to that scene's own real camera+world viewport (not a
separate GUI-only one like the splash uses, since there's a real scene behind it).

Two real bugs found and fixed along the way:

- **Mouse input was never routed to the GUI at all.** `cGui` doesn't poll `iMouse` on its own -
  nothing in `HPL2/core` calls `SendMousePos()`/`SendMouseClickDown()`/`Up()` for you; the real
  Dark Descent game module does that itself every frame in `amnesia/src/game/
  LuxInputHandler.cpp`, which this Phase 0 scaffold has no equivalent of (the splash never hit
  this, since it polls `iMouse` directly rather than routing through widget click messages).
  Buttons were visible and the `cGuiSet` was set as the focused set, but clicks were silently
  dropped and the cursor never moved from `(0,0)`. Fixed by making `cSomaMainMenu` an
  `iUpdateable` that pumps mouse position and left-click edges into `cGui` every frame, the same
  thing `LuxInputHandler.cpp` does for the real game.
- **DevIL mis-decodes a real SOMA font atlas format.** SOMA's `vera.fnt`'s first texture page,
  `fonts/vera_00.dds`, is an uncompressed 8bpp alpha-only DDS (`DDPF_ALPHA`, 8-bit). DevIL
  decodes it as `IL_RGB` with every byte zeroed rather than the real per-pixel alpha coverage the
  header describes - confirmed via a standalone probe against the real file. An all-zero RGB
  texture has no alpha channel (samples as opaque black), so every glyph rendered as a solid
  black box - the menu's title/button text was completely invisible before this was found (the
  second font page, `vera_01.dds`, is a normal RGBA DDS and was never affected - this port simply
  never exercised text through this specific page before, since nothing drew real text until
  this menu). Fixed narrowly in `HPL2/core/sources/impl/BitmapLoaderDevilDDS.cpp`: a new
  `TryLoadUncompressedAlphaDDS()` parses the DDS header directly and reads the raw pixel payload,
  entirely bypassing DevIL, but only for files matching this exact pixel-format signature
  (`DDPF_ALPHA`, 8bpp) - every other DDS variant (compressed DXT1/3/5, uncompressed RGB/RGBA/
  Luminance) falls through to the normal DevIL path untouched. This is shared `HPL2/core` code;
  verified zero Dark Descent regression (clean headless boot, empty hpl.log, unchanged Profiles
  menu screenshot) since Dark Descent's own font/texture data doesn't use this DDS variant.

**Verified live, headless, both interactively** (not just visually present - actual click
routing): screenshot confirms the title text, both buttons, and a visible cursor all render
correctly; injecting a real `mouse_move` + `mouse_button` click (via the headless `input`
command) on "New Game" correctly loaded `00_01_apartment.hpm` and teleported the camera to the
real `PlayerStartArea_1` coordinates, confirmed via `camera_state`; a separate run's "Quit" click
correctly triggered the engine's real shutdown sequence ("User Exit" in hpl.log) and the process
exited cleanly a few seconds later (normal engine cleanup time, not a hang). `ctest` stays 4/4
green.

### Remaining open items

- No "Load Game"/settings/other menu functionality - genuinely out of scope for this pass
  (New Game + Quit is the minimum for "interactive"), not attempted.
- The menu's visual design is plain/functional (skin's default button/label styling, no custom
  layout beyond centering) - real SOMA's actual menu art was never a goal here, see above.
- `CubeMap Type="Rect"` silently loading as a plain 2D texture (`MaterialManager.cpp:496`,
  documented in the previous PORTING_NOTES section) remains unfixed and is unrelated to this work.
