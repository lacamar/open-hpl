Name:           open-hpl
Version:        1.3.63
Release:        1%{?dist}
Summary:        Native aarch64 port of Frictional Games' HPL engine

License:        GPL-3.0-or-later
URL:            https://github.com/lacamar/open-hpl
Source0:        %{name}-%{version}.tar.gz

ExclusiveArch:  aarch64

BuildRequires:  cmake
BuildRequires:  gcc-c++
BuildRequires:  perl-interpreter
BuildRequires:  ImageMagick
BuildRequires:  desktop-file-utils
BuildRequires:  pkgconfig(sdl2)
BuildRequires:  pkgconfig(gl)
BuildRequires:  pkgconfig(glu)
BuildRequires:  pkgconfig(zlib)
BuildRequires:  DevIL-devel
BuildRequires:  glew-devel
BuildRequires:  assimp-devel
BuildRequires:  libtheora-devel
BuildRequires:  libvorbis-devel
BuildRequires:  libogg-devel
BuildRequires:  openal-soft-devel
BuildRequires:  libjpeg-turbo-devel
BuildRequires:  angelscript-devel
BuildRequires:  fltk-devel
BuildRequires:  libX11-devel
BuildRequires:  libXext-devel
BuildRequires:  libXft-devel
BuildRequires:  fontconfig-devel

Requires:       hicolor-icon-theme

%description
Unofficial aarch64 Linux port of Frictional Games' GPLv3 HPL2 engine and
Amnesia: The Dark Descent. Not affiliated with Frictional Games.

Game data is not included: the launchers run against your Steam install.
Experimental launchers for Amnesia: A Machine for Pigs, SOMA, Amnesia: Rebirth
and Amnesia: The Bunker are included.

%prep
%autosetup -p1

%build
cd amnesia/src
%cmake \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DOPENHPL_VERSION=%{version} \
    -DUSE_GAMEPAD=ON \
    -DAMNESIA_WITH_SERIAL=OFF
%cmake_build --target Amnesia --target Launcher --target Soma \
    --target PhysicsNewtonTests --target CStringTests --target PlatformXdgPathTests --target HpslTranspilerTests
cd -

magick 'amnesia/src/game/Lux.ico[0]' %{name}.png

%install
pkgdir=%{buildroot}%{_libexecdir}/%{name}
for b in Amnesia Launcher Soma; do
    install -Dpm0755 -t $pkgdir amnesia/src/redhat-linux-build/$b.bin.%{_target_cpu}
done
install -Dpm0644 -t $pkgdir/compat soma/data/compat/*.dae
for g in soma rebirth bunker; do
    install -Dpm0644 -t %{buildroot}%{_datadir}/%{name}/$g $g/data/script_api.txt
done

install -Dpm0644 /dev/stdin $pkgdir/steam-common.sh <<'EOF'
set -eu
PKGDIR=%{_libexecdir}/%{name}

# oh_gamedir <appid> <steam dir> <icon name>: sets $gamedir
oh_gamedir() {
    for steam in "$HOME/.steam/steam" "$HOME/.local/share/Steam" \
        "$HOME/.var/app/com.valvesoftware.Steam/.local/share/Steam" ""; do
        [ -n "$steam" ] || { echo "Open HPL: no Steam install found" >&2; exit 1; }
        [ -d "$steam/steamapps/common" ] && break
    done
    # Same size as the packaged placeholder, so the Steam icon wins the theme lookup.
    icon="${XDG_DATA_HOME:-$HOME/.local/share}/icons/hicolor/128x128/apps/$3.png"
    if [ ! -e "$icon" ]; then
        src=$(find "$steam/appcache/librarycache/$1" -maxdepth 1 -regextype posix-extended \
            -regex '.*/[0-9a-f]{40}\.jpg' 2>/dev/null | head -n1)
        [ -z "$src" ] || install -Dm0644 "$src" "$icon"
    fi
    gamedir="$steam/steamapps/common/$2"
    [ -d "$gamedir" ] || gamedir=$(sed -n 's/^[[:space:]]*"path"[[:space:]]*"\(.*\)"$/\1/p' \
        "$steam/steamapps/libraryfolders.vdf" 2>/dev/null | while IFS= read -r lib; do
            if [ -d "$lib/steamapps/common/$2" ]; then echo "$lib/steamapps/common/$2"; break; fi
        done)
    [ -d "$gamedir" ] || { echo "Open HPL: install $2 via Steam first" >&2; exit 1; }
}
EOF

launcher() {
    install -Dpm0755 /dev/stdin %{buildroot}%{_bindir}/open-hpl-$1 <<EOF
#!/bin/sh
. %{_libexecdir}/%{name}/steam-common.sh
oh_gamedir $2 "$3" open-hpl-$1
exec \$PKGDIR/$4 "\$gamedir" "\$@"
EOF
    install -Dpm0644 /dev/stdin %{buildroot}%{_datadir}/applications/open-hpl-$1.desktop <<EOF
[Desktop Entry]
Type=Application
Name=$6
Comment=$5
Keywords=Open HPL;HPL;Frictional Games;
Exec=open-hpl-$1
Icon=open-hpl-$1
Categories=Game;
Terminal=false
EOF
    install -Dpm0644 %{name}.png %{buildroot}%{_datadir}/icons/hicolor/128x128/apps/open-hpl-$1.png
    desktop-file-validate %{buildroot}%{_datadir}/applications/open-hpl-$1.desktop
}

launcher amnesia 57300 "Amnesia The Dark Descent" \
    Launcher.bin.aarch64 "Open HPL native aarch64 build" "Amnesia: The Dark Descent"
launcher machine-for-pigs 239200 "Machine for Pigs" \
    Amnesia.bin.aarch64 "Open HPL experimental: Dark Descent's game code on AMFP's data" "Amnesia: A Machine for Pigs"
launcher soma 282140 "SOMA" \
    Soma.bin.aarch64 "Open HPL experimental native aarch64 port" "SOMA"
launcher rebirth 999220 "Amnesia Rebirth" \
    Soma.bin.aarch64 "Open HPL experimental native aarch64 port" "Amnesia: Rebirth"
launcher bunker 1944430 "Amnesia The Bunker" \
    Soma.bin.aarch64 "Open HPL experimental native aarch64 port" "Amnesia: The Bunker"

%check
cd amnesia/src
%ctest

%files
%license LICENSE THIRD_PARTY_LICENSES.md
%doc README.md PORTING_NOTES.md
%{_bindir}/open-hpl-*
%{_libexecdir}/%{name}/
%{_datadir}/%{name}/
%{_datadir}/applications/open-hpl-*.desktop
%{_datadir}/icons/hicolor/128x128/apps/open-hpl-*.png

%changelog
* Fri Oct 30 2026 lacamar <lchlnm@pm.me> - 1.3.63-1
- Add F9 in-game bug report

* Mon Oct 26 2026 lacamar <lchlnm@pm.me> - 1.3.62-1
- Implement cLux_DeloadMap
- Run tests in %check

* Mon Oct 26 2026 Lachlan Marie <lchlnm@pm.me> - 1.3.61-2
- Spec moved into repo
- Run ctest in %%check

* Tue Oct 20 2026 Lachlan Marie <lchlnm@pm.me> - 1.3.61-1
- Fix stretched posed cable chains and ragdolls

* Tue Oct 20 2026 Lachlan Marie <lchlnm@pm.me> - 1.3.60-1
- Fix falling off zeppelin platform at theta transition

* Mon Oct 19 2026 Lachlan Marie <lchlnm@pm.me> - 1.3.59-1
- Fix omnitool panel/slot sounds
- Fix welding sparks and torch sound following Kate
- Fix T-pose corpse in delta
- Fix black box voice playback

* Mon Oct 19 2026 Lachlan Marie <lchlnm@pm.me> - 1.3.58-1
- Fix jumping after loading an underwater save
- Official save date format

* Mon Oct 19 2026 Lachlan Marie <lchlnm@pm.me> - 1.3.57-1
- Fix voice lines randomly skipped

* Mon Oct 19 2026 Lachlan Marie <lchlnm@pm.me> - 1.3.56-1
- Refuse to run with writable Steam libraries
- Protect all Steam library folders

* Sun Oct 18 2026 Lachlan Marie <lchlnm@pm.me> - 1.3.55-1
- Rebirth/Bunker launchers run Soma engine
- Drop scaffold binaries
- Bunker: tagged start map
- Fix new game crash
- Fix text frame alignment
- Fix NaN black squares from zero mesh tangents
- Fix map timestamp rate
- Fix pause for sounds and voices
- Rebirth/Bunker spot light shadows

* Wed Oct 14 2026 Lachlan Marie <lchlnm@pm.me> - 1.3.54-1
- Static map props physics
- Liquid trickle
- Prefer shipped .msh
- Map pose scale fix
- Oversized metric, material triage

* Wed Oct 14 2026 Lachlan Marie <lchlnm@pm.me> - 1.3.53-1
- Load HPL3 .msh for old FBX
- Posed props skip autoplay
- Empty map sounds match official

* Wed Oct 14 2026 Lachlan Marie <lchlnm@pm.me> - 1.3.52-1
- Fix water shader fallback
- Load PathNodePatrol/InterestPoint nodes
- Fix Newton deep tree crash
- Fix FBX bone weight overflow
- Rebirth allowlist entries

* Wed Oct 14 2026 Lachlan Marie <lchlnm@pm.me> - 1.3.51-1
- Rebirth: physics mass mul applied
- Rebirth: module line of sight callback
- Rebirth: voice time left
- Rebirth: 01_01 conformance allowlist
- soma-play: focus pick for Rebirth/Bunker

* Wed Oct 14 2026 Lachlan Marie <lchlnm@pm.me> - 1.3.50-1
- Rebirth: script blur
- Rebirth: prop collision callback
- Rebirth: attack damage, proximity sensor
- Rebirth: fix player always dead

* Wed Oct 14 2026 Lachlan Marie <lchlnm@pm.me> - 1.3.49-1
- Rebirth: sRGB textures and gobos
- Rebirth: FXAA
- Rebirth: fear tentacles
- Rebirth: level door map change
- Fix same-named sound banks
- Animation message callbacks
- Extension-less animated textures
- Light brightness fades
- Looping events survive max playbacks

* Wed Oct 14 2026 Lachlan Marie <lchlnm@pm.me> - 1.3.48-1
- Fix bloom blow-out
- Rebirth: script user components
- Rebirth: layered colour grading
- Rebirth: door damage meshes
- Quieter shader patch log

* Tue Oct 13 2026 Lachlan Marie <lchlnm@pm.me> - 1.3.47-1
- Rebirth: night vision
- Rebirth: fix black frame, caves crash
- Rebirth: bloom, colour grading
- Rebirth: interact connections, FOV muls

* Tue Oct 13 2026 Lachlan Marie <lchlnm@pm.me> - 1.3.46-1
- Rebirth: memory flashback, voices, sketchbook
- Rebirth: hints, font colours
- Rebirth: fix magenta terrain edges
- HPL3 entity sleep

* Tue Oct 13 2026 Lachlan Marie <lchlnm@pm.me> - 1.3.45-1
- Rebirth: fix wake-up camera
- Rebirth: fix normal map channels
- Rebirth: fix black specks
- Fix degenerate tangents

* Tue Oct 13 2026 Lachlan Marie <lchlnm@pm.me> - 1.3.44-1
- SOMA: fix floaty thrown props
- SOMA: fix blur after reading

* Tue Oct 13 2026 Lachlan Marie <lchlnm@pm.me> - 1.3.43-1
- SOMA: fix hurt overlay
- SOMA: centre subtitles
- Fix GUI text batch overflow

* Tue Oct 13 2026 Lachlan Marie <lchlnm@pm.me> - 1.3.42-1
- Rebirth/Bunker: boot on SOMA module
- Rebirth: fog, box lights, terrain holes
- Rebirth: vertexblend material, player body
- Rebirth/Bunker: FSB5 Vorbis, DX10 DDS
- AMFP: script API, all maps load
- SOMA: map preload cache, image trail fix
- Install Rebirth/Bunker script_api.txt

* Sun Oct 11 2026 Lachlan Marie <lchlnm@pm.me> - 1.3.41-1
- SOMA: FMOD EQ/filter, pitch, pan envelopes
- SOMA: FMOD reverb send
- SOMA: water vertex waves
- SOMA: translucent refraction blend, DoF, fog weight
- SOMA: terrain queries, line of sight, ray casts
- SOMA: far GUI screen caching
- SOMA: readable dissolve fade-in
- Hinge position reset, camera smoothing toggle
- Save map cache to XDG
- Fix Newton ray termination
- Fix headless exit hang
* Fri Oct 09 2026 Lachlan Marie <lchlnm@pm.me> - 1.3.40-1
- SOMA: preload next map while playing
- SOMA: animated loading icon, boot splash
- SOMA: live settings apply
- SOMA: freeze world while paused
- SOMA: keep mouse captured on screens
- SOMA: lipsync from .anno files
- SOMA: map bone poses fixed
- SOMA: carry props across map transfers
- SOMA: terrain detail textures
- SOMA: quit on window close/SIGTERM
* Wed Oct 07 2026 Lachlan Marie <lchlnm@pm.me> - 1.3.39-1
- SOMA: edge glow
- SOMA: color grading fades, save camera state
- SOMA: voice end padding, effect fades
- SOMA: death FX terminal, runtime entity create/destroy
- SOMA: billboard brightness, fog/irradiance fades
- SOMA: joint limit sounds, FBX .anm bone units
- SOMA: liquid area deactivation fix
- SOMA: keep map name across loads

* Wed Oct 07 2026 Lachlan Marie <lchlnm@pm.me> - 1.3.38-1
- Amnesia: settings to XDG config dir
- Flags and logs to XDG state dir
- Move existing settings on first run
- OpenAL debug log to XDG state dir

* Tue Oct 06 2026 Lachlan Marie <lchlnm@pm.me> - 1.3.37-1
- SOMA: lens flares, detail maps, sRGB GUI
- SOMA: liquid buoyancy, HPL3 sun shadows
- SOMA: dialog branch events, save fixes
- Create missing XDG dirs on first run
- No OpenAL temp log in $HOME
- Rebirth/Bunker: log to XDG state dir
- Find games in all Steam libraries
- Fix Newton crash on disjoint hulls

* Mon Oct 05 2026 Lachlan Marie <lchlnm@pm.me> - 1.3.36-1
- SOMA: force fields, creature tails, agent pitch
- SOMA: end credits, 05_01 -> 05_02
- SOMA: terrain blend layers, decals, undergrowth
- SOMA: sun cascaded shadows, underwater fog
- SOMA: omnitool and voice screens
- SOMA: agent movement like the real engine
- SOMA: save and map transfer fixes
- HPL2: in-tree Newton 2.36
- HPL2: HPL3-style static collision combine

* Sun Oct 04 2026 Lachlan Marie <lchlnm@pm.me> - 1.3.35-1
- Fix build on Fedora 43

* Sun Oct 04 2026 Lachlan Marie <lchlnm@pm.me> - 1.3.34-1
- SOMA: liquid surfaces and fog
- SOMA: agents, death, game-over reload
- SOMA: GUI camera textures, ImGui fixes
- SOMA: save format v16
- SOMA: cached script bytecode
- HPL2: delayed occlusion culling
- HPL2: Newton joint limits

* Sat Oct 03 2026 Lachlan Marie <lchlnm@pm.me> - 1.3.33-1
- Never write to game dirs
- Caches in XDG cache dir
- Launchers run from package dir

* Sat Oct 03 2026 Lachlan Marie <lchlnm@pm.me> - 1.3.32-1
- SOMA: bloom, film grain
- SOMA: marine snow particles

* Sat Oct 03 2026 Lachlan Marie <lchlnm@pm.me> - 1.3.31-1
- Dev HUD: version, device, OS, GPU, FPS
- SOMA: HUD option, gamma slider, secret codes
- SOMA: HPL3 fog, directional light, script post effects
- SOMA: translucency, detail maps, soft particles
- SOMA: lights, particles, sounds, billboards in saves
- SOMA: animated meshes, NPC animations, drawers

* Fri Oct 02 2026 Lachlan Marie <lchlnm@pm.me> - 1.3.30-1
- SOMA: spot shadows, box lights, light probes
- SOMA: HDR tone mapping, colour grading, SSAO, depth of field
- SOMA: terrain geometry and collision
- SOMA: skybox colour, material sway, specular gobos
- SOMA: FBX skeletons, bone sockets, animation events
- SOMA: FMOD events, soundscapes, music
- SOMA: terminal and GUI screens, hints, screen text
- SOMA: props, critters, agents, step climbing
- SOMA: gamepad profiles, fullscreen resolution, game mode panel
- SOMA: 01_02 vent robot sequence, 01_03 intro
- Source from GitHub
- Drop unused upstream tools and sources

* Wed Sep 30 2026 Lachlan Marie <lchlnm@pm.me> - 1.3.29-1
- SOMA: entity collide groups (bed no longer blocks the player)
- Fix build with GCC 15

* Wed Sep 30 2026 Lachlan Marie <lchlnm@pm.me> - 1.3.28-1
- SOMA: run the game's own scripts (recovered script API)
- SOMA: script player, input, interaction, doors, drawers, terminals
- SOMA: voice, dialog, FMOD event sounds
- SOMA: checkpoint and in-place saves
- SOMA: official main menu and boot splash
- Install script_api.txt

* Thu Sep 24 2026 Lachlan Marie <lchlnm@pm.me> - 1.3.27-1
- SOMA: fix lighting shifting while turning (fog depth)
- SOMA: fix blue lighting buffer
- SOMA: hide inactive map entities (detached legs)
- SOMA: fix oversized skinned meshes
- SOMA: FXAA anti-aliasing
- SOMA: phone ring and pickup sounds
- SOMA: re-enable mesh cache

* Wed Sep 23 2026 Lachlan Marie <lchlnm@pm.me> - 1.3.26-1
- SOMA: fix invisible drawers and doors (joint axis)
- SOMA: pin jointed props, restoring frame rate

* Wed Sep 23 2026 Lachlan Marie <lchlnm@pm.me> - 1.3.25-1
- SOMA: fix all meshes loading 100x too large
- SOMA: drop mesh cache (caused a blue cast)

* Mon Sep 21 2026 Lachlan Marie <lchlnm@pm.me> - 1.3.24-1
- SOMA: fix lighting (G-buffer sampling, per-light falloff/brightness)
- SOMA: load normal maps (ATI2/BC5U)
- SOMA: load decals, billboards, particles, fog areas, detail meshes, FBX meshes
- SOMA: all 29 maps boot; several crash fixes
- SOMA: mesh cache under XDG cache dir
- Add assimp dependency

* Sat Sep 12 2026 Lachlan Marie <lchlnm@pm.me> - 1.3.23-1
- No user-facing changes - internal diagnostics and documentation only,
  ahead of continued work on SOMA's real apartment map still rendering
  too dark.

* Thu Sep 10 2026 Lachlan Marie <lchlnm@pm.me> - 1.3.22-1
- SOMA: fixed a crash loading the real apartment map right after the
  intro slideshow.

* Wed Sep 09 2026 Lachlan Marie <lchlnm@pm.me> - 1.3.21-1
- SOMA: fixed a real bug that could cause the game to hang on a black
  screen right after launch instead of showing the boot splash.

* Wed Sep 09 2026 Lachlan Marie <lchlnm@pm.me> - 1.3.20-1
- SOMA: fixed a real bug that could rewrite files inside your Steam
  install directory on launch, which could trigger a "files failed to
  validate" repair in Steam.

* Wed Sep 09 2026 Lachlan Marie <lchlnm@pm.me> - 1.3.19-1
- SOMA: fixed real-time lights being silently culled and never rendered
  in the apartment map. The room still renders darker than intended -
  a further fix is in progress.

* Wed Sep 09 2026 Lachlan Marie <lchlnm@pm.me> - 1.3.18-1
- SOMA: fixed a crash loading the real apartment map (collision-only
  meshes like block_box's could read a negative array index).

* Wed Sep 09 2026 Lachlan Marie <lchlnm@pm.me> - 1.3.17-2
- SOMA: fixed an instant crash on launch ("Could not load vertex buffer
  from mesh 'core_box.dae'") by deploying the same compat meshes the
  Rebirth/Bunker launchers already deploy.

* Wed Sep 09 2026 Lachlan Marie <lchlnm@pm.me> - 1.3.17-1
- SOMA: fixed a real bug causing the game to rewrite its own real Steam
  files on every launch, which could trigger repeated file-validation
  failures.
- Engine binaries can now run against game data anywhere, without needing
  to be copied into the game's own install folder.

* Tue Sep 08 2026 Lachlan Marie <lchlnm@pm.me> - 1.3.16-1
- SOMA: fixed the boot splash order, added the animated loading icon,
  and made loading genuinely gate skipping.
- SOMA: fixed missing sounds (phone ring, car horns, and other ambient
  sounds) and added audio pause when the window loses focus.
- SOMA: the intro slideshow now paces correctly, and Simon's phone call
  waits for the player to actually answer it.
- SOMA: added a player HUD, a real in-game pause menu, and a difficulty
  select screen for New Game.

* Mon Sep 07 2026 Lachlan Marie <lchlnm@pm.me> - 1.3.15-1
- Fixed a bug that could corrupt the real Steam game files during testing;
  test tooling now refuses to run against a real install.
- SOMA: real animated loading icon, fixed menu music/ambience lingering
  into gameplay, fixed overlapping intro voice-over, real Options toggle
  switches and gamma-screen instructions, live resolution changes, and a
  new in-game pause menu (Escape).
- SOMA: added the apartment map's phone-call scene.

* Sun Sep 06 2026 Lachlan Marie <lchlnm@pm.me> - 1.3.14-1
- SOMA: fixed the remaining HDR-precision bug affecting box lights and
  fog areas (matches the earlier point-light/transparency fix).

* Sun Sep 06 2026 Lachlan Marie <lchlnm@pm.me> - 1.3.13-1
- SOMA: fixed severe map-rendering corruption and invisible New Game
  intro slides/subtitles.
- SOMA: real Options settings now actually work (resolution,
  anti-aliasing, field of view, subtitles, mouse sensitivity,
  rebindable keys).
- SOMA: fixed the loading-bar's position/size on the boot screen.

* Sat Sep 05 2026 Lachlan Marie <lchlnm@pm.me> - 1.3.12-1
- SOMA: real boot/splash sequence with loading bar, floating debris
  effect, custom cursor, real Options menu widgets and working menu
  sounds, and a New Game intro slideshow.
- Partial fix for a window-resize rendering bug (still open for SOMA
  specifically; fixed for Amnesia: The Dark Descent).

* Sat Sep 05 2026 Lachlan Marie <lchlnm@pm.me> - 1.3.10-1
- SOMA: menu music, a first-boot gamma calibration screen, a real
  Options menu (volume/gamma/vsync/fullscreen), and a real player
  controller (gravity, walking, jumping, collision) on gameplay maps.

* Fri Sep 04 2026 Lachlan Marie <lchlnm@pm.me> - 1.3.9-1
- Fixed a window-resize bug (e.g. a tiling compositor fullscreening the
  window) that left rendering pinned to a stale-sized corner with the
  rest of the screen black.
- Fixed the Launcher writing its config/log outside the XDG Base
  Directory spec.
- SOMA: New Game now loads the real declared start map instead of a
  hardcoded wrong one.

* Fri Sep 04 2026 Lachlan Marie <lchlnm@pm.me> - 1.3.8-1
- SOMA: rewrote the main menu to match the real game's own menu logic,
  read directly out of its shipped AngelScript source and debug-info
  binary - correct 1280x720-scaled layout, real background/dirt-corner
  art, an animated glitch title and cathedral ghost-trail effect, and
  real text-based menu buttons with the real teal highlight bar.

* Fri Sep 04 2026 Lachlan Marie <lchlnm@pm.me> - 1.3.7-1
- SOMA: the main menu now uses SOMA's own real background/title art
  instead of a generic placeholder.

* Fri Sep 04 2026 Lachlan Marie <lchlnm@pm.me> - 1.3.6-1
- SOMA: fixed the splash logos (wrong texture format, oversized second
  logo) and added a real interactive main menu (New Game / Quit).

* Fri Sep 04 2026 Lachlan Marie <lchlnm@pm.me> - 1.3.5-1
- SOMA: added OPENHPL_SOMA_MAP env var to load a real map on normal boot.
- Trimmed this file and README.md.

* Fri Sep 04 2026 Lachlan Marie <lchlnm@pm.me> - 1.3.4-1
- SOMA now renders real lit, textured, correctly-exposed geometry: fixed
  missing light/skybox/fog shader uniforms, applied SOMA's own per-area
  exposure data, and fixed an HDR precision bug in glass/translucent
  materials. See PORTING_NOTES.md for details.
- Headless test runs no longer wake the screen, play sound, or run more
  than one instance at once (dev/CI tooling only, no effect on normal play).

* Thu Sep 03 2026 Lachlan Marie <lchlnm@pm.me> - 1.3.3-1
- Added a regression test suite (CTest) covering string/path helpers and
  SOMA's shader transpiler.
- Added a "Show FPS" checkbox to Options > Graphics.
- Minor rendering optimization: skip redundant texture-unit rebinds.
- Wayland: exclusive fullscreen now degrades correctly; Launcher window
  now has a proper app-id.
- Fixed two headless-automation-server crashes/bugs (dev tooling only).
- SOMA/Rebirth/Bunker: extended the shader transpiler, added entity/area
  loaders for Rebirth, fixed Bunker's spawn point. All three remain
  tech-preview, pre-alpha.

* Wed Sep 02 2026 Lachlan Marie <lchlnm@pm.me> - 1.3.2-1
- Save games, config, logs, map caches, and screenshots now follow the
  XDG Base Directory spec instead of scattering across
  ~/.frictionalgames, the Steam game directory, and ~/Desktop. Existing
  saves are migrated automatically on first run.

* Wed Sep 02 2026 Lachlan Marie <lchlnm@pm.me> - 1.3.1-12
- Fixed the in-game UI Scale setting at 2x/3x/4x: several screens (loading
  screen, HUD, inventory, journal, credits) rendered oversized and spilled
  off-screen. Also fixed a crash in the headless test server hit while
  verifying this.

* Wed Sep 02 2026 Lachlan Marie <lchlnm@pm.me> - 1.3.1-11
- Fixed desktop-entry icons often showing the generic placeholder instead
  of each game's real Steam thumbnail.
- Added "Open HPL" branding/keywords to all desktop entries.

* Wed Sep 02 2026 Lachlan Marie <lchlnm@pm.me> - 1.3.1-10
- Fixed the splash/pre-menu screens not skipping on most keypresses.
- Added an opt-in hidden-window mode to the headless automation server.
- Added three more EXPERIMENTAL desktop entries (SOMA, Amnesia: Rebirth,
  Amnesia: The Bunker), each a native aarch64 Phase 0 scaffold: boots the
  engine against that game's real data and shows one map via a free-fly
  debug camera, no player/menu/scripts yet.

* Tue Sep 01 2026 Lachlan Marie <lchlnm@pm.me> - 1.3.1-9
- Fixed the UI Scale setting (added in 1.3.1-7): most menus and the splash
  screen were unusable at 2x/3x/4x.

* Tue Sep 01 2026 Lachlan Marie <lchlnm@pm.me> - 1.3.1-8
- The Launcher and game windows now run as native Wayland clients instead
  of through XWayland.

* Tue Sep 01 2026 Lachlan Marie <lchlnm@pm.me> - 1.3.1-7
- Fixed the resolution dropdown dropping options shared between two
  displays.
- Added an in-game UI Scale control (Options > Graphics).

* Tue Sep 01 2026 Lachlan Marie <lchlnm@pm.me> - 1.3.1-6
- Fixed a real engine bug: teleporting the player mid-fall could tunnel
  through floor collision at the destination.

* Mon Aug 31 2026 Lachlan Marie <lchlnm@pm.me> - 1.3.1-5
- Re-added an experimental "Amnesia: A Machine for Pigs" entry, this time
  running Dark Descent's real native binary against AMFP's data (no x86_64
  emulation). Level data loads and scripts run; some collision/gameplay
  bugs remain.

* Mon Aug 31 2026 Lachlan Marie <lchlnm@pm.me> - 1.3.1-4
- Removed the box64-based Machine for Pigs entry - this project is
  native-aarch64-only, no emulation fallback.

* Mon Aug 31 2026 Lachlan Marie <lchlnm@pm.me> - 1.3.1-3
- Source refresh only; no shipped-binary changes. Picks up early
  reverse-engineering work on AMFP and SOMA.

* Sun Aug 30 2026 Lachlan Marie <lchlnm@pm.me> - 1.3.1-2
- Split the desktop entry into per-game entries; added a soft box64
  dependency for the (since-removed) Machine for Pigs entry.
- Fixed the Launcher hardcoding the wrong binary name on non-x86 targets.

* Sun Aug 30 2026 Lachlan Marie <lchlnm@pm.me> - 1.3.1-1
- Renamed from amnesia-dark-descent to open-hpl. No functional changes.
