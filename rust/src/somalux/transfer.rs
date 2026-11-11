//! Map change, transfer and preload decisions (`SomaLux.cpp`, `cLuxMapHandler::PerformMapChange`).
//!
//! Official PerformMapChange carries the player and props only when the target map was preloaded;
//! otherwise the player gets the start area. Kept as is: a map change with both a start position
//! and a transfer area drops the transfer (bd open-hpl-dbi).

use crate::math::{
    Vector2f, Vector3f, angle_from_points_2d, matrix_inverse, matrix_mul, matrix_mul_vec,
    matrix_translate,
};
use crate::string;
use crate::types::Matrixf;

pub fn same_map(a: &[u8], b: &[u8]) -> bool {
    let key = |s: &[u8]| string::to_lower(&string::set_file_ext(string::file_name(s), b""));
    key(a) == key(b)
}

/// Unwrapped, in (-2pi, 0]: yaw limits set by map scripts clamp against it.
pub fn start_yaw(area: &Matrixf) -> f32 {
    -angle_from_points_2d(
        Vector2f { x: 0.0, y: 0.0 },
        Vector2f {
            x: area.m[0][2],
            y: area.m[2][2],
        },
    )
}

#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct Begin {
    /// The pending transfer area applies (the change had no start position).
    pub use_transfer: bool,
    pub preloaded: bool,
    /// The C++ side looks the transfer area up in the current map.
    pub lookup_area: bool,
}

pub fn begin(
    start_empty: bool,
    pending_transfer_empty: bool,
    has_map: bool,
    preload_map: &[u8],
    map: &[u8],
) -> Begin {
    let use_transfer = start_empty && !pending_transfer_empty;
    let preloaded = !preload_map.is_empty() && same_map(preload_map, map);
    Begin {
        use_transfer,
        preloaded,
        lookup_area: has_map && preloaded && use_transfer,
    }
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Rel {
    pub mtx_rel: Matrixf,
    pub inv: Matrixf,
    pub yaw_rel: f32,
    pub pitch: f32,
    pub active_size: i32,
}

pub struct Body {
    pub feet: Vector3f,
    pub yaw: f32,
    pub active_size: i32,
}

pub fn capture(area: &Matrixf, body: &Body, camera_pitch: Option<f32>) -> Rel {
    let inv = matrix_inverse(area);
    Rel {
        mtx_rel: matrix_translate(matrix_mul_vec(&inv, body.feet)),
        inv,
        yaw_rel: body.yaw - start_yaw(area),
        pitch: camera_pitch.unwrap_or(0.0),
        active_size: body.active_size,
    }
}

/// Whether a prop whose first body is at `pos` (world) is inside the transfer area of `size`.
pub fn carries(inv: &Matrixf, pos: Vector3f, size: Vector3f) -> bool {
    let v = matrix_mul_vec(inv, pos);
    !(v.x.abs() > size.x / 2.0 || v.y.abs() > size.y / 2.0 || v.z.abs() > size.z / 2.0)
}

#[derive(Clone, Copy, Debug, PartialEq)]
pub struct Arrival {
    pub feet: Vector3f,
    pub yaw: f32,
    pub crouching: bool,
}

pub fn arrive(new_area: &Matrixf, rel: &Rel) -> Arrival {
    let m = matrix_mul(new_area, &rel.mtx_rel);
    Arrival {
        feet: Vector3f {
            x: m.m[0][3],
            y: m.m[1][3],
            z: m.m[2][3],
        },
        yaw: start_yaw(new_area) + rel.yaw_rel,
        crouching: rel.active_size == 1,
    }
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum AfterLoad {
    LoadFailed,
    /// Start-area placement only.
    Done,
    /// Look the transfer area up in the new map and restore the carried state.
    Transfer,
}

pub fn after_load(loaded: bool, captured: bool, has_current_map: bool) -> AfterLoad {
    if !loaded {
        AfterLoad::LoadFailed
    } else if captured && has_current_map {
        AfterLoad::Transfer
    } else {
        AfterLoad::Done
    }
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Restore {
    Skip,
    Set,
    MakeDynamicThenSet,
}

pub fn restore_prop(found: bool, bodies_match: bool, is_static: bool) -> Restore {
    if !found || !bodies_match {
        Restore::Skip
    } else if is_static {
        Restore::MakeDynamicThenSet
    } else {
        Restore::Set
    }
}

/// Preload throttle state (`glPreloadPrio`, `PreloadUpdate` statics).
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct Preload {
    pub prio: i32,
    pub last_frame: u32,
    pub debt_ms: f64,
}

impl Default for Preload {
    fn default() -> Self {
        Preload {
            prio: 2,
            last_frame: !0,
            debt_ms: 0.0,
        }
    }
}

const BUDGET_MS: [u64; 5] = [0, 2, 4, 8, 25];

#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct Tick {
    /// An older preload is still held: take (and discard) it first.
    pub drop_old: bool,
    pub start: bool,
    pub step: bool,
    pub budget_ms: u64,
}

/// `want` is the requested preload map, `loaded` the one the loader holds.
pub fn preload_tick(
    st: &mut Preload,
    want: &[u8],
    loaded: &[u8],
    running: bool,
    frame: u32,
) -> Tick {
    let mut t = Tick::default();
    if !want.is_empty() && loaded != want {
        t.drop_old = !loaded.is_empty();
        t.start = true;
    }
    let running = running || t.start;
    if frame == st.last_frame || !running || st.prio <= 0 {
        return t;
    }
    st.last_frame = frame;
    let budget = BUDGET_MS
        .get(st.prio.clamp(0, 4) as usize)
        .copied()
        .unwrap_or(0);
    if st.debt_ms > 0.0 {
        st.debt_ms -= budget as f64;
        return t;
    }
    t.step = true;
    t.budget_ms = budget;
    t
}

/// After the slice: the time it overran its budget is paid back by skipping frames.
pub fn preload_stepped(st: &mut Preload, elapsed_ms: u64, budget_ms: u64) {
    st.debt_ms = (elapsed_ms as f64 - budget_ms as f64).max(0.0);
}

/// Mutable view `SomaTakePreloadedWorld` acts on.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Take {
    /// Nothing is preloading.
    None,
    /// Finish the load; keep the world when it is the requested map.
    Finish { same: bool },
}

pub fn take_plan(loaded: &[u8], map: &[u8]) -> Take {
    if loaded.is_empty() {
        Take::None
    } else {
        Take::Finish {
            same: same_map(loaded, map),
        }
    }
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Taken {
    /// Activate the world and hand it out with its load report.
    Use,
    /// Delete the pending entities and, when a world loaded, destroy it.
    Discard { destroy_world: bool },
}

pub fn taken(have_world: bool, same: bool) -> Taken {
    if have_world && same {
        Taken::Use
    } else {
        Taken::Discard {
            destroy_world: have_world,
        }
    }
}

pub fn preload_ready(loaded: &[u8], running: bool, map: &[u8]) -> bool {
    !loaded.is_empty() && !running && same_map(loaded, map)
}
