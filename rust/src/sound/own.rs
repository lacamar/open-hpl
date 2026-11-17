//! Owned sound objects: the handler with its entries and global setting tables, the sound entity
//! data records and the sound entities. C++ keeps only the channels, the resource managers, the
//! physics world and the game's event objects, reached through `SndHost`.

use super::channel::{CChannel, Channel, ChannelOps, EntryHost};
use super::entity_data::{self, LoadError, Names, Props};
use super::entry::{EntryState, Step};
use super::play::{self, PlayParams};
use crate::engine::multi_settings::{self as ms, Entry as GsEntry, SlotAction};
use crate::scene::soundentity::{self, PlayResult, SoundCfg, SoundHost, SoundState};
use crate::string::{float_to_string, int_to_string};
use crate::types::{Matrixf, Vector3f};
use crate::xml::Doc;
use std::collections::VecDeque;
use std::ffi::c_void;
use std::io::Write;
use std::sync::atomic::{AtomicPtr, Ordering};
use std::sync::{Mutex, OnceLock};

pub const ENTRY_TYPE_WORLD: u32 = 1;
pub const ENTRY_TYPE_GUI: u32 = 2;

pub const LOG_ERROR: i32 = 0;
pub const LOG_WARNING: i32 = 1;

/// Calls on an `iSoundEntityEvent` (a game object); every entry takes the event first.
#[repr(C)]
pub struct EventOps {
    pub play: unsafe extern "C" fn(*mut c_void),
    pub stop: unsafe extern "C" fn(*mut c_void, bool),
    pub fade_in: unsafe extern "C" fn(*mut c_void, f32),
    pub fade_out: unsafe extern "C" fn(*mut c_void, f32),
    pub is_stopped: unsafe extern "C" fn(*mut c_void) -> bool,
    pub update: unsafe extern "C" fn(*mut c_void, f32),
    pub destroy: unsafe extern "C" fn(*mut c_void),
}

/// The C++ objects the sound layer still reaches: channels, resources, physics, the entity shell.
#[repr(C)]
pub struct SndHost {
    pub channel: *const ChannelOps,
    pub event: *const EventOps,
    pub create_channel:
        unsafe extern "C" fn(*mut c_void, *const u8, usize, i32, bool, *mut bool) -> *mut c_void,
    pub delete_channel: unsafe extern "C" fn(*mut c_void),
    pub is_blocked: unsafe extern "C" fn(*mut c_void, *mut c_void, *const Vector3f) -> bool,
    pub listener_position: unsafe extern "C" fn(*mut c_void, *mut Vector3f),
    pub listener_matrix: unsafe extern "C" fn(*mut c_void, *mut Matrixf),
    pub listener_axes: unsafe extern "C" fn(*mut c_void, *mut [f32; 6]),
    pub app_time: unsafe extern "C" fn() -> u64,
    pub log: unsafe extern "C" fn(i32, *const u8, usize),
    pub preload: unsafe extern "C" fn(*mut c_void, *const u8, usize),
    pub entity_pos: unsafe extern "C" fn(*mut c_void, *mut Vector3f),
    pub entity_name: unsafe extern "C" fn(*mut c_void, *mut usize) -> *const u8,
    pub world_is_emitter: unsafe extern "C" fn(*mut c_void) -> bool,
    pub data_release: unsafe extern "C" fn(*mut c_void),
    pub on_start: unsafe extern "C" fn(*mut c_void),
    pub create_stream: unsafe extern "C" fn(*mut c_void, *const u8, usize, bool) -> *mut c_void,
    pub set_lock: unsafe extern "C" fn(*mut c_void, *const u8, usize, f32, bool),
}

fn type_slot(entry_type: u32) -> Option<usize> {
    (entry_type != 0).then(|| entry_type.trailing_zeros() as usize)
}

/// `cSoundEntry`; `cSoundEntry*` in C++ is the address of this box.
pub struct Rec {
    pub state: EntryState,
    pub name: Vec<u8>,
    pub channel: *mut c_void,
    pub id: i32,
    pub callback: *mut Entity,
    handler: *mut Handler,
}

struct Core {
    host: *const SndHost,
    lowlevel: *mut c_void,
    resources: *mut c_void,
    world: *mut c_void,
    silent: bool,
    volume: [f32; 32],
    speed: [f32; 32],
    id_count: i32,
    count: i32,
}

impl Core {
    pub(super) fn host(&self) -> &SndHost {
        // SAFETY: `ohpl_sndh_new` contract: the table outlives the handler.
        unsafe { &*self.host }
    }

    fn channel_ops(&self) -> &ChannelOps {
        // SAFETY: the host table points at a static channel table.
        unsafe { &*self.host().channel }
    }

    fn log(&self, level: i32, msg: &[u8]) {
        // SAFETY: table contract.
        unsafe { (self.host().log)(level, msg.as_ptr(), msg.len()) }
    }

    pub(super) fn listener_pos(&self) -> Vector3f {
        let mut v = Vector3f::default();
        // SAFETY: table contract.
        unsafe { (self.host().listener_position)(self.lowlevel, &raw mut v) };
        v
    }
}

impl EntryHost for Core {
    fn global_speed(&self, entry_type: u32) -> f32 {
        type_slot(entry_type).map_or(1.0, |i| self.speed.get(i).copied().unwrap_or(1.0))
    }
    fn global_volume(&self, entry_type: u32) -> f32 {
        type_slot(entry_type).map_or(1.0, |i| self.volume.get(i).copied().unwrap_or(1.0))
    }
    fn silent(&self) -> bool {
        self.silent
    }
    fn is_blocked(&mut self, pos: Vector3f) -> bool {
        // SAFETY: table contract.
        unsafe { (self.host().is_blocked)(self.world, self.lowlevel, &raw const pos) }
    }
    fn listener_position(&self) -> Vector3f {
        self.listener_pos()
    }
    fn listener_matrix(&self) -> Matrixf {
        let mut m = Matrixf::default();
        // SAFETY: table contract.
        unsafe { (self.host().listener_matrix)(self.lowlevel, &raw mut m) };
        m
    }
}

pub struct PlayArgs<'a> {
    pub name: &'a [u8],
    pub looping: bool,
    pub volume: f32,
    pub pos: Vector3f,
    pub min_dist: f32,
    pub max_dist: f32,
    pub entry_type: u32,
    pub relative: bool,
    pub is_3d: bool,
    pub priority_modifier: i32,
    pub stream: bool,
}

pub struct Handler {
    core: Core,
    #[allow(clippy::vec_box)] // C++ holds `cSoundEntry*`: records keep their address
    entries: Vec<Box<Rec>>,
    settings: [Vec<Option<GsEntry>>; 2],
    recent: VecDeque<Vec<u8>>,
}

fn dump_file() -> Option<&'static Mutex<std::fs::File>> {
    static F: OnceLock<Option<Mutex<std::fs::File>>> = OnceLock::new();
    F.get_or_init(|| {
        let path = std::env::var_os("OHPL_ESND_DUMP")?;
        std::fs::File::create(path).ok().map(Mutex::new)
    })
    .as_ref()
}

impl Handler {
    /// # Safety
    /// `host` outlives the handler.
    pub unsafe fn new(host: *const SndHost, lowlevel: *mut c_void, resources: *mut c_void) -> Self {
        Self {
            core: Core {
                host,
                lowlevel,
                resources,
                world: std::ptr::null_mut(),
                silent: false,
                volume: [1.0; 32],
                speed: [1.0; 32],
                id_count: 0,
                count: 0,
            },
            entries: Vec::new(),
            settings: [Vec::new(), Vec::new()],
            recent: VecDeque::new(),
        }
    }

    pub fn lowlevel(&self) -> *mut c_void {
        self.core.lowlevel
    }

    pub fn set_world(&mut self, world: *mut c_void) {
        self.core.world = world;
    }

    pub fn silent(&self) -> bool {
        self.core.silent
    }

    pub fn set_silent(&mut self, silent: bool) {
        self.core.silent = silent;
    }

    pub fn global_volume(&self, entry_type: u32) -> f32 {
        self.core.global_volume(entry_type)
    }

    pub fn global_speed(&self, entry_type: u32) -> f32 {
        self.core.global_speed(entry_type)
    }

    pub fn len(&self) -> usize {
        self.entries.len()
    }

    pub fn is_empty(&self) -> bool {
        self.entries.is_empty()
    }

    pub fn entry_at(&mut self, i: usize) -> *mut Rec {
        self.entries
            .get_mut(i)
            .map_or(std::ptr::null_mut(), |r| &raw mut **r)
    }

    pub fn recent(&self) -> &VecDeque<Vec<u8>> {
        &self.recent
    }

    pub fn is_valid(&self, rec: *const Rec, id: i32) -> bool {
        self.entries
            .iter()
            .find(|r| std::ptr::eq(&***r, rec))
            .is_some_and(|r| r.id == id)
    }

    fn find(&mut self, name: &[u8]) -> *mut Rec {
        let lower = name.to_ascii_lowercase();
        self.entries
            .iter_mut()
            .find(|r| r.name == lower)
            .map_or(std::ptr::null_mut(), |r| &raw mut **r)
    }

    fn ch<'a>(&self, rec: &Rec) -> CChannel<'a> {
        // SAFETY: a record's channel is live for the table until the record is freed.
        unsafe { CChannel::new(&*(*self.core.host).channel, rec.channel) }
    }

    pub fn stop(&mut self, name: &[u8]) -> bool {
        let p = self.find(name);
        if p.is_null() {
            return false;
        }
        // SAFETY: `p` is a live record of this handler.
        self.stop_rec(unsafe { &mut *p });
        true
    }

    pub fn is_playing(&mut self, name: &[u8]) -> bool {
        let p = self.find(name);
        // SAFETY: `p` is null or a live record of this handler.
        !p.is_null() && self.ch(unsafe { &*p }).is_playing()
    }

    pub fn stop_rec(&self, rec: &mut Rec) {
        let mut ch = self.ch(rec);
        rec.state.stop(&mut ch);
    }

    pub fn for_each(&mut self, mask: u32, mut f: impl FnMut(&Self, &mut Rec)) {
        let mut i = 0;
        while i < self.entries.len() {
            let Some(p) = self.entries.get_mut(i).map(|r| &raw mut **r) else {
                break;
            };
            // SAFETY: boxed records keep their address; `f` does not remove entries.
            let rec = unsafe { &mut *p };
            if rec.state.entry_type & mask != 0 {
                f(self, rec);
            }
            i += 1;
        }
    }

    pub fn set_paused(&self, rec: &Rec, paused: bool) {
        self.ch(rec).set_paused(paused);
    }

    fn tab(&mut self, kind: usize) -> &mut Vec<Option<GsEntry>> {
        let [volume, speed] = &mut self.settings;
        if kind == 0 { volume } else { speed }
    }

    fn tab_ref(&self, kind: usize) -> &[Option<GsEntry>] {
        let [volume, speed] = &self.settings;
        if kind == 0 { volume } else { speed }
    }

    fn settings_slot(&mut self, kind: usize, id: i32, force: bool) -> Option<&mut GsEntry> {
        let v = self.tab(kind);
        let idx = usize::try_from(id).ok();
        let occupied = idx.and_then(|i| v.get(i)).is_some_and(Option::is_some);
        match ms::get_slot_action(id, v.len(), occupied, force) {
            SlotAction::Null => return None,
            SlotAction::Existing => {}
            SlotAction::Create => *v.get_mut(idx?)? = Some(fresh_entry()),
            SlotAction::ResizeAndCreate => {
                v.resize(idx?.checked_add(1)?, None);
                *v.get_mut(idx?)? = Some(fresh_entry());
            }
        }
        v.get_mut(idx?)?.as_mut()
    }

    pub fn setting_entry(&mut self, kind: usize, id: i32, force: bool) -> *mut GsEntry {
        self.settings_slot(kind, id, force)
            .map_or(std::ptr::null_mut(), |e| &raw mut *e)
    }

    pub fn setting_exists(&self, kind: usize, id: i32) -> bool {
        usize::try_from(id)
            .ok()
            .and_then(|i| self.tab_ref(kind).get(i))
            .is_some_and(Option::is_some)
    }

    pub fn setting_destroy(&mut self, kind: usize, id: i32) {
        if let Some(s) = usize::try_from(id)
            .ok()
            .and_then(|i| self.tab(kind).get_mut(i))
        {
            *s = None;
        }
    }

    pub fn setting_calc(&self, kind: usize, types: u32, calc: i32, default: f32) -> f32 {
        ms::calc_results(self.tab_ref(kind), types, calc, default)
    }

    pub fn setting_create(&mut self, kind: usize) -> i32 {
        let v = self.tab(kind);
        let occupied: Vec<bool> = v.iter().map(Option::is_some).collect();
        let i = ms::create_slot(&occupied);
        if let Some(slot) = v.get_mut(i) {
            *slot = Some(fresh_entry());
        } else {
            v.push(Some(fresh_entry()));
        }
        i32::try_from(i).unwrap_or(-1)
    }

    fn setting_target(&mut self, kind: usize, id: i32, types: u32) -> (i32, *mut GsEntry) {
        let id = if id < 0 {
            self.setting_create(kind)
        } else {
            id
        };
        let e = self.setting_entry(kind, id, true);
        // SAFETY: forced creation yields a live slot.
        unsafe { (*e).types = types };
        (id, e)
    }

    pub fn set_global(&mut self, kind: usize, val: f32, types: u32, id: i32) -> i32 {
        let (id, e) = self.setting_target(kind, id, types);
        // SAFETY: as `setting_target`.
        unsafe { (*e).set_val_and_dest(val) };
        id
    }

    pub fn fade_global(
        &mut self,
        kind: usize,
        dest: f32,
        speed: f32,
        types: u32,
        id: i32,
        destroy: bool,
    ) -> i32 {
        let (id, e) = self.setting_target(kind, id, types);
        // SAFETY: as `setting_target`.
        unsafe { (*e).fade_to(dest, speed, destroy) };
        id
    }

    pub fn update(&mut self, dt: f32) {
        let [volume, speed] = &self.settings;
        for (i, (v, sp)) in self
            .core
            .volume
            .iter_mut()
            .zip(self.core.speed.iter_mut())
            .enumerate()
        {
            let m = 1u32 << i;
            *v = ms::calc_results(volume, m, 1, 1.0);
            *sp = ms::calc_results(speed, m, 1, 1.0);
        }
        for s in &mut self.settings {
            for slot in s.iter_mut() {
                if slot.as_mut().is_some_and(|e| e.update(dt)) {
                    *slot = None;
                }
            }
        }
        let mut i = 0;
        while let Some(rec) = self.entries.get_mut(i) {
            let rec = &mut **rec;
            let alive = {
                let mut ch = CChannel::new_for(&self.core, rec.channel);
                match rec.state.update(dt, &mut ch, &mut self.core) {
                    Step::Alive => true,
                    Step::Finished { release } => {
                        if release && !rec.callback.is_null() {
                            // SAFETY: entities clear their callback before they are freed.
                            soundentity::priority_release(unsafe { &mut (*rec.callback).state });
                        }
                        ch.stop();
                        false
                    }
                }
            };
            if alive {
                i += 1;
            } else {
                let rec = self.entries.remove(i);
                self.free_rec(rec);
            }
        }
        self.core.count += 1;
        self.dump();
    }

    fn free_rec(&self, rec: Box<Rec>) {
        if !rec.channel.is_null() {
            let mut ch = self.ch(&rec);
            ch.stop();
            // SAFETY: table contract; the channel is not used afterwards.
            unsafe { (self.core.host().delete_channel)(rec.channel) };
        }
    }

    pub fn clear(&mut self) {
        for rec in std::mem::take(&mut self.entries) {
            self.free_rec(rec);
        }
    }

    pub fn play(&mut self, a: &PlayArgs, not_enough: &mut bool) -> *mut Rec {
        *not_enough = false;
        if a.name.is_empty() {
            return std::ptr::null_mut();
        }
        let listener = self.core.listener_pos();
        let dist_prio =
            play::distance_priority(a.pos, listener, a.min_dist, a.max_dist, a.is_3d, a.relative);
        let mut ne = false;
        // SAFETY: table contract.
        let channel = unsafe {
            (self.core.host().create_channel)(
                self.core.resources,
                a.name.as_ptr(),
                a.name.len(),
                dist_prio + a.priority_modifier,
                a.stream,
                &raw mut ne,
            )
        };
        if channel.is_null() {
            *not_enough = ne;
            let text = |pre: &str, post: &str| [pre.as_bytes(), a.name, post.as_bytes()].concat();
            if ne {
                self.core.log(
                    LOG_WARNING,
                    &text("Could not start sound '", "', too many sounds playing!\n"),
                );
            } else {
                self.core
                    .log(LOG_ERROR, &text("Can't find sound '", "'!\n"));
            }
            return std::ptr::null_mut();
        }
        let params = PlayParams {
            pos: a.pos,
            min_dist: a.min_dist,
            max_dist: a.max_dist,
            priority_modifier: a.priority_modifier,
            entry_type: a.entry_type,
            looping: a.looping,
            relative: a.relative,
            is_3d: a.is_3d,
            silent: self.core.silent,
        };
        let lm = self.core.listener_matrix();
        play::setup_channel(
            &mut CChannel::new_for(&self.core, channel),
            &params,
            dist_prio,
            &lm,
        );
        let mut rec = Box::new(Rec {
            state: EntryState::new(a.volume, a.entry_type, a.is_3d, false),
            name: a.name.to_ascii_lowercase(),
            channel,
            id: self.core.id_count,
            callback: std::ptr::null_mut(),
            handler: self,
        });
        let p = &raw mut *rec;
        self.entries.push(rec);
        self.core.id_count = self.core.id_count.wrapping_add(1);

        // SAFETY: table contract.
        let time = unsafe { (self.core.host().app_time)() } as i32;
        let dist = if a.is_3d && !a.relative {
            play::dist(a.pos, listener)
        } else {
            play::dist(a.pos, Vector3f::default())
        };
        let mut line = int_to_string(time, 0);
        for part in [
            b"|".to_vec(),
            a.name.to_vec(),
            b"|".to_vec(),
            float_to_string(a.volume, 20, false),
            format!(
                "|{}|{}|{}|",
                u8::from(a.looping),
                u8::from(a.is_3d),
                u8::from(a.relative)
            )
            .into_bytes(),
            float_to_string(dist, 20, false),
            b"|".to_vec(),
            float_to_string(a.min_dist, 20, false),
            b"|".to_vec(),
            float_to_string(a.max_dist, 20, false),
        ] {
            line.extend(part);
        }
        self.recent.push_back(line);
        if self.recent.len() > 256 {
            self.recent.pop_front();
        }
        p
    }

    fn dump(&mut self) {
        let Some(file) = dump_file() else { return };
        let mut out = format!("f {} n={}\n", self.core.count, self.entries.len()).into_bytes();
        // printf prints NaN as "nan"/"-nan" by sign
        let fx = |x: f32, prec: usize| {
            if x.is_nan() {
                (if x.is_sign_negative() { "-nan" } else { "nan" }).to_string()
            } else {
                format!("{x:.prec$}")
            }
        };
        for rec in &self.entries {
            let mut ch = self.ch(rec);
            let p = ch.position();
            out.extend(b"e ");
            out.extend(&rec.name);
            out.extend(
                format!(
                    " t={} id={} ch={} vol={} paused={} loop={} stop={} mul={} spd={} 3d={} pos={},{},{}\n",
                    rec.state.entry_type,
                    rec.id,
                    u8::from(ch.is_playing()),
                    fx(ch.volume(), 4),
                    u8::from(ch.paused()),
                    u8::from(ch.looping()),
                    u8::from(rec.state.stop_disabled),
                    fx(rec.state.volume_mul, 4),
                    fx(ch.speed(), 4),
                    u8::from(rec.state.is_3d),
                    fx(p.x, 3),
                    fx(p.y, 3),
                    fx(p.z, 3)
                )
                .into_bytes(),
            );
        }
        if let Ok(mut f) = file.lock() {
            let _ = f.write_all(&out).and_then(|()| f.flush());
        }
    }
}

fn fresh_entry() -> GsEntry {
    GsEntry {
        val: 1.0,
        dest: 1.0,
        speed: 0.0,
        types: 0,
        destroy_at_dest: false,
    }
}

impl<'a> CChannel<'a> {
    fn new_for(core: &Core, ch: *mut c_void) -> Self {
        // SAFETY: the host table's channel table is static; `ch` is a live channel.
        unsafe { CChannel::new(&*core.host().channel, ch) }
    }
}

impl Rec {
    pub fn set_filter(&self, which: i32, gain: f32) {
        self.chan().set_filter(which, gain);
    }

    pub fn set_paused(&self, paused: bool) {
        self.chan().set_paused(paused);
    }

    pub fn total_time(&self) -> f64 {
        self.chan().total_time()
    }

    pub fn set_looping(&self, looping: bool) {
        self.chan().set_looping(looping);
    }

    pub fn set_position(&self, pos: Vector3f) {
        self.chan().set_position(pos);
    }

    pub fn handler(&self) -> *mut Handler {
        self.handler
    }

    pub fn channel_ops(&self) -> &ChannelOps {
        // SAFETY: the owning handler outlives its records.
        unsafe { (*self.handler).core.channel_ops() }
    }

    pub(super) fn chan(&self) -> CChannel<'_> {
        // SAFETY: as `channel_ops`; the record's channel is live.
        unsafe { CChannel::new(self.channel_ops(), self.channel) }
    }
}

/// `cSoundEntityData`.
pub struct Data {
    names: Names,
    previous: [i32; 3],
    pub fade_start: bool,
    pub fade_stop: bool,
    pub volume: f32,
    pub max_distance: f32,
    pub min_distance: f32,
    pub stream: bool,
    pub looping: bool,
    pub use_3d: bool,
    pub blockable: bool,
    pub block_volume_mul: f32,
    pub random: f32,
    pub interval: f32,
    pub priority: i32,
}

static LAZY_LOAD: std::sync::atomic::AtomicBool = std::sync::atomic::AtomicBool::new(false);

pub fn set_lazy_load(lazy: bool) {
    LAZY_LOAD.store(lazy, std::sync::atomic::Ordering::Relaxed);
}

impl Default for Data {
    fn default() -> Self {
        Self {
            names: Names::default(),
            previous: [-1; 3],
            fade_start: false,
            fade_stop: false,
            volume: 1.0,
            max_distance: 0.0,
            min_distance: 0.0,
            stream: false,
            looping: false,
            use_3d: true,
            blockable: false,
            block_volume_mul: 0.0,
            random: 1.0,
            interval: 0.0,
            priority: 0,
        }
    }
}

impl Data {
    pub fn num(&self, ty: usize) -> usize {
        self.names.get(ty).map_or(0, Vec::len)
    }

    pub fn has(&self, ty: usize) -> bool {
        self.num(ty) > 0
    }

    pub fn name(&self, ty: usize, idx: i32) -> &[u8] {
        let size = i32::try_from(self.num(ty)).unwrap_or(0);
        entity_data::name_index(size, idx)
            .and_then(|i| self.names.get(ty)?.get(i))
            .map_or(&[], Vec::as_slice)
    }

    pub fn random_name(&mut self, ty: usize, skip_previous: bool) -> &[u8] {
        let size = i32::try_from(self.num(ty)).unwrap_or(0);
        let prev = self.previous.get(ty).copied().unwrap_or(-1);
        let Some((idx, remember)) = entity_data::random_index(size, prev, skip_previous) else {
            return &[];
        };
        if remember && let Some(p) = self.previous.get_mut(ty) {
            *p = idx;
        }
        usize::try_from(idx)
            .ok()
            .and_then(|i| self.names.get(ty)?.get(i))
            .map_or(&[], Vec::as_slice)
    }

    pub fn set_name(&mut self, ty: usize, idx: i32, name: &[u8]) {
        let size = i32::try_from(self.num(ty)).unwrap_or(0);
        if let Some(i) = entity_data::name_index(size, idx)
            && let Some(s) = self.names.get_mut(ty).and_then(|v| v.get_mut(i))
        {
            *s = name.to_vec();
        }
    }

    /// What the SOMA event layer sets on the data it builds for an event.
    pub fn setup(&mut self, min_distance: f32, max_distance: f32, looping: bool, use_3d: bool) {
        self.volume = 1.0;
        self.min_distance = min_distance;
        self.max_distance = max_distance;
        self.looping = looping;
        self.use_3d = use_3d;
        self.stream = false;
        self.fade_start = false;
        self.fade_stop = false;
    }

    pub fn add_name(&mut self, ty: usize, name: &[u8]) {
        if let Some(v) = self.names.get_mut(ty) {
            v.push(name.to_vec());
        }
    }

    pub fn load(&mut self, doc: Option<&Doc>) -> i32 {
        let Some(doc) = doc else { return 1 };
        let result = entity_data::parse(doc, &mut self.names);
        match result {
            Ok(p) => {
                self.apply(&p);
                0
            }
            Err(LoadError::Xml) => 1,
            Err(LoadError::NoRoot) => 2,
            Err(LoadError::NoSounds) => 3,
            Err(LoadError::NoProperties) => 4,
        }
    }

    fn apply(&mut self, p: &Props) {
        self.use_3d = p.use_3d;
        self.looping = p.looping;
        self.stream = p.stream;
        self.blockable = p.blockable;
        self.block_volume_mul = p.block_volume_mul;
        self.volume = p.volume;
        self.max_distance = p.max_distance;
        self.min_distance = p.min_distance;
        self.fade_start = p.fade_start;
        self.fade_stop = p.fade_stop;
        self.random = p.random;
        self.interval = p.interval;
        self.priority = p.priority;
    }

    fn cfg(&self) -> SoundCfg {
        SoundCfg {
            interval: self.interval,
            random: self.random,
            looping: self.looping,
            use_3d: self.use_3d,
            has_start: self.has(1),
        }
    }

    /// # Safety
    /// `resources` is the resource manager the host table expects.
    pub unsafe fn preload(&self, host: &SndHost, resources: *mut c_void) {
        if self.stream || LAZY_LOAD.load(std::sync::atomic::Ordering::Relaxed) {
            return;
        }
        for list in &self.names {
            for name in list {
                // SAFETY: table contract.
                unsafe { (host.preload)(resources, name.as_ptr(), name.len()) };
            }
        }
    }
}

/// `cSoundEntity`'s state; the C++ object keeps the `iEntity3D` base and holds this by pointer.
pub struct Entity {
    pub state: SoundState,
    handler: *mut Handler,
    data: *mut Data,
    data_shell: *mut c_void,
    shell: *mut c_void,
    world: *mut c_void,
    entries: [*mut Rec; 3],
    ids: [i32; 3],
    pub creation_id: i32,
    pub force_gui: bool,
    pub min_distance: f32,
    pub max_distance: f32,
    pub volume: f32,
    event: *mut c_void,
}

impl Entity {
    /// # Safety
    /// `handler` and `data` outlive the entity; `shell` is the `cSoundEntity`.
    pub unsafe fn new(
        handler: *mut Handler,
        data: *mut Data,
        data_shell: *mut c_void,
        shell: *mut c_void,
        world: *mut c_void,
        remove_when_over: bool,
        creation_id: i32,
    ) -> Box<Self> {
        // SAFETY: caller contract.
        let (h, d) = unsafe { (&*handler, &*data) };
        // SAFETY: the handler's resources pointer is the one the host table expects.
        unsafe { d.preload(h.core.host(), h.core.resources) };
        let silent = h.core.silent;
        Box::new(Self {
            state: soundentity::init(&d.cfg(), remove_when_over, silent),
            handler,
            data,
            data_shell,
            shell,
            world,
            entries: [std::ptr::null_mut(); 3],
            ids: [-1; 3],
            creation_id,
            force_gui: false,
            min_distance: d.min_distance,
            max_distance: d.max_distance,
            volume: if silent { 0.0 } else { d.volume },
            event: std::ptr::null_mut(),
        })
    }

    fn host(&self) -> &SndHost {
        // SAFETY: the handler outlives its entities.
        unsafe { (*self.handler).core.host() }
    }

    pub fn data_ptr(&self) -> *mut Data {
        self.data
    }

    pub fn set_event(&mut self, event: *mut c_void) {
        self.event = event;
    }

    pub fn event(&self) -> *mut c_void {
        self.event
    }

    pub fn entry(&self, ty: usize, check_valid: bool) -> *mut Rec {
        let e = self
            .entries
            .get(ty)
            .copied()
            .unwrap_or(std::ptr::null_mut());
        if check_valid && !e.is_null() && !self.valid(ty) {
            return std::ptr::null_mut();
        }
        e
    }

    fn valid(&self, ty: usize) -> bool {
        // SAFETY: handler outlives the entity.
        unsafe {
            (*self.handler).is_valid(
                self.entries
                    .get(ty)
                    .copied()
                    .unwrap_or(std::ptr::null_mut()),
                self.ids.get(ty).copied().unwrap_or(-1),
            )
        }
    }

    pub fn is_stopped(&self) -> bool {
        // SAFETY: the event is live while set.
        if !self.event.is_null() {
            return unsafe { ((*self.host().event).is_stopped)(self.event) };
        }
        soundentity::is_stopped(&self.state, !self.entries[2].is_null())
    }
}

/// Field access through the raw entity pointer: no `&mut Entity` exists while the state machine
/// runs, because it holds `&mut state` and calls back into this context.
macro_rules! f {
    ($c:expr, $field:ident) => {
        // SAFETY: the entity is live for the call.
        unsafe { (*$c.0).$field }
    };
}

struct Ctx(*mut Entity);

impl Ctx {
    fn host(&self) -> &SndHost {
        // SAFETY: the handler outlives the entity.
        unsafe { (*f!(self, handler)).core.host() }
    }

    fn handler<'a>(&self) -> &'a mut Handler {
        // SAFETY: the handler outlives the entity; its calls here do not re-enter the entity.
        unsafe { &mut *f!(self, handler) }
    }

    fn rec<'a>(&self, t: i32) -> Option<&'a mut Rec> {
        let p = *f!(self, entries).get(usize::try_from(t).ok()?)?;
        // SAFETY: callers check validity first, as the original did.
        unsafe { p.as_mut() }
    }

    fn entry_at(&self, t: i32) -> *mut Rec {
        usize::try_from(t)
            .ok()
            .and_then(|t| f!(self, entries).get(t).copied())
            .unwrap_or(std::ptr::null_mut())
    }

    fn valid_at(&self, t: i32) -> bool {
        let id = usize::try_from(t)
            .ok()
            .and_then(|t| f!(self, ids).get(t).copied())
            .unwrap_or(-1);
        self.handler().is_valid(self.entry_at(t), id)
    }

    fn set_entry(&self, t: i32, p: *mut Rec) {
        // SAFETY: the entity is live.
        if let Some(s) = usize::try_from(t)
            .ok()
            .and_then(|t| unsafe { (*self.0).entries.get_mut(t) })
        {
            *s = p;
        }
    }

    fn pos(&self) -> Vector3f {
        let mut p = Vector3f::default();
        // SAFETY: table contract.
        unsafe { (self.host().entity_pos)(f!(self, shell), &raw mut p) };
        p
    }
}

impl SoundHost for Ctx {
    fn silent(&mut self) -> bool {
        self.handler().core.silent
    }
    fn has_entry(&mut self, t: i32) -> bool {
        !self.entry_at(t).is_null()
    }
    fn is_valid(&mut self, t: i32) -> bool {
        self.valid_at(t)
    }
    fn clear_entry(&mut self, t: i32) {
        self.set_entry(t, std::ptr::null_mut());
    }
    fn stop_entry(&mut self, t: i32) {
        if let Some(r) = self.rec(t) {
            self.handler().stop_rec(r);
        }
    }
    fn fade_in(&mut self, t: i32, speed: f32) {
        if let Some(r) = self.rec(t) {
            r.state.fade_in(1.0, speed);
        }
    }
    fn fade_out(&mut self, t: i32, speed: f32) {
        if let Some(r) = self.rec(t) {
            r.state.fade_out(speed);
        }
    }
    fn update_position(&mut self, t: i32) {
        let p = self.pos();
        if let Some(r) = self.rec(t) {
            let mut ch = r.chan();
            if !ch.position_is_relative() {
                ch.set_position(p);
            }
        }
    }
    fn play_sound(&mut self, t: i32, looping: bool) -> PlayResult {
        let Ok(ty) = usize::try_from(t) else {
            return PlayResult::NoName;
        };
        // SAFETY: the data record outlives the entity; the borrow ends here.
        let (name, use_3d, priority, stream, blockable, block_mul) = unsafe {
            let d = &mut *f!(self, data);
            (
                d.random_name(ty, true).to_vec(),
                d.use_3d,
                d.priority,
                d.stream,
                d.blockable,
                d.block_volume_mul,
            )
        };
        if name.is_empty() {
            return PlayResult::NoName;
        }
        let route = crate::scene::h12b_ent::snd_play_route(f!(self, force_gui), use_3d);
        let range = route & 4 != 0;
        let pos = if range {
            self.pos()
        } else {
            Vector3f {
                x: 0.0,
                y: 0.0,
                z: 1.0,
            }
        };
        let mut ne = false;
        let args = PlayArgs {
            name: &name,
            looping,
            volume: f!(self, volume),
            pos,
            min_dist: if range { f!(self, min_distance) } else { 100.0 },
            max_dist: if range { f!(self, max_distance) } else { 200.0 },
            entry_type: if route & 1 != 0 {
                ENTRY_TYPE_GUI
            } else {
                ENTRY_TYPE_WORLD
            },
            relative: route & 2 != 0,
            is_3d: true,
            priority_modifier: priority,
            stream,
        };
        let rec = self.handler().play(&args, &mut ne);
        self.set_entry(t, rec);
        // SAFETY: `rec` is null or a live record of the handler.
        let Some(r) = (unsafe { rec.as_mut() }) else {
            if !ne {
                let mut n = 0usize;
                // SAFETY: table contract; the name is valid for `n` bytes.
                let en = unsafe {
                    let p = (self.host().entity_name)(f!(self, shell), &raw mut n);
                    std::slice::from_raw_parts(p, n)
                };
                let msg = [
                    b"Couldn't play sound '".as_slice(),
                    &name,
                    b"' for sound entity ",
                    en,
                    b"\n",
                ]
                .concat();
                // SAFETY: table contract.
                unsafe { (self.host().log)(LOG_ERROR, msg.as_ptr(), msg.len()) };
            }
            return PlayResult::Failed {
                not_enough_channels: ne,
            };
        };
        // SAFETY: the entity is live.
        if let Some(id) = unsafe { (*self.0).ids.get_mut(ty) } {
            *id = r.id;
        }
        let mut ch = r.chan();
        ch.set_blockable(blockable);
        ch.set_block_volume_mul(block_mul);
        ch.set_priority_modifier(priority);
        if ty == 0 {
            r.callback = self.0;
        }
        PlayResult::Played
    }
    fn on_start(&mut self) {
        // SAFETY: table contract.
        unsafe { (self.host().on_start)(f!(self, shell)) };
    }
    fn emitter(&mut self) -> bool {
        // SAFETY: table contract.
        unsafe { (self.host().world_is_emitter)(f!(self, world)) }
    }
    fn listener_sqr_dist(&mut self) -> f32 {
        let l = self.handler().core.listener_pos();
        super::entry::dist_sqr(l, self.pos())
    }
}

/// The entity operations; `e` is live and not borrowed. The event object may re-enter the
/// entity, so no reference to it is kept across an event call.
///
/// # Safety
/// `e` is a live entity.
pub unsafe fn ent_play(e: *mut Entity, play_start: bool) {
    let c = Ctx(e);
    let ev = f!(c, event);
    if !ev.is_null() {
        // SAFETY: the entity is live; the event is live while set.
        unsafe {
            (*e).state.stopped = false;
            ((*c.host().event).play)(ev);
        }
        return;
    }
    let cfg = Ctx::cfg(&c);
    // SAFETY: the state machine only reaches other fields, through `c`.
    soundentity::play(unsafe { &mut (*e).state }, &mut Ctx(e), &cfg, play_start);
}

/// # Safety
/// As `ent_play`.
pub unsafe fn ent_stop(e: *mut Entity, play_end: bool) {
    let c = Ctx(e);
    let ev = f!(c, event);
    if !ev.is_null() {
        // SAFETY: as `ent_play`.
        unsafe {
            (*e).state.stopped = true;
            ((*c.host().event).stop)(ev, play_end);
        }
        return;
    }
    let cfg = c.cfg();
    // SAFETY: as `ent_play`.
    soundentity::stop(unsafe { &mut (*e).state }, &mut Ctx(e), &cfg, play_end);
}

/// # Safety
/// As `ent_play`.
pub unsafe fn ent_fade_in(e: *mut Entity, speed: f32) {
    let c = Ctx(e);
    let ev = f!(c, event);
    if !ev.is_null() {
        // SAFETY: as `ent_play`.
        unsafe {
            (*e).state.stopped = false;
            (*e).state.fading_out = false;
            ((*c.host().event).fade_in)(ev, speed);
        }
        return;
    }
    let cfg = c.cfg();
    // SAFETY: as `ent_play`.
    soundentity::fade_in(unsafe { &mut (*e).state }, &mut Ctx(e), &cfg, speed);
}

/// # Safety
/// As `ent_play`.
pub unsafe fn ent_fade_out(e: *mut Entity, speed: f32) {
    let c = Ctx(e);
    let ev = f!(c, event);
    if !ev.is_null() {
        // SAFETY: as `ent_play`.
        unsafe {
            (*e).state.fading_out = true;
            ((*c.host().event).fade_out)(ev, speed);
        }
        return;
    }
    // SAFETY: as `ent_play`.
    soundentity::fade_out(unsafe { &mut (*e).state }, &mut Ctx(e), speed);
}

/// # Safety
/// As `ent_play`.
pub unsafe fn ent_update(e: *mut Entity, dt: f32) {
    let c = Ctx(e);
    let ev = f!(c, event);
    if !ev.is_null() {
        // SAFETY: as `ent_play`.
        unsafe { ((*c.host().event).update)(ev, dt) };
        return;
    }
    let (cfg, max) = (c.cfg(), f!(c, max_distance));
    // SAFETY: as `ent_play`.
    soundentity::update(unsafe { &mut (*e).state }, &mut Ctx(e), &cfg, dt, max);
}

impl Ctx {
    fn cfg(&self) -> SoundCfg {
        // SAFETY: the data record outlives the entity.
        unsafe { (*f!(self, data)).cfg() }
    }
}

/// `~cSoundEntity` minus the C++ base: the event, the entries it still owns, the data user count.
///
/// # Safety
/// `e` came from `Entity::new` and is not used afterwards.
pub unsafe fn ent_free(e: *mut Entity) {
    registry_remove(&ENTITIES, e as usize);
    let c = Ctx(e);
    let ev = f!(c, event);
    if !ev.is_null() {
        // SAFETY: the event is live while set.
        unsafe { ((*c.host().event).destroy)(ev) };
    }
    for t in 0..3 {
        if c.valid_at(t)
            && let Some(r) = c.rec(t)
        {
            r.callback = std::ptr::null_mut();
            c.handler().stop_rec(r);
        }
    }
    // SAFETY: table contract.
    unsafe { (c.host().data_release)(f!(c, data_shell)) };
    // SAFETY: caller contract.
    drop(unsafe { Box::from_raw(e) });
}

static HANDLER: AtomicPtr<Handler> = AtomicPtr::new(std::ptr::null_mut());
static ENTITIES: Mutex<Vec<(usize, usize)>> = Mutex::new(Vec::new());
static DATAS: Mutex<Vec<(usize, usize)>> = Mutex::new(Vec::new());

pub(super) fn set_handler(h: *mut Handler) {
    HANDLER.store(h, Ordering::Release);
}

/// The engine's sound handler, for game code that plays and fades sounds directly.
pub fn handler() -> Option<&'static mut Handler> {
    // SAFETY: the pointer is set from `ohpl_sndh_new` and cleared before the handler is freed;
    // the engine thread is the only user.
    unsafe { HANDLER.load(Ordering::Acquire).as_mut() }
}

fn registry_add(reg: &Mutex<Vec<(usize, usize)>>, shell: usize, rec: usize) {
    if let Ok(mut r) = reg.lock() {
        r.push((shell, rec));
    }
}

fn registry_remove(reg: &Mutex<Vec<(usize, usize)>>, rec: usize) {
    if let Ok(mut r) = reg.lock() {
        r.retain(|e| e.1 != rec);
    }
}

fn registry_get(reg: &Mutex<Vec<(usize, usize)>>, shell: usize) -> usize {
    reg.lock()
        .ok()
        .and_then(|r| r.iter().find(|e| e.0 == shell).map(|e| e.1))
        .unwrap_or(0)
}

pub(super) fn register_entity(shell: *mut c_void, e: *mut Entity) {
    registry_add(&ENTITIES, shell as usize, e as usize);
}

pub(super) fn register_data(shell: *mut c_void, d: *mut Data) {
    registry_add(&DATAS, shell as usize, d as usize);
}

pub(super) fn unregister_data(d: *mut Data) {
    registry_remove(&DATAS, d as usize);
}

/// The sound entity record of a `cSoundEntity*`, or null.
pub fn entity_of(shell: usize) -> *mut Entity {
    registry_get(&ENTITIES, shell) as *mut Entity
}

/// The entity data record of a `cSoundEntityData*`, or null.
pub fn data_of(shell: usize) -> *mut Data {
    registry_get(&DATAS, shell) as *mut Data
}

impl Handler {
    pub fn listener(&self) -> ([f32; 3], [f32; 3], [f32; 3]) {
        let p = self.core.listener_pos();
        let mut axes = [0.0f32; 6];
        // SAFETY: table contract.
        unsafe { (self.core.host().listener_axes)(self.core.lowlevel, &raw mut axes) };
        (
            [p.x, p.y, p.z],
            [axes[0], axes[1], axes[2]],
            [axes[3], axes[4], axes[5]],
        )
    }

    /// `cSoundHandler::FadeOutAll`: fades the entries whose type intersects `mask`.
    pub fn fade_out_all(&mut self, mask: u32, speed: f32, disable_stop: bool) {
        self.for_each(mask, |_, rec| {
            rec.state.fade_out(speed);
            if disable_stop {
                rec.state.stop_disabled = true;
            }
        });
    }

    pub fn setting_val(&mut self, kind: usize, id: i32) -> f32 {
        // SAFETY: a returned slot pointer is live until the table changes.
        unsafe { self.setting_entry(kind, id, false).as_ref() }.map_or(1.0, |e| e.val)
    }
}

impl Entity {
    pub fn world_position(&self) -> Vector3f {
        let mut p = Vector3f::default();
        // SAFETY: table contract.
        unsafe { (self.host().entity_pos)(self.shell, &raw mut p) };
        p
    }

    pub fn data(&self) -> &Data {
        // SAFETY: the data record outlives the entity.
        unsafe { &*self.data }
    }
}

/// The SOMA fade-in of an entity's main sound: starts it quietly, then fades the entry in (or
/// sets its volume when `speed` is not positive).
///
/// # Safety
/// As `ent_play`.
pub unsafe fn fade_main_in(e: *mut Entity, vol: f32, speed: f32) {
    // SAFETY: caller contract.
    unsafe { ent_play(e, false) };
    // SAFETY: the entity is live; a valid entry record is live in the handler.
    if let Some(r) = unsafe { (*e).entry(0, false).as_mut() } {
        if speed > 0.0 {
            r.state.fade_in(vol, speed);
        } else {
            r.state.volume_mul = vol;
            r.state.volume_fade_dest = vol;
        }
    }
}

impl Entity {
    /// Back to the data's own distances and volume.
    pub fn reset_props(&mut self) {
        // SAFETY: the data record outlives the entity.
        let d = unsafe { &*self.data };
        self.min_distance = d.min_distance;
        self.max_distance = d.max_distance;
        self.volume = d.volume;
    }
}

impl Handler {
    /// `cSoundHandler::PlayGui` and `PlayGuiStream`: a relative, non-3D sound at (0, 0, 1).
    pub fn play_gui(&mut self, name: &[u8], looping: bool, volume: f32, stream: bool) -> *mut Rec {
        let args = PlayArgs {
            name,
            looping,
            volume,
            pos: Vector3f {
                x: 0.0,
                y: 0.0,
                z: 1.0,
            },
            min_dist: 1.0,
            max_dist: 1000.0,
            entry_type: ENTRY_TYPE_GUI,
            relative: true,
            is_3d: false,
            priority_modifier: 0,
            stream,
        };
        self.play(&args, &mut false)
    }
}
