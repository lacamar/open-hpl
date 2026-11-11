//! C ABI for the Lux entity layer, mirrored in `soma/src/game/RustSomaEntity.h`.

use super::effects;
use super::geom::{self, Obb};
use super::logic::{self, GuiGate};
use super::motion::{self, Move, Rotate};
use super::procanim::{self, ProcAnim, Track};
use super::props::{self, Op, Vars};
use crate::string_ffi::units;
use crate::types::{Color, Matrixf, Vector3f};
use std::ffi::c_void;

const _: () = {
    use std::mem::{offset_of, size_of};
    assert!(size_of::<Move>() == 32 && offset_of!(Move, goal) == 4);
    assert!(size_of::<Rotate>() == 108 && offset_of!(Rotate, goal) == 4);
    assert!(size_of::<ProcAnim>() == 28 && offset_of!(ProcAnim, time) == 4);
};

/// # Safety
/// `p` is valid for reads.
unsafe fn rd<T: Copy>(p: *const T) -> T {
    // SAFETY: caller contract.
    unsafe { *p }
}

/// # Safety
/// `p` is valid for writes.
unsafe fn wr<T>(p: *mut T, v: T) {
    // SAFETY: caller contract.
    unsafe { p.write(v) }
}

/// # Safety
/// `p` is valid and exclusively borrowed.
unsafe fn rw<'a, T>(p: *mut T) -> &'a mut T {
    // SAFETY: caller contract.
    unsafe { &mut *p }
}

/// # Safety
/// `m` and the vectors are readable; `Move` is exclusively borrowed.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs1_move_start(
    m: *mut Move,
    goal: *const Vector3f,
    acc: f32,
    max_speed: f32,
    slowdown: f32,
    reset: bool,
) {
    // SAFETY: caller contract.
    unsafe { rw(m).start(rd(goal), acc, max_speed, slowdown, reset) }
}

/// 0: not moving, 1: `out` is the new position, 2: as 1 and the goal was reached.
///
/// # Safety
/// Pointers are valid; `m` is exclusively borrowed.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs1_move_step(
    m: *mut Move,
    dt: f32,
    pos: *const Vector3f,
    out: *mut Vector3f,
) -> i32 {
    // SAFETY: caller contract.
    match unsafe { rw(m).step(dt, rd(pos)) } {
        None => 0,
        Some((to, done)) => {
            // SAFETY: caller contract.
            unsafe { wr(out, to) };
            1 + i32::from(done)
        }
    }
}

/// # Safety
/// `m` is exclusively borrowed.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs1_move_stop(m: *mut Move) {
    // SAFETY: caller contract.
    unsafe { rw(m).stop() }
}

/// # Safety
/// Pointers are valid; `r` is exclusively borrowed.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs1_rotate_align(
    r: *mut Rotate,
    goal: *const Matrixf,
    acc: f32,
    max_speed: f32,
    slowdown: f32,
    reset: bool,
    pivot: *const Vector3f,
) {
    // SAFETY: caller contract.
    unsafe { rw(r).align_to(&rd(goal), acc, max_speed, slowdown, reset, rd(pivot)) }
}

/// # Safety
/// Pointers are valid; `r` is exclusively borrowed.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs1_rotate_speed(
    r: *mut Rotate,
    acc: f32,
    goal_speed: f32,
    axis: *const Vector3f,
    reset: bool,
    pivot: *const Vector3f,
) {
    // SAFETY: caller contract.
    unsafe { rw(r).at_speed(acc, goal_speed, rd(axis), reset, rd(pivot)) }
}

/// # Safety
/// `r` is exclusively borrowed.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs1_rotate_stop(r: *mut Rotate) {
    // SAFETY: caller contract.
    unsafe { rw(r).stop() }
}

/// 0: nothing to apply, 1: `out` is the matrix to set, 2: as 1 and the goal was reached.
///
/// # Safety
/// Pointers are valid; `r` is exclusively borrowed.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs1_rotate_step(
    r: *mut Rotate,
    dt: f32,
    m: *const Matrixf,
    out: *mut Matrixf,
) -> i32 {
    // SAFETY: caller contract.
    match unsafe { rw(r).step(dt, &rd(m)) } {
        None => 0,
        Some(o) => {
            // SAFETY: caller contract.
            unsafe { wr(out, o.matrix) };
            1 + i32::from(o.done)
        }
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gs1_static_move_enter(moving: bool, flag: bool, loop_present: bool) -> bool {
    motion::static_move_enter(moving, flag, loop_present)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gs1_static_move_plan(moving: bool, flag: bool, loop_alive: bool) -> i32 {
    motion::static_move_plan(moving, flag, loop_alive)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gs1_is_moving(
    moving: bool,
    rotate_mode: i32,
    rotate_max_speed: f32,
) -> bool {
    motion::is_moving(moving, rotate_mode, rotate_max_speed)
}

/// # Safety
/// The arrays hold `n` elements each.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs1_char_stopped_step(
    n: usize,
    collide: *const bool,
    default_collide: *const bool,
    lin_sqr: *const f32,
    ang_sqr: *const f32,
    clear: *mut bool,
) -> bool {
    let mut out = vec![false; n];
    // SAFETY: caller contract.
    let done = unsafe {
        motion::char_stopped_step(
            units(collide, n),
            units(default_collide, n),
            units(lin_sqr, n),
            units(ang_sqr, n),
            &mut out,
        )
    };
    for (i, c) in out.into_iter().enumerate() {
        // SAFETY: caller contract.
        unsafe { clear.add(i).write(c) };
    }
    done
}

/// # Safety
/// Pointers are valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs1_outside_push(
    body_pos: *const Vector3f,
    player_pos: *const Vector3f,
    lin_vel: *const Vector3f,
    out: *mut Vector3f,
) -> bool {
    // SAFETY: caller contract.
    match motion::outside_push(unsafe { rd(body_pos) }, unsafe { rd(player_pos) }, unsafe {
        rd(lin_vel)
    }) {
        Some(v) => {
            // SAFETY: caller contract.
            unsafe { wr(out, v) };
            true
        }
        None => false,
    }
}

/// # Safety
/// `alpha` is valid for reads and writes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs1_effects_fade_setup(
    on: bool,
    fade_time: f32,
    off_alpha: f32,
    alpha: *mut f32,
) -> f32 {
    // SAFETY: caller contract.
    unsafe { effects::fade_setup(on, fade_time, off_alpha, rw(alpha)) }
}

/// # Safety
/// Pointers are valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs1_effect_color(
    def: *const Color,
    base: *const Color,
    alpha: f32,
    out: *mut Color,
) {
    // SAFETY: caller contract.
    unsafe { wr(out, effects::effect_color(rd(def), rd(base), alpha)) }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gs1_effects_on(alpha: f32, active: bool) -> bool {
    effects::effects_on(alpha, active)
}

/// # Safety
/// `col` is valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs1_light_lit(col: *const Color, on: bool) -> bool {
    // SAFETY: caller contract.
    effects::light_lit(unsafe { rd(col) }, on)
}

/// # Safety
/// The float pointers are valid for reads and writes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs1_effects_update_fades(
    mesh_alpha: *mut f32,
    mesh_speed: f32,
    has_mesh: bool,
    alpha: *mut f32,
    speed: *mut f32,
    off_alpha: f32,
    dt: f32,
) -> u32 {
    // SAFETY: caller contract.
    unsafe {
        effects::update_fades(
            rw(mesh_alpha),
            mesh_speed,
            has_mesh,
            rw(alpha),
            rw(speed),
            off_alpha,
            dt,
        )
    }
}

/// # Safety
/// The float pointers are valid for reads and writes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs1_color_fade_begin(
    time: f32,
    color_time: *mut f32,
    color_t: *mut f32,
) -> bool {
    // SAFETY: caller contract.
    unsafe { effects::color_fade_begin(time, rw(color_time), rw(color_t)) }
}

/// # Safety
/// Pointers are valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs1_color_fade_step(
    from: *const Color,
    to: *const Color,
    color_time: *mut f32,
    color_t: *mut f32,
    dt: f32,
    out: *mut Color,
) -> bool {
    // SAFETY: caller contract.
    match unsafe { effects::color_fade_step(rd(from), rd(to), rw(color_time), rw(color_t), dt) } {
        Some(c) => {
            // SAFETY: caller contract.
            unsafe { wr(out, c) };
            true
        }
        None => false,
    }
}

/// # Safety
/// The float pointers are valid for reads and writes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs1_liquid_fade(
    target: f32,
    time: f32,
    amount: f32,
    cur_target: *mut f32,
    speed: *mut f32,
) -> bool {
    // SAFETY: caller contract.
    unsafe { effects::liquid_fade(target, time, amount, rw(cur_target), rw(speed)) }
}

/// # Safety
/// `amount` is valid for reads and writes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs1_liquid_update(
    dt: f32,
    amount: *mut f32,
    target: f32,
    speed: f32,
) -> i32 {
    // SAFETY: caller contract.
    unsafe { effects::liquid_update(dt, rw(amount), target, speed) }
}

/// # Safety
/// `a` is exclusively borrowed.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs1_proc_play(
    a: *mut ProcAnim,
    length: f32,
    looping: bool,
    amount_fade: f32,
    speed_fade: f32,
) {
    // SAFETY: caller contract.
    unsafe { rw(a).play(length, looping, amount_fade, speed_fade) }
}

/// # Safety
/// `a` is exclusively borrowed.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs1_proc_step(a: *mut ProcAnim, dt: f32) -> bool {
    // SAFETY: caller contract.
    unsafe { rw(a).step(dt) }
}

/// # Safety
/// Pointers are valid; `easing` holds `easing_len` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs1_proc_track(
    a: *const ProcAnim,
    rotate: bool,
    reverse: bool,
    axes: *const Vector3f,
    min: f32,
    max: f32,
    cycles: i32,
    easing: *const u8,
    easing_len: usize,
    out: *mut Matrixf,
) {
    // SAFETY: caller contract.
    unsafe {
        let t = Track {
            rotate,
            reverse,
            axes: rd(axes),
            min,
            max,
            cycles,
            easing: units(easing, easing_len),
        };
        wr(out, procanim::track_offset(&*a, &t));
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gs1_damage_amount(amount: f32, strength: i32, toughness: i32) -> f32 {
    logic::damage_amount(amount, strength, toughness)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gs1_health_breaks(health: f32, disable_breakable: bool) -> bool {
    logic::health_breaks(health, disable_breakable)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gs1_break_gate(
    breakable: bool,
    broken: bool,
    active: bool,
    speed: f32,
    has_map: bool,
    time: f64,
    start: f64,
) -> bool {
    logic::break_gate(breakable, broken, active, speed, has_map, time, start)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gs1_break_energy_exceeds(
    speed: f32,
    mass: f32,
    other_mass: f32,
    v1: f32,
    v2: f32,
    min_energy: f32,
) -> bool {
    logic::break_energy_exceeds(speed, mass, other_mass, v1, v2, min_energy)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gs1_anim_update_action(
    has_mesh: bool,
    current: i32,
    playing: bool,
    queued: bool,
    has_callback: bool,
) -> i32 {
    logic::anim_update_action(has_mesh, current, playing, queued, has_callback)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gs1_play_plan(
    has_mesh: bool,
    idx: i32,
    name_empty: bool,
    num_states: i32,
    fade: f32,
) -> i32 {
    logic::play_plan(has_mesh, idx, name_empty, num_states, fade)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gs1_stop_plan(
    has_mesh: bool,
    idx: i32,
    num_states: i32,
    current: i32,
    active: bool,
    fade: f32,
) -> i32 {
    logic::stop_plan(has_mesh, idx, num_states, current, active, fade)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gs1_max_interact_distance(own: f32, default: f32) -> f32 {
    logic::max_interact_distance(own, default)
}

/// Writes the state to send; false when the connection does not take `state`.
///
/// # Safety
/// `out` is writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs1_connection_state(
    state: i32,
    states_used: i32,
    invert: bool,
    out: *mut i32,
) -> bool {
    match logic::connection_state(state, states_used, invert) {
        Some(s) => {
            // SAFETY: caller contract.
            unsafe { wr(out, s) };
            true
        }
        None => false,
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gs1_interact_connection_input(t: f32, invert: bool) -> f32 {
    logic::interact_connection_input(t, invert)
}

/// # Safety
/// The strings hold their lengths in bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs1_body_name_matches(
    body: *const u8,
    body_len: usize,
    want: *const u8,
    want_len: usize,
) -> bool {
    // SAFETY: caller contract.
    unsafe { logic::body_name_matches(units(body, body_len), units(want, want_len)) }
}

/// # Safety
/// The float and bool pointers are valid for reads and writes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs1_gui_fade_step(
    fade: *mut f32,
    speed: *mut f32,
    active: *mut bool,
    dt: f32,
) {
    // SAFETY: caller contract.
    unsafe { logic::gui_fade_step(rw(fade), rw(speed), rw(active), dt) }
}

/// Returns false when the GUI update is skipped, else the step in `step`.
///
/// # Safety
/// `acc`, `dirty` and `step` are valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs1_gui_gate(
    focused: bool,
    fps: f32,
    update_out_of_view: bool,
    has_sub_mesh: bool,
    sub_mesh_rendered: bool,
    acc: *mut f32,
    dirty: *mut bool,
    dt: f32,
    step: *mut f32,
) -> bool {
    let g = GuiGate {
        focused,
        fps,
        update_out_of_view,
        has_sub_mesh,
        sub_mesh_rendered,
    };
    // SAFETY: caller contract.
    match logic::gui_gate(&g, unsafe { rw(acc) }, unsafe { rw(dirty) }, dt) {
        Some(s) => {
            // SAFETY: caller contract.
            unsafe { wr(step, s) };
            true
        }
        None => false,
    }
}

/// # Safety
/// Pointers are valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs1_aabb_to_obb(
    min: *const Vector3f,
    max: *const Vector3f,
    out: *mut Obb,
) {
    // SAFETY: caller contract.
    unsafe { wr(out, geom::aabb_to_obb(rd(min), rd(max))) }
}

/// # Safety
/// Pointers are valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs1_matrix_obb(
    m: *const Matrixf,
    min: *const Vector3f,
    max: *const Vector3f,
    out: *mut Obb,
) {
    // SAFETY: caller contract.
    unsafe { wr(out, geom::matrix_obb(&rd(m), rd(min), rd(max))) }
}

/// # Safety
/// Pointers are valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs1_point_in_obb(b: *const Obb, p: *const Vector3f) -> bool {
    // SAFETY: caller contract.
    geom::point_in_obb(&unsafe { rd(b) }, unsafe { rd(p) })
}

/// # Safety
/// Pointers are valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs1_obb_overlap(a: *const Obb, b: *const Obb) -> bool {
    // SAFETY: caller contract.
    geom::obb_overlap(&unsafe { rd(a) }, &unsafe { rd(b) })
}

/// # Safety
/// Pointers are valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs1_ray_hits_obb(
    b: *const Obb,
    start: *const Vector3f,
    dir: *const Vector3f,
    max_dist: f32,
    t: *mut f32,
) -> bool {
    // SAFETY: caller contract.
    match geom::ray_hits_obb(
        &unsafe { rd(b) },
        unsafe { rd(start) },
        unsafe { rd(dir) },
        max_dist,
    ) {
        Some(v) => {
            // SAFETY: caller contract.
            unsafe { wr(t, v) };
            true
        }
        None => false,
    }
}

/// # Safety
/// Pointers are valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs1_area_body(
    m: *const Matrixf,
    size: *const Vector3f,
    out: *mut Matrixf,
    out_size: *mut Vector3f,
) {
    // SAFETY: caller contract.
    unsafe {
        let (mtx, sz) = geom::area_body(&rd(m), rd(size));
        wr(out, mtx);
        wr(out_size, sz);
    }
}

/// # Safety
/// Pointers are valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs1_relative_matrix(
    base: *const Matrixf,
    m: *const Matrixf,
    out: *mut Matrixf,
) {
    // SAFETY: caller contract.
    unsafe { wr(out, geom::relative_matrix(&rd(base), &rd(m))) }
}

/// Writes the matrix to set and returns true, or returns false when nothing changes.
///
/// # Safety
/// Pointers are valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs1_attachment_step(
    parent: *const Matrixf,
    prev: *const Matrixf,
    offset: *const Matrixf,
    cur: *const Matrixf,
    use_rotation: bool,
    locked: bool,
    out: *mut Matrixf,
) -> bool {
    // SAFETY: caller contract.
    let r = unsafe {
        geom::attachment_step(
            &rd(parent),
            &rd(prev),
            &rd(offset),
            &rd(cur),
            use_rotation,
            locked,
        )
    };
    match r {
        Some(m) => {
            // SAFETY: caller contract.
            unsafe { wr(out, m) };
            true
        }
        None => false,
    }
}

/// # Safety
/// Pointers are valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs1_liquid_extent(b: *const Obb, out: *mut Vector3f) {
    // SAFETY: caller contract.
    unsafe { wr(out, geom::liquid_extent(&rd(b))) }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gs1_liquid_wave_height(
    surface_y: f32,
    amp: f32,
    freq: f32,
    time: f32,
    cx: f32,
    cz: f32,
) -> f32 {
    geom::liquid_wave_height(surface_y, amp, freq, time, cx, cz)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gs1_liquid_body_under(bv_max_y: f32, center_y: f32, ext_y: f32) -> bool {
    geom::liquid_body_under(bv_max_y, center_y, ext_y)
}

/// # Safety
/// `b` is valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs1_liquid_surface(b: *const Obb) -> f32 {
    // SAFETY: caller contract.
    geom::liquid_surface(&unsafe { rd(b) })
}

#[repr(C)]
pub struct VarsOps {
    pub get_string: unsafe extern "C" fn(
        *mut c_void,
        *const u8,
        usize,
        unsafe extern "C" fn(*mut c_void, *const u8, usize),
        *mut c_void,
    ),
    pub get_bool: unsafe extern "C" fn(*mut c_void, *const u8, usize, bool) -> bool,
    pub get_float: unsafe extern "C" fn(*mut c_void, *const u8, usize, f32) -> f32,
    pub get_int: unsafe extern "C" fn(*mut c_void, *const u8, usize, i32) -> i32,
}

#[repr(C)]
pub struct VarsRef {
    pub ops: *const VarsOps,
    pub ctx: *mut c_void,
}

unsafe extern "C" fn collect(ctx: *mut c_void, p: *const u8, n: usize) {
    // SAFETY: `ctx` is the `Vec` handed to `get_string` below; `p` holds `n` bytes.
    unsafe { (*ctx.cast::<Vec<u8>>()).extend_from_slice(units(p, n)) }
}

impl Vars for VarsRef {
    fn string(&self, name: &str) -> Vec<u8> {
        let mut out = Vec::new();
        // SAFETY: the C++ side keeps `ops` and `ctx` alive for the call.
        unsafe {
            ((*self.ops).get_string)(
                self.ctx,
                name.as_ptr(),
                name.len(),
                collect,
                std::ptr::from_mut(&mut out).cast(),
            );
        }
        out
    }

    fn boolean(&self, name: &str, def: bool) -> bool {
        // SAFETY: as in `string`.
        unsafe { ((*self.ops).get_bool)(self.ctx, name.as_ptr(), name.len(), def) }
    }

    fn float(&self, name: &str, def: f32) -> f32 {
        // SAFETY: as in `string`.
        unsafe { ((*self.ops).get_float)(self.ctx, name.as_ptr(), name.len(), def) }
    }

    fn int(&self, name: &str, def: i32) -> i32 {
        // SAFETY: as in `string`.
        unsafe { ((*self.ops).get_int)(self.ctx, name.as_ptr(), name.len(), def) }
    }
}

#[repr(C)]
pub struct OpOut {
    pub kind: u32,
    pub s0: *const u8,
    pub n0: usize,
    pub s1: *const u8,
    pub n1: usize,
    pub f0: f32,
    pub f1: f32,
    pub b0: bool,
    pub b1: bool,
    pub b2: bool,
    pub i0: i32,
}

impl OpOut {
    fn new(kind: u32) -> Self {
        Self {
            kind,
            s0: std::ptr::null(),
            n0: 0,
            s1: std::ptr::null(),
            n1: 0,
            f0: 0.0,
            f1: 0.0,
            b0: false,
            b1: false,
            b2: false,
            i0: 0,
        }
    }

    fn s0(mut self, s: &[u8]) -> Self {
        (self.s0, self.n0) = (s.as_ptr(), s.len());
        self
    }

    fn s1(mut self, s: &[u8]) -> Self {
        (self.s1, self.n1) = (s.as_ptr(), s.len());
        self
    }
}

fn to_out(op: &Op) -> OpOut {
    match *op {
        Op::InteractCallback { func, auto_remove } => OpOut {
            b0: auto_remove,
            ..OpOut::new(0).s0(func)
        },
        Op::LookAt {
            func,
            auto_remove,
            check_center,
            check_ray,
            max_distance,
            delay,
        } => OpOut {
            b0: auto_remove,
            b1: check_center,
            b2: check_ray,
            f0: max_distance,
            f1: delay,
            ..OpOut::new(1).s0(func)
        },
        Op::CollideCallback {
            child,
            func,
            to_player,
        } => OpOut {
            b0: to_player,
            ..OpOut::new(2).s0(child).s1(func)
        },
        Op::UserVar(s) => OpOut::new(3).s0(s),
        Op::ConnectionCallback(s) => OpOut::new(4).s0(s),
        Op::Connection {
            entity,
            invert,
            states_used,
        } => OpOut {
            b0: invert,
            i0: states_used,
            ..OpOut::new(5).s0(entity)
        },
        Op::Health(f) => OpOut {
            f0: f,
            ..OpOut::new(6)
        },
        Op::Breakable => OpOut::new(7),
        Op::MaxInteractDistance(f) => OpOut {
            f0: f,
            ..OpOut::new(8)
        },
        Op::InteractionDisabled => OpOut::new(9),
    }
}

/// Parses the instance variables of an entity; each op reaches `sink` in order, with strings valid
/// for the call only.
///
/// # Safety
/// `inst` and `vars` are live for the call; `sink` accepts `sink_ctx` and an `OpOut`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs1_apply_instance_vars(
    inst: *const VarsRef,
    vars: *const VarsRef,
    sink: unsafe extern "C" fn(*mut c_void, *const OpOut),
    sink_ctx: *mut c_void,
) {
    // SAFETY: caller contract.
    let (inst, vars) = unsafe { (&*inst, &*vars) };
    props::apply_instance_vars(inst, vars, &mut |op| {
        let out = to_out(&op);
        // SAFETY: caller contract; `out` outlives the call.
        unsafe { sink(sink_ctx, &out) };
    });
}
