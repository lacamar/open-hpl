//! `cPhysics` module state (`OHPL_IMPL=rust`): world list, surface-data map, impact throttle, surface
//! file loading. Objects stay C++ behind one host op; the registry lock is never held across an op.

use super::surf::{self, SurfaceState};
use super::surface_xml::{self, SurfaceDef};
use super::world::ImpactCounts;
use super::wreg::{Wa, WorldOp};
use crate::obj::{Arena, Handle};
use std::collections::BTreeMap;
use std::sync::{LazyLock, Mutex, MutexGuard};

pub const PO_WORLD_NEW: i32 = 101;
pub const PO_DELETE_WORLD: i32 = 102;
pub const PO_SURFACE_NEW: i32 = 103;
pub const PO_DELETE_SURFACE: i32 = 104;
pub const PO_TO_MATERIAL: i32 = 105;
pub const PO_LOG_LOAD_ERROR: i32 = 106;
pub const PO_SURFACE_STATE: i32 = 107;
pub const PO_SURFACE_SET_STR: i32 = 108;

/// `cPhysics` scalars shared with the C++ accessors; layout mirrored by `OhplPhysState`.
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct PhysState {
    pub impact_duration: f32,
    pub max_impacts: i32,
    pub log: bool,
}

impl Default for PhysState {
    fn default() -> Self {
        Self {
            impact_duration: 0.4,
            max_impacts: 6,
            log: false,
        }
    }
}

#[derive(Default)]
struct PhysBook {
    op: Option<WorldOp>,
    me: usize,
    worlds: Vec<usize>,
    surfaces: BTreeMap<Vec<u8>, usize>,
    impacts: ImpactCounts,
}

static PHYS: LazyLock<Mutex<Arena<PhysBook>>> = LazyLock::new(|| Mutex::new(Arena::default()));

fn phys() -> Option<MutexGuard<'static, Arena<PhysBook>>> {
    PHYS.lock().ok()
}

fn with<R>(book: u64, f: impl FnOnce(&mut PhysBook) -> R) -> Option<R> {
    let h = Handle::from_bits(book)?;
    phys()?.get_mut(h).map(f)
}

fn op(book: u64, code: i32, mut p: [usize; 6], i0: i32) -> usize {
    let Some((Some(op), me)) = with(book, |b| (b.op, b.me)) else {
        return 0;
    };
    if code != PO_TO_MATERIAL && p[0] == 0 {
        p[0] = me;
    }
    let mut a = Wa {
        p,
        f: [0.0; 2],
        i: [i0, 0],
        s: std::ptr::null(),
        n: 0,
        rf: 0.0,
        rv: [0.0; 3],
    };
    // SAFETY: the op was registered with the module and outlives it; `a` is a live block.
    unsafe { op(code, &mut a) }
}

pub fn new(op: WorldOp, me: usize) -> u64 {
    phys()
        .and_then(|mut s| {
            s.insert(PhysBook {
                op: Some(op),
                me,
                ..PhysBook::default()
            })
        })
        .map_or(0, Handle::to_bits)
}

/// Worlds are deleted before the surface data, like the destructor.
pub fn free(book: u64) {
    let worlds = with(book, |b| std::mem::take(&mut b.worlds)).unwrap_or_default();
    for w in worlds {
        op(book, PO_DELETE_WORLD, [0, w, 0, 0, 0, 0], 0);
    }
    let surfaces = with(book, |b| std::mem::take(&mut b.surfaces)).unwrap_or_default();
    for s in surfaces.into_values() {
        op(book, PO_DELETE_SURFACE, [0, s, 0, 0, 0, 0], 0);
    }
    if let (Some(h), Some(mut s)) = (Handle::from_bits(book), phys()) {
        s.remove(h);
    }
}

pub fn create_world(book: u64, add_surface_data: bool) -> usize {
    let w = op(book, PO_WORLD_NEW, [0; 6], 0);
    with(book, |b| b.worlds.push(w));
    if add_surface_data {
        let all: Vec<usize> =
            with(book, |b| b.surfaces.values().copied().collect()).unwrap_or_default();
        for s in all {
            op(book, PO_TO_MATERIAL, [s, w, 0, 0, 0, 0], 0);
        }
    }
    w
}

pub fn destroy_world(book: u64, w: usize) {
    with(book, |b| {
        if let Some(p) = b.worlds.iter().position(|&x| x == w) {
            b.worlds.remove(p);
        }
    });
    op(book, PO_DELETE_WORLD, [0, w, 0, 0, 0, 0], 0);
}

/// `std::map::insert`: a duplicate name keeps the first surface; the new object is returned unregistered.
pub fn create_surface(book: u64, name: &[u8]) -> usize {
    let s = op(
        book,
        PO_SURFACE_NEW,
        [0, name.as_ptr() as usize, name.len(), 0, 0, 0],
        0,
    );
    with(book, |b| {
        b.surfaces.entry(name.to_vec()).or_insert(s);
    });
    s
}

pub fn get_surface(book: u64, name: &[u8]) -> usize {
    with(book, |b| b.surfaces.get(name).copied())
        .flatten()
        .unwrap_or(0)
}

pub fn surface_len(book: u64) -> usize {
    with(book, |b| b.surfaces.len()).unwrap_or(0)
}

pub fn surface_at(book: u64, i: usize) -> Option<(*const u8, usize, usize)> {
    with(book, |b| {
        b.surfaces
            .iter()
            .nth(i)
            .map(|(k, v)| (k.as_ptr(), k.len(), *v))
    })
    .flatten()
}

pub fn can_play(book: u64, max: i32) -> bool {
    with(book, |b| b.impacts.can_play(max)).unwrap_or(false)
}

pub fn add_impact(book: u64) {
    with(book, |b| b.impacts.add());
}

pub fn impact_count(book: u64) -> usize {
    with(book, |b| b.impacts.len()).unwrap_or(0)
}

pub fn update_impacts(book: u64, dt: f32, duration: f32) {
    with(book, |b| b.impacts.update(dt, duration));
}

fn set_str(book: u64, surface: usize, which: i32, s: &[u8]) {
    op(
        book,
        PO_SURFACE_SET_STR,
        [0, surface, s.as_ptr() as usize, s.len(), 0, 0],
        which,
    );
}

fn apply_def(book: u64, def: &SurfaceDef) {
    // SAFETY: the parser hands out strings that live for the call.
    let name = unsafe { def.name.bytes() };
    let s = create_surface(book, name);
    let st = op(book, PO_SURFACE_STATE, [0, s, 0, 0, 0, 0], 0) as *mut SurfaceState;
    // SAFETY: the host returns the state embedded in the live surface; nothing else touches it here.
    if let Some(st) = unsafe { st.as_mut() } {
        st.elasticity = def.elasticity;
        st.kinetic_friction = def.kinetic_friction;
        st.static_friction = def.static_friction;
        st.priority = def.priority;
        st.elasticity_mode = def.elasticity_mode;
        st.friction_mode = def.friction_mode;
        st.min_scrape_speed = def.min_scrape_speed;
        st.min_scrape_freq = def.min_scrape_freq;
        st.min_scrape_freq_speed = def.min_scrape_freq_speed;
        st.middle_scrape_speed = def.middle_scrape_speed;
        st.max_scrape_freq_speed = def.max_scrape_freq_speed;
        st.max_scrape_freq = def.max_scrape_freq;
        st.min_scrape_contacts = def.min_scrape_contacts;
        st.min_roll_speed = def.min_roll_speed;
        st.min_roll_freq = def.min_roll_freq;
        st.min_roll_volume = def.min_roll_volume;
        st.min_roll_freq_speed = def.min_roll_freq_speed;
        st.middle_roll_speed = def.middle_roll_speed;
        st.max_roll_freq_speed = def.max_roll_freq_speed;
        st.max_roll_freq = def.max_roll_freq;
        st.max_roll_volume = def.max_roll_volume;
        st.roll_axis_flags = def.roll_axis_flags;
    }
    // SAFETY: as above.
    unsafe {
        set_str(book, s, 0, def.user_var.bytes());
        set_str(book, s, 1, def.step_type.bytes());
        set_str(book, s, 2, def.scrape_sound.bytes());
        set_str(book, s, 3, def.roll_sound.bytes());
    }
    let rs = if st.is_null() {
        0
    } else {
        // SAFETY: as above.
        unsafe { (*st).rs }
    };
    for (list, n, hit) in [
        (def.impacts, def.n_impacts, false),
        (def.hits, def.n_hits, true),
    ] {
        for i in 0..n {
            // SAFETY: the parser's arrays hold `n` entries.
            let d = unsafe { *list.add(i) };
            let imp = surf::create_impact(rs, d.min_speed, hit);
            // SAFETY: as above.
            unsafe { surf::set_impact_strings(rs, imp, d.sound.bytes(), d.ps.bytes(), d.ps_prio) };
        }
    }
}

/// `cPhysics::LoadSurfaceData`: the file is read whole, parsed and every material becomes surface data.
pub fn load_surface(book: u64, path: &[u8]) -> bool {
    let read = std::str::from_utf8(path)
        .ok()
        .map(std::path::Path::new)
        .map(std::fs::read);
    let Some(Ok(data)) = read else {
        op(
            book,
            PO_LOG_LOAD_ERROR,
            [0, path.as_ptr() as usize, path.len(), 0, 0, 0],
            0,
        );
        return false;
    };
    let ok = surface_xml::parse(&data, true, |def| apply_def(book, def));
    if !ok {
        op(
            book,
            PO_LOG_LOAD_ERROR,
            [0, path.as_ptr() as usize, path.len(), 0, 0, 0],
            0,
        );
    }
    ok
}
