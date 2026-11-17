//! S9: map-level state of `SomaLux.cpp` (timers, map-change requests, preload and pause bookkeeping,
//! collide groups, update-when-paused set) and its script natives. The C++ shell keeps the engine
//! objects (world, entities, script runtime) and answers the host ops below.

use super::s5_input_ffi::Io;
use super::s5c_game::{Ops, with_ops};
use super::{pause::Pause, transfer::Preload};
use crate::somascript::s2_as::{Engine, Frame, Soma, str_bytes};
use crate::somascript::s2_host;
use crate::string;
use std::cell::{RefCell, UnsafeCell};
use std::collections::HashMap;
use std::ffi::c_void;

pub mod op {
    pub const FIRE: u32 = 101;
    pub const STR_REF: u32 = 102;
    pub const INIT_CONFIG: u32 = 103;
    pub const EXIT: u32 = 104;
    pub const SCRIPT_PAUSED: u32 = 105;
    pub const SET_PAUSED: u32 = 106;
    pub const PRELOAD_READY: u32 = 107;
    pub const HIDE_CROSSHAIR: u32 = 108;
    pub const UPDATES_PER_SEC: u32 = 109;
    pub const ARR_PUSH_STR: u32 = 110;
    pub const ARR_RESIZE: u32 = 111;
    pub const ARR_SET: u32 = 112;
    pub const APP_TIME: u32 = 113;
    pub const SLEEP: u32 = 114;
    pub const GAME_TIME: u32 = 115;
    pub const DUMP_ENABLED: u32 = 1;
    pub const DUMP_LOG: u32 = 2;
}

pub struct Timer {
    id: u64,
    name: Vec<u8>,
    func: Vec<u8>,
    time: f32,
    paused: bool,
    user_float: f32,
    user_int: i32,
    user_string: Vec<u8>,
    length: f32,
    removed: bool,
}

#[derive(Default)]
struct MapTimers {
    timers: std::collections::VecDeque<Timer>,
    time: f64,
    stamp: i32,
    updating: bool,
    firing: Option<u64>,
    next_id: u64,
}

impl MapTimers {
    fn push(&mut self, mut t: Timer, front: bool) {
        t.id = self.next_id;
        self.next_id += 1;
        if front {
            self.timers.push_front(t);
        } else {
            self.timers.push_back(t);
        }
    }

    fn find(&mut self, name: &[u8]) -> Option<&mut Timer> {
        self.timers.iter_mut().find(|t| t.name == name)
    }

    fn index_of(&self, id: u64) -> Option<usize> {
        self.timers.iter().position(|t| t.id == id)
    }

    fn live(&self, t: &Timer) -> bool {
        !t.removed && (self.firing != Some(t.id) || t.time > 0.0)
    }

    fn add(&mut self, name: &[u8], time: f32, func: &[u8]) {
        let time = if time > 0.0 { time } else { 0.001 };
        self.push(
            Timer {
                id: 0,
                name: name.to_vec(),
                func: func.to_vec(),
                time,
                paused: false,
                user_float: 0.0,
                user_int: 0,
                user_string: Vec::new(),
                length: time,
                removed: false,
            },
            true,
        );
    }

    fn remove(&mut self, name: &[u8]) {
        for t in self.timers.iter_mut().filter(|t| t.name == name) {
            t.removed = true;
        }
        if !self.updating {
            self.timers.retain(|t| !t.removed);
        }
    }

    fn restart_current(&mut self, time: f32) {
        let Some(id) = self.firing else { return };
        if let Some(t) = self.timers.iter_mut().find(|t| t.id == id) {
            t.time = if time < 0.0 { t.length } else { time };
        }
    }
}

thread_local! {
    static MAPS: RefCell<HashMap<usize, MapTimers>> = RefCell::new(HashMap::new());
}

fn with_map<R>(map: usize, f: impl FnOnce(&mut MapTimers) -> R) -> R {
    MAPS.with(|m| f(m.borrow_mut().entry(map).or_default()))
}

fn dump(o: &dyn Ops, line: &str) {
    if o.go(op::DUMP_ENABLED, Io::new()).0 != 0 {
        send(o, op::DUMP_LOG, format!("s9 {line}").as_bytes());
    }
}

fn send(o: &dyn Ops, code: u32, s: &[u8]) -> (i32, Io) {
    let mut io = Io::new();
    io.s = s.as_ptr();
    io.n = s.len();
    o.go(code, io)
}

fn lossy(b: &[u8]) -> String {
    String::from_utf8_lossy(b).into_owned()
}

pub fn timers_update(o: &dyn Ops, map: usize, dt: f32) {
    let ids = with_map(map, |m| {
        for t in m.timers.iter_mut().filter(|t| !t.paused) {
            t.time -= dt;
        }
        m.time += f64::from(dt);
        m.stamp += 1;
        m.updating = true;
        m.timers.iter().map(|t| t.id).collect::<Vec<_>>()
    });
    for id in ids {
        let due = with_map(map, |m| {
            let i = m.index_of(id)?;
            let t = m.timers.get(i)?;
            if t.paused || t.time > 0.0 || t.removed {
                return None;
            }
            m.firing = Some(id);
            Some((t.name.clone(), t.func.clone()))
        });
        let Some((name, func)) = due else { continue };
        dump(o, &format!("tfire {} {}", lossy(&name), lossy(&func)));
        let mut io = Io::new();
        io.p[0] = map as *mut c_void;
        let joined = [name.as_slice(), func.as_slice()].join(&0u8);
        io.s = joined.as_ptr();
        io.n = joined.len();
        o.go(op::FIRE, io);
        with_map(map, |m| {
            m.firing = None;
            if let Some(i) = m.index_of(id)
                && m.timers.get(i).is_some_and(|t| t.time <= 0.0)
            {
                m.timers.remove(i);
            }
        });
    }
    with_map(map, |m| {
        m.updating = false;
        m.timers.retain(|t| !t.removed);
    });
}

pub fn collide_flag(groups: &mut Vec<Vec<u8>>, spec: &[u8]) -> u32 {
    let (mut member, mut exclude) = (0u32, 0u32);
    for tok in string::split(spec, None, true) {
        let Some((&sign, name)) = tok.split_first() else {
            continue;
        };
        if tok.len() < 2 || (sign != b'+' && sign != b'-') {
            continue;
        }
        let idx = match groups.iter().position(|g| g == name) {
            Some(i) => i,
            None if groups.len() >= 16 => continue,
            None => {
                groups.push(name.to_vec());
                groups.len() - 1
            }
        };
        if sign == b'+' {
            member |= 1 << idx;
        } else {
            exclude |= 1 << idx;
        }
    }
    if member == 0 && exclude == 0 {
        return 0;
    }
    (if member != 0 { member } else { 0xFFFF }) | ((0xFFFF & !exclude) << 16)
}

pub fn collide_match(a: u32, b: u32) -> bool {
    a & (b >> 16) & 0xFFFF != 0 && b & (a >> 16) & 0xFFFF != 0
}

/// Bunker `StartMap` attributes are "Tag:value, Tag:value"; an untagged entry matches any tag.
pub fn start_map_value(config: &[u8], tag: &[u8]) -> Vec<u8> {
    let trim = |s: &[u8]| -> Vec<u8> {
        let ws = |c: &u8| b" \t\r\n".contains(c);
        let a = s.iter().position(|c| !ws(c));
        let b = s.iter().rposition(|c| !ws(c));
        match (a, b) {
            (Some(a), Some(b)) => s.get(a..=b).unwrap_or_default().to_vec(),
            _ => Vec::new(),
        }
    };
    for entry in string::split(config, Some(b","), true) {
        let (etag, val) = match entry.iter().position(|&c| c == b':') {
            Some(c) => {
                let (head, tail) = entry.split_at(c);
                (trim(head), tail.get(1..).unwrap_or_default())
            }
            None => (Vec::new(), entry),
        };
        if tag.is_empty() || etag.is_empty() || etag == tag {
            return trim(val);
        }
    }
    Vec::new()
}

#[derive(Default)]
struct Requests {
    map: Vec<u8>,
    start: Vec<u8>,
    transfer: Vec<u8>,
    preload_map: Vec<u8>,
    fade_in: Option<f32>,
    new_game: bool,
    is_transfer: bool,
    groups: Vec<Vec<u8>>,
    update_when_paused: Vec<usize>,
    out: Vec<u8>,
}

thread_local! {
    static REQ: RefCell<Requests> = RefCell::new(Requests::default());
}

fn req<R>(f: impl FnOnce(&mut Requests) -> R) -> R {
    REQ.with(|r| f(&mut r.borrow_mut()))
}

fn request_map_change(map: &[u8], start: &[u8]) {
    req(|r| {
        r.map = string::set_file_ext(string::file_name(map), b"hpm");
        r.start = start.to_vec();
        r.transfer.clear();
    });
}

struct Cell<T>(UnsafeCell<T>);
// SAFETY: only the main thread touches these.
unsafe impl<T> Sync for Cell<T> {}

static PRELOAD: Cell<Preload> = Cell(UnsafeCell::new(Preload {
    prio: 2,
    last_frame: !0,
    debt_ms: 0.0,
}));
static PAUSE: Cell<Pause> = Cell(UnsafeCell::new(Pause {
    music: false,
    sounds: false,
    ambience: false,
    saving_enabled: true,
    saving_disabled_time: 0.0,
}));

fn text<'a>(f: &Frame, i: usize) -> &'a [u8] {
    // SAFETY: the declaration passes a `const tString &in` in slot `i`.
    unsafe { str_bytes(f.ptr_arg(i)) }
}

fn str_ref(o: &dyn Ops, b: &[u8]) -> *mut c_void {
    send(o, op::STR_REF, b).1.p[1]
}

fn init_config(o: &dyn Ops, level: &[u8], name: &[u8]) -> Vec<u8> {
    send(o, op::INIT_CONFIG, &[level, name].join(&0u8)).1.text()
}

fn start_value(o: &dyn Ops, name: &[u8], tag: &[u8]) -> Vec<u8> {
    start_map_value(&init_config(o, b"StartMap", name), tag)
}

fn request_new_game(map: &[u8], start: &[u8]) {
    request_map_change(map, start);
    req(|r| r.new_game = true);
}

fn set_paused(o: &dyn Ops, b: bool, m: bool, s: bool, a: bool) {
    let mut io = Io::new();
    io.i = [i32::from(b), i32::from(m), i32::from(s), i32::from(a)];
    let _ = o.go(op::SET_PAUSED, io);
}

fn with_o<R>(f: impl FnOnce(&dyn Ops) -> R) -> Option<R> {
    with_ops(f)
}

fn this_map(f: &Frame) -> usize {
    f.this as usize
}

pub fn register(e: Engine) {
    let s = Soma(e);
    let bunker = s2_host::game_id() == b"bunker";
    let rebirth = s2_host::game_id() == b"rebirth";
    s.func(
        "void cLux_ChangeMap(const tString&in asMapName, const tString&in asStartPos, const tString&in asTransferArea, const tString&in asStartSound, const tString&in asEndSound)",
        |f| change_map(f, None),
    );
    s.func(
        "void cLux_ChangeMap(const tString&in asMapName, const tString&in asStartPos, const tString&in asTransferArea, const tString&in asStartSound, const tString&in asEndSound, bool abShowLoadingScreen, float afFadeInTime=-1.0f)",
        |f| change_map(f, Some(f.arg::<f32>(6))),
    );
    if bunker {
        s.func("void cLux_StartNewGame(const tString&in asMapName=\"\", const tString&in asStartPos=\"\")", |f| {
            with_o(|o| {
                let (map, pos) = (text(f, 0), text(f, 1));
                let map = if map.is_empty() { start_value(o, b"File", b"") } else { map.to_vec() };
                let pos = if pos.is_empty() { start_value(o, b"Pos", b"") } else { pos.to_vec() };
                request_new_game(&map, &pos);
            });
        });
        for (decl, key) in [
            (
                "const tString& cLux_GetStartMapFile(const tString &in asTag=\"\")",
                &b"File"[..],
            ),
            (
                "const tString& cLux_GetStartMapFolder(const tString &in asTag=\"\")",
                b"Folder",
            ),
            (
                "const tString& cLux_GetStartMapPos(const tString &in asTag=\"\")",
                b"Pos",
            ),
        ] {
            s.func(decl, move |f| {
                let p = with_o(|o| str_ref(o, &start_value(o, key, text(f, 0))));
                f.ret_ref(p.unwrap_or(std::ptr::null_mut()));
            });
        }
    } else {
        s.func("void cLux_StartNewGame()", |_| {
            with_o(|o| {
                request_new_game(&start_value(o, b"File", b""), &start_value(o, b"Pos", b""))
            });
        });
    }
    s.func("void cLux_StartMap(const tString&in asMapName)", |f| {
        request_new_game(text(f, 0), b"")
    });
    s.func("const tString &cLux_GetMainMenuFile()", |f| {
        let p = with_o(|o| str_ref(o, &init_config(o, b"MainMenu", b"File")));
        f.ret_ref(p.unwrap_or(std::ptr::null_mut()));
    });
    s.func("void cLux_Exit()", |_| {
        with_o(|o| o.go(op::EXIT, Io::new()));
    });
    if bunker {
        s.func(
            "void cLux_SetGamePaused(bool abX, bool abPauseMusic, bool abPauseSounds, bool abPauseAmbience, eSoundEntryType aPauseSoundTypes = eSoundEntryType_All)",
            |f| {
                with_o(|o| {
                    let mut io = Io::new();
                    io.i = [f.arg::<u8>(0).into(), f.arg::<u8>(1).into(), f.arg::<u8>(2).into(), f.arg::<u8>(3).into()];
                    io.p[1] = f.arg::<u32>(4) as usize as *mut c_void;
                    io.f[0] = 1.0;
                    o.go(op::SET_PAUSED, io)
                });
            },
        );
    } else if rebirth {
        s.func(
            "void cLux_SetGamePaused(bool abX, bool abPauseMusic, bool abPauseSounds, bool abPauseAmbience)",
            |f| {
                with_o(|o| set_paused(o, f.arg(0), f.arg(1), f.arg(2), f.arg(3)));
            },
        );
    } else {
        s.func("void cLux_SetGamePaused(bool abX)", |f| {
            with_o(|o| set_paused(o, f.arg(0), true, true, true));
        });
    }
    s.func(
        "void cLux_AddUpdateWhenPausedEntity(cMeshEntity@ apMeshEntity)",
        |f| {
            let p = f.handle_arg(0) as usize;
            f.release_handles();
            if p != 0 {
                req(|r| {
                    if !r.update_when_paused.contains(&p) {
                        r.update_when_paused.push(p);
                    }
                });
            }
        },
    );
    s.func(
        "void cLux_RemoveUpdateWhenPausedEntity(cMeshEntity@ apMeshEntity)",
        |f| {
            let p = f.handle_arg(0) as usize;
            f.release_handles();
            req(|r| r.update_when_paused.retain(|&x| x != p));
        },
    );
    s.func("void cLux_DisableSaving(float afDuration)", |f| {
        // SAFETY: main-thread static, no other reference is live.
        let st = unsafe { &mut *PAUSE.0.get() };
        super::pause::disable_saving(st, f.arg(0));
    });
    s.func("void cLux_EnableSaving()", |_| {
        // SAFETY: as above.
        unsafe { (*PAUSE.0.get()).saving_enabled = true };
    });
    s.func("bool cLux_GetSavingEnabled()", |f| {
        // SAFETY: as above.
        f.ret(unsafe { (*PAUSE.0.get()).saving_enabled });
    });
    s.func("bool cLux_GetFootageModeActive()", |f| f.ret(false));
    s.func(
        "void cSound_PreloadSoundEvent(const tString&in asInternalPath)",
        |_| {},
    );
    s.func("void cLux_SetComputeAverageFrameColor(bool abX)", |_| {});
    s.func("void cLux_LoadScreenSetForceBackground(bool abX)", |_| {});
    s.method(
        "cLuxInputHandler",
        "void SetGamepadColor(int alId, const cColor&in aColor)",
        |_| {},
    );
    s.func("void cSystem_ProfileStart(const tString&in asName)", |_| {});
    s.func("void cSystem_ProfileEnd(const tString&in asName)", |_| {});
    s.func("bool cLux_GetGamePaused()", |f| {
        f.ret(with_o(|o| o.go(op::SCRIPT_PAUSED, Io::new()).0 != 0).unwrap_or(false));
    });
    s.func("bool cLux_GetHideCrosshair()", |f| {
        f.ret(with_o(|o| o.go(op::HIDE_CROSSHAIR, Io::new()).0 != 0).unwrap_or(false));
    });
    s.func("bool cLux_IsChangingMap()", |f| {
        f.ret(req(|r| !r.map.is_empty()))
    });
    s.func("bool cLux_MapChangeIsTransfer()", |f| {
        f.ret(req(|r| r.is_transfer))
    });
    s.func("bool cLux_IsReadyToChangeMap()", |f| {
        let want = req(|r| r.preload_map.clone());
        f.ret(
            want.is_empty()
                || with_o(|o| send(o, op::PRELOAD_READY, &want).0 != 0).unwrap_or(false),
        );
    });
    s.func("bool cLux_IsPlayGoReady(int&out alETA)", |f| {
        *f.rm::<i32>(0) = 0;
        f.ret(true);
    });
    s.func("bool cLux_IsStreamingMap()", move |f| {
        f.ret(req(|r| !r.preload_map.is_empty()) && !bunker)
    });
    s.func(
        "void cLux_PreloadMap(const tString&in asMapName, eWorldStreamPriority aPrio = eWorldStreamPriority_Normal)",
        |f| {
            req(|r| r.preload_map = text(f, 0).to_vec());
            // SAFETY: main-thread static.
            unsafe { (*PRELOAD.0.get()).prio = f.arg(1) };
        },
    );
    s.func(
        "void cLux_SetMapPreloadPriority(eWorldStreamPriority aPrio)",
        |f| {
            // SAFETY: main-thread static.
            unsafe { (*PRELOAD.0.get()).prio = f.arg(0) };
        },
    );
    for ty in ["cMeshEntity", "cSubMeshEntity", "cLuxProp"] {
        s.method_new(ty, "void SetRenderLayer(int alX)", |_| {});
    }
    if s2_host::game_id() != b"soma" {
        for ty in ["cLuxFootPlacement", "cLuxHandPlacement"] {
            s.method_new(
                ty,
                "void LoadFromVariables(cResourceVarsObject@ apVars)",
                |_| {},
            );
        }
        s.method_new(
            "cMeshEntity",
            "void SetUpdateBonesWhenCulled(bool abX)",
            |_| {},
        );
        s.method_new("cMeshEntity", "void SetDisableSleep(bool abX)", |_| {});
        s.method_new("cLuxProp", "void DisableSubMeshMerge(tString asX)", |_| {});
        s.method_new("cLuxPathfinder", "void SetClimbWalls(bool abX)", |_| {});
        s.method_new(
            "cLuxCharMover",
            "void SetDynamicObjectPlayerAvoidanceActive(bool abX)",
            |_| {},
        );
        s.method_new(
            "iLuxAchievementHandler",
            "void UnlockAchievement(int alID)",
            |_| {},
        );
        s.method_new(
            "iLuxAchievementHandler",
            "bool GetAllAchievementsUnlocked()",
            |f| f.ret(false),
        );
        s.method_new(
            "iLuxAchievementHandler",
            "tString GetAchievementName(int alID)",
            |f| f.ret_string(b""),
        );
        s.method_new("cLuxAgent", "bool GetRagdollActive()", |f| f.ret(false));
        s.method_new(
            "cLuxPathfinder",
            "void AddPathBlockerBody(tID alBodyID)",
            |_| {},
        );
        s.method_new(
            "cLuxPathfinder",
            "void RemovePathBlockerBody(tID alBodyID)",
            |_| {},
        );
        s.method_new(
            "cLuxPathfinder",
            "void SetMaxGoalsPerIsland(int alX)",
            |_| {},
        );
        s.method_new(
            "cLuxPathfinder",
            "void SetDirectPathMaxDistanceMul(float afX)",
            |_| {},
        );
        s.method_new(
            "iCharacterBody",
            "void SetSeparateMoveAndForceCollision(bool abX)",
            |_| {},
        );
        s.method_new(
            "iCharacterBody",
            "void SetMaxCollisionPoints(int alX)",
            |_| {},
        );
    }
    s.func("const tString& cSystem_GetPlatformName()", |f| {
        let p = with_o(|o| str_ref(o, b"Linux"));
        f.ret_ref(p.unwrap_or(std::ptr::null_mut()));
    });
    s.func_new("ePlatform cSystem_GetPlatform()", |f| f.ret(1i32));
    s.func("uint cSystem_GetApplicationTime()", |f| {
        f.ret(with_o(|o| o.go(op::APP_TIME, Io::new()).0 as u32).unwrap_or(0));
    });
    s.func("void cSystem_Sleep(uint alMilliSecs)", |f| {
        let mut io = Io::new();
        io.i[0] = f.arg::<u32>(0) as i32;
        with_o(|o| o.go(op::SLEEP, io));
    });
    s.func("double cLux_GetGameTime()", |f| {
        let io = with_o(|o| o.go(op::GAME_TIME, Io::new()).1).unwrap_or_else(Io::new);
        let bits = u64::from(io.i[0] as u32) | u64::from(io.i[1] as u32) << 32;
        f.ret(f64::from_bits(bits));
    });
    s.func(
        "void cSystem_GetAvailableVideoModes(array<cVector2l> &inout avScreenSizes, array<int> &inout avBpps, array<int> &inout avMinRefreshRates, int alMinBpp, int alMinRefreshRate, bool abRemoveDuplicates)",
        video_modes,
    );
    register_map_natives(&s);
}

fn bytes_per_pixel(format: u32) -> i32 {
    const fn fourcc(a: u8, b: u8, c: u8, d: u8) -> u32 {
        a as u32 | (b as u32) << 8 | (c as u32) << 16 | (d as u32) << 24
    }
    if format != 0 && (format >> 28) & 0xF != 1 {
        let two = [
            fourcc(b'Y', b'U', b'Y', b'2'),
            fourcc(b'U', b'Y', b'V', b'Y'),
            fourcc(b'Y', b'V', b'Y', b'U'),
        ];
        return if two.contains(&format) { 2 } else { 1 };
    }
    (format & 0xFF) as i32
}

fn video_modes(f: &Frame) {
    use crate::sys::sdl2::{
        SDL_DisplayMode, SDL_GetDisplayMode, SDL_GetNumDisplayModes, SDL_GetNumVideoDisplays,
    };
    let (min_bpp, min_rate, unique): (i32, i32, bool) = (f.arg(3), f.arg(4), f.arg(5));
    let mut modes: Vec<(i32, i32, i32)> = Vec::new();
    let mut rates: Vec<i32> = Vec::new();
    for d in 0..SDL_GetNumVideoDisplays() {
        for m in 0..SDL_GetNumDisplayModes(d) {
            let mut mode = SDL_DisplayMode {
                format: 0,
                w: 0,
                h: 0,
                refresh_rate: 0,
                driverdata: std::ptr::null_mut(),
            };
            // SAFETY: `mode` is a writable SDL_DisplayMode.
            if unsafe { SDL_GetDisplayMode(d, m, &mut mode) } != 0 {
                continue;
            }
            let bpp = bytes_per_pixel(mode.format) * 8;
            if bpp < min_bpp || (mode.refresh_rate != 0 && mode.refresh_rate < min_rate) {
                continue;
            }
            modes.push((mode.w, mode.h, bpp));
            if mode.refresh_rate != 0 && !rates.contains(&mode.refresh_rate) {
                rates.push(mode.refresh_rate);
            }
        }
    }
    modes.sort_by_key(|&(w, h, _)| (w, h));
    if unique {
        modes.dedup_by_key(|&mut (w, h, _)| (w, h));
    }
    rates.sort_unstable();
    if rates.is_empty() {
        rates.push(60);
    }
    with_o(|o| {
        let fill = |arr: *mut c_void, n: usize, value: &dyn Fn(usize) -> Vec<i32>| {
            let mut io = Io::new();
            io.p[0] = arr;
            io.i[0] = n as i32;
            o.go(op::ARR_RESIZE, io);
            for i in 0..n {
                let mut v = value(i);
                let mut io = Io::new();
                io.p[0] = arr;
                io.p[1] = v.as_mut_ptr().cast();
                io.i[0] = i as i32;
                o.go(op::ARR_SET, io);
            }
        };
        let at = |i: usize| modes.get(i).copied().unwrap_or_default();
        fill(f.ptr_arg(0), modes.len(), &|i| vec![at(i).0, at(i).1]);
        fill(f.ptr_arg(1), modes.len(), &|i| vec![at(i).2]);
        fill(f.ptr_arg(2), rates.len(), &|i| {
            vec![rates.get(i).copied().unwrap_or(0)]
        });
    });
}

fn change_map(f: &Frame, fade: Option<f32>) {
    request_map_change(text(f, 0), text(f, 1));
    req(|r| {
        r.transfer = text(f, 2).to_vec();
        if let Some(x) = fade {
            r.fade_in = Some(x);
        }
    });
    let line = format!(
        "SOMA script: change map to {} ({})\n",
        lossy(&req(|r| r.map.clone())),
        lossy(text(f, 1))
    );
    with_o(|o| send(o, 36, line.as_bytes()));
}

const M: &str = "cLuxMap";

fn register_map_natives(s: &Soma) {
    s.method(
        M,
        "void AddTimer(const tString&in asName, float afTime, const tString&in asFunction)",
        |f| {
            let (n, fun) = (text(f, 0), text(f, 2));
            with_o(|o| dump(o, &format!("tadd {} {}", lossy(n), lossy(fun))));
            with_map(this_map(f), |m| m.add(n, f.arg(1), fun));
        },
    );
    s.method(M, "void RemoveTimer(const tString&in asName)", |f| {
        with_o(|o| dump(o, &format!("trem {}", lossy(text(f, 0)))));
        with_map(this_map(f), |m| m.remove(text(f, 0)));
    });
    s.method(M, "void RestartCurrentTimer(float afTime = -1)", |f| {
        with_map(this_map(f), |m| m.restart_current(f.arg(0)));
    });
    s.method(
        M,
        "const tString& GetTimerUserVarString(const tString&in asName)",
        |f| {
            let v = with_map(this_map(f), |m| {
                m.find(text(f, 0))
                    .map(|t| t.user_string.clone())
                    .unwrap_or_default()
            });
            f.ret_ref(with_o(|o| str_ref(o, &v)).unwrap_or(std::ptr::null_mut()));
        },
    );
    s.method(
        M,
        "int IncTimerUserVarInt(const tString&in asName, int alX)",
        |f| {
            let x: i32 = f.arg(1);
            f.ret(with_map(this_map(f), |m| {
                m.find(text(f, 0)).map_or(0, |t| {
                    t.user_int = t.user_int.wrapping_add(x);
                    t.user_int
                })
            }));
        },
    );
    s.method(
        M,
        "float IncTimerUserVarFloat(const tString&in asName, float afX)",
        |f| {
            let x: f32 = f.arg(1);
            f.ret(with_map(this_map(f), |m| {
                m.find(text(f, 0)).map_or(0.0, |t| {
                    t.user_float += x;
                    t.user_float
                })
            }));
        },
    );
    s.method(
        M,
        "bool GetTimersNamed(const tString&in asName, array<tString>&inout avNames)",
        |f| {
            let names: Vec<Vec<u8>> = with_map(this_map(f), |m| {
                m.timers
                    .iter()
                    .filter(|t| string::matches_wildcard(text(f, 0), &t.name))
                    .map(|t| t.name.clone())
                    .collect()
            });
            with_o(|o| {
                for n in &names {
                    let mut io = Io::new();
                    io.p[0] = f.ptr_arg(1);
                    io.s = n.as_ptr();
                    io.n = n.len();
                    o.go(op::ARR_PUSH_STR, io);
                }
            });
            f.ret(!names.is_empty());
        },
    );
    s.method(M, "int GetTimeStamp()", |f| {
        f.ret(with_map(this_map(f), |m| m.stamp))
    });
    s.method(M, "float GetElapsedTime(int alTimeStamp)", |f| {
        let st: i32 = f.arg(0);
        let diff = with_map(this_map(f), |m| m.stamp).wrapping_sub(st);
        let ups = with_o(|o| o.go(op::UPDATES_PER_SEC, Io::new()).0).unwrap_or(1);
        f.ret(diff as f32 / ups as f32);
    });
    s.method(M, "float GetTimerTime(const tString&in asName)", |f| {
        f.ret(with_map(this_map(f), |m| {
            m.find(text(f, 0)).map_or(0.0, |t| t.time)
        }));
    });
    s.method(
        M,
        "void SetTimerPaused(const tString&in asName, bool abX)",
        |f| {
            let p: bool = f.arg(1);
            with_map(this_map(f), |m| {
                m.timers
                    .iter_mut()
                    .filter(|t| t.name == text(f, 0))
                    .for_each(|t| t.paused = p)
            });
        },
    );
    s.method(
        M,
        "void SetTimerUserVarFloat(const tString&in asName, float afX)",
        |f| {
            with_map(this_map(f), |m| {
                m.find(text(f, 0))
                    .iter_mut()
                    .for_each(|t| t.user_float = f.arg(1))
            });
        },
    );
    s.method(
        M,
        "void SetTimerUserVarInt(const tString&in asName, int alX)",
        |f| {
            with_map(this_map(f), |m| {
                m.find(text(f, 0))
                    .iter_mut()
                    .for_each(|t| t.user_int = f.arg(1))
            });
        },
    );
    s.method(
        M,
        "void SetTimerUserVarString(const tString&in asName, const tString&in asX)",
        |f| {
            with_map(this_map(f), |m| {
                m.find(text(f, 0))
                    .iter_mut()
                    .for_each(|t| t.user_string = text(f, 1).to_vec())
            });
        },
    );
    s.method(
        M,
        "float GetTimerUserVarFloat(const tString&in asName)",
        |f| {
            f.ret(with_map(this_map(f), |m| {
                m.find(text(f, 0)).map_or(0.0, |t| t.user_float)
            }));
        },
    );
    s.method(M, "int GetTimerUserVarInt(const tString&in asName)", |f| {
        f.ret(with_map(this_map(f), |m| {
            m.find(text(f, 0)).map_or(0, |t| t.user_int)
        }));
    });
}

// C ABI

#[repr(C)]
pub struct Pending {
    map: *const u8,
    map_n: usize,
    start: *const u8,
    start_n: usize,
    transfer: *const u8,
    transfer_n: usize,
}

#[repr(C)]
pub struct TimerOut {
    name: *const u8,
    name_n: usize,
    func: *const u8,
    func_n: usize,
    user: *const u8,
    user_n: usize,
    time: f32,
    user_float: f32,
    length: f32,
    user_int: i32,
    paused: bool,
    live: bool,
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s9_preload_state() -> *mut Preload {
    PRELOAD.0.get()
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s9_pause_state() -> *mut Pause {
    PAUSE.0.get()
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s9_collide_match(a: u32, b: u32) -> bool {
    collide_match(a, b)
}

/// # Safety
/// `s` points at `n` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_s9_collide_flag(s: *const u8, n: usize) -> u32 {
    // SAFETY: caller contract.
    let spec = unsafe { std::slice::from_raw_parts(s, n) };
    req(|r| collide_flag(&mut r.groups, spec))
}

/// # Safety
/// `a`/`b` point at `an`/`bn` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_s9_request_map_change(
    a: *const u8,
    an: usize,
    b: *const u8,
    bn: usize,
    new_game: bool,
) {
    // SAFETY: caller contract.
    let (m, st) = unsafe {
        (
            std::slice::from_raw_parts(a, an),
            std::slice::from_raw_parts(b, bn),
        )
    };
    request_map_change(m, st);
    if new_game {
        req(|r| r.new_game = true);
    }
}

/// # Safety
/// `out` is writable; the pointers stay valid until the next request.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_s9_take_pending(out: *mut Pending) -> bool {
    req(|r| {
        if r.map.is_empty() {
            return false;
        }
        let map = std::mem::take(&mut r.map);
        r.out = [map.as_slice(), &r.start, &r.transfer].concat();
        let (a, b) = (map.len(), r.start.len());
        let base = r.out.as_ptr();
        // SAFETY: `out` is writable by contract; offsets lie inside `r.out`.
        unsafe {
            out.write(Pending {
                map: base,
                map_n: a,
                start: base.add(a),
                start_n: b,
                transfer: base.add(a + b),
                transfer_n: r.out.len() - a - b,
            });
        }
        true
    })
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s9_take_new_game() -> bool {
    req(|r| std::mem::take(&mut r.new_game))
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s9_take_fade() -> f32 {
    req(|r| r.fade_in.take().unwrap_or(-1.0))
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s9_set_transfer(on: bool) {
    req(|r| r.is_transfer = on);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s9_is_transfer() -> bool {
    req(|r| r.is_transfer)
}

/// # Safety
/// `s` points at `n` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_s9_set_preload_map(s: *const u8, n: usize) {
    // SAFETY: caller contract.
    let v = unsafe { std::slice::from_raw_parts(s, n) }.to_vec();
    req(|r| r.preload_map = v);
}

/// # Safety
/// `n` is writable; the pointer stays valid until the next call.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_s9_preload_map(n: *mut usize) -> *const u8 {
    req(|r| {
        // SAFETY: caller contract.
        unsafe { n.write(r.preload_map.len()) };
        r.preload_map.as_ptr()
    })
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s9_uwp_clear() {
    req(|r| r.update_when_paused.clear());
}

/// # Safety
/// `out` has room for `cap` pointers.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_s9_uwp_list(out: *mut usize, cap: usize) -> usize {
    req(|r| {
        let n = r.update_when_paused.len().min(cap);
        for (i, p) in r.update_when_paused.iter().take(n).enumerate() {
            // SAFETY: `i < cap` by contract.
            unsafe { out.add(i).write(*p) };
        }
        n
    })
}

/// # Safety
/// `engine` is a live `asIScriptEngine`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_s9_register_natives(engine: *mut c_void) {
    register(Engine(engine));
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s9_map_free(map: usize) {
    MAPS.with(|m| m.borrow_mut().remove(&map));
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s9_map_time(map: usize) -> f64 {
    with_map(map, |m| m.time)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s9_map_stamp(map: usize) -> i32 {
    with_map(map, |m| m.stamp)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s9_timers_update(map: usize, dt: f32) {
    let _ = with_ops(|o| timers_update(o, map, dt));
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s9_timer_count(map: usize) -> usize {
    with_map(map, |m| m.timers.len())
}

/// # Safety
/// `out` is writable; the strings stay valid until the timers change.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_s9_timer_get(map: usize, i: usize, out: *mut TimerOut) -> bool {
    with_map(map, |m| {
        let Some(t) = m.timers.get(i) else {
            return false;
        };
        // SAFETY: `out` is writable by contract.
        unsafe {
            out.write(TimerOut {
                name: t.name.as_ptr(),
                name_n: t.name.len(),
                func: t.func.as_ptr(),
                func_n: t.func.len(),
                user: t.user_string.as_ptr(),
                user_n: t.user_string.len(),
                time: t.time,
                user_float: t.user_float,
                length: t.length,
                user_int: t.user_int,
                paused: t.paused,
                live: m.live(t),
            });
        }
        true
    })
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s9_timers_clear(map: usize) {
    with_map(map, |m| m.timers.clear());
}

/// # Safety
/// The strings point at their lengths.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_s9_timer_push_back(map: usize, t: *const TimerOut) {
    // SAFETY: caller contract.
    let t = unsafe { &*t };
    // SAFETY: as above.
    let (name, func, user) = unsafe {
        (
            std::slice::from_raw_parts(t.name, t.name_n),
            std::slice::from_raw_parts(t.func, t.func_n),
            std::slice::from_raw_parts(t.user, t.user_n),
        )
    };
    with_map(map, |m| {
        m.push(
            Timer {
                id: 0,
                name: name.to_vec(),
                func: func.to_vec(),
                time: t.time,
                paused: t.paused,
                user_float: t.user_float,
                user_int: t.user_int,
                user_string: user.to_vec(),
                length: t.length,
                removed: false,
            },
            false,
        );
    });
}

#[cfg(test)]
#[path = "s9_tests.rs"]
mod tests;
