//! `cSurfaceData` storage and glue (`OHPL_IMPL=rust`): the scalar state is a repr(C) struct embedded in
//! the C++ object (members are references into it), impact/hit lists and defaults live here.

use super::host::Str;
use super::surface::SurfaceParams;
use super::wreg::{Wa, WorldOp};
use std::sync::{LazyLock, Mutex, MutexGuard};

use crate::obj::{Arena, Handle};

pub const SO_DELETE_IMPACT: i32 = 201;
pub const SO_NEW_IMPACT: i32 = 202;
pub const SO_IMPACT_SET_SOUND: i32 = 203;
pub const SO_IMPACT_SET_PS: i32 = 204;
pub const SO_IMPACT_SET_PRIO: i32 = 205;
pub const SO_PRELOAD_SOUND: i32 = 206;
pub const SO_PRELOAD_PS: i32 = 207;
pub const SO_STR: i32 = 208;
pub const SO_IMPACT_STR: i32 = 209;
pub const SO_MAT_GET: i32 = 210;
pub const SO_MAT_CREATE: i32 = 211;
pub const SO_MAT_SETF: i32 = 212;
pub const SO_MAT_SETMODE: i32 = 213;
pub const SO_MAT_SET_SURFACE: i32 = 214;

/// Scalar surface properties; layout mirrored by `OhplSurfaceState` in `SurfaceData.h`.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct SurfaceState {
    pub rs: u64,
    pub friction_mode: i32,
    pub elasticity_mode: i32,
    pub elasticity: f32,
    pub static_friction: f32,
    pub kinetic_friction: f32,
    pub priority: i32,
    pub min_scrape_speed: f32,
    pub min_scrape_freq: f32,
    pub min_scrape_freq_speed: f32,
    pub max_scrape_freq: f32,
    pub max_scrape_freq_speed: f32,
    pub middle_scrape_speed: f32,
    pub min_scrape_contacts: i32,
    pub min_roll_speed: f32,
    pub min_roll_freq: f32,
    pub min_roll_volume: f32,
    pub min_roll_freq_speed: f32,
    pub max_roll_freq: f32,
    pub max_roll_volume: f32,
    pub max_roll_freq_speed: f32,
    pub middle_roll_speed: f32,
    pub min_roll_contacts: i32,
    pub roll_axis_flags: u32,
}

const _: () = assert!(std::mem::size_of::<SurfaceState>() == 104);

impl SurfaceState {
    fn f32_mut(&mut self, id: i32) -> Option<&mut f32> {
        Some(match id {
            0 => &mut self.elasticity,
            1 => &mut self.static_friction,
            2 => &mut self.kinetic_friction,
            3 => &mut self.min_scrape_speed,
            4 => &mut self.min_scrape_freq,
            5 => &mut self.min_scrape_freq_speed,
            6 => &mut self.max_scrape_freq,
            7 => &mut self.max_scrape_freq_speed,
            8 => &mut self.middle_scrape_speed,
            9 => &mut self.min_roll_speed,
            10 => &mut self.min_roll_freq,
            11 => &mut self.min_roll_volume,
            12 => &mut self.min_roll_freq_speed,
            13 => &mut self.max_roll_freq,
            14 => &mut self.max_roll_volume,
            15 => &mut self.max_roll_freq_speed,
            16 => &mut self.middle_roll_speed,
            _ => return None,
        })
    }

    pub fn set_f32(&mut self, id: i32, v: f32) {
        if let Some(f) = self.f32_mut(id) {
            *f = v;
        }
    }

    pub fn get_f32(&mut self, id: i32) -> f32 {
        self.f32_mut(id).map_or(0.0, |f| *f)
    }

    pub fn set_i32(&mut self, id: i32, v: i32) {
        match id {
            0 => self.priority = v,
            1 => self.friction_mode = v,
            2 => self.elasticity_mode = v,
            3 => self.min_scrape_contacts = v,
            4 => self.roll_axis_flags = v as u32,
            5 => self.min_roll_contacts = v,
            _ => {}
        }
    }

    pub fn get_i32(&self, id: i32) -> i32 {
        match id {
            0 => self.priority,
            1 => self.friction_mode,
            2 => self.elasticity_mode,
            3 => self.min_scrape_contacts,
            4 => self.roll_axis_flags as i32,
            _ => self.min_roll_contacts,
        }
    }

    /// The constructor defaults (`cSurfaceData::cSurfaceData`); the rest starts at zero.
    pub fn with_defaults(rs: u64) -> Self {
        Self {
            rs,
            elasticity: 0.5,
            static_friction: 0.3,
            kinetic_friction: 0.3,
            min_scrape_speed: 0.6,
            min_scrape_freq: 0.7,
            min_scrape_freq_speed: 1.0,
            max_scrape_freq: 2.0,
            max_scrape_freq_speed: 3.0,
            middle_scrape_speed: 2.0,
            ..Self::default()
        }
    }
}

#[derive(Default)]
struct SurfBook {
    op: Option<WorldOp>,
    me: usize,
    impacts: Vec<(usize, f32)>,
    hits: Vec<(usize, f32)>,
}

static SURF: LazyLock<Mutex<Arena<SurfBook>>> = LazyLock::new(|| Mutex::new(Arena::default()));

fn surf() -> Option<MutexGuard<'static, Arena<SurfBook>>> {
    SURF.lock().ok()
}

fn with<R>(rs: u64, f: impl FnOnce(&mut SurfBook) -> R) -> Option<R> {
    let h = Handle::from_bits(rs)?;
    surf()?.get_mut(h).map(f)
}

fn op(rs: u64, code: i32, p: [usize; 6], f0: f32, i0: i32) -> Option<Wa> {
    let (op, me) = with(rs, |b| (b.op, b.me))?;
    let mut a = Wa {
        p,
        f: [f0, 0.0],
        i: [i0, 0],
        s: std::ptr::null(),
        n: 0,
        rf: 0.0,
        rv: [0.0; 3],
    };
    a.p[0] = if a.p[0] == 0 { me } else { a.p[0] };
    // SAFETY: the op was registered with the surface and outlives it; `a` is a live block.
    let r = unsafe { op?(code, &mut a) };
    a.p[5] = r;
    Some(a)
}

/// Starts a surface: allocates its list record and returns the initial state.
pub fn new(op: WorldOp, me: usize) -> SurfaceState {
    let rs = surf()
        .and_then(|mut s| {
            s.insert(SurfBook {
                op: Some(op),
                me,
                ..SurfBook::default()
            })
        })
        .map_or(0, Handle::to_bits);
    SurfaceState::with_defaults(rs)
}

/// Deletes the impact and hit data (impacts first), then the record.
pub fn free(rs: u64) {
    let lists = with(rs, |b| (b.impacts.clone(), b.hits.clone()));
    if let Some((impacts, hits)) = lists {
        for (p, _) in impacts.into_iter().chain(hits) {
            op(rs, SO_DELETE_IMPACT, [0, p, 0, 0, 0, 0], 0.0, 0);
        }
    }
    if let (Some(h), Some(mut s)) = (Handle::from_bits(rs), surf()) {
        s.remove(h);
    }
}

pub fn create_impact(rs: u64, min_speed: f32, hit: bool) -> usize {
    let Some(a) = op(rs, SO_NEW_IMPACT, [0; 6], min_speed, 0) else {
        return 0;
    };
    let p = a.p[5];
    with(rs, |b| {
        if hit { &mut b.hits } else { &mut b.impacts }.push((p, min_speed));
    });
    p
}

pub fn impact_at(rs: u64, hit: bool, i: i32) -> usize {
    with(rs, |b| {
        let l = if hit { &b.hits } else { &b.impacts };
        usize::try_from(i)
            .ok()
            .and_then(|i| l.get(i))
            .map_or(0, |e| e.0)
    })
    .unwrap_or(0)
}

pub fn impact_num(rs: u64, hit: bool) -> i32 {
    with(rs, |b| {
        i32::try_from(if hit { b.hits.len() } else { b.impacts.len() }).unwrap_or(0)
    })
    .unwrap_or(0)
}

/// First entry whose minimum speed is not above `speed` (lists are stored fastest first).
pub fn impact_from_speed(rs: u64, hit: bool, speed: f32) -> usize {
    with(rs, |b| {
        let l = if hit { &b.hits } else { &b.impacts };
        l.iter().find(|e| speed >= e.1).map_or(0, |e| e.0)
    })
    .unwrap_or(0)
}

pub fn set_impact_strings(rs: u64, imp: usize, sound: &[u8], ps: &[u8], prio: i32) {
    op(
        rs,
        SO_IMPACT_SET_SOUND,
        [0, imp, sound.as_ptr() as usize, sound.len(), 0, 0],
        0.0,
        0,
    );
    op(
        rs,
        SO_IMPACT_SET_PS,
        [0, imp, ps.as_ptr() as usize, ps.len(), 0, 0],
        0.0,
        0,
    );
    op(rs, SO_IMPACT_SET_PRIO, [0, imp, 0, 0, 0, 0], 0.0, prio);
}

fn surf_str(rs: u64, which: i32) -> &'static [u8] {
    op(rs, SO_STR, [0; 6], 0.0, which).map_or(&[], |a| {
        // SAFETY: the host returns a member string of the live surface.
        unsafe { Str { p: a.s, n: a.n }.bytes() }
    })
}

fn impact_str(rs: u64, imp: usize, which: i32) -> &'static [u8] {
    op(rs, SO_IMPACT_STR, [0, imp, 0, 0, 0, 0], 0.0, which).map_or(&[], |a| {
        // SAFETY: the host returns a member string of the live impact data.
        unsafe { Str { p: a.s, n: a.n }.bytes() }
    })
}

fn preload(rs: u64, ps: bool, s: &[u8]) {
    if !s.is_empty() {
        let code = if ps { SO_PRELOAD_PS } else { SO_PRELOAD_SOUND };
        op(rs, code, [0, s.as_ptr() as usize, s.len(), 0, 0, 0], 0.0, 0);
    }
}

/// `cSurfaceData::PreloadData`: roll and scrape sounds, then every impact entry, then every hit entry.
pub fn preload_data(rs: u64) {
    preload(rs, false, surf_str(rs, 3));
    preload(rs, false, surf_str(rs, 2));
    for hit in [false, true] {
        for i in 0..impact_num(rs, hit) {
            let imp = impact_at(rs, hit, i);
            preload(rs, false, impact_str(rs, imp, 0));
            preload(rs, true, impact_str(rs, imp, 1));
        }
    }
}

/// `cSurfaceData::ToMaterial`: finds or creates the world's material and copies the properties.
pub fn to_material(rs: u64, st: &SurfaceState, name: &[u8], world: usize) -> usize {
    let n = [name.as_ptr() as usize, name.len()];
    let mut mat = op(rs, SO_MAT_GET, [0, world, n[0], n[1], 0, 0], 0.0, 0).map_or(0, |a| a.p[5]);
    if mat == 0 {
        mat = op(rs, SO_MAT_CREATE, [0, world, n[0], n[1], 0, 0], 0.0, 0).map_or(0, |a| a.p[5]);
    }
    for (which, v) in [
        (0, st.elasticity),
        (1, st.kinetic_friction),
        (2, st.static_friction),
    ] {
        op(rs, SO_MAT_SETF, [0, mat, 0, 0, 0, 0], v, which);
    }
    for (which, v) in [(0, st.elasticity_mode), (1, st.friction_mode)] {
        op(
            rs,
            SO_MAT_SETMODE,
            [0, mat, 0, 0, 0, 0],
            0.0,
            which | (v << 8),
        );
    }
    op(rs, SO_MAT_SET_SURFACE, [0, mat, 0, 0, 0, 0], 0.0, 0);
    mat
}

pub fn params(
    st: &SurfaceState,
    me: usize,
    physics: usize,
    scrape: Str,
    roll: Str,
) -> SurfaceParams {
    SurfaceParams {
        this: me,
        physics,
        min_scrape_speed: st.min_scrape_speed,
        min_scrape_freq: st.min_scrape_freq,
        min_scrape_freq_speed: st.min_scrape_freq_speed,
        max_scrape_freq: st.max_scrape_freq,
        max_scrape_freq_speed: st.max_scrape_freq_speed,
        middle_scrape_speed: st.middle_scrape_speed,
        min_scrape_contacts: st.min_scrape_contacts,
        min_roll_speed: st.min_roll_speed,
        min_roll_freq: st.min_roll_freq,
        min_roll_volume: st.min_roll_volume,
        min_roll_freq_speed: st.min_roll_freq_speed,
        max_roll_freq: st.max_roll_freq,
        max_roll_volume: st.max_roll_volume,
        max_roll_freq_speed: st.max_roll_freq_speed,
        middle_roll_speed: st.middle_roll_speed,
        roll_axis_flags: st.roll_axis_flags,
        scrape_sound: scrape,
        roll_sound: roll,
    }
}
