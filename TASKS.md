# Tasks

- Shared engine: mute audio while window is unfocused, resume on refocus (2026-09-08)
  - DONE: user reported audio kept playing continuously while alt-tabbed away, on every game
    module. Fixed at the shared `HPL2/core` engine layer, not gated on any per-game flag.
    `cEngine::Update()` already broadcasts `AppLostInputFocus()`/`AppGotInputFocus()` to every
    globally-registered `iUpdateable` off real SDL window-focus polling (pre-existing, used by
    Dark Descent's own `cLuxMapHandler` sound-pause already) - `cSound` (`HPL2/core/sources/
    sound/Sound.cpp`) is one such updateable and now overrides both to mute/restore the real
    device master volume (`iLowLevelSound::SetVolume()`), the same call every game's own Options
    volume slider already uses. Added a test-only `"focus"` input type to the headless control
    protocol's `CmdInput` (`HPL2/core/sources/system/HeadlessControl.cpp`) to drive this directly
    for headless verification, since a hidden headless window can never receive a real focus
    transition. Verified live headless against real SOMA data: mute/restore fire correctly and
    idempotently over the real control socket; Dark Descent regression confirmed via a clean full
    rebuild, all 4 ctest suites green, and a real headless Amnesia boot reaching its own expected
    behavior unchanged. Full writeup, citations, and verification detail (plus its one honestly
    stated limit - a real alt-tab itself needs interactive, non-headless confirmation) in
    PORTING_NOTES.md's newest section.

- SOMA: New Game intro sequence's "stilted"/overlapping voice lines - schedule-based advance
  replaced with real audio-completion polling (2026-09-07)
  - DONE: soma/src/game/SomaIntroSequence.{h,cpp} - user reported the intro slideshow's dialogue
    was "stilted," missing correct timing, with overlapping audio ("some missing component of the
    voice line scheduling"). Root cause: real 00_00_intro.hps/helper_audio.hps's
    Voice_PlayWhenPossible() gates advancing to the next Subject on real audio completion
    (Voice_AnySceneIsActive()==false, polled every 0.2s) plus a real afMinQuietTime of 0.0f (not
    the documented 5s default) - not a fixed timeline. This port's AdvanceVoice() instead switched
    Subjects purely on elapsed wall-clock time against a schedule, using each line's mfHoldTime (a
    probed real .ogg duration + fixed reading tail) as a duration estimate - close but not exact,
    so the port would sometimes start the next line/Subject's PlayGui() while the previous one's
    real audio was still playing. Fixed: AdvanceVoice() now requires BOTH the schedule's nominal
    start time AND a direct sound-system query (cSoundHandler::IsPlaying(), passed the same
    filename PlayLine() gave PlayGui()) confirming the previous line's real audio has actually
    finished, mirroring the real engine's own gating exactly. This let the synthesized per-line
    mfHoldTime estimate be removed entirely for every real-audio line (kept only for the one real
    line with no voice file at all, "For what?"). Also confirmed via a full line-by-line diff
    against 00_00_intro.hps's OnStart() that every real AddSlide()/AddVoice()/AddEvent() call was
    already represented in this port's data - no dialogue was actually missing, the complaint was
    the overlap bug making lines read as skipped. Verified live, headless: a temporary diagnostic
    Log() (removed before finalizing) traced every PlayLine()/audio-finished event across the
    entire sequence (all 5 Subjects, all 11 real lines) - zero overlap at any transition, every
    PLAY at or after the prior line's own FINISHED tick. ctest 4/4 green before and after. Full
    writeup with exact real-source citations and the full trace in PORTING_NOTES.md.

- SOMA: real toggle widget, gamma instructions, live Resolution apply, ESC pause menu (2026-09-07)
  - DONE: `soma/src/game/SomaMainMenu.{h,cpp}` - added `DrawOptionsToggleControl()`, the real
    on/off switch widget (`startmenu_options_button_on/off.tga`) `OptionMenu_ButtonOptionsToggle()`
    actually draws via a separate `OptionMenu_OptionsCheckbox()` function - a previous pass's own
    comment wrongly concluded it shares the arrow-cycle-bar `eKind_MultiSelect` uses; rereading
    `helper_imgui_options.hps` directly this session found the real, separate function. Reflection/
    Refraction/Display Mode/V-Sync now show the real two-segment switch, not the cycle bar.
  - DONE: `soma/src/game/SomaGammaScreen.{h,cpp}` - added the real `GammaInstructions0` string
    (`config/base_english.lang`) above the checkerboard/slider, word-wrapped via
    `iFontData::GetWordWrapRows()`. Confirmed the reference screenshot's "glitchy S logo" is
    already baked into `gamma_background.tga`'s own pixels - no extra draw call needed.
  - DONE: `soma/src/game/SomaMainMenu.cpp`'s Resolution row now calls the existing
    `cLowLevelGraphicsSDL::ForceWindowSize()` live (same call `HeadlessControl.cpp`'s `resize`
    command already uses) instead of "takes effect on next launch" - `CheckAndUpdateScreenSize()`
    (already called every frame) picks up the resize with no restart. Display Mode (fullscreen/
    windowed) checked and confirmed to have no equivalent live call anywhere in
    `cLowLevelGraphicsSDL` - left persisted-only, documented as actually checked this pass.
  - DONE: ESC pause menu during real gameplay - `cSomaMainMenu` gained a paused mode (reduced
    Resume/Options/Quit-to-Main-Menu item list, `ShowPaused()`/`HidePaused()`/`IsPaused()`),
    `cSomaBase` gained one new bridge pair (`SetGameplayPaused()`/`IsGameplayPaused()`) so the
    player and menu don't need direct pointers to each other, and `cSomaPlayer::Update()` gained a
    minimal raw-keyboard-queue Escape check (no `cAction` exists for Escape) that toggles it.
    "Quit to Main Menu" is a known, documented partial approximation (re-shows the title menu over
    the still-loaded gameplay map rather than reloading `main_menu.hpm`) - a full fix needs
    `SomaBase` world/camera-controller teardown/rebuild plumbing, flagged rather than guessed at.
  - Verified live/headless (`/tmp/soma-agenta98-test`, real SOMA data symlinked read-only):
    screenshots of Video→Rendering (toggle widget) and the gamma screen (instructions text);
    Resolution click producing a real 1280x720→1280x800 live framebuffer resize (confirmed via
    both the screenshot's own BMP dimensions and the `hpl.log` "applied live" line); and, on the
    real `00_01_apartment.hpm` map, Escape showing the paused menu, `camera_state` position
    staying identical while holding `W` during pause, Escape again closing the menu, and position
    then moving correctly. `ctest` 4/4 green throughout. Full writeup, citations, and the Quit-to-
    Main-Menu limitation's exact next steps in PORTING_NOTES.md.

- SOMA: prop collision investigation (found already working, corrected the record) + a real apartment phone-call hand-port (2026-09-07)
  - DONE: two tasks. (1) Investigated the reported "furniture has no collision" gap
    (`soma/src/game/SomaLoaders.{h,cpp}`'s entity loading path) and found it does not reproduce:
    `cEntityLoader_Object::Load()` (shared with Dark Descent's own `LuxProp`) already generically
    builds real Newton bodies from each `.ent`'s own `<Shapes>`/`<Bodies>` XML, independent of
    entity type. Verified live via a temporary diagnostic log against a real `00_01_apartment.hpm`
    boot: 353/425 (83%) of real map entities already get real bodies (every actual piece of
    furniture checked - beds, desks, cabinets, sinks, shelving, the `BedCollider`/
    `BedCollider_Crouch` invisible block_box volumes), and the remaining 72 (17%) are all
    correctly-authored collisionless decorative clutter (plant leaves, wall shrub decoration,
    post-its, alarm-clock digit decals) with no `<Bodies>` in their own real `.ent` files -
    matching real SOMA's own behavior. Also physically confirmed via the real player controller:
    walking toward `bed_1`'s real position shows genuine collide-and-slide deceleration, not a
    clean pass-through. No code change was needed or made for this task - this corrects an
    incorrect inference in this file's/PORTING_NOTES.md's own prior "props get no collision body
    at all" note (reached by symmetry with the `WorldLoaderHpm.cpp` static-geometry fix, without
    checking `cEntityLoader_Object`'s own pre-existing generic body creation). See
    PORTING_NOTES.md for the full investigation, including one genuinely-out-of-scope gap found
    incidentally (falling out of the compiled level bounds walking far enough past the bed in one
    specific direction - a real static-geometry coverage gap, not a prop-collision one).
  - DONE: new `soma/src/game/SomaApartmentIntroCall.{h,cpp}` - a one-map hand-port of
    `00_01_apartment.hps`'s David Munshi phone call (real timer delay, real ring, real 11-line
    `1_PhoneCall` dialogue with real voice-over `.ogg` files, audio-completion-gated advance same
    as `cSomaIntroSequence`'s own pattern), modeled directly on `cSomaIntroSequence`'s shape and
    carrying the same explicit "not a step toward general script execution" disclaimer. Two
    honest, clearly-logged gaps: auto-answers the call 4s after the ring starts (no real
    interact-with-entity system exists anywhere in this codebase yet) instead of waiting for a
    real player interaction; both real phone SFX are FMOD-banked and unavailable (a
    `"(phone ringing...)"` subtitle line stands in). Wired into `cSomaBase::LoadMap()` with a
    single new one-map-gated `if` block, same pattern as `cSomaIntroSequence`'s own construction.
    Verified live across 3 independent headless boots: `hpl.log` shows the ring/auto-answer/
    11-of-11-lines-finished sequence firing in order and on schedule every time; a real mid-call
    screenshot shows the real subtitle text on screen. Re-confirmed the original block_box/
    ShowMesh corruption fix is still intact (clean, uncorrupted `PlayerStartArea_1` screenshot).
    All 4 ctest suites green; full `Amnesia`/`Soma` rebuild confirms zero Dark Descent regression
    risk. Full writeup in PORTING_NOTES.md.

- SOMA: the real splash/boot sequence, properly reverse-engineered (2026-09-05)
  - DONE: soma/src/game/SomaSplash.{h,cpp} fully rewritten after user feedback that the previous
    session's splash "still not correct... missing visual effects, sound effects, and loading bar".
    Re-derived the real sequence from config/game.cfg's <General> block (SplashScreen/
    SplashScreenMusic/LoadingBar/LoadingFrame - confirmed native/compiled via `strings
    Soma.bin.x86_64`, not script), script/modules/MenuHandler.hps's GuiPreMenu() (real FG-logo
    fade/hold timing and SFX/music cues), and direct inspection of the real assets. Two phases:
    FG logo (real ~2.5s/~0.5s/~2s fade-in/hold/fade-out, matching script exactly) then a new
    Premenu.png + loading_bar.dds/loading_frame.dds "boot/init" screen (the real glitchy
    "INITIALIZATION..." screen the user's own screenshot showed) with a time-based bar-fill
    animation (clipped via a persistent cGuiClipRegion, not cGuiSet::DrawGfx()'s nonexistent
    source-rect param). Removed a real, confirmed bug from the previous session: the second splash
    image was graphics/imgui/credits/soma_logo_splash_static.dds, an end-credits asset, not part
    of the real boot sequence at all. Also extended a concurrent session's new
    soma/src/game/SomaMenuSfx.{h,cpp} (a real FSB5 bank reader) with two more real sample names
    this session found via `strings` on special_fx_stream.fsb - FG_Logo_Sting and menu_bg_noise -
    resolving that file's own documented "FG_Menu_Sting may resolve to..." gap, so the splash's
    real sting/ambience SFX now actually play (extracted, verified as valid PCM16 WAV via
    ffprobe). SplashScreenMusic ("loadscreen_background.ogg") is a real plain Vorbis OGG file,
    wired in via the existing cMusicHandler. Verified live, headless: ctest 4/4 green; a real
    screenshot mid-boot-phase shows Premenu.png's glitch text + the red loading bar correctly
    clipped/filled, closely matching the real Premenu.png reference image; a follow-up screenshot
    confirms a clean, crash-free hand-off into the main menu. Full writeup in PORTING_NOTES.md,
    including exact citations for every real value used and what's still an approximation (exact
    bar/frame pixel position - no further evidence recovered from the closed native boot code).

- SOMA: a real physics-based player controller, and real static-geometry collision (2026-09-05)
  - DONE: soma/src/game/SomaPlayer.{h,cpp} - a real iCharacterBody-based player controller
    (gravity, real per-axis walk speeds, mouse-look, ground-gated jump) replacing the free-fly
    debug camera for real game maps loaded via SomaBase::LoadMap() (main menu scenes keep the
    free-fly camera unconditionally; OPENHPL_SOMA_FREECAM=1 opts back into free-fly for game maps
    too). All body/movement constants reverse-engineered from SOMA's own real script source
    (script/player/Player_Types.hps, Player.hps, MoveState_Normal.hps, config/game.cfg), not
    guessed. Also fixed: HPL2/core/{include,sources}/resources/WorldLoaderHpm.{h,cpp} (SOMA's own
    real .hpm map loader) previously created ZERO physics bodies for any level geometry at all (a
    known, documented Phase-1 gap from before any player controller existed) - StaticObject/
    Primitive meshes now get a real static collision body each when their real "Collides"
    attribute (previously read but ignored) is true. Found and fixed two real, live-reproduced
    bugs along the way: (1) a SIGSEGV use-after-free (character body destroyed after its owning
    physics world, not before - real coredumpctl backtrace), (2) iCharacterBody::StopMovement()
    silently zeroing the persistent mfMoveAcc/mfMoveDeacc tuning constants (not just transient move
    state as its name implies), which permanently disabled all future WASD movement - the root
    cause of "keyboard input registers but the player never moves" after a long, methodical chase
    (ruled out headless-input-tooling issues, mouse-look injection - genuinely a separate,
    confirmed no-op tooling gap - and collision before finding this). Verified live and
    headlessly on the real 00_01_apartment.hpm map: correct spawn position/facing from the map's
    real PlayerStart Area, gravity + real floor collision (falls a small amount then rests, was
    previously infinite free-fall), WASD movement in the correct direction that stops dead against
    real wall collision, and jump (real ~0.74m hop, lands back on the same floor). ctest 4/4 green
    throughout; a full Amnesia target rebuild after the shared WorldLoaderHpm.cpp change confirmed
    it's inert for Dark Descent (that loader is SOMA-only, registered for the "hpm" extension
    only). Also found, NOT fixed (flagged for the user, out of scope - must not touch Steam
    directories): two real build-artifact binaries (Soma.bin.aarch64/OpenHplSoma.bin.aarch64) sit
    inside the real Steam SOMA install directory from an earlier session, predating this project's
    current "never write to a Steam game directory" rule. Full writeup, all verification evidence,
    and a real characterization of the real AngelScript OnStart() gap (see below) in
    PORTING_NOTES.md.
  - Not done: real mouse-look is implemented but NOT verified live (the existing headless "input"
    command's mouse_move injection is a confirmed no-op for GetRelPosition() - reads a real SDL
    OS-level accumulator, not the injected event's own fields - a pre-existing headless-tooling gap,
    not something this session could fix within scope); dynamic map Entities (props/furniture/
    doors, ~400+ per real map) still get no collision body at all, only StaticObjects/Primitives
    (walls/floors/ceilings); no crouch/lean/footstep sounds.
  - Investigated but NOT attempted (real AngelScript OnStart() execution, priority 2 of this
    session's brief): characterized concretely why this is its own multi-session project, not a
    follow-up task - this codebase's entire AngelScript integration (used successfully by Dark
    Descent) does zero #include preprocessing and has literally zero RegisterObjectType() calls
    anywhere, for any game; SOMA's real scripts need both a real #include resolver and a genuinely
    new object-type registration layer (iCharacterBody, a cLuxMap-equivalent, vector/color types as
    real script objects) that doesn't exist today. See PORTING_NOTES.md for the full citations.

- SOMA: a real Options screen off the main menu's Options button (2026-09-05)
  - DONE: read the real Options screen directly out of a real SOMA install
    (script/modules/MenuHandler.hps's GuiOptions()/GuiOptionsAudio()/GuiOptionsVideoDisplay()/
    GuiOptionsVideoGamma() + script/custom_depth/helper_custom_depth_imgui/helper_imgui_options.hps's
    layout constants/widget helpers), same methodology as the main menu. Scoped down from the real
    8-tab tree (Gameplay/Controls/Video{Display,PostEffect,World,Gamma}/Audio) to only what this
    engine has a real, working backend for: master Volume (cSound), Gamma/VSync (cLowLevelGraphics,
    both live), Fullscreen (persisted, applies on next launch - no live API exists, matching Dark
    Descent's own Fullscreen setting). New soma/src/game/SomaConfig.{h,cpp} (small Load()/Save()
    class, mirrors amnesia/src/game/LuxConfigHandler.{h,cpp}'s shape) persists these four settings
    to main_settings.cfg under $XDG_CONFIG_HOME/open-hpl/soma/ - SOMA previously had zero persisted
    config at all. Options screen itself lives inside cSomaMainMenu (soma/src/game/
    SomaMainMenu.{h,cpp}, an eSomaMenuScreen state + generic cSomaOptionsRow rows), not a separate
    class, so it shares the main menu's background/font/highlight-bar/1280x720-canvas conventions;
    real corner/border frame + slider/toggle/highlight-bar art (graphics/startmenu/gfx/) used
    throughout. Found and fixed a real, confirmed-live bug along the way: cSomaBase::InitEngine()
    (and a second, pre-existing instance in cSomaBase::Init() itself) could Log() before
    SetLogFile() ran, which - confirmed live from a real headless test run - wrote into the real
    Steam SOMA install's hpl.log via this project's own established "symlink hpl.log for tailing"
    scratch-dir pattern; fixed by reordering both call sites. Verified live, headless, via real
    injected mouse clicks/drags: full navigation (Main -> Options -> Audio -> Volume drag -> Back ->
    Display -> Fullscreen/VSync toggle -> Gamma drag -> Back -> Back -> Main), main_settings.cfg
    recording each change correctly, and a full process restart confirming persistence actually
    works (hpl.log showed "fs:1" - fullscreen correctly re-applied). ctest 4/4 green immediately
    after this session's changes. Full writeup in PORTING_NOTES.md, including a note on an
    unrelated concurrent session's own in-progress work (SomaPlayer/WorldLoaderHpm.cpp) that left
    the shared build broken afterward - confirmed unrelated to this entry.
  - Not attempted: Controls/Gameplay tabs, Resolution/TextureQuality/ShadowQuality/SSAO/AA/refresh
    rate/FOV/subtitles (none of these have a working backend in this engine yet, left out entirely
    rather than drawn as dead controls); pixel-exact 9-slice panel frame math (hand-composited
    approximation using the real assets/colour instead, see PORTING_NOTES.md).

- SOMA: splash screens + a real interactive main menu (2026-09-04)
  - DONE: fixed two real splash bugs (soma/src/game/SomaSplash.cpp) - logo textures requested
    GL_TEXTURE_RECTANGLE for compressed DDS data (spec doesn't support that combination, switched
    to eTextureType_2D) and the second logo (2048x1024) rendered at native size with no
    scale-to-fit, mostly off-screen on a normal display. Both logos verified rendering correctly
    at full opacity and correct size via a properly-timed headless screenshot (see PORTING_NOTES.md
    for a real multi-hour false alarm along the way - inconsistent screenshot timing relative to
    the splash's own fade timer looked exactly like a severe GUI rendering bug and wasn't one).
  - DONE: added a real interactive main menu (soma/src/game/SomaMainMenu.{h,cpp}) - title label,
    "New Game" (loads 00_01_apartment.hpm via the existing LoadMap()), "Quit" (cEngine::Exit()).
    Plain native GuiSet menu, not a recreation of SOMA's real ImGui-based menu (unintegrated
    toolkit). Found and fixed two real bugs while building it: cGui never received mouse input at
    all (nothing in HPL2/core pumps iMouse into cGui - the real game's own LuxInputHandler.cpp
    does that manually every frame, which this scaffold had no equivalent of until now), and
    DevIL mis-decodes SOMA's real uncompressed-8bpp-alpha font atlas format as all-zero RGB
    (every glyph rendered as a solid black box) - fixed narrowly in
    HPL2/core/sources/impl/BitmapLoaderDevilDDS.cpp, bypassing DevIL only for that exact
    pixel-format signature. Verified live, headless, via real injected mouse clicks (not just
    visual presence): "New Game" loads the real map and teleports to the real PlayerStart
    (confirmed via camera_state), "Quit" triggers a real clean engine shutdown. Zero Dark Descent
    regression from the shared DDS-loader change (verified live). Full writeup in PORTING_NOTES.md.
    Not attempted: Load Game/settings, any menu functionality beyond New Game + Quit.
  - DONE (follow-up, user feedback that the menu looked nothing like real SOMA): found
    graphics/startmenu/ ships a complete, unused real 2D menu art set (background, title +
    flicker-animation frames, button graphics, controller icons). Wired the real background and
    title logo into the menu (soma/src/game/SomaMainMenu.{h,cpp}); buttons still use the generic
    skin (real button art needs a custom material/skin entry, not just a texture swap - follow-up).
    Verified live: renders correctly, New Game click still works. Full writeup in PORTING_NOTES.md.
    Still open: real button art, title flicker animation, splash/menu are still a hard cut where
    the real game appears to blend them (see Premenu.png reference image, described in
    PORTING_NOTES.md), and a real ImGui integration (the actual menu tech) remains out of scope.
  - DONE (follow-up, user asked to decompile/read the real SOMA binary to find how the menu
    actually works rather than approximate it - soma/src/game/SomaMainMenu.{h,cpp} fully rewritten):
    SOMA's real Soma.bin.x86_64 ships with full debug info (unstripped), and its real menu logic
    ships as plain uncompiled AngelScript text alongside it - script/modules/MenuHandler.hps
    (~7300 lines, a cScrMenuHandler module) and script/custom_depth/helper_custom_depth_imgui/
    helper_imgui_options.hps, both read directly out of SOMA's own game data rather than
    recreated from memory/guesswork. Confirmed: the real menu draws through Frictional's own
    "cImGui" immediate-mode system (cLux_GetCurrentImGui()), not cGuiSet/cWidget - GetMainMenuFile()
    is unrelated (that's cWidgetMainMenu, the in-game pause-menu widget). Ported the real,
    observable behaviour into this engine's existing cGuiSet primitives (no AngelScript/cImGui
    integration exists here):
      - Root cause of "looks strange": the real script's "values based on 1280x720 resolution"
        constants were previously being used as raw screen pixels. OptionMenu_GetScaledSize()/
        GetTopLeftOffset() (the real script's aspect-ratio-correcting scale) is exactly what this
        engine's own cGuiSet::SetVirtualSize() already does (Dark Descent's menus use it too) -
        now sets a real 1280x720 virtual canvas and uses the real script's constants verbatim
        (kMainMenuButtonPos, kTitlePos, kTitleSize, kOptionMenu_ButtonBgSize, ...).
      - Real background is "menu_background.tga" (GuiBackground()), not "startmenu_background.tga"
        - an earlier pass's wrong guess.
      - Added the real phase-1 dirt-corner overlays, the real animated 3-layer "cathedral"
        ghost-trail effect behind the title (GuiBackground_DrawCathFacePart - p1_left/right/
        jaw.tga, sine-wobble, front/back scale+colour blend, ported constants verbatim), and the
        real title glitch-flicker state machine (random flicker-frame swap + ghost overlay +
        slow colour pulse, real timing constants from GuiBackground(), ported from the script's
        named-timer hash to plain countdown floats).
      - Menu items are real text buttons (Sansation Large Bold 36px, captions from
        config/base_english.lang's Menu category: CONTINUE/NEW GAME/LOAD GAME/OPTIONS/EXIT), not
        skin-drawn graphical buttons - OptionMenu_ButtonMainMenu() never draws button art for the
        main menu, only a highlight bar (startmenu_button_long.tga, real teal tint) behind the
        focused item, with black-on-highlight text (matches real script exactly: bSelected ?
        cColor(0,1) : cColor(1,1)). Continue/Load Game render as real disabled grey labels (no
        save system here, same as a real fresh install with mbCanContinue false). Hit-testing is
        direct immediate-mode mouse-rect testing against mpGuiSet->GetMousePos() every frame
        (cGui::SendMousePos() already virtual-size-converts it) - not cWidgetButton/focus
        routing, matching the real script's own ImGui_DoButtonExt() immediate-mode style rather
        than Dark Descent's widget system.
      - Real click behaviour: a 0.15s flash (random jitter-frame texture on the clicked item)
        before the action actually runs (real "ButtonClicked" timer), not an instant action.
    Verified live via headless screenshots (1280x720 headless render = the virtual canvas
    1:1) at rest, hovering "New Game" (real teal highlight bar + black text + mid-glitch title
    frame all present), and clicking "Exit" (real white jitter-flash on the clicked row, then a
    clean engine shutdown a few seconds later - confirmed via gdb backtrace mid-shutdown, not a
    hang). Builds clean (soma_game target). Not reproduced (real, but out of scope - no save/
    options/controller systems exist in this scaffold yet): the exit confirmation message box,
    click/glitch sound effects, save-dependent Continue/LoadGame enablement, and background
    phases 2-5 (progress-gated - this scaffold has no progress tracking, always phase 1).

- SOMA: fix the translucent/forward rendering pass HPSL gap (dispatched to background agent, 2026-09-04)
  - IN PROGRESS (branch worktree-agent-ae944cae3b9e77b3b): split the single "gray/magenta
    artifact" into its two real, independent root causes via per-object bisection (temporarily
    logging every real object/material hitting cRendererDeferred::RenderTranslucent(), then
    continue-skipping candidates by name/material, rebuild, re-screenshot - same discipline as
    the prior session's per-pass bisection, one level more granular). **Fixed and verified
    live**: MaterialType_BasicTranslucent.cpp never registered/set `avInvScreenSize`
    (deferred_transparent_frag.hpsl's refraction UV uniform - Dark Descent's own
    deferred_transparent_frag.glsl never declares it, so this material type's C++ never had a
    slot for it), which defaulted to (0,0) and collapsed every refracting translucent surface's
    sampled UV to the framebuffer's stale (0,0) corner - this was the large flat-gray "window"
    shape (static_objects/urban/walls/plain_glass_livingroom.mat). After the fix, real refracted
    background wall/texture detail is visible through the glass (pixel-diff confirmed, not a
    placebo); ctest 4/4 green; zero Dark Descent regression (empty hpl.log, correct Profiles
    menu screenshot, headless boot).
  - **Fixed and verified live (master, follow-up pass, same day)**: the magenta shape was
    entities/technical/block_box/block_box.mat (a real "blocking volume"/collision-marker prop,
    not garbage) hitting deferred_transparent_frag.hpsl's unconditional "×8 HDR precision boost"
    (the shader's own comment) on `Add`/`Alpha`/`PremulAlpha`/`UseEnvMap`/`UseRefraction`, which
    Dark Descent's real .glsl equivalent never has and this port had no compensating downstream
    divide for. Fixed by removing the boost at its source in the transpiler itself
    (soma/src/game/HpslTranspiler.cpp's new RemoveUncompensatedHdrPrecisionBoost(), a
    std::regex-based line removal in the TranspileHpslToGlsl() pipeline - confirmed via a full
    corpus search this exact pattern exists in exactly one real HPSL file, so it can't affect
    anything else), not the originally-planned GL_CONSTANT_COLOR blend-state approach (rejected
    as more invasive and not exact for Alpha/PremulAlpha blend modes). 100% contained to
    SOMA-only code, zero Dark Descent regression risk by construction. Verified live: the
    material's on-screen pixel went from clipped/saturated bright magenta to (58,2,24), matching
    raw_texture_color × this scene's own exposure multiplier almost exactly - a real, correctly-
    scoped fix, not a coincidental color shift. ctest 4/4 green. Note: an earlier verification
    pass wrongly concluded this fix had zero effect, due to a `cp` deploy step hitting an
    interactive overwrite prompt and silently timing out without actually deploying the new
    binary - corrected after re-deploying cleanly (`rm -f` the destination first, always, when
    redeploying a same-named test binary in this environment - `cp` can hang instead of erroring).
  - **Still open**: static_objects/urban/plain_glass_livingroom.mat's CubeMap texture unit is
    declared Type="Rect" (a real, non-cubemap DDS file, confirmed via its DDS caps2 header), which
    cMaterialManager::GetType() (MaterialManager.cpp:496) doesn't recognize and silently loads
    as a plain 2D texture bound where the shader expects samplerCube - real content-format
    question (what does HPL3's real renderer do with Type="Rect"?), not a one-line fix, left
    uninvestigated. Exposure-system upgrade (real spatial blend between multiple ExposureAreas,
    WhiteCut) and a second-map sweep are also still not attempted. Full writeup in
    PORTING_NOTES.md under "SOMA/HPSL: the ×8 HDR-precision-boost gap, actually fixed this time".

- Headless testing must never wake the physical screen or make real sound
  - DONE (master, this session): SDL_DisableScreenSaver() (resets the X
    server's idle/DPMS timer) and the real X11 input-grab/relative-mouse
    (XWarpPointer) calls are now skipped whenever OPENHPL_HEADLESS_SOCKET is
    set; OpenAL is forced to ALSOFT_DRIVERS=null (a real, silently-
    succeeding null device). All inert during normal play. Verified live:
    a headless SOMA boot opens OpenAL's "No Output" device and reaches
    "Game Running" identically to before.
- Only one headless instance should run at a time
  - DONE (master, this session): concurrent headless instances (multiple
    worktree-agents each testing their own build) were found competing for
    the same GPU - FPS as low as ~3-8 in a scene that renders ~22 FPS with
    only one instance running. Added
    AcquireHeadlessSingleInstanceLock() (HPL2/core/{include,sources}/system/
    HeadlessControl.{h,cpp}) - an flock() on a fixed path under
    $XDG_RUNTIME_DIR, held for the process's life and released
    automatically (crash-safe) by the kernel. A second instance blocks
    (doesn't fail) until the first exits, logging once so
    scripts/headless-check.sh's CPU-tick hang heuristic doesn't misreport a
    queued process as stuck. Called from cSDLEngineSetup's constructor -
    the earliest possible headless-gated point, before SDL_Init - so a
    queued instance doesn't even open a window/X11 connection while
    waiting. Verified live with two concurrent SOMA instances (see
    PORTING_NOTES.md would be the usual place for this, but this fix is
    small/self-contained enough that its own commit message has the full
    detail and live-verification steps).
- Engine test suite
  - DONE (branch worktree-agent-a4c6a0baa91b3ca13): Added HPL2/tests/CStringTests.cpp
    (file path helpers + string/value conversions in cString) and
    HPL2/tests/PlatformXdgPathTests.cpp (XDG Base Directory / xdg-user-dirs
    resolution in cPlatform::GetSystemSpecialPath), wired into the existing
    CTest setup in HPL2/tests/CMakeLists.txt. All 3 tests (incl. the
    pre-existing PhysicsNewtonTests) pass via a clean cmake configure + build +
    `ctest --test-dir amnesia/src/build`.
- Native FPS counter (can be enabled/disabled in menu)
  - DONE (branch worktree-agent-af71945259e6a8e9b): The engine already had a
    full FPS overlay (cLuxDebugHandler::mbShowFPS, drawn every frame via the
    always-active "GameDebug" GuiSet, persisted to user_settings.cfg
    [Debug]/ShowFPS) but the only toggle was the developer-only debug window
    (gated behind main_settings.cfg [Main]/LoadDebugMenu, off by default).
    Added cLuxDebugHandler::GetShowFPS()/SetShowFPS() and a "Show FPS"
    checkbox in the normal Options > Graphics menu
    (amnesia/src/game/LuxMainMenu_Options.{h,cpp}), following the existing
    VSync/GuiScale load/save pattern - no new timing or config code needed.
    Verified live via the headless automation server + SDL input injection:
    checkbox renders, click toggles it, OK persists ShowFPS=true to
    user_settings.cfg, the overlay appears in-game and updates every frame,
    and the setting survives a full process restart without re-toggling.
    Note: a checkbox placed beside VSync in the same row rendered nothing
    despite valid geometry/clip-region checks (likely occluded by the
    group's decorative border graphic, not root-caused) - placed as its own
    row below GuiScale instead, which renders correctly.
- Low-risk rendering optimisations
  - DONE (branch worktree-agent-a6152b7d4eedad1f4): Audited HPL2/core's
    renderer (Renderer.cpp/RendererDeferred.cpp/RenderFunctions.cpp/
    RenderList.cpp) and found its GL-state caching already extensive and
    deliberate - SetTexture/SetBlendMode/SetCullMode/SetDepthTest/SetProgram/
    SetVertexBuffer/SetMatrix/SetScissorRect/SetAlphaMode/SetChannelMode all
    already skip redundant driver calls at the iRenderFunctions layer; shadow
    maps already have a caster/transform cache (ShadowMapNeedsUpdate) instead
    of re-rendering every frame; GPU uniform locations are cached at first
    lookup; render-list sort comparators use a precomputed ViewSpaceZ, not
    per-comparison recomputation; the GUI already batches draws by
    texture/material/clip-region. Two low-level GL state checks
    (cLowLevelGraphicsSDL::SetCullActive/SetCullMode's cache check, and
    SetCurrentFrameBuffer's `if(true)`) look redundant but are deliberately
    disabled - BeginRendering/EndRendering rely on them always reaching the
    driver to resync hardware state that other code (GUI draws, other
    renderer instances) can change through paths that bypass the
    higher-level per-instance cache; confirmed by their being original
    Frictional code (git blame), not a porting artifact, and left untouched.
    Found and fixed one genuine, narrowly-scoped redundancy:
    cLowLevelGraphicsSDL::SetTexture() (HPL2/core/sources/impl/
    LowLevelGraphicsSDL.cpp) unconditionally called glActiveTextureARB every
    invocation even when the unit hadn't changed, and glEnable(target) even
    when the same target was already left enabled by the previous bind on
    that unit - both real per-call driver calls on the hottest texture-bind
    path (every changed texture unit of every material, every render pass:
    G-buffer, each shadow map, SSAO, post effects), both fully safe to skip
    (glActiveTextureARB is only ever called from this one file, so no
    cross-subsystem cache-desync risk; glEnable on an already-enabled
    capability is a defined no-op, so skipping it cannot change what
    renders). Added an mlCurrentActiveTextureUnit cache and an
    already-enabled check; verified headless (OPENHPL_HEADLESS_SOCKET,
    hidden X11 window) against the real Dark Descent install - the
    profile/pre-menu scene (deferred G-buffer, the torch's shadow-casting
    spotlight, SSAO, GUI compositing, all exercising this path heavily)
    renders identically across repeated screenshots, no artifacts, no new
    hpl.log warnings. Left as a follow-up rather than attempted (see new
    bullet below): cRendererDeferred::SetupLightsAndRenderQueries()
    hplNew()s a fresh cDeferredLight per visible light every single frame
    and STLDeleteAll_NoClear()s the whole list at the start of the next -
    real per-frame heap churn, but pooling/reusing them safely would need a
    full field-by-field audit of every write path across ~200 lines of
    RendererDeferred.cpp to rule out stale-field bugs, which is more than
    "low risk" allows.
- First class Wayland and Linux support
  - DONE (branch worktree-agent-a31bde44ba89ffbd9): Investigated video driver
    selection, HiDPI, fullscreen mode-setting, cursor grab/relative mouse, and
    desktop metadata. Found video-driver selection and relative-mouse/grab
    already correct (confirmed live this session on a real native-Wayland
    desktop - normal launch comes up as a genuine Wayland toplevel, no
    XWayland; no manual mouse-warp hack anywhere). Fixed two real bugs:
    (1) LowLevelGraphicsSDL::Init() now uses SDL_WINDOW_FULLSCREEN_DESKTOP
    instead of exclusive SDL_WINDOW_FULLSCREEN when the active SDL driver is
    Wayland (which has no exclusive-fullscreen modesetting), X11/Windows
    unaffected; (2) the Launcher (FLTK) now sets Fl_Window::default_xclass()
    to the game binary's basename, since it previously had no app-id/WM_CLASS
    set at all and showed up as generic "FLTK" to compositors/taskbars.
    Both fixes build clean and the fixed Amnesia binary was verified via
    scripts/headless-check.sh (boots, stays CPU-active, control-socket ping
    and a correct-looking screenshot all succeed) - but per an explicit
    mid-session instruction to keep testing headless, and since headless mode
    itself forces the X11 driver, **neither fix's actual Wayland-specific
    behavior was verified in a live interactive Wayland session** - flagging
    this honestly rather than claiming it. HiDPI scaling (no
    SDL_WINDOW_ALLOW_HIGHDPI, no drawable-size-aware viewport/mouse code) and
    a missing StartupWMClass in the out-of-repo RPM spec's five .desktop
    entries were found but deliberately left as follow-ups (see new bullets
    below) rather than risking a working port. Full writeup in
    PORTING_NOTES.md under "First-class Wayland/Linux desktop integration".
- Fix physics and event scripting bugs in Amnesia Machine for Pigs
  - IN PROGRESS (branch worktree-agent-a1292f55af32b9024): AMFP's real
    physics/scripts only run via the shared amnesia/src/game binary +
    AngelScript compat shims (b2f51f4), not the free-fly amfp/src/game
    scaffold (no player/scripts there to have bugs). Found and fixed two
    real, verified bugs in the headless-control test harness itself
    (amnesia/src/game/LuxBase.cpp): (1) the "state" command SIGSEGV'd
    (NULL iCharacterBody deref) whenever called before a map was loaded -
    confirmed via coredumpctl+gdb; (2) the "start_map" command never
    switched the input handler/updater container the way the real UI does,
    so injected movement input was silently dropped even though the action
    system correctly registered it - confirmed via live gdb. Both fixed and
    re-verified with zero regression against real Dark Descent data. With
    both fixed, reproduced a real, striking symptom - the player falling
    through geometry into an endless void and taking fatal fall damage
    during a long scripted intro cutscene - but could not disentangle
    whether this is the previously-hypothesized (b2f51f4) discrete-
    collision-at-seams bug in AMFP's batched floor mesh, or an artifact of
    blindly holding one movement key with no camera control through a
    multi-minute scripted sequence never designed to be driven that way.
    Full writeup, repro steps, and a proposed more-surgical next repro in
    PORTING_NOTES.md under "AMFP physics/scripting bug hunt".

- SOMA support via reverse engineering and binary analysis
  - IN PROGRESS (branch worktree-agent-af551f566c8c3a08b): Extended the
    existing soma/src/game/HpslTranspiler.{h,cpp} HPSL->GLSL transpiler
    proof-of-concept beyond the trivial clear_vtx/clear_frag pair, proven
    (string-level, see below) against five more real files from a real SOMA
    install (null_vtx/null_frag, deferred_depthonly_frag,
    deferred_posteffect_quad_vtx, debug_overdraw_frag): added mul(A,B)->
    (A*B) and sample(tex,uv)->texture2D/textureCube/texture2DRect(tex,uv)
    intrinsic rewriting, stripped the D3D-style ": N" texture-binding-index
    suffix on uniform declarations, and fixed a real bug from the previous
    pass - HPSL's px_vPosition convention output was being emitted as a
    plain GLSL varying instead of gl_Position (vertex)/gl_FragCoord
    (fragment), which would have compiled but not actually rendered
    correctly. Added HPL2/tests/HpslTranspilerTests.cpp (new CTest,
    dependency-free string-level checks on the transpiler's output,
    confirmed passing via `ctest --test-dir amnesia/src/build
    --output-on-failure` alongside zero-regression PhysicsNewtonTests).
    **Could not re-verify live against a real GL context this session**:
    the sandboxed execution environment blocked both deploying a built
    binary into the real SOMA Steam install and launching it from a
    symlink-farmed scratch directory outside it (reads from the real
    install were fine; writing/launching there was refused by an
    environment-level command classifier) - so the existing
    HpslTranspilerSelfTest.cpp live-glCompileShader() self-test (extended
    to cover all seven files) is unverified this pass; the new CTest is
    strictly weaker (syntax, not a real compile) but is what was actually
    run. Full findings, corpus inventory (77 real .hpsl files sized/
    categorized), and next steps in PORTING_NOTES.md under "SOMA: HPSL
    transpiler extended". Not close to real rendering: the actual
    material shaders that matter (deferred_base_vtx.hpsl, 982 lines) need
    constant-buffer and GPU-instancing (cTextureBuffer) support this pass
    didn't attempt - see new bullets below.
  - IN PROGRESS (branch worktree-agent-a385d484e71b61dce): Entity/area
    loaders for SOMA's own taxonomy, mirroring rebirth/src/game/
    RebirthLoaders.{h,cpp}'s approach - see new soma/src/game/
    SomaLoaders.{h,cpp}, registered from cSomaBase::InitEngine() (SomaBase.cpp).
    Collected the full Type/AreaType list two ways: a live headless boot of
    two real maps (00_01_apartment.hpm, 00_02_subway.hpm) reading hpl.log's
    "no loader"/"no area loader" warnings, and a full census across the
    entire real install (`grep -rohE 'EntityType="[^"]+"' entities/`,
    `grep -ohE 'AreaType="[^"]+"' maps/.../*.hpm_Area`) - the census is more
    complete than any handful of sampled maps, since SOMA's per-map .hpm_Entity
    data has no Type attribute of its own; each Entity instance only
    references a FileIndex into a .ent file, and the real EntityType lives in
    that .ent's own `<UserDefinedVariables EntityType="...">`. 46 entity types
    (StaticProp/Prop_Rigid/Prop_Grab/... plus Agent_*/Critter_* monster/NPC
    types, registered too since the generic loader is equally inert for any
    type name) and 24 AreaTypes (PlayerStart gets a real cStartPosEntity via
    a dedicated loader mirroring cRebirthAreaLoader_PlayerStart; the rest -
    Trigger, Soundscape, VisibilityArea, VisibilityPortal, InteractAux,
    CameraAnimation, etc. - are no-ops, same as cRebirthAreaLoader_Noop).
    Also wired an optional PlayerStart-by-name lookup into
    cSomaBase::LoadMap()/the "start_map" headless command (new `pos=` field) -
    nice-to-have from the task brief, done and verified live. **Verified
    end-to-end**: real 00_01_apartment.hpm went from "0 entities, 0 areas" to
    "425 entities, 80 areas" (145 static objects/70 lights unchanged, as
    expected), 00_02_subway.hpm went 0->214 entities/0->37 areas, zero
    remaining "no loader"/"no area loader" warnings in hpl.log on either map,
    and `camera_state` after `start_map ... pos=PlayerStartArea_1` reported
    the exact map-authored PlayerStart coordinates (y offset +0.5 applied).
    A headless screenshot after the fix shows real per-entity geometry
    present in the scene (as expected, still unlit/wrongly-shaded pending the
    separate HPSL->GLSL shader work above - not a regression from this
    change, static geometry rendered the same way before it).
    **Investigated and corrected the "sounddata.cfg never loaded" gap
    documented lower in this file - it turns out not to be the real
    problem.** `sounddata.cfg` does not exist as a file anywhere in the real
    SOMA (or Rebirth or Bunker) Steam install despite being declared in
    main_init.cfg, and Dark Descent's own LuxBase.cpp never reads any such
    key either (its main_init.cfg has no `SoundData` entry at all - the
    "mirror LuxBase.cpp" suggestion in the existing bullet was based on a
    false premise). The actual reason every `.hpm_Sound` reference fails
    ("Cannot find sound entity ..."/"Couldn't create SoundEntity ...") is
    that SOMA/Rebirth/Bunker's real sound content is FMOD Studio/Designer
    banks (sounds/**/*.fev + *.fsb + *.fdp, confirmed present in all three
    real installs) - a completely different sound architecture from HPL2's
    own OpenAL-backed cSoundEntityManager/.snt-XML system that
    cWorld::CreateSoundEntity() expects; essentially zero real .snt files
    ship with any of the three games (one stray .snt found in SOMA's lang/
    tree, unrelated). No sounddata.cfg-loading fix, however written, could
    have found these sounds - the gap is a missing FMOD-bank sound backend
    entirely, a genuinely multi-week undertaking (parse/link the FMOD bank
    format, or re-encode banks to individual OGG+.snt pairs), not the
    "small, self-contained" fix the existing bullet assumed. Corrected that
    bullet below rather than leaving it to mislead the next reader; did not
    attempt an FMOD backend this session (out of scope). Full detail in
    PORTING_NOTES.md's "SOMA: entity/area loaders, and the real
    sounddata.cfg story" section.
  - IN PROGRESS (branch worktree-agent-a616b92d063d83a8f): Implemented HPSL constant-buffer
    (`cBuffer NAME [: N] { ... };`) support in the transpiler (`FlattenConstantBuffers()` in
    `soma/src/game/HpslTranspiler.cpp`) - flattens each block into plain top-level `uniform
    TYPE NAME;` declarations, since HPSL's own cBuffer members are always referenced
    unqualified (same as this engine's existing non-cBuffer uniforms) and GLSL 120 has no
    named-uniform-block syntax to target anyway. **Correction**: the "PreprocessParser.h
    constant-buffer comment" flagged as unread across two prior sessions does not exist -
    grepped the whole file, zero matches; whatever prompted that phrasing wasn't real. Also
    found and exploited a bigger structural shortcut: `deferred_base_vtx.hpsl`/
    `deferred_base_frag.hpsl`'s entire body is `@ifdef UseTextureBuffer`-gated with a "TEMP
    BACKWARD COMBATABILITY" `@else` branch that avoids `cTextureBuffer`/GPU-instancing
    entirely (a single flat `cBuffer`, plain `cTexture2D` instancing only when
    `UseMeshInstancing`/`UseStaticMeshInstancing` are separately set) - so the chosen strategy
    is for whichever code selects HPSL combo-variables to simply never define those three,
    steering every material onto the already-transpilable branch and accepting no GPU
    instancing, rather than porting `cTextureBuffer` itself (still rejected with a clear error
    if actually encountered - no GLSL 120 equivalent exists). Fixed three more real gaps found
    only by attempting real files: `cMatrix3f`->`mat3` (unmapped, caused a live GLSL compile
    failure), a custom-`attribute` fallback for vertex inputs with no fixed-function GLSL 120
    built-in (`vtx_vTangent`/`vtx_vBoneIndices`/`vtx_vBoneWeight` - not yet wired to real
    per-vertex data on the C++ mesh-upload side, flagged as a follow-up below), and the
    comment-parsing bug in `deferred_gbuffer_solid_frag.hpsl`'s parameter list that a different
    concurrent session (branch worktree-agent-a98dec63e5599d81b, see its entry below) found
    live via the real `GpuShaderManager` wiring but left for this transpiler-focused task to
    fix (`StripLineComments()`). **Verified live, real GL compile**: contrary to two prior
    sessions' reports of an environment-level block on launching a built `Soma.bin.aarch64`,
    launching directly worked this time (scratch dir + symlinked real data,
    `OPENHPL_HEADLESS_SOCKET` for headless mode, `XDG_STATE_HOME` overridden too since
    `hpl.log`'s new XDG-compliant path - see the "hpl.log path collides" bullet below - is
    shared machine-wide across every concurrent SOMA process, not just per-install as before);
    `hpl.log` confirms `HpslTranspilerSelfTest: overall result: PASS` for all ten files now
    covered, including `deferred_base_vtx.hpsl`, `deferred_base_frag.hpsl`, and
    `deferred_gbuffer_solid_frag.hpsl` (a real, minimal-combo material shader triple) all
    PASSED - real GLSL 120, real `glCompileShader()`, live. Also added 7 new dependency-free
    regression cases to `HPL2/tests/HpslTranspilerTests.cpp` (15 total, all passing via `ctest
    --test-dir amnesia/src/build -R HpslTranspilerTests`). Full writeup in PORTING_NOTES.md
    under "SOMA: HPSL constant buffers + a real material shader pair transpiles". This
    session's minimal combo doesn't exercise `UseNormalMapping`/`UseSkeleton`/`UseSway`/
    `UseDissolve`/`MaterialType_Translucent`/etc. - each may surface its own unmapped
    type/construct the same way `cMatrix3f` did; widening the combo and re-running the live
    self-test is the fastest way to find the next one (see new bullets below for concrete
    follow-ups, including the mesh-upload attribute wiring this session's scope couldn't
    reach).
  - CONTINUED (same branch worktree-agent-a616b92d063d83a8f, after the coordinator merged the
    above plus the separate GpuShaderManager wiring onto master and ran start_map against real
    00_01_apartment.hpm with real .mat-driven combos): fixed three more real compile gaps the
    live pipeline surfaced in sequence (`UseDepth`/`UseLinearDepth` combo-variable naming
    mismatch - fixed in GpuShaderManager.cpp's HPSL-fallback path, not the transpiler, since
    it's HPL2's own material C++ unconditionally setting a name written for Dark Descent's own
    shaders; `cTexture2DCmp`/`sampleCmp()` -> `sampler2DShadow`/`shadow2D()`; `cVector*l` +
    `load()` -> `ivecN` + `texelFetch()`, with a per-file-only `#version 130` bump since
    texelFetch needs it). All confirmed live: zero "Couldn't transpile"/"Failed to compile"/
    "Couldn't create program" errors remain for any currently-requested real material shader.
    **Then found and fixed a much bigger rendering-correctness bug**, root-caused (not
    guessed) via a temporary render-list object-count diagnostic confirming ~427-430 solid
    objects WERE being submitted for rendering every frame (ruling out occlusion/culling) while
    the screen showed only skybox in every direction/position: this engine's own C++
    (RenderFunctions.cpp's iRenderFunctions::SetMatrix(), used for every solid-object draw)
    feeds per-object transforms exclusively through the legacy fixed-function matrix stack -
    the same mechanism this engine's own hand-written GLSL 120 shaders rely on
    (`gl_Position = ftransform();`, no custom uniform at all) - so the transpiler's own
    flattened `uniform mat4 a_mtxModelViewProjection;` (and 3 siblings) was never getting real
    per-object data, sitting at GLSL's default zero value forever: degenerate, invisible
    geometry that compiled perfectly fine. New `SubstituteFixedFunctionMatrixUniforms()` in
    HpslTranspiler.cpp maps `a_mtxModelViewProjection`/`a_mtxModelView`/`a_mtxProjection`/
    `a_mtxNormal` to their exact GLSL fixed-function built-in equivalents
    (`gl_ModelViewProjectionMatrix`/`gl_ModelViewMatrix`/`gl_ProjectionMatrix`/
    `gl_NormalMatrix`) instead of leaving them as uniforms; `a_mtxModel` (no fixed-function
    equivalent exists) and `a_mtxUV` (already correctly fed real data through a different,
    working by-name mechanism) deliberately left alone. **Verified live, dramatic before/after
    screenshots**: before, only skybox visible at every tested camera position/angle
    (including directly beside a real light source); after, the skybox is now correctly
    occluded by real geometry silhouettes at the same positions - solid objects are genuinely
    being drawn in their correct world-space positions for the first time this port has ever
    confirmed. Geometry still renders unlit/black rather than diffuse-lit - investigated
    further with the same instrument-and-verify discipline: a second temporary diagnostic
    confirmed 37 lights DO reach the render list every frame at a position right next to a real
    light (ruling out "zero lights visible"), yet `deferred_light_frag.hpsl` is never requested
    at all - the gap is somewhere in `RenderLights()`'s five light-type/size/shadow dispatch
    functions or the `InitLightRendering()` bucketing that feeds them
    (RendererDeferred.cpp:1829), not a shader-syntax problem. Deliberately not chased further
    this pass - shared HPL2 core code used by every module including Dark Descent, higher risk
    to touch speculatively than the SOMA-only work above, and proper diagnosis needs more
    instrumentation/time than remained. Also found, not fixed, a separate class of gap: several
    shader requests in this engine's C++ ask for exact filenames that don't exist anywhere in
    the real .hpsl corpus (`deferred_illumination_frag.glsl` vs real
    `deferred_illumination_solid_frag.hpsl`, `deferred_gbuffer_skybox_frag.glsl` vs real
    `deferred_skybox_frag.hpsl`, `deferred_decal_frag.glsl` vs real
    `deferred_gbuffer_decal_frag.hpsl`, `posteffect_bloom_blur_vtx.glsl` with no obviously
    corresponding real file) - same shape of fix as the UseDepth alias but for filenames, not
    attempted this pass. 6 more regression cases added (21 total, all passing). Both fix
    commits left on the branch, not merged/pushed. Full writeup, all four before/after
    screenshots' context, and concrete next steps in PORTING_NOTES.md under "SOMA: real
    geometry finally renders - matrix-uniform bug found and fixed; lighting still missing".
- Amnesia Rebirth via reverse engineering and binary analysis
  - IN PROGRESS (branch worktree-agent-a2b5ccf8cae169af3): Confirmed via real
    binary/data analysis that Rebirth is HPL3-generation, same as SOMA (not
    HPL2) - its Steam install ships core/shaders/hpsl/*.hpsl (SOMA's exact
    shader set: tessellation/SSAO/SSSSS passes, no HPL2 equivalent) instead of
    compiled .glsl, and its maps load via the already-shared WorldLoaderHpm
    (.hpm) parser. Implemented rebirth/src/game/RebirthLoaders.{h,cpp}:
    reverse-engineered entity/area "Type" loaders for Rebirth's own taxonomy
    (Prop_Rigid, Prop_Grab, Prop_SwingDoor, Prop_Readable,
    Prop_SketchbookReadable, StaticProp, PlayerStart, Trigger, Soundscape -
    none of these are Dark Descent's own type names), wired into
    RebirthBase.cpp's InitEngine(). Verified end-to-end via the headless
    automation server (confirmed the headless path itself works for this
    module for the first time - ping/camera_state/log_tail/screenshot/quit
    all round-tripped against a real running process): on Rebirth's real
    01_00_intro.hpm start map, entities loaded went 0 -> 77 and areas 0 -> 15,
    the debug camera now spawns at the map's real PlayerStart (was falling
    back to world origin), and a headless screenshot shows real prop geometry
    (a propeller, engine nacelle) actually present in the scene. Also added
    camera_state/set_camera headless commands (Rebirth had none registered).
    Still nowhere near playable: real lit/textured rendering needs the same
    open HPSL->GLSL translation work SOMA needs (every deferred_*.glsl
    material shader fails to load, so all geometry currently renders as flat
    black silhouettes - screenshot-confirmed); no player controller, scripts,
    or gameplay-type behavior (grabbing, doors, notes) exists, only inert
    static geometry. New blockers found this session, not yet acted on: a
    handful of placeholder meshes reference .fbx (format loader deliberately
    removed from this port), material type 'projectedUV' is unrecognized,
    world-entity sub-type 'LensFlare' is unrecognized. See new bullets below
    for concrete next steps.
- Amnesia Bunker support via reverse engineering and binary analysis
  - IN PROGRESS (branch worktree-agent-a648529be0638f7df): Verified via binary strings
    (`E:\bunker\hpl3\...` embedded debug paths in AmnesiaTheBunker.exe) and shader inventory
    (109 `.hpsl` files under `core/shaders/hpsl/`, effectively zero real `.glsl`) that the
    Bunker runs on HPL3, same lineage as SOMA/Rebirth - the task brief's "maybe closer to
    HPL2" hypothesis is false for shaders/rendering, confirmed rather than assumed. Its `.hpm`
    map format is the same HPL3 split-track layout already handled generically by
    `cWorldLoaderHpm` (added for SOMA, reused unmodified). Implemented and verified live
    (headless, real game data) a concrete fix: `bunker/src/game/BunkerAreaLoader.h`/`.cpp`, a
    `PlayerStart`-AreaType `iAreaLoader` (same mechanism as Dark Descent's own
    `cLuxAreaNodeLoader_PlayerStart`) so the debug camera now spawns at the map's real
    `Start_Begin` coordinates instead of world origin - confirmed via a new headless
    `camera_state` control command matching the map's authored `WorldPos` exactly. See
    PORTING_NOTES.md's "Amnesia: The Bunker - engine generation verified, real PlayerStart
    spawn fix" section for full detail. Not close to a real boot: no shaders (HPSL->GLSL
    translation, same multi-week scope as SOMA's), no sound (`sounddata.cfg` never loaded),
    no entity-type loaders, no player/scripts. See new bullets below for next steps.

- Fix double-slash in xdg-user-dirs $HOME substitution: PlatformUnix.cpp's
  GetXDGUserDir() builds its own $HOME with a trailing slash already appended,
  then substitutes it verbatim into a "$HOME/Photos"-shaped value read from
  ~/.config/user-dirs.dirs, producing a doubled slash (e.g.
  "/home/user//Photos/"). Harmless in practice (POSIX collapses repeated
  slashes) but not strictly correct; found via HPL2/tests/PlatformXdgPathTests.cpp
  (TestUserDirsFileIsParsed), which currently asserts the double-slash as documented
  current behavior rather than fixing it (out of that task's scope).

- Shared HPSL->GLSL shader translation layer for HPL3-generation games
  (SOMA and Amnesia: Rebirth both need this - confirmed via binary/data
  analysis that both ship core/shaders/hpsl/*.hpsl instead of compiled
  .glsl, the same shader set almost verbatim). A proof-of-concept
  best-effort transpiler already exists at soma/src/game/HpslTranspiler.{h,cpp}
  (proven only against the smallest shader pair, clear_vtx/clear_frag - not
  wired into real rendering). This is a large, open-ended rendering
  architecture task, not a quick fix - whoever picks it up should evaluate
  reusing/extending that transpiler against Rebirth's real shader set too
  (deferred_terrain_tess_cs.hpsl and friends use tessellation stages with no
  HPL2 renderer equivalent - a full port likely needs new
  cRendererDeferred-equivalent code, not just transpiled shader text).
- Amnesia: Rebirth: add an .fbx mesh loader, or confirm which specific
  meshes need it and whether they're skippable placeholder/debug content.
  Found via a real headless boot of 01_00_intro.hpm: 9 "No loader for file
  extension 'fbx' found!" errors, clustered near "block_out_tools"/
  "block_temp.mat" (looks like non-shipping blockout/placeholder geometry,
  but not confirmed) and a LensFlare-related entity. This port deliberately
  removed the original x86-only FBX SDK loader (see README.md) since Dark
  Descent's own shipped content never needed it - worth confirming Rebirth's
  actual playable content doesn't need it either before investing in restoring
  FBX support.
- HPL2/core/sources/resources/EntityLoader_Object.cpp's cEntityLoader_Object::Load()
  has a fixed, hardcoded set of recognized <Entity>-embedded "WorldEntity"
  sub-types (Billboard/ParticleSystem/Sound/Beam/Light etc.) that logs
  "Entity world entity type 'X' is unknown!" for anything else. A real
  Amnesia: Rebirth map (01_00_intro.hpm) references a 'LensFlare' sub-type
  this list doesn't have. This is shared HPL2 core code (not rebirth/-local),
  so flagging as its own task rather than a rebirth/-owned agent editing
  core/ directly - needs whoever owns HPL2 core changes to add LensFlare
  (and audit real Dark Descent/AMFP/SOMA/Rebirth data for any other missing
  sub-types) if lens flare rendering is wanted.
- HPL2/core/sources/resources/MaterialManager.cpp's material type registry
  (see "Invalid material type 'X'!" at MaterialManager.cpp:348) doesn't
  recognize 'projectedUV', a material type at least one real Amnesia: Rebirth
  map (01_00_intro.hpm, static_objects/technical/block_out_tools/block_temp.mat)
  references. Also shared HPL2 core code - likely low priority until the
  HPSL/GLSL shader gap above is solved anyway, since Rebirth's
  deferred_projected_uv_{frag,vtx}.hpsl shaders would still need translating
  even once the material type itself is recognized.

- Amnesia: The Bunker also needs the shared HPSL->GLSL translation layer
  (see the "Shared HPSL->GLSL shader translation layer for HPL3-generation
  games" bullet above) - confirmed via binary/data analysis this session
  that its core/shaders/hpsl/*.hpsl set is the same HPL3 pipeline as
  SOMA's/Rebirth's (deferred_base_vtx.hpsl, deferred_gbuffer_solid_frag.hpsl,
  terrain tessellation shaders, etc.) - not a separate problem, just another
  consumer of the same eventual fix.
- **CORRECTED (was wrong)**: Phase 0 scaffolds (soma/, rebirth/, bunker/)
  never load sounddata.cfg, so every real map's .snt sound-entity references
  fail ("Couldn't create SoundEntity"/"Cannot find sound entity" - 90+
  instances just in the Bunker's trenches.hpm_Sound) - this part is still
  true, but the previously-suggested fix ("wire whatever cSoundHandler API
  loads sounddata.cfg, small and self-contained") is not: investigated for
  SOMA this session and found sounddata.cfg does not exist as a file
  anywhere in any of the three real Steam installs despite being declared in
  main_init.cfg, and Dark Descent's own LuxBase.cpp - the suggested mirror
  target - never reads any such key either (its main_init.cfg has no
  SoundData entry at all). The real content those .hpm_Sound references
  point at (e.g. "physics/wood/robust/roll", "Entities_Urban/kitchen/
  fridge/hum_loop") ships as FMOD Studio/Designer banks
  (sounds/**/*.fev+*.fsb+*.fdp, confirmed present in all three real
  installs), not HPL2's own OpenAL-backed .snt-XML sound-entity format that
  cWorld::CreateSoundEntity()/cSoundEntityManager expect - essentially zero
  real .snt files ship with any of the three games. The actual gap is a
  missing FMOD-bank sound backend (parse/link the FMOD bank format, or
  re-encode banks to individual OGG+.snt pairs at build/package time) - a
  genuinely multi-week undertaking, not a small config-loading fix. See
  PORTING_NOTES.md's "SOMA: entity/area loaders, and the real sounddata.cfg
  story" section for the full investigation. Not attempted this session for
  any of the three modules (out of scope) - whoever picks this up next
  should design around FMOD banks, not sounddata.cfg.
  - IN PROGRESS (branch worktree-agent-a98dec63e5599d81b): Wired the existing
    HPSL->GLSL transpiler (soma/src/game/HpslTranspiler.{h,cpp}, owned/extended
    by a separate concurrent agent - not touched here beyond reading its
    header) into the real shader-loading path,
    HPL2/core/sources/resources/GpuShaderManager.cpp's
    cGpuShaderManager::CreateShader() (both the variable-container branch real
    materials use via cProgramComboManager, and the plain-resource branch),
    which previously just errored ("Couldn't find file 'X.glsl' in resources")
    whenever a `.glsl` lookup failed. HPL2/core can't link soma/-owned code
    directly, so added a static function-pointer hook,
    `cGpuShaderManager::SetHpslTranspileCallback()` (HPL2/core/include/
    resources/GpuShaderManager.h, NULL by default), and had
    cSomaBase::Init() (soma/src/game/SomaBase.cpp) register
    TranspileHpslToGlsl through it; Dark Descent/AMFP/Rebirth/Bunker never
    call it, so it stays NULL and the new fallback code path is provably dead
    for them. **Verified zero regression for Dark Descent**: built both
    Amnesia and Soma from one CMake tree, A/B'd a pre-change vs post-change
    Amnesia binary headlessly against the real Dark Descent install (git
    stash for the "before" build) - both runs produce zero hpl.log output,
    and `compare`'s pixel-difference between the two runs' screenshots (~6500
    of 8.3M pixels, ~0.08%) is statistically identical to the inherent
    frame-to-frame noise (torch/particle flicker) between two runs of the
    *same* baseline binary (~6435 pixels) - i.e. no measurable behavior
    change. **Verified the wiring itself works against SOMA's real
    00_01_apartment.hpm** (start_map headless command): hpl.log now shows
    real material shader loads for `deferred_base_vtx.glsl` failing with
    "Couldn't transpile HPSL shader 'deferred_base_vtx.glsl' (from
    'deferred_base_vtx.hpsl'): ..." instead of the old "Couldn't find file"
    - i.e. the .hpsl fallback is genuinely found, preprocessed, and handed to
    the transpiler for every real material shader request. Found two
    transpiler gaps live (in addition to the already-known constant-buffer/
    instancing ones) blocking every real material: an unmapped vertex-input
    semantic `vtx_vTangent` (deferred_base_vtx.hpsl) and a comment-parsing bug
    in the parameter parser (deferred_gbuffer_solid_frag.hpsl, a `//diffuse
    rgb` comment line inside a parameter list is mis-parsed as a parameter) -
    for the other agent extending HpslTranspiler.cpp, not fixed here (out of
    this task's scope). **Rendering is still fully black**: a before/after
    screenshot pair of the real apartment map (before = this session's SOMA
    build with the wiring reverted via git stash, after = with it) is
    pixel-identical (PIL ImageChops.difference().getbbox() == None) - the
    wiring alone can't render anything until the transpiler gaps above are
    fixed, exactly as expected. None of the simpler shaders the transpiler
    already handles (deferred_depthonly_frag/deferred_posteffect_quad_vtx/
    debug_overdraw_frag/null_*/clear_*) turned out to be requested by
    CreateShader during a real apartment-map frame, so no *additional* live
    GL-compile proof beyond the existing self-test was obtained this pass.
    New finding, out of scope here, added as its own top-level bullet below:
    SOMA's hpl.log path (~/.local/state/open-hpl/soma/hpl.log) is shared and
    fopen(...,"w")-truncated by every process launched, with no per-instance
    uniqueness - a real problem for the multi-agent headless-testing workflow
    this repo now uses, discovered when a concurrent agent's own SOMA test
    process's log content got wiped by this session launching another
    instance.
- Amnesia: The Bunker needs its own entity/area "Type" loader set, the same
  idea as rebirth/src/game/RebirthLoaders.{h,cpp} (see the Rebirth entry
  above) but for the Bunker's own taxonomy - not yet reverse-engineered this
  session (only the single PlayerStart AreaType was scoped/fixed, see this
  file's Bunker entry above and PORTING_NOTES.md). A real headless boot logs
  "Couldn't find loader for type 'Prop_Rigid'/'StaticProp'/etc." the same
  way Rebirth's did before RebirthLoaders.cpp; inventorying the Bunker's
  actual entity Type strings (grep a real install's *.hpm_Entity/*.hpm_Area
  files for Type="..."/AreaType="...") and implementing loaders for them is
  the concrete next Bunker-specific step, independent of the shader work.

- HiDPI/fractional-scaling support for the game window (Amnesia/AMFP/Soma/
  Rebirth/Bunker, all via the shared HPL2 SDL2 windowing code,
  HPL2/core/sources/impl/LowLevelGraphicsSDL.cpp): SDL_WINDOW_ALLOW_HIGHDPI is
  never passed to SDL_CreateWindow(), and nothing calls
  SDL_GL_GetDrawableSize() - mvScreenSize (used for glViewport, the GUI's
  virtual-to-screen mapping, everything) comes from SDL_GetWindowSize() alone,
  which is the *logical* (points) size, not the real framebuffer pixel size.
  On a scaled display (confirmed live this session on a real desktop with two
  outputs both at Wayland Scale: 2) the game renders at half the real pixel
  resolution and gets bitmap-upscaled by the compositor - blurry, not a
  crash/regression, but not "first class" either. A correct fix needs
  SDL_WINDOW_ALLOW_HIGHDPI plus routing SDL_GL_GetDrawableSize()'s physical
  size through to mvScreenSize instead of SDL_GetWindowSize(), and scaling
  MouseSDL.cpp's window-coordinate mouse events by the same drawable/window
  ratio so hit-testing still lines up. Touches viewport/framebuffer-size code
  pervasively (dozens of GetScreenSize() call sites across
  HPL2/core/sources/graphics/ and amnesia/src/game/Lux*.cpp) - deliberately
  not attempted as part of the Wayland/Linux support task to avoid a
  half-done regression (mismatched viewport vs. framebuffer = rendering only
  into a corner of the window). See PORTING_NOTES.md.
- Packaging: ~/.local/rpm/specs/open-hpl.spec's five generated .desktop
  entries (open-hpl-amnesia/-machine-for-pigs/-soma/-rebirth/-bunker) set
  Exec=/Icon= but no StartupWMClass=, and none of their desktop-file IDs match
  the app-id the real running game window presents (SDL's argv[0]-basename
  default - confirmed live this session). Without a matching StartupWMClass,
  compositors/shells fall back to a generic icon for the actual gameplay
  window in the taskbar/alt-tab/dock even though the right icon is installed.
  Concrete fix, one line added to each entry (matches the actual
  deployed/renamed binary basename each wrapper script execs, per the spec's
  own %install section): StartupWMClass=Amnesia.bin.aarch64 (amnesia - also
  covers the Launcher now that its own app-id was fixed to match, see
  PORTING_NOTES.md), =AmnesiaOnAmfp.bin.aarch64 (machine-for-pigs),
  =OpenHplSoma.bin.aarch64 (soma), =OpenHplRebirth.bin.aarch64 (rebirth),
  =OpenHplBunker.bin.aarch64 (bunker). Not applied here - this file lives
  outside the git repo (personal RPM packaging, not tracked/reviewable via
  this branch), so left as a documented follow-up rather than edited
  directly. See PORTING_NOTES.md.
- cRendererDeferred::SetupLightsAndRenderQueries() (HPL2/core/sources/
  graphics/RendererDeferred.cpp) hplNew()s a fresh cDeferredLight for every
  visible light, every single frame, then STLDeleteAll_NoClear()s the whole
  mvTempDeferredLights list at the start of the next frame's call - real
  per-frame heap churn (allocation count = visible light count each frame).
  Found while doing the "Low-risk rendering optimisations" task (see DONE
  entry above) but deliberately not attempted there: pooling/reusing
  cDeferredLight instances instead of hplNew/hplDelete-ing them every frame
  would need a full field-by-field audit of every write path across the
  ~200 lines of RendererDeferred.cpp that populate a cDeferredLight (only
  mpShadowTexture/mbCastShadows get constructor defaults; every other field
  - mClipRect, m_mtxViewSpaceRender/Transform, mbInsideNearPlane, mpQuery,
  mShadowResolution - is written by scattered, type/path-conditional code
  later in the frame) to rule out a reused-but-not-fully-overwritten field
  leaking stale data from a previous frame's different light. Real
  candidate for a future performance pass, not "low risk" on its own.
- **RESOLVED this session (branch worktree-agent-a616b92d063d83a8f)**: SOMA/HPSL transpiler
  constant-buffer support, cTextureBuffer/instancing strategy, and a first real material
  shader transpile - see the new IN PROGRESS bullet under "SOMA support via reverse
  engineering and binary analysis" above and PORTING_NOTES.md's "SOMA: HPSL constant buffers +
  a real material shader pair transpiles" section for full detail. Kept below (struck to
  RESOLVED rather than deleted) since the original questions/context may still be useful:
  helper_type_arguments.hpsl's "constant buffer chosen by MaterialType" turned out to be
  real HLSL-derived `cBuffer` syntax (not a preprocessor-only indirection), solved by flat
  `uniform` rewriting; the "PreprocessParser.h constant-buffer comment" referenced below does
  not actually exist (grepped, zero matches - a correction, not a re-confirmation);
  cTextureBuffer itself is avoided (not solved) by steering real materials onto
  deferred_base_vtx.hpsl's already-simpler `@else UseTextureBuffer`-undefined branch; and
  deferred_base_vtx.hpsl/deferred_base_frag.hpsl/deferred_gbuffer_solid_frag.hpsl now
  transpile and compile as real GLSL 120, live-verified.
- SOMA/HPSL transpiler mesh-upload wiring follow-up (new, from the RESOLVED work above): the
  new `attribute vtx_vTangent`/`vtx_vBoneIndices`/`vtx_vBoneWeight` GLSL declarations (see
  soma/src/game/HpslTranspiler.cpp) compile but aren't fed real per-vertex data yet - nothing
  on the C++ mesh-upload side (cVertexBuffer -> LowLevelGraphicsSDL, wherever
  cGpuShaderManager::CreateShader()'s calling convention lives) calls
  glBindAttribLocation()/glVertexAttribPointer() for these attribute names. Needed before
  normal-mapped or skinned materials can render correctly, even though they now compile.
  Out of a shader-source-only transpiler's reach - needs whoever owns the mesh/vertex-buffer
  upload path.
- SOMA/HPSL transpiler: widen the live self-test's material combo beyond this session's
  minimal one (UseUv/UseNormals/UseColor/UseDiffuse for deferred_base_vtx/frag,
  UseNormals/UseLinearDepth for deferred_gbuffer_solid_frag) to exercise UseNormalMapping,
  UseSkeleton, UseSway, UseColorMul, UseDissolve, MaterialType_Translucent, etc. - each may
  surface its own unmapped type/construct the same way cMatrix3f did this session (e.g.
  cVector4l/cVector2l, seen but not needed by this session's combo, used by
  UseSkeleton/UseTextureBuffer paths - likely ivec4/ivec2, unverified). See
  soma/src/game/HpslTranspilerSelfTest.cpp's RunHpslTranspilerSelfTest().
- ~~SOMA/HPSL transpiler: understand and implement constant-buffer support.~~ RESOLVED, see
  above.
  helper_type_arguments.hpsl (227 lines, @included by every real deferred-
  rendering .hpsl shader e.g. deferred_base_vtx.hpsl) implements HPSL's
  "constant buffer chosen by MaterialType" indirection layer referenced but
  never actually read in HPL2/core/include/system/PreprocessParser.h's
  comments (flagged, unread, across two separate sessions now - soma/src/
  game/HpslTranspiler.h's history and PORTING_NOTES.md). This blocks every
  real material shader from transpiling at all (deferred_base_vtx.hpsl,
  deferred_gbuffer_solid_frag.hpsl, deferred_light_frag.hpsl, etc. all
  @include it) - the single biggest concrete unknown standing between the
  current transpiler proof-of-concept (soma/src/game/HpslTranspiler.{h,cpp},
  proven against 7 small non-material shaders) and anything that actually
  renders. Read helper_type_arguments.hpsl from a real SOMA install
  (core/shaders/hpsl/) and PreprocessParser.h's constant-buffer comment
  first; likely needs new transpiler support for GLSL 120 uniform-block-
  equivalent syntax (GLSL 120 predates real uniform blocks - may need a
  flat-uniform-list expansion instead, unexplored).
- ~~SOMA/HPSL transpiler: cTextureBuffer / GPU-instancing support.~~ RESOLVED (via the avoid-
  it strategy above, not a real port), see above.
  deferred_base_vtx.hpsl declares `uniform cTextureBuffer aInstanceBuffer :
  15;` - no GLSL 120 equivalent type exists (samplerBuffer is GLSL 140+/
  GL_EXT_texture_buffer_object territory). Needs either a GLSL version bump
  specifically for HPSL-derived shaders (open question: does that break
  anything else HPL2's own hand-written .glsl shaders assume about the
  fixed-function pipeline they still use, e.g. gl_Vertex/gl_MultiTexCoordN -
  those are removed in GLSL 150+ core profile) or a different instancing
  strategy entirely (e.g. per-draw-call uniforms instead of a texture-buffer
  instance array, giving up GPU instancing). Unexplored; blocks
  deferred_base_vtx.hpsl same as the constant-buffer gap above.
- ~~SOMA/HPSL transpiler: once constant buffers + instancing are understood, attempt
  transpiling a real material shader.~~ RESOLVED, see above.
  deferred_base_vtx.hpsl/
  deferred_base_frag.hpsl (982/610 lines) are the ones actually blocking
  rendering, but base_vtx.hpsl/base_frag.hpsl (106/75 lines - note: no
  "deferred_" prefix, a different/simpler non-deferred base shader,
  unexamined) and deferred_skybox_frag.hpsl/base_skybox_{frag,vtx}.hpsl
  (32-33 lines each) are smaller real-material candidates worth trying
  first, not yet examined at all.
- ~~Re-verify soma/src/game/HpslTranspilerSelfTest.cpp (a live glCompileShader() check
  against a real GL context inside a fully-booted SOMA process...) against the real SOMA
  install~~ RESOLVED this session - the environment did NOT block it this time (see above);
  now covers 10 real files including 3 real material shaders, all PASS.
  - blocked this
  session (see the SOMA IN PROGRESS entry above) by a sandboxed execution
  environment that refused to let a built binary be deployed into or
  launched anywhere that could reach the real Steam install's data,
  including via a read-only symlink farm in a scratch directory outside it.
  A session/environment with permission to actually launch the built Soma
  binary against real game data should re-run this and report whether the
  string-level transpiler work this session (HPL2/tests/HpslTranspilerTests.cpp)
  actually produces GLSL that compiles for real, not just looks right.

- Add glInvalidateFramebuffer/glDiscardFramebufferEXT hints at the engine's
  render-target switches (HPL2/core/sources/impl/LowLevelGraphicsSDL.cpp's
  SetFrameBuffer, called 10+ times a frame in RendererDeferred.cpp: G-buffer,
  linear-depth copy, SSAO, SSAO blur, each light's shadow map, lighting
  accumulation, post effects, composite). Confirmed via grep across the whole
  engine: zero calls to either function exist anywhere in this codebase.
  On a tile-based GPU (this machine is Apple Silicon via Asahi Linux/Mesa's
  AGX driver) every one of those switches forces the driver to conservatively
  store each tile back to system memory, even for attachments about to be
  overwritten or never read again - real, avoidable bandwidth this ~2010
  immediate-mode-GPU-era renderer never had to think about. Purely additive
  (a hint, not a behavior change) so genuinely low-risk; found while
  investigating a live 2x FPS drop in Entrance Hall (02_entrance_hall.map)
  looking toward its two shadow-casting spotlights vs. away from them - see
  PORTING_NOTES.md for the full investigation. Good first target: the
  linear-depth buffer (RendererDeferred.cpp:1125) and each per-light shadow
  map, since both are written once and read exactly once per frame.
- Investigate deriving linear depth in-shader instead of
  cRendererDeferred's separate mpLinearDepthBuffer pass
  (RendererDeferred.cpp:1125, SetFrameBuffer(mpLinearDepthBuffer)) - a full
  extra off-chip texture write+read every frame for something derivable
  from the G-buffer depth already resident on-tile. Bigger/riskier than the
  invalidate-hint task above (touches whatever downstream code samples
  mpLinearDepthBuffer - SSAO and others - not just additive), so do that one
  first and treat this as a follow-up.
- Investigate replacing the deferred renderer's stencil two-pass point-light
  volume technique (RendererDeferred.cpp's eDeferredLightList_* stencil-mark-
  then-read pattern, classic ~2008-era desktop-GPU deferred lighting) with
  something that doesn't read back the stencil/depth attachment it just wrote
  within the same pass - that read-after-write-same-tile-attachment pattern
  is exactly what forces conservative flushes on tile-based GPU drivers.
  Large, risky rewrite (touches the whole per-light rendering path for every
  game/module sharing HPL2/core) - needs real before/after profiling on
  actual tile-based hardware to justify, not a first move.
- Request a core/forward-compatible GL context instead of whatever
  compatibility-profile default Mesa hands back (HPL2/core/sources/impl/
  LowLevelGraphicsSDL.cpp never calls SDL_GL_SetAttribute(SDL_GL_CONTEXT_
  PROFILE_MASK/MAJOR_VERSION/MINOR_VERSION)). Would shed legacy fixed-
  function state-tracking overhead, but several of the engine's own
  hand-written .glsl shaders may assume fixed-function pipeline access
  (gl_Vertex/gl_MultiTexCoordN etc., per the SOMA transpiler notes above) -
  needs an audit of what those shaders actually rely on before attempting,
  since a core profile removes that entirely. Unexplored, not started.
- Add an in-engine light/shadow-map render-stats overlay (mlNumberOfLightsRendered
  and mlNumberOfOcclusionQueries already exist as per-frame counters on
  iRenderer/cRenderer, HPL2/core/include/graphics/Renderer.h, but nothing
  surfaces them anywhere - not even the debug menu). Would turn "why did FPS
  halve here" investigations like the Entrance Hall one above from manual map-
  file/code archaeology into a two-second on-screen readout - genuinely useful
  alongside the new Show FPS counter (Native FPS counter task above), same
  GameDebug GuiSet it already draws through.

- SOMA (and likely Rebirth/Bunker) headless-testing hpl.log path collides
  across concurrent processes: cSomaBase::InitEngine() (soma/src/game/
  SomaBase.cpp) calls SetLogFile() with a fixed path derived from XDG state
  home (~/.local/state/open-hpl/soma/hpl.log, added in 696c49c), the same
  path for every launched instance regardless of which build/worktree/agent
  launched it. cLogWriter::ReopenFile() (HPL2/core/sources/impl/
  LowLevelSystemSDL.cpp) opens it with fopen(...,"w") - full truncation, not
  append - so any second SOMA process started while a first is still running
  (now a normal occurrence with multiple concurrent agents each headlessly
  testing their own Soma.<branch>.aarch64 build) silently wipes whatever the
  first process had already logged, corrupting both processes' hpl.log-based
  evidence with no error or warning to either. Discovered live this session
  (worktree-agent-a98dec63e5599d81b, see the SOMA HPSL-wiring entry above)
  when launching a second headless SOMA instance truncated a concurrent
  agent's in-progress log. A fix should make the log path unique per
  process (e.g. suffix with getpid(), or let OPENHPL_HEADLESS_SOCKET's path
  double as a log-path-uniqueness key since that's already how this repo's
  headless workflow distinguishes concurrent instances) - same shape of fix
  likely applies to amnesia/src/game/LuxBasePersonal.h's equivalent Amnesia/
  AMFP logging setup and rebirth/bunker's, not just SOMA's, though only
  SOMA's was actually observed colliding this session.


- ~~SOMA: real-time light rendering never invokes deferred_light_frag.hpsl at all, even when
  lights are confirmed visible.~~ RESOLVED (branch worktree-agent-a04a93f1374af0e56) - and
  the premise turned out to be stale, not the real bug. Re-diagnosed from scratch with fresh
  temporary instrumentation (same discipline, removed again): on current master,
  InitLightRendering() already buckets 22 of the 37 real lights into non-empty
  mvSortedLights[] buckets every frame, and SetupProgramAndTextures() already calls
  GenerateProgram(eDefferredProgramMode_Lights, ...) successfully every frame -
  deferred_light_frag.hpsl transpiles and compiles clean, zero "Couldn't transpile" errors.
  Whatever produced the original "never invokes it at all" symptom no longer reproduces
  (most likely fixed as a side effect of the matrix-uniform-substitution fix two sessions
  ago). The actual remaining cause of flat-black rendering was three separate real
  uniform/texture-binding gaps between HPSL's real shader source and this engine's C++
  uniform-feeding code, found by diffing the transpiled deferred_light_frag.glsl output
  against both the real .hpsl source and Dark Descent's own working deferred_light_frag.glsl:
  (1) avScreenToFarPlane/avInvScreenSize (HPSL's position-reconstruction uniforms, a
  different technique from Dark Descent's own per-vertex gvFarPlanePos varying) were never
  set anywhere in HPL2/core, left at GLSL's zero default; (2) the spotlight
  view-projection matrix was registered/set under HPL2's own name (a_mtxSpotViewProj) but
  HPSL's real uniform is named a_mtxLightViewProj - GetVariableAsId() fails closed for a
  name that doesn't exist, so the real uniform stayed at zero, causing a NaN via
  divide-by-zero in every spot light's cone/near-clip attenuation term; (3) the biggest one -
  every fragment-shader sampler (aDiffuseMap/aNormalDepthMap/aSpecMap/aShadowMap/
  aShadowOffsetMap) silently defaulted to texture unit 0, since HPL2's sampler-to-unit
  binding mechanism reads "@define sampler_NAME N" preprocessor lines that only exist in
  Dark Descent's own hand-written .glsl (HPSL instead encodes the same info as a D3D-style
  "uniform cTextureX NAME : N;" suffix that the transpiler discards with nothing downstream
  to consume it) - fixed with a new ApplyHpslTextureBindings() helper in
  GpuShaderManager.cpp that extracts the ": N" indices from the pre-transpile HPSL text and
  feeds the existing iGpuShader::AddSamplerUnit() mechanism directly. Also gave HPSL's
  afFalloffPow/afSpotFalloffPow (light falloff exponents with no HPL2-native equivalent
  field to read) a fixed 2.0 default instead of the degenerate "no falloff at all" GLSL-zero
  default, needed once the other three fixes actually landed real per-light data (without
  it, overlapping lights immediately saturated the accumulation buffer to solid white).
  **Verified live, headless, real 00_01_apartment.hpm**: before -> flat black (matches the
  documented baseline exactly); after fixes 1-2 only -> visibly different but garbled
  rainbow noise along geometry edges (confirms the sampler-unit gap as a distinct, separate
  bug); after all fixes -> real per-pixel lighting/shading gradient tied to actual geometry,
  confirmed via a warm-toned vertical-stripe gradient across a wall surface at a camera
  position away from the nearest lights (still overexposed close to lights - a real,
  separate tone-mapping/exposure follow-up, not a "pipeline broken" one, see
  PORTING_NOTES.md). **Zero regression confirmed for Dark Descent**: built Amnesia from the
  same tree, headless-booted to the real torch-lit Profiles menu scene - hpl.log stays
  completely empty (this game only writes it on warning/error) and the screenshot is
  visually identical to known-good (correct torch gradient/shadowing, no white blowout) -
  expected, since every new uniform/binding this session added is either SOMA-HPSL-only or
  fails closed as a harmless no-op against Dark Descent's own real shader text (confirmed via
  GetVariableAsId()/the new regex, both designed to no-op on a name/pattern that doesn't
  exist). All 4 existing CTest cases still pass unchanged. Full writeup with all
  screenshots' context in PORTING_NOTES.md under "SOMA: real-time lights now render lit
  geometry - three real uniform/binding gaps found and fixed, not a dispatch bug".
- ~~SOMA: several shader filenames this engine's C++ hardcodes don't match any real file in the
  HPSL corpus~~ RESOLVED (master, this session) - added a filename-alias table to
  GpuShaderManager.cpp (illumination/skybox/decal, same shape as the existing
  UseDepth->UseLinearDepth combo-variable alias) plus a real fix for the actual cause of most
  of the "deferred_base_vtx.glsl couldn't find file" noise: cGraphics::Init()'s core/shaders
  resource-dir registration was non-recursive, so nothing under SOMA's real core/shaders/hpsl/
  subdirectory was ever found during early bootstrap. posteffect_bloom_* genuinely has no real
  HPSL equivalent anywhere in the corpus (confirmed by a full search) - PostEffect_Bloom.cpp
  now bails out gracefully (pass-through) when its programs fail to build. Also found and fixed
  a systemic sampler-binding bug (every HPSL fragment shader's samplers were silently
  defaulting to texture unit 0). Verified live: all filename-alias/deferred_base_vtx.glsl
  errors gone, zero "Failed to link" errors remain. Full writeup in PORTING_NOTES.md.

- SOMA magenta full-screen rendering artifact and camera-independent rendering
  - PARTIALLY RESOLVED (master, this session): root-caused and fixed the camera-independence
    part - iRenderer::RenderBasicSkyBox() rendered the skybox cubemap through the
    fixed-function pipeline (SetProgram(NULL)), which produces solid wrong/saturated color
    instead of real cubemap sampling on this project's real Mesa/AGX test platform, and being
    drawn at infinite depth, filled the whole frame regardless of camera orientation. Fixed via
    a new iRenderer::GetSkyBoxProgram() virtual that cRendererDeferred overrides with its real,
    already-built mpSkyBoxProgram. Also fixed a same-shape uniform gap this surfaced in
    deferred_fog_frag.hpsl (needs avScreenToFarPlane/avInvScreenSize, same as
    deferred_light_frag.hpsl's already-fixed case). Verified live: screenshots now genuinely
    differ across camera moves (proven via pixel diff), FPS recovered from as low as ~3-8 to
    ~19-22 in the same scene.
  - CORRECTION (master, this session, later same night): the magenta/purple triangular patches
    themselves are **NOT** actually gone - an earlier claim to that effect compared screenshots
    at two different camera angles and wrongly treated the second (which happened not to be
    looking at them) as proof.
  - ROOT-CAUSED for real (master, this session, later still) via bisection (temporarily
    force-return each render pass in turn, rebuild, re-screenshot the same real
    PlayerStartArea_1 pose, compare - not guessing): disabling
    iRenderer::RenderBasicSkyBox() entirely = no change (skybox fully ruled out, its real
    cubemap texture loads fine - confirmed live via a temporary diagnostic, an earlier "maybe
    it's an incomplete cubemap" theory was itself a bug in a throwaway DDS-parsing script, not
    a real engine issue); disabling cRendererDeferred::RenderDecals() entirely = no change
    (decals ruled out); disabling cRendererDeferred::RenderTranslucent() entirely = **the whole
    image goes flat black** - every gray wall surface and every purple triangle visible from
    this camera pose is drawn by the translucent/forward render pass
    (eMaterialRenderMode_Diffuse), not the deferred G-buffer pipeline every fix so far this
    session targeted. Real, plausible content-wise (real glass/windows in an apartment,
    same lab_env.dds environment-map convention used by several real reflective entity
    materials found earlier this session) - but this pass's own real HPSL shader filename(s)/
    uniform names have never been checked the way every deferred-path one has been today.
    **Not fixed - this is the concrete next step**, see PORTING_NOTES.md's newest SOMA section
    ("the magenta/gray artifact root-caused for real") for exactly what to check next.

- SOMA overexposed/blown-out rendering (real per-area exposure data unread)
  - PARTIALLY RESOLVED (master, this session): SOMA's real map data ships a .hpm_ExposureArea
    sidecar (real per-area Exposure/WhiteCut/TransitionTime data, e.g.
    00_01_apartment.hpm_ExposureArea's ExposureArea_1 has Exposure="-1.2") this engine never
    had any loader for. Added cWorldLoaderHpm::LoadExposureAreaTrack() (a new sidecar track
    loader, SOMA-only code, zero Dark Descent risk) and cWorld::SetGlobalExposure()/
    GetGlobalExposure() (defaults to 1.0, a true no-op for every world that never sets it).
    Applied via a new untextured Mul-blend fullscreen quad in
    cRendererDeferred::CopyToFrameBuffer(), skipped entirely at the 1.0 default. Deliberately
    simplified: applies only the FIRST ExposureArea found as one flat global scale, not the
    real system's spatial blend/fade between multiple overlapping areas as the camera moves
    (WorldPos/Scale/TransitionTime are read but unused) - a real, honest first step. Verified
    live: hpl.log shows "applying global exposure -1.200000 EV (0.435275 linear)"; a real
    PlayerStartArea_1 screenshot went from solid overexposed white to a properly-exposed
    gray-toned scene with visible wall texture detail - large, clearly real improvement.
    Follow-up: implement the real spatial blend/transition-time system instead of a single
    flat global value, and apply WhiteCut (currently read but unused).

- SOMA box lights/fog: same uncompensated "×8 HDR precision boost" gap as the already-fixed
  deferred_transparent_frag.hpsl/deferred_light_frag.hpsl cases
  - DONE (master, this session): deferred_light_box_frag.hpsl (box lights, two main() variants -
    one SH-probe path spelling the uniform "vLightColor.xyz", one plain path spelling it just
    "vLightColor") and deferred_fog_frag.hpsl (two shapes depending on which @ifdef branch
    survives preprocessing - a standalone "px_vColor.xyz *= 8.0;" compound-assign, and a
    "vFogColor.xyz * 8" - integer 8, not 8.0 - baked into a plain assignment) both had the
    identical real, corpus-confirmed, unfixed convention flagged as the top "concrete next step"
    in PORTING_NOTES.md's box-light/fog section. Fixed the same way, in the same function
    (soma/src/game/HpslTranspiler.cpp's RemoveUncompensatedHdrPrecisionBoost()): three new regex
    patterns strip just the "* 8.0"/"* 8" factor (or remove the standalone compound-assign line
    entirely), never the whole expression, since unlike vFinalColor/light_frag's boost these
    aren't always the sole RHS. Three new regression tests added to HpslTranspilerTests.cpp
    (TestBoxLightBoostRemoved, TestFogBoostRemoved - covers both vLightColor spellings and both
    fog shapes); all 4 ctest suites green (PhysicsNewtonTests/CStringTests/PlatformXdgPathTests/
    HpslTranspilerTests). 100% contained to soma/src/game/HpslTranspiler.cpp - SOMA-only
    transpiler code, zero Dark Descent/AMFP/Rebirth/Bunker reachability, zero regression risk by
    construction. Not yet live-verified against a real scene with active box lights/fog areas
    specifically (would need locating a map/camera pose where one of these two shaders is
    actually the dominant contributor to a visible pixel, unlike the light_frag/block_box.mat
    fixes which had an obvious, easy-to-repro magenta target) - the game_edge_glow.hpsl and
    null_frag_array(2).hpsl instances of the same "* 8.0" text (also present in the corpus, per
    the same earlier grep) were deliberately left alone: game_edge_glow.hpsl's comment doesn't
    confirm the same "increase precision" intent and it's a much lower-traffic effect, and
    null_frag_array.hpsl is explicitly commented "Null shader, bound when no other shader is
    bound" (an error-fallback path, not normally live) - lower confidence, not worth the same risk
    calculus as the two "most valuable first" targets actually named in PORTING_NOTES.md.

- CRITICAL: real Steam SOMA install files got corrupted badly enough that Steam's own integrity
  check flagged and force-re-downloaded them, despite this project's standing zero-tolerance
  rule against ever writing into a real game install directory
  - ROOT-CAUSED AND FIXED (master, this session): `scripts/headless-check.sh`'s own header
    comment documented (incorrectly, per this project's real convention) that its target binary
    "must already be deployed inside its real game directory" - the script then `rm -f`'s a log
    file there and `cd`'s into that directory before launching. Following the script's own
    documented usage against a binary actually deployed into the real Steam install (rather than
    a scratch dir with read-only-symlinked game data) explains the corruption: any relative-path
    write inside the engine with cwd = the real install directory lands there for real. Fixed by
    making the script hard-refuse (exit 1) any target path containing a `steamapps/common`
    segment, and rewriting its header to describe the correct (scratch-dir-only) usage.
  - Also fixed a second, smaller, real contributing gap: `cSomaBase::InitMainConfig()` loaded
    `config/main_init.cfg` via a relative path *before* `SetLogFile()` ran - on a load failure,
    `cConfigFile::Load()`'s `Error()` call would have fallen back to the engine's default bare
    relative `"hpl.log"`, resolved against cwd (the same danger as above). This was the one
    call site missed when the equivalent bug was fixed for two other pre-`SetLogFile()` sites
    in an earlier session (see PORTING_NOTES.md ~"Headless testing must never..."). Fixed by
    extracting the log-file-setup block out of `InitEngine()` into a new `SetupLogFile()`,
    called first thing in `cSomaBase::Init()` - before `ParseCommandLine()`/`InitMainConfig()` -
    so every `Log()`/`Error()` call in the whole init sequence has a real, XDG-routed
    destination from the very start.
  - Added a third, shared, defense-in-depth backstop in `HPL2/core/sources/impl/
    LowLevelSystemSDL.cpp`'s `cLogWriter::ReopenFile()` (used by every game module, not just
    SOMA): refuses to open (falls back to stderr-only with a clear message) any log path that
    resolves - after joining with cwd, for a relative path - to something containing
    `steamapps/common` (or its Windows-path-separator spelling). This protects against the same
    failure mode being reintroduced by a future test script or manual command, independent of
    whether any single game module's own init-order is currently correct.
  - Verified live: rebuilt Soma/Amnesia in a dedicated build dir, ran both from real scratch-dir
    setups (real game data read-only-symlinked in, XDG_STATE_HOME etc. overridden to scratch
    subdirs) - both booted and answered a real headless control-socket ping; both real Steam
    install directories' `hpl.log` (Amnesia's absent entirely, SOMA's a pre-existing stale file
    from before this fix) were confirmed untouched (unchanged/absent) by either run. Also
    directly tested `scripts/headless-check.sh`'s new guard against a fabricated
    `.../steamapps/common/SOMA/fakebin` path (refuses, exit 1) and a legitimate scratch path
    (passes through to the normal "not executable" check, exit 1 for an unrelated reason) - no
    false positive. All 4 ctest suites green throughout
    (PhysicsNewtonTests/CStringTests/PlatformXdgPathTests/HpslTranspilerTests).

- SOMA: real brainscan loading-icon animation + menu ambient sound never stops after New Game
  (2026-09-07)
  - DONE: added the real 26-frame `brain_01.dds`..`brain_26.dds` loading-icon animation
    (`config/game.cfg`'s `LoadingIcon`) to the boot-init splash phase, bottom-right corner,
    ~12fps - see `soma/src/game/SomaSplash.cpp`'s new `DrawBrainIcon()`.
  - DONE: fixed a real, user-reported bug - the main menu's looping "MenuBGNoise" ambient
    (`special_fx/frontend/main_menu_bg`) never stopped, including after New Game loaded a real
    map, because `cSomaSplash::EnterPhase()` started it fully fire-and-forget. Now stored
    (`cSoundEntry*`/id pair, same `IsValid()` guard pattern as
    `amnesia/src/game/LuxEnemy_ManPig.cpp`'s `mpMindFuckSound`) and stopped via a new
    `cSomaSplash::StopMenuAmbient()`, called from `cSomaMainMenu::SetVisible(false)` alongside
    the existing music-stop. Required one small unavoidable addition outside this pass's two
    owned files: a `GetSplash()` accessor on `cSomaBase` (it already had a private `mpSplash`)
    so `cSomaMainMenu` (which already holds `mpBase`) can reach it - same pattern as its
    existing `GetDebugCamera()`/`GetConfig()`. See PORTING_NOTES.md's newest SOMA section for
    full evidence/citations and live verification detail (headless screenshots for the icon
    animation; a real injected "NEW GAME" click + `iSoundChannel::IsPlaying()` log evidence
    going 1→0 across the stop, for the ambient fix). All 4 ctest suites
    (PhysicsNewtonTests/CStringTests/PlatformXdgPathTests/HpslTranspilerTests) green.
    100% SOMA-only (`cSomaSplash`/`cSomaMainMenu` classes) - zero Dark Descent/AMFP/Rebirth/
    Bunker reachability.

- SOMA: real boot order corrected, real progress/unskippable loading, icon timing evidence
  (2026-09-08)
  - DONE: fixed a real, user-reported bug - `soma/src/game/SomaSplash.{h,cpp}` had the FG
    Frictional Games logo phase FIRST and the native boot-init glitch screen (Premenu.png/red
    loading bar/brain icon) SECOND, backwards from the real game. Re-derived the real order from
    scratch via `nm -C`/`objdump -d` on the real `Soma.bin.x86_64` (`cLuxBase::Init()`'s call
    sequence shows the native `cLuxLoadHandler` boot splash starts before the main loop/any script
    ever runs) plus `script/modules/MenuHandler.hps` (`GuiPreMenu()`'s FG-logo phase is only
    reachable from the already-active main menu's own update loop). Swapped
    `eSomaSplashPhase_BootInit`/`eSomaSplashPhase_FGLogo`'s order.
  - DONE (partial, honestly scoped): task 3's real-progress/unskippable-loading ask. Added
    `cSomaBase::PreloadMainMenuWorld()` (small, additive, flagged touch to `SomaBase.h/.cpp`) so
    the real main-menu-world load now happens during the boot-init splash phase (triggered
    synchronously from `cSomaSplash`'s constructor, before `cEngine::Run()`'s main loop even
    starts) instead of only after the whole splash finishes. `InitMainMenuScene()` reuses the
    cached world instead of double-loading; the `OPENHPL_SOMA_MAP` test-map branch destroys it
    if unused. A new `mbRealBootWorkDone` flag genuinely gates `eSomaSplashPhase_BootInit`'s
    advance (skip or timeout) on this real load having finished - by construction this is always
    already true given this engine's single-threaded boot, but the check is now explicit rather
    than an implicit accident. The bar's own fill animation is still honestly time-based (no full
    threaded job-queue restructure attempted - out of scope per this project's own
    reverted-tonemap-pass precedent).
  - DONE (evidence adopted): `objdump -d` on `cLuxLoadHandler::OnDraw()`/`DrawBigIcon()` found a
    real ~15fps ping-pong (bounce, not loop) animation pattern for the brainscan loading icon -
    adopted in `DrawBrainIcon()` (`mfBrainFrameRate` 12→15, now bounces 0↔25). NOT adopted: a
    512x512 native icon size also found in the same disassembly - left as the previous session's
    already-user-checked modest corner size, since the evidence was ambiguous about which of two
    reused Draw*Icon() variants applies to the boot splash specifically vs. later level-load
    screens. See PORTING_NOTES.md's newest SOMA section for the full citations and honesty
    breakdown.
  - Verified live: all 4 ctest suites green; `log_tail` (live socket command) confirms exactly one
    `main_menu.hpm` load per boot, occurring before the engine's own "Game Running" line (i.e.
    during splash construction, not after); a real injected mouse click through the gamma screen
    reached a fully working main menu with zero regressions. Could NOT get a live screenshot of
    the mid-boot-init visuals themselves this session - same documented shared-machine headless
    single-instance-lock contention (3-4 concurrent agent processes queued throughout) a prior
    splash session already hit; order/timing correctness instead confirmed via the log evidence
    above. 100% SOMA-only (`SomaSplash.{h,cpp}`); `SomaBase.{h,cpp}` touch is small/additive per
    this task's own scope note (one new method + one new field + two call-site edits).

- SOMA: real map-authored ambient sounds (car honking, dogs, seagulls, fridge hum, DVD idle,
  vent loop) were silent - loader existed, target `.snt` resource never did (2026-09-08)
  - DONE: root-caused precisely before touching code - `cWorldLoaderHpm::LoadSoundsTrack()`
    already runs unmodified for every map and already calls `cEngineFileLoading::LoadSound()`
    for real `.hpm_Sound` entities (confirmed 7 real ones in `00_01_apartment.hpm_Sound`:
    fridge_hum/dvd_idle/vent_sound_1/dogs/car_drive/seagulls/dogs_1); the real gap is that their
    `SoundEntityFile` values are real FMOD Ex/Studio event paths with no matching `.snt`
    resource anywhere in the real install (confirmed: only one real `.snt` exists in the whole
    game, unrelated), so `cSoundEntityManager::CreateSoundEntity()` always failed - confirmed
    live via hpl.log ("Couldn't create SoundEntity ... 0 sounds") BEFORE writing any fix.
  - DONE: fixed via new `soma/src/game/SomaAmbientSfx.{h,cpp}` + `SomaAmbientSfxVorbisSetup.h` -
    extracts the real audio from `sounds/level/00_06_lab.fsb` (PCM16: car_drive/distant_dog/
    seagull), `sounds/entities/entities_urban.fsb` (Vorbis, a NEW real FMOD codebook preset
    pulled from the same public MIT-licensed python-fsb5 table SomaMenuSfxVorbisSetup.h's
    existing preset came from), and `sounds/entities/Entities_Station.fsb` (Vorbis, reuses the
    existing preset) using a bounded FSB5 parser mirroring `SomaMenuSfx.cpp`'s established
    pattern, then synthesizes real `SOUNDENTITY` `.snt` XML sidecars (exact real schema,
    verified against the one genuine `.snt` in the install) into a cache resource dir. Zero
    changes to any shared loader (`WorldLoaderHpm.cpp`/`EngineFileLoading.cpp`/
    `SoundEntityManager.cpp` all untouched) - they now simply find a real resource where they
    used to find nothing. Wired via one new call, `cSomaAmbientSfx::EnsureCached()`, right next
    to `RegisterSomaLoaders()` in `cSomaBase::Init()`.
  - Verified live: headless-booted `00_01_apartment.hpm` via the real `start_map` control
    command; hpl.log's sound-load summary went from "...0 sounds" to "...7 sounds", and a
    temporary (added-then-reverted) diagnostic in `cSoundEntity::PlaySound()` confirmed all 7
    actually START PLAYING real extracted audio, including `car_drive`/`dogs`/`seagulls` - the
    exact sounds the user reported missing. `ffprobe`/`ffmpeg -f null -` independently confirmed
    the extracted Vorbis/PCM16 files are genuinely valid, fully-decodable audio. All 4 ctest
    suites (PhysicsNewtonTests/CStringTests/PlatformXdgPathTests/HpslTranspilerTests) green.
    See PORTING_NOTES.md's newest SOMA section for full citations, the two documented
    simplifications (fixed spot-repeat timing; one shared dog-bark pool for both real "dogs"/
    "dogs_1" entities), and the known out-of-scope physics-impact-sound gap left honest.

- SOMA: intro-sequence slide-cadence bug, a real phone-interact system (no more auto-answer),
  a real player HUD (crosshair + interact prompt), and the intro quote card's too-small font
  (2026-09-08)
  - DONE (task 1): `SomaIntroSequence.cpp`'s `AdvanceSlides()` used a `while` loop to advance
    `mlCurrentSlideIndex`; real `00_00_intro.hps`'s own `Update()` uses a single `if` per frame,
    which can never skip a slide's entire visible lifetime under a frame-time hitch the way a
    `while` can (several real slide gaps are as short as 0.27s). Fixed by matching the real
    per-frame semantics. Investigated and ruled out the task brief's own hypothesis (that the
    prior session's voice-completion-gating fix desynced slide timing) - confirmed by reading
    real `Update()` in full that slides and voice-subject enqueueing both advance off the same
    shared schedule completely independently of each other, both in the real game and in this
    port.
  - DONE (tasks 2/3): built a minimal, hand-authored interact-point system on `cSomaPlayer`
    (`RegisterInteractPoint()`/`GetCurrentLookTarget()`/`WasInteractedWith()` - point+cone+
    real-physics-line-of-sight, not a general Area/trigger-volume reader) plus a real HUD
    (crosshair + `"[E] Interact"` prompt, drawn via `cSomaPlayer::OnDraw()`'s own new GUI-only
    overlay viewport). `cSomaApartmentIntroCall` now registers the real wake-up phone's actual
    interact point (`InteractCellPhone_Dummy`, NOT the separate later-game `Simon_Phone`
    landline - see PORTING_NOTES.md for the full disambiguation) and waits for a real interact
    keypress while looking at it instead of the previous 4-second auto-answer timer - matching
    real `00_01_apartment.hps`'s own indefinite wait. Required one small, flagged `SomaBase.h`
    hook (`GetPlayer()` accessor, no `.cpp` change).
  - DONE (task 4): intro quote card font size was 30/24 (quote/signature); real
    `00_00_intro.hps`'s own `DrawTextSlide()` specifies 36/30. Fixed to match exactly.
  - Verified live, headless throughout (see PORTING_NOTES.md for full evidence): a temporary
    diagnostic trace (removed before this commit) confirmed all 19 real intro slides fire within
    15-20ms of their exact real scheduled time, in order, none skipped; a screenshot with the
    player aimed at the real phone position shows the crosshair AND interact prompt appearing
    together (vs. crosshair-only when not looking at it); an injected real interact keypress
    while aimed at the phone produced the log line confirming Munshi's real dialogue started
    (and did not start before that keypress, across many real seconds of ringing); a screenshot
    of the quote card confirms the larger, correct font size. All 4 ctest suites
    (PhysicsNewtonTests/CStringTests/PlatformXdgPathTests/HpslTranspilerTests) green in a
    dedicated build dir (removed after). 100% contained to `SomaIntroSequence.{h,cpp}`/
    `SomaApartmentIntroCall.{h,cpp}`/`SomaPlayer.{h,cpp}` plus the one `SomaBase.h` accessor -
    zero Dark Descent/AMFP/Rebirth/Bunker reachability, zero `SomaMainMenu`/`SomaSplash`/
    `SomaLoaders` reachability (other agents' owned files, untouched). This interact system is a
    scoped, narrow addition for this one real interactable - not a generalizable input-binding/
    interaction framework; see PORTING_NOTES.md's "Not done / open items" for what a real
    version would still need.

- SOMA: real pause-menu item list + exit-confirm dialog, and a real New Game difficulty screen
  (2026-09-08)
  - DONE: fixed the pause menu drawing the FULL title-screen chrome (background/logo/cathedral
    face/particles) behind its item list - real `MenuHandler.hps` draws none of that while
    paused; replaced with a solid dark overlay (`DrawPauseBackground()`) and the real 4-item
    list `RETURN TO THE GAME`/`OPTIONS`/`EXIT`/`SAVE AND EXIT` (was a placeholder
    `RESUME`/`OPTIONS`/`QUIT TO MAIN MENU` with no real SOMA precedent).
  - DONE: real EXIT/SAVE AND EXIT confirm dialog (`DrawExitConfirmDialog()`/
    `UpdateExitConfirmDialog()`) - real "ARE YOU SURE YOU WANT TO EXIT WITHOUT SAVING?"/"...EXIT
    TO MENU?" strings, real teal-bordered panel (reuses the existing Options-screen frame
    helper), real Yes/No button art. SAVE AND EXIT honestly behaves identically to EXIT (logged
    plainly) - no SOMA-specific save system exists anywhere in this engine (checked: only
    HPL2/core's generic serialization framework and Dark Descent's own concrete save handler,
    neither wired up for SOMA).
  - DONE: real New Game difficulty-select screen (`eSomaMenuScreen_NewGameDifficulty`) between
    the title screen's NEW GAME click and the actual map load - real "GAME MODE:" NORMAL/SAFE
    cycle control with the real verbatim description text for each, real START GAME/BACK
    buttons, wired so START GAME proceeds into the real intro sequence exactly as NEW GAME used
    to.
  - Investigated (and ruled out live) a real captured-and-blurred pause background matching Dark
    Descent's own `cLuxMainMenu` precedent - this engine's frame loop has no point at which "the
    last composited frame" is a well-defined buffer to read back, and separately this port's own
    viewport renders solid white (not the live scene) once `cSomaPlayer::SetActive(false)` takes
    effect - `SomaPlayer.*` is outside this pass's file-ownership boundary, flagged as a known
    next step rather than fixed here. See PORTING_NOTES.md's newest SOMA section for the full
    citation trail and both ruled-out attempts.
  - **Verified live**: full real click-through (title → NEW GAME → difficulty screen incl. mode
    toggle → START GAME → real intro sequence) and separately reached live gameplay, injected a
    real Escape keypress → pause menu → EXIT → confirm dialog → YES → back to the real title
    screen, all via real injected mouse/keyboard events and headless screenshots at each step.
    All 4 ctest suites (PhysicsNewtonTests/CStringTests/PlatformXdgPathTests/HpslTranspilerTests)
    green. 100% contained to `soma/src/game/SomaMainMenu.{h,cpp}` - zero Dark Descent/AMFP/
    Rebirth/Bunker reachability.
