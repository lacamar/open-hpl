//! C ABI of the ImGui port, mirrored in `soma/src/game/RustSomaS8d.h`: the host table the C++
//! engine shell fills in (gui set, textures, fonts, input actions, camera textures, script arrays)
//! and the entry points the remaining C++ callers use.

use super::imgui::State;
use super::s8d::{self, GfxDesc, Host, ImGui, Op, Ptr, Timer};
use super::s8d_hud;
use super::s8d_text::{SubAction, parse_string};
use crate::gui::g1_gfx::{self, Element};
use crate::gui::g1_set::{self, Clip, Set};
use crate::math::{Vector2f, Vector3f};
use crate::somascript::s2_as::Engine;
use crate::types::Color;
use std::ffi::c_void;
use std::sync::OnceLock;

type Bytes = unsafe extern "C" fn(*mut c_void, *const u8, usize);
type Words = unsafe extern "C" fn(*mut c_void, *const u32, usize);
type SubCb = unsafe extern "C" fn(*mut c_void, i32, i32, *const u8, usize);

#[repr(C)]
pub struct OhplS8dGfx {
    file: *const u8,
    file_n: usize,
    ty: i32,
    material: i32,
    uv_min: [f32; 2],
    uv_max: [f32; 2],
    alpha_limit: [f32; 2],
}

#[repr(C)]
pub struct OhplS8dDraw {
    text: *const u32,
    text_n: usize,
    pos: [f32; 3],
    size: [f32; 2],
    color: [f32; 4],
    material: i32,
    align: i32,
    angle: f32,
}

#[repr(C)]
pub struct OhplS8dHost {
    string: unsafe extern "C" fn(usize, Bytes, *mut c_void),
    intern: unsafe extern "C" fn(*const u8, usize) -> usize,
    gfx_element: unsafe extern "C" fn(*const OhplS8dGfx) -> usize,
    gfx_name: unsafe extern "C" fn(usize, Bytes, *mut c_void),
    white: unsafe extern "C" fn() -> usize,
    font: unsafe extern "C" fn(*const u8, usize) -> usize,
    font_name: unsafe extern "C" fn(usize, Bytes, *mut c_void),
    font_length: unsafe extern "C" fn(usize, *const f32, *const u32, usize) -> f32,
    font_wrap:
        unsafe extern "C" fn(usize, f32, f32, *const f32, *const u32, usize, Words, *mut c_void),
    draw_font: unsafe extern "C" fn(usize, usize, *const OhplS8dDraw),
    screen_size: unsafe extern "C" fn(*mut f32, *mut i32),
    create_set: unsafe extern "C" fn(*const u8, usize) -> usize,
    set_hud_virtual: unsafe extern "C" fn(usize, *const f32, *const f32),
    set_render: unsafe extern "C" fn(usize),
    wstr: unsafe extern "C" fn(usize, Words, *mut c_void),
    array_size: unsafe extern "C" fn(usize) -> usize,
    array_at: unsafe extern "C" fn(usize, usize) -> usize,
    array_resize: unsafe extern "C" fn(usize, usize),
    array_push_wstring: unsafe extern "C" fn(usize, *const u32, usize),
    array_push_bool: unsafe extern "C" fn(usize, bool),
    array_push_i32: unsafe extern "C" fn(usize, i32),
    action_subs: unsafe extern "C" fn(*const u8, usize, SubCb, *mut c_void) -> i32,
    translate_button: unsafe extern "C" fn(*const u8, usize, Words, *mut c_void),
    key_name: unsafe extern "C" fn(i32, Bytes, *mut c_void),
    string_duration: unsafe extern "C" fn(*const u32, usize) -> f32,
    camtex_create: unsafe extern "C" fn(*const u8, usize, i32, i32, u32, f32, f32, f32),
    camtex_destroy: unsafe extern "C" fn(*const u8, usize),
    camtex_settings: unsafe extern "C" fn(*const u8, usize, f32, f32, f32),
    camtex_matrix: unsafe extern "C" fn(*const u8, usize, usize),
    camtex_attach: unsafe extern "C" fn(*const u8, usize, usize),
}

static HOST: OnceLock<&'static OhplS8dHost> = OnceLock::new();

/// # Safety
/// `h` lives for the whole process.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_s8d_install(h: *const OhplS8dHost) {
    // SAFETY: caller contract.
    if let Some(h) = unsafe { h.as_ref() } {
        let _ = HOST.set(h);
    }
}

unsafe extern "C" fn push_bytes(ctx: *mut c_void, p: *const u8, n: usize) {
    // SAFETY: `ctx` is the `Vec<u8>` the caller passed; `p` covers `n` bytes.
    unsafe { (*ctx.cast::<Vec<u8>>()).extend_from_slice(slice(p, n)) }
}

unsafe extern "C" fn push_words(ctx: *mut c_void, p: *const u32, n: usize) {
    // SAFETY: as `push_bytes`.
    unsafe { (*ctx.cast::<Vec<u32>>()).extend_from_slice(slice(p, n)) }
}

unsafe extern "C" fn push_row(ctx: *mut c_void, p: *const u32, n: usize) {
    // SAFETY: as `push_bytes`.
    unsafe { (*ctx.cast::<Vec<Vec<u32>>>()).push(slice(p, n).to_vec()) }
}

unsafe extern "C" fn push_sub(ctx: *mut c_void, kind: i32, code: i32, p: *const u8, n: usize) {
    // SAFETY: `ctx` is the `Vec<SubAction>` the caller passed.
    unsafe {
        (*ctx.cast::<Vec<SubAction>>()).push(SubAction {
            kind,
            code,
            name: slice(p, n).to_vec(),
        });
    }
}

/// # Safety
/// `p` covers `n` elements (or `n` is 0).
unsafe fn slice<'a, T>(p: *const T, n: usize) -> &'a [T] {
    if n == 0 || p.is_null() {
        &[]
    } else {
        // SAFETY: caller contract.
        unsafe { std::slice::from_raw_parts(p, n) }
    }
}

struct CHost;
static CH: CHost = CHost;

fn t() -> &'static OhplS8dHost {
    match HOST.get() {
        Some(h) => h,
        None => std::process::abort(),
    }
}

fn draw_of(o: &Op) -> OhplS8dDraw {
    OhplS8dDraw {
        text: o.text.as_ptr(),
        text_n: o.text.len(),
        pos: o.pos,
        size: o.size,
        color: o.color,
        material: o.material,
        align: o.align,
        angle: o.angle,
    }
}

impl Host for CHost {
    fn string(&self, p: Ptr) -> Vec<u8> {
        let mut v = Vec::new();
        // SAFETY: plain host call with a live sink.
        unsafe { (t().string)(p, push_bytes, (&raw mut v).cast()) };
        v
    }
    fn intern(&self, s: &[u8]) -> Ptr {
        // SAFETY: `s` is valid for the call.
        unsafe { (t().intern)(s.as_ptr(), s.len()) }
    }
    fn gfx_element(&self, d: &GfxDesc<'_>) -> Ptr {
        let g = OhplS8dGfx {
            file: d.file.as_ptr(),
            file_n: d.file.len(),
            ty: d.ty,
            material: d.material,
            uv_min: d.uv_min,
            uv_max: d.uv_max,
            alpha_limit: d.alpha_limit,
        };
        // SAFETY: `g` and the file bytes are valid for the call.
        unsafe { (t().gfx_element)(&raw const g) }
    }
    fn gfx_image_size(&self, gfx: Ptr) -> [f32; 2] {
        let mut v = Vector2f::default();
        // SAFETY: the script passes a live element.
        unsafe { g1_gfx::ohpl_g1_gfx_image_size(gfx as *mut Element, &mut v) };
        [v.x, v.y]
    }
    fn gfx_name(&self, gfx: Ptr) -> Vec<u8> {
        let mut v = Vec::new();
        // SAFETY: as `string`.
        unsafe { (t().gfx_name)(gfx, push_bytes, (&raw mut v).cast()) };
        v
    }
    fn white(&self) -> Ptr {
        // SAFETY: plain host call.
        unsafe { (t().white)() }
    }
    fn font(&self, file: &[u8]) -> Ptr {
        // SAFETY: `file` is valid for the call.
        unsafe { (t().font)(file.as_ptr(), file.len()) }
    }
    fn font_name(&self, font: Ptr) -> Vec<u8> {
        let mut v = Vec::new();
        // SAFETY: as `string`.
        unsafe { (t().font_name)(font, push_bytes, (&raw mut v).cast()) };
        v
    }
    fn font_length(&self, font: Ptr, size: [f32; 2], text: &[u32]) -> f32 {
        // SAFETY: arguments are valid for the call.
        unsafe { (t().font_length)(font, size.as_ptr(), text.as_ptr(), text.len()) }
    }
    fn font_wrap(
        &self,
        font: Ptr,
        width: f32,
        row_h: f32,
        size: [f32; 2],
        text: &[u32],
    ) -> Vec<Vec<u32>> {
        let mut rows: Vec<Vec<u32>> = Vec::new();
        // SAFETY: arguments are valid for the call; the sink collects the rows.
        unsafe {
            (t().font_wrap)(
                font,
                width,
                row_h,
                size.as_ptr(),
                text.as_ptr(),
                text.len(),
                push_row,
                (&raw mut rows).cast(),
            );
        }
        rows
    }
    fn draw_gfx(&self, set: Ptr, gfx: Ptr, o: &Op) {
        g1_set::draw_gfx_traced(
            set as *mut Set,
            gfx as *mut Element,
            Vector3f {
                x: o.pos[0],
                y: o.pos[1],
                z: o.pos[2],
            },
            Vector2f {
                x: o.size[0],
                y: o.size[1],
            },
            Color {
                r: o.color[0],
                g: o.color[1],
                b: o.color[2],
                a: o.color[3],
            },
            o.material,
            o.angle,
        );
    }
    fn draw_font(&self, set: Ptr, font: Ptr, o: &Op) {
        let d = draw_of(o);
        // SAFETY: `d` and the text are valid for the call.
        unsafe { (t().draw_font)(set, font, &raw const d) }
    }
    fn clip_current(&self, set: Ptr) -> Ptr {
        g1_set::current_clip(set as *mut Set) as Ptr
    }
    fn clip_child(&self, parent: Ptr, pos: [f32; 3], size: [f32; 2]) -> Ptr {
        let (pos, size) = (
            Vector3f {
                x: pos[0],
                y: pos[1],
                z: pos[2],
            },
            Vector2f {
                x: size[0],
                y: size[1],
            },
        );
        // SAFETY: `parent` is a live clip region.
        unsafe { g1_set::ohpl_g1_clip_create_child(parent as *mut Clip, &pos, &size) as Ptr }
    }
    fn clip_set(&self, set: Ptr, clip: Ptr) {
        g1_set::set_current_clip(set as *mut Set, clip as *mut Clip);
    }
    fn set_info(&self, set: Ptr) -> ([f32; 2], [f32; 2], bool) {
        g1_set::set_info(set as *mut Set)
    }
    fn screen_size(&self) -> ([f32; 2], [i32; 2]) {
        let (mut f, mut i) = ([0.0f32; 2], [0i32; 2]);
        // SAFETY: out parameters are live.
        unsafe { (t().screen_size)(f.as_mut_ptr(), i.as_mut_ptr()) };
        (f, i)
    }
    fn create_set(&self, name: &[u8]) -> Ptr {
        // SAFETY: `name` is valid for the call.
        unsafe { (t().create_set)(name.as_ptr(), name.len()) }
    }
    fn set_hud_virtual(&self, set: Ptr, size: [f32; 2], offset: [f32; 2]) {
        // SAFETY: arrays are valid for the call.
        unsafe { (t().set_hud_virtual)(set, size.as_ptr(), offset.as_ptr()) }
    }
    fn set_render(&self, set: Ptr) {
        // SAFETY: plain host call.
        unsafe { (t().set_render)(set) }
    }
    fn wstr(&self, p: Ptr) -> Vec<u32> {
        let mut v = Vec::new();
        // SAFETY: as `string`.
        unsafe { (t().wstr)(p, push_words, (&raw mut v).cast()) };
        v
    }
    fn array_size(&self, a: Ptr) -> usize {
        // SAFETY: plain host call.
        unsafe { (t().array_size)(a) }
    }
    fn array_at(&self, a: Ptr, i: usize) -> Ptr {
        // SAFETY: plain host call.
        unsafe { (t().array_at)(a, i) }
    }
    fn array_resize(&self, a: Ptr, n: usize) {
        // SAFETY: plain host call.
        unsafe { (t().array_resize)(a, n) }
    }
    fn array_push_wstring(&self, a: Ptr, s: &[u32]) {
        // SAFETY: `s` is valid for the call.
        unsafe { (t().array_push_wstring)(a, s.as_ptr(), s.len()) }
    }
    fn array_push_bool(&self, a: Ptr, b: bool) {
        // SAFETY: plain host call.
        unsafe { (t().array_push_bool)(a, b) }
    }
    fn array_push_i32(&self, a: Ptr, v: i32) {
        // SAFETY: plain host call.
        unsafe { (t().array_push_i32)(a, v) }
    }
    fn action_subs(&self, name: &[u8]) -> Option<Vec<SubAction>> {
        let mut v: Vec<SubAction> = Vec::new();
        // SAFETY: `name` is valid for the call; the sink collects the bindings.
        let n =
            unsafe { (t().action_subs)(name.as_ptr(), name.len(), push_sub, (&raw mut v).cast()) };
        (n >= 0).then_some(v)
    }
    fn translate_button(&self, name: &[u8]) -> Vec<u32> {
        let mut v = Vec::new();
        // SAFETY: as `string`.
        unsafe {
            (t().translate_button)(name.as_ptr(), name.len(), push_words, (&raw mut v).cast())
        };
        v
    }
    fn key_name(&self, key: i32) -> Vec<u8> {
        let mut v = Vec::new();
        // SAFETY: as `string`.
        unsafe { (t().key_name)(key, push_bytes, (&raw mut v).cast()) };
        v
    }
    fn string_duration(&self, text: &[u32]) -> f32 {
        // SAFETY: `text` is valid for the call.
        unsafe { (t().string_duration)(text.as_ptr(), text.len()) }
    }
    fn camtex_create(&self, name: &[u8], size: [i32; 2], fps: u32, fov: f32, near: f32, far: f32) {
        // SAFETY: `name` is valid for the call.
        unsafe {
            (t().camtex_create)(
                name.as_ptr(),
                name.len(),
                size[0],
                size[1],
                fps,
                fov,
                near,
                far,
            )
        }
    }
    fn camtex_destroy(&self, name: &[u8]) {
        // SAFETY: `name` is valid for the call.
        unsafe { (t().camtex_destroy)(name.as_ptr(), name.len()) }
    }
    fn camtex_settings(&self, name: &[u8], fov: f32, near: f32, far: f32) {
        // SAFETY: `name` is valid for the call.
        unsafe { (t().camtex_settings)(name.as_ptr(), name.len(), fov, near, far) }
    }
    fn camtex_matrix(&self, name: &[u8], m: Ptr) {
        // SAFETY: `name` is valid for the call.
        unsafe { (t().camtex_matrix)(name.as_ptr(), name.len(), m) }
    }
    fn camtex_attach(&self, name: &[u8], ent: Ptr) {
        // SAFETY: `name` is valid for the call.
        unsafe { (t().camtex_attach)(name.as_ptr(), name.len(), ent) }
    }
}

fn host() -> &'static CHost {
    &CH
}

/// # Safety
/// `e` is the script engine.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_s8d_register(e: *mut c_void) {
    super::s8d_natives::register(Engine(e), host());
}

fn im<'a>(p: *mut ImGui) -> &'a mut ImGui {
    // SAFETY: handed out by `ohpl_s8d_new`/`ohpl_s8d_hud`, never freed.
    unsafe { &mut *p }
}

/// # Safety
/// `name` is valid for `n` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_s8d_new(name: *const u8, n: usize, set: usize) -> *mut ImGui {
    // SAFETY: caller contract.
    ImGui::new_boxed(host(), unsafe { slice(name, n) }, set)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s8d_hud() -> *mut ImGui {
    s8d_hud::hud(host())
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s8d_draw_imguis() {
    s8d_hud::draw_imguis(host());
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s8d_render_imguis() {
    s8d_hud::render_imguis(host());
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s8d_current() -> *mut ImGui {
    s8d::current()
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s8d_set_current(p: *mut ImGui) {
    s8d::set_current(p);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s8d_input_focus() -> *mut ImGui {
    s8d::input_focus(host())
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s8d_script_input_focus() -> *mut ImGui {
    s8d::script_input_focus()
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s8d_prev_input_focus() -> *mut ImGui {
    s8d::prev_input_focus()
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s8d_set_input_focus(p: *mut ImGui, show_mouse: bool) {
    // SAFETY: `p` is null or a UI of this port.
    unsafe { s8d::set_input_focus(host(), p, show_mouse) };
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s8d_update_focus_history() {
    s8d::update_focus_history();
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s8d_game_hud_focus() -> bool {
    s8d::game_hud_focus()
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s8d_set_game_hud_focus(b: bool) {
    s8d::set_game_hud_focus(b);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s8d_set(p: *mut ImGui) -> usize {
    im(p).set
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s8d_name(p: *mut ImGui) -> usize {
    im(p).name_ptr
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s8d_begin(p: *mut ImGui, dt: f32) {
    im(p).begin(dt);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s8d_end(p: *mut ImGui) {
    im(p).end(host());
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s8d_draw_all(p: *mut ImGui) {
    im(p).draw_all(host());
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s8d_drawn(p: *mut ImGui) -> usize {
    im(p).drawn_count()
}

/// # Safety
/// `sink` is valid for `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_s8d_debug_ops(
    p: *mut ImGui,
    max: usize,
    sink: Bytes,
    ctx: *mut c_void,
) {
    let s = im(p).debug_ops(host(), max);
    // SAFETY: caller contract.
    unsafe { sink(ctx, s.as_ptr(), s.len()) };
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s8d_send_mouse(p: *mut ImGui, x: i32, y: i32, rx: i32, ry: i32) {
    im(p).send_mouse(host(), [x, y], [rx, ry]);
}

/// # Safety
/// Both point at two floats.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_s8d_send_mouse_virtual(
    p: *mut ImGui,
    pos: *const f32,
    rel: *const f32,
) {
    // SAFETY: caller contract.
    let (a, b) = unsafe { (slice(pos, 2), slice(rel, 2)) };
    if let ([a0, a1], [b0, b1]) = (a, b) {
        im(p).send_mouse_virtual([*a0, *a1], [*b0, *b1]);
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s8d_mouse_pos(p: *mut ImGui) -> *mut f32 {
    im(p).mouse_pos.as_mut_ptr()
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s8d_cursor_3d(p: *mut ImGui) -> *mut f32 {
    im(p).cursor_3d.as_mut_ptr()
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s8d_screen_clear(p: *mut ImGui, offline: bool) -> *mut f32 {
    let g = im(p);
    if offline {
        g.screen_offline_clear.as_mut_ptr()
    } else {
        g.screen_clear.as_mut_ptr()
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s8d_show_mouse(p: *mut ImGui) -> bool {
    im(p).show_mouse
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s8d_set_show_mouse(p: *mut ImGui, b: bool) {
    im(p).show_mouse = b;
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s8d_state(p: *mut ImGui, id: u64) -> *mut State {
    im(p).state(id)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s8d_state_count(p: *mut ImGui) -> usize {
    im(p).states.len()
}

/// # Safety
/// `id` is writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_s8d_state_at(p: *mut ImGui, i: usize, id: *mut u64) -> *mut State {
    let g = im(p);
    match g.states.iter_mut().nth(i) {
        Some((k, v)) => {
            // SAFETY: caller contract.
            unsafe { *id = *k };
            std::ptr::from_mut::<State>(&mut **v)
        }
        None => std::ptr::null_mut(),
    }
}

/// `op` 0 get-or-set, 1 set, 2 add; `kind` 0 int, 1 float, 2 vector, 3 colour. The value is read
/// from `input` and, for op 0, the result written to `out`.
///
/// # Safety
/// `input` and `out` hold a value of `kind`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_s8d_state_op(
    p: *mut ImGui,
    id: u64,
    op: i32,
    kind: i32,
    input: *const u8,
    out: *mut u8,
) {
    use s8d::Val;
    // SAFETY: caller contract.
    let v = unsafe {
        match kind {
            0 => Val::Int(input.cast::<i32>().read_unaligned()),
            1 => Val::Float(input.cast::<f32>().read_unaligned()),
            2 => Val::Vec(input.cast::<[f32; 3]>().read_unaligned()),
            _ => Val::Col(input.cast::<[f32; 4]>().read_unaligned()),
        }
    };
    let g = im(p);
    match op {
        1 => g.set_val(id, v),
        2 => g.inc_val(id, v),
        _ => {
            let r = g.get_or_set(id, v);
            // SAFETY: caller contract.
            unsafe {
                match r {
                    Val::Int(x) => out.cast::<i32>().write_unaligned(x),
                    Val::Float(x) => out.cast::<f32>().write_unaligned(x),
                    Val::Vec(x) => out.cast::<[f32; 3]>().write_unaligned(x),
                    Val::Col(x) => out.cast::<[f32; 4]>().write_unaligned(x),
                }
            }
        }
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s8d_timer_count(p: *mut ImGui) -> usize {
    im(p).timers.len()
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s8d_timer_at(p: *mut ImGui, i: usize) -> *const Timer {
    im(p)
        .timers
        .get(i)
        .map_or(std::ptr::null(), std::ptr::from_ref)
}

/// # Safety
/// `t` points at a timer.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_s8d_timer_push(p: *mut ImGui, t: *const Timer) {
    // SAFETY: caller contract.
    im(p).timers.push(unsafe { *t });
}

/// # Safety
/// `goal` points at four floats.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_s8d_fade(
    p: *mut ImGui,
    id: u64,
    ty: i32,
    goal: *const f32,
    time: f32,
    easing: i32,
    replace: bool,
) {
    // SAFETY: caller contract.
    if let [a, b, c, d] = unsafe { slice(goal, 4) } {
        im(p).fade(id, ty, [*a, *b, *c, *d], time, easing, replace);
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s8d_stop_fade(p: *mut ImGui, id: u64) {
    im(p).fades.remove(&id);
}

/// # Safety
/// `file` is valid for `n` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_s8d_preload_gfx(file: *const u8, n: usize, ty: i32) {
    // SAFETY: caller contract.
    s8d::preload_gfx(host(), unsafe { slice(file, n) }, ty);
}

/// # Safety
/// `text` is valid for `n` units; `sink` for `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_s8d_parse_string(
    text: *const u32,
    n: usize,
    sink: Words,
    ctx: *mut c_void,
) {
    // SAFETY: caller contract.
    let out = parse_string(
        host(),
        unsafe { slice(text, n) },
        None,
        super::s8d_sd::Sd(std::ptr::null()),
    );
    unsafe { sink(ctx, out.as_ptr(), out.len()) };
}

const _: () = {
    use std::mem::size_of;
    assert!(size_of::<State>() == 56);
    assert!(size_of::<Timer>() == 16);
};
