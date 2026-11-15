//! Engine calls of the main menu over the shared menu host (`luxmenu::host::call`); codes are
//! mirrored in `amnesia/src/game/RustMenuHost.cpp`.

use super::super::host::{Args, Wd, call, narrow};
use std::marker::PhantomData;

pub mod code {
    pub const SKIN_NEW: i32 = 200;
    pub const SET_NEW: i32 = 201;
    pub const IS_CHINESE: i32 = 202;
    pub const SKIN_FONT_MUL: i32 = 203;
    pub const SET_ACTIVE: i32 = 204;
    pub const VP_NEW: i32 = 205;
    pub const VP_ACTIVE: i32 = 206;
    pub const VP_VISIBLE: i32 = 207;
    pub const VP_ADD_SET: i32 = 208;
    pub const VP_WORLD: i32 = 209;
    pub const VP_CAMERA: i32 = 210;
    pub const CFG: i32 = 211;
    pub const GFX_RECT: i32 = 212;
    pub const GFX_IMAGE: i32 = 213;
    pub const GFX_SIZE: i32 = 214;
    pub const GFX_TEX: i32 = 215;
    pub const GFX_DESTROY: i32 = 216;
    pub const DRAW_GFX: i32 = 217;
    pub const SET_VSIZE: i32 = 218;
    pub const WIDGET_DESTROY: i32 = 219;
    pub const SET_DESTROY_ALL: i32 = 220;
    pub const WIDGET_NAME: i32 = 221;
    pub const WIDGET_HAS_FOCUS: i32 = 222;
    pub const WIDGET_ENABLED: i32 = 223;
    pub const WIDGET_NAV: i32 = 224;
    pub const WIDGET_FONT_TYPE: i32 = 225;
    pub const WIDGET_FONT_LEN: i32 = 226;
    pub const FONT_LOAD: i32 = 227;
    pub const SET_DRAW_FOCUS: i32 = 228;
    pub const SET_GET_DRAW_FOCUS: i32 = 229;
    pub const SET_GET_FOCUSED: i32 = 230;
    pub const POPUP_OPTS: i32 = 231;
    pub const PROCESS_MOUSE_DOWN: i32 = 232;
    pub const WORLD_DESTROY: i32 = 233;
    pub const CAMERA_DESTROY: i32 = 234;
    pub const WORLD_LOAD: i32 = 235;
    pub const CAMERA_NEW: i32 = 236;
    pub const CAMERA_FOV: i32 = 237;
    pub const CAMERA_ASPECT: i32 = 238;
    pub const CAMERA_PLACE: i32 = 239;
    pub const SCREEN_SIZE: i32 = 240;
    pub const TEX_NEW: i32 = 241;
    pub const TEX_DESTROY: i32 = 242;
    pub const FB_NEW: i32 = 243;
    pub const FB_DESTROY: i32 = 244;
    pub const PROG_NEW: i32 = 245;
    pub const PROG_BIND: i32 = 246;
    pub const GFX_STATE: i32 = 247;
    pub const GFX_ORTHO: i32 = 248;
    pub const GFX_COPY_SCREEN: i32 = 249;
    pub const GFX_SET_FB: i32 = 250;
    pub const GFX_SET_TEX: i32 = 251;
    pub const GFX_QUAD: i32 = 252;
    pub const GFX_FLUSH: i32 = 253;
    pub const LABEL_NAMED: i32 = 254;
    pub const WIDGET_SET_GLOBAL_POS: i32 = 256;
    pub const GUI_FOCUS: i32 = 257;
    pub const WIDGET_CENTER: i32 = 258;
    pub const MAP_LOADED: i32 = 260;
    pub const MUSIC: i32 = 261;
    pub const PAUSE_MAP: i32 = 262;
    pub const PROGLOG: i32 = 263;
    pub const INPUT_STATE: i32 = 264;
    pub const LOCK_INPUT: i32 = 265;
    pub const RELATIVE_MOUSE: i32 = 266;
    pub const FLAG: i32 = 267;
    pub const SET_HARD: i32 = 268;
    pub const TINDER: i32 = 269;
    pub const SAVE: i32 = 270;
    pub const HAS_MAP: i32 = 271;
    pub const DESTROY_MAP: i32 = 272;
    pub const UPDATER: i32 = 273;
    pub const CUSTOM_CLEAR: i32 = 274;
    pub const PROFILE: i32 = 275;
    pub const START_GAME: i32 = 276;
    pub const DRAW_MENU_SCREEN: i32 = 277;
    pub const ZOOM_SOUND: i32 = 278;
    pub const MAP_NAME: i32 = 279;
    pub const UNLOCK_NOPE: i32 = 280;
    pub const LOG_ERROR: i32 = 281;
}

pub mod flag {
    pub const FULLSCREEN: i32 = 0;
    pub const DEBUG_WINDOW: i32 = 1;
    pub const GAMEPAD: i32 = 2;
    pub const NO_USER_CONFIG: i32 = 3;
    pub const ALLOW_HARD: i32 = 4;
    pub const NO_CUSTOM_STORY: i32 = 5;
    pub const PTEST: i32 = 6;
    pub const SAVE_EXISTS: i32 = 7;
    pub const EXIT_DIRECT: i32 = 8;
    pub const PLAYER_DEAD: i32 = 9;
    pub const HARD: i32 = 10;
    pub const DEMO_ON_ALL_EXIT: i32 = 11;
    pub const BUILD_GAMEPAD: i32 = 12;
    pub const BUILD_DEMO: i32 = 13;
}

pub mod music {
    pub const PLAY: i32 = 0;
    pub const STOP: i32 = 1;
    pub const PAUSE: i32 = 2;
    pub const RESUME: i32 = 3;
}

pub mod input {
    pub const NULL: i32 = 0;
    pub const GAME: i32 = 1;
    pub const MAIN_MENU: i32 = 2;
}

pub mod updater {
    pub const RESET_ALL: i32 = 0;
    pub const DEFAULT: i32 = 1;
    pub const DEMO_END: i32 = 2;
    pub const ENGINE_EXIT: i32 = 3;
}

pub mod profile {
    pub const CREATE_DEFAULT: i32 = 0;
    pub const SET_DEFAULT: i32 = 1;
    pub const INIT_CONFIG: i32 = 2;
}

pub mod save {
    pub const AUTO_SAVE: i32 = 0;
    pub const AUTO_LOAD: i32 = 1;
    pub const HARD_SAVE: i32 = 2;
}

pub mod gfx_state {
    pub const BLEND: i32 = 0;
    pub const DEPTH_TEST: i32 = 1;
    pub const DEPTH_WRITE: i32 = 2;
}

/// Argument block builder; string slices stay borrowed until the call returns.
pub struct C<'a> {
    a: Args,
    _s: PhantomData<&'a [u32]>,
}

impl Default for C<'_> {
    fn default() -> Self {
        C {
            a: Args::new(),
            _s: PhantomData,
        }
    }
}

impl<'a> C<'a> {
    pub fn new() -> C<'a> {
        C::default()
    }

    pub fn p(mut self, i: usize, v: usize) -> C<'a> {
        if let Some(x) = self.a.p.get_mut(i) {
            *x = v;
        }
        self
    }

    pub fn f(mut self, i: usize, v: f32) -> C<'a> {
        if let Some(x) = self.a.f.get_mut(i) {
            *x = v;
        }
        self
    }

    pub fn i(mut self, i: usize, v: i32) -> C<'a> {
        if let Some(x) = self.a.i.get_mut(i) {
            *x = v;
        }
        self
    }

    pub fn s(mut self, i: usize, t: &'a [u32]) -> C<'a> {
        if let (Some(p), Some(n)) = (self.a.s.get_mut(i), self.a.n.get_mut(i)) {
            *p = t.as_ptr();
            *n = t.len();
        }
        self
    }

    pub fn run(mut self, code: i32) -> usize {
        call(code, &mut self.a)
    }

    pub fn args(mut self, code: i32) -> (usize, Args) {
        let r = call(code, &mut self.a);
        (r, self.a)
    }

    pub fn out_str(mut self, code: i32) -> Vec<u32> {
        call(code, &mut self.a);
        let (p, n) = (self.a.s[0], self.a.n[0]);
        if p.is_null() || n == 0 {
            return Vec::new();
        }
        // SAFETY: the host leaves `s[0]` pointing at `n` live code units until its next op.
        unsafe { std::slice::from_raw_parts(p, n) }.to_vec()
    }
}

pub fn w(s: &str) -> Vec<u32> {
    super::super::host::w(s)
}

fn bit(b: bool) -> i32 {
    i32::from(b)
}

pub fn skin_new(file: &str) -> usize {
    C::new().s(0, &w(file)).run(code::SKIN_NEW)
}

pub fn set_new(skin: usize, name: &str) -> usize {
    C::new().p(0, skin).s(0, &w(name)).run(code::SET_NEW)
}

pub fn is_chinese() -> bool {
    C::new().run(code::IS_CHINESE) != 0
}

pub fn skin_font_mul(skin: usize, m: f32) {
    C::new().p(0, skin).f(0, m).run(code::SKIN_FONT_MUL);
}

pub fn set_active(set: usize, on: bool) {
    C::new().p(0, set).i(0, bit(on)).run(code::SET_ACTIVE);
}

pub fn vp_new() -> usize {
    C::new().run(code::VP_NEW)
}

pub fn vp_active(vp: usize, on: bool) {
    C::new().p(0, vp).i(0, bit(on)).run(code::VP_ACTIVE);
}

pub fn vp_visible(vp: usize, on: bool) {
    C::new().p(0, vp).i(0, bit(on)).run(code::VP_VISIBLE);
}

pub fn vp_add_set(vp: usize, set: usize) {
    C::new().p(0, vp).p(1, set).run(code::VP_ADD_SET);
}

pub fn vp_world(vp: usize, world: usize) {
    C::new().p(0, vp).p(1, world).run(code::VP_WORLD);
}

pub fn vp_camera(vp: usize, cam: usize) {
    C::new().p(0, vp).p(1, cam).run(code::VP_CAMERA);
}

pub fn cfg_float(sec: &str, key: &str, def: f32) -> f32 {
    C::new()
        .i(0, 0)
        .f(0, def)
        .s(0, &w(sec))
        .s(1, &w(key))
        .args(code::CFG)
        .1
        .f[0]
}

pub fn cfg_vec2(sec: &str, key: &str) -> [f32; 2] {
    let a = C::new()
        .i(0, 1)
        .s(0, &w(sec))
        .s(1, &w(key))
        .args(code::CFG)
        .1;
    [a.f[0], a.f[1]]
}

pub fn cfg_string(sec: &str, key: &str, def: &str) -> String {
    narrow(
        &C::new()
            .i(0, 2)
            .s(0, &w(sec))
            .s(1, &w(key))
            .s(2, &w(def))
            .out_str(code::CFG),
    )
}

pub fn gfx_rect(c: [f32; 4]) -> usize {
    C::new()
        .f(0, c[0])
        .f(1, c[1])
        .f(2, c[2])
        .f(3, c[3])
        .run(code::GFX_RECT)
}

pub fn gfx_image(file: &str) -> usize {
    C::new().s(0, &w(file)).run(code::GFX_IMAGE)
}

/// Active size of the gfx element.
pub fn gfx_size(gfx: usize) -> [f32; 2] {
    let a = C::new().p(0, gfx).args(code::GFX_SIZE).1;
    [a.f[0], a.f[1]]
}

pub fn gfx_tex(tex: usize, alpha_material: bool) -> usize {
    C::new()
        .p(0, tex)
        .i(0, bit(alpha_material))
        .run(code::GFX_TEX)
}

pub fn gfx_destroy(gfx: usize) {
    C::new().p(0, gfx).run(code::GFX_DESTROY);
}

/// `cGuiSet::DrawGfx`; `alpha` None draws with the default colour, else `cColor(1, alpha)`.
pub fn draw_gfx(set: usize, gfx: usize, pos: [f32; 3], size: [f32; 2], alpha: Option<f32>) {
    C::new()
        .p(0, set)
        .p(1, gfx)
        .f(0, pos[0])
        .f(1, pos[1])
        .f(2, pos[2])
        .f(3, size[0])
        .f(4, size[1])
        .f(5, alpha.unwrap_or(0.0))
        .i(0, bit(alpha.is_some()))
        .run(code::DRAW_GFX);
}

pub fn set_vsize(set: usize) -> [f32; 2] {
    let a = C::new().p(0, set).args(code::SET_VSIZE).1;
    [a.f[0], a.f[1]]
}

pub fn widget_destroy(set: usize, w: Wd) {
    C::new().p(0, set).p(1, w.0).run(code::WIDGET_DESTROY);
}

pub fn set_destroy_all(set: usize) {
    C::new().p(0, set).run(code::SET_DESTROY_ALL);
}

pub fn widget_name(w: Wd) -> String {
    narrow(&C::new().p(0, w.0).out_str(code::WIDGET_NAME))
}

pub fn has_focus(w: Wd) -> bool {
    C::new().p(0, w.0).run(code::WIDGET_HAS_FOCUS) != 0
}

pub fn is_enabled(w: Wd) -> bool {
    C::new().p(0, w.0).run(code::WIDGET_ENABLED) != 0
}

pub fn nav_get(w: Wd, dir: i32) -> Wd {
    Wd(C::new().p(0, w.0).i(0, dir).run(code::WIDGET_NAV))
}

pub fn set_font_type(w: Wd, font: usize) {
    C::new().p(0, w.0).p(1, font).run(code::WIDGET_FONT_TYPE);
}

/// Length of `text` in the widget's font at `size`.
pub fn font_len(w: Wd, size: [f32; 2], text: &[u32]) -> f32 {
    C::new()
        .p(0, w.0)
        .f(0, size[0])
        .f(1, size[1])
        .s(0, text)
        .args(code::WIDGET_FONT_LEN)
        .1
        .f[0]
}

pub fn font_load(file: &str) -> usize {
    C::new().s(0, &w(file)).run(code::FONT_LOAD)
}

pub fn set_draw_focus(set: usize, on: bool) {
    C::new().p(0, set).i(0, bit(on)).run(code::SET_DRAW_FOCUS);
}

pub fn draw_focus(set: usize) -> bool {
    C::new().p(0, set).run(code::SET_GET_DRAW_FOCUS) != 0
}

pub fn focused(set: usize) -> Wd {
    Wd(C::new().p(0, set).run(code::SET_GET_FOCUSED))
}

/// Copies the set's draw-focus flag to the popup's set and turns kill-on-escape off.
pub fn popup_opts(popup: usize, set: usize) {
    C::new().p(0, popup).p(1, set).run(code::POPUP_OPTS);
}

/// Forwards the message data of the running callback as a mouse-down to `w`.
pub fn process_mouse_down(w: Wd) -> bool {
    C::new().p(0, w.0).run(code::PROCESS_MOUSE_DOWN) != 0
}

pub fn world_destroy(world: usize) {
    C::new().p(0, world).run(code::WORLD_DESTROY);
}

pub fn camera_destroy(cam: usize) {
    C::new().p(0, cam).run(code::CAMERA_DESTROY);
}

pub fn world_load(file: &str) -> usize {
    C::new().s(0, &w(file)).run(code::WORLD_LOAD)
}

pub fn camera_new() -> usize {
    C::new().run(code::CAMERA_NEW)
}

pub fn camera_fov(cam: usize, fov: f32) {
    C::new().p(0, cam).f(0, fov).run(code::CAMERA_FOV);
}

pub fn camera_aspect(cam: usize, aspect: f32) {
    C::new().p(0, cam).f(0, aspect).run(code::CAMERA_ASPECT);
}

/// Places the camera at the world's start pos `name` (identity when `world` is null); false when
/// the start pos is missing.
pub fn camera_place(cam: usize, world: usize, name: &str) -> bool {
    C::new()
        .p(0, cam)
        .p(1, world)
        .s(0, &w(name))
        .run(code::CAMERA_PLACE)
        != 0
}

/// Screen size as `cGraphics` reports it: float and integer.
pub fn screen_size() -> ([f32; 2], [i32; 2]) {
    let a = C::new().args(code::SCREEN_SIZE).1;
    ([a.f[0], a.f[1]], [a.i[0], a.i[1]])
}

/// Rect render-target texture of `size`, clamped to the edge when `clamp`.
pub fn tex_new(name: &str, size: [i32; 2], clamp: bool) -> usize {
    C::new()
        .i(0, size[0])
        .i(1, size[1])
        .i(2, bit(clamp))
        .s(0, &w(name))
        .run(code::TEX_NEW)
}

pub fn tex_destroy(tex: usize) {
    C::new().p(0, tex).run(code::TEX_DESTROY);
}

pub fn fb_new(name: &str, tex: usize) -> usize {
    C::new().p(0, tex).s(0, &w(name)).run(code::FB_NEW)
}

pub fn fb_destroy(fb: usize) {
    C::new().p(0, fb).run(code::FB_DESTROY);
}

pub fn prog_new(name: &str, vtx: &str, frag: &str, horizontal: bool) -> usize {
    C::new()
        .s(0, &w(name))
        .s(1, &w(vtx))
        .s(2, &w(frag))
        .i(0, bit(horizontal))
        .run(code::PROG_NEW)
}

pub fn prog_bind(prog: usize, bind: bool) {
    C::new().p(0, prog).i(0, bit(bind)).run(code::PROG_BIND);
}

pub fn gfx_state(which: i32, on: bool) {
    C::new().i(0, which).i(1, bit(on)).run(code::GFX_STATE);
}

/// Orthographic projection of `size` (-1000..1000) and an identity model-view matrix.
pub fn gfx_ortho(size: [f32; 2]) {
    C::new().f(0, size[0]).f(1, size[1]).run(code::GFX_ORTHO);
}

pub fn gfx_copy_screen(tex: usize) {
    C::new().p(0, tex).run(code::GFX_COPY_SCREEN);
}

pub fn gfx_set_fb(fb: usize) {
    C::new().p(0, fb).run(code::GFX_SET_FB);
}

pub fn gfx_set_tex(tex: usize) {
    C::new().p(0, tex).run(code::GFX_SET_TEX);
}

pub fn gfx_quad(size: [f32; 2]) {
    C::new().f(0, size[0]).f(1, size[1]).run(code::GFX_QUAD);
}

pub fn gfx_flush() {
    C::new().run(code::GFX_FLUSH);
}

pub fn label_named(set: usize, pos: [f32; 3], text: &[u32], name: &[u32]) -> Wd {
    Wd(C::new()
        .p(0, set)
        .f(0, pos[0])
        .f(1, pos[1])
        .f(2, pos[2])
        .s(0, text)
        .s(1, name)
        .run(code::LABEL_NAMED))
}

pub fn set_global_pos(w: Wd, p: [f32; 3]) {
    C::new()
        .p(0, w.0)
        .f(0, p[0])
        .f(1, p[1])
        .f(2, p[2])
        .run(code::WIDGET_SET_GLOBAL_POS);
}

/// `cGui::SetFocus`; null clears it.
pub fn gui_focus(set: usize) {
    C::new().p(0, set).run(code::GUI_FOCUS);
}

pub fn center(w: Wd) {
    C::new().p(0, w.0).run(code::WIDGET_CENTER);
}

pub fn map_loaded() -> bool {
    C::new().run(code::MAP_LOADED) != 0
}

pub fn music_play(name: &str) {
    C::new().i(0, music::PLAY).s(0, &w(name)).run(code::MUSIC);
}

pub fn music_op(kind: i32) {
    C::new().i(0, kind).run(code::MUSIC);
}

pub fn pause_map() {
    C::new().run(code::PAUSE_MAP);
}

pub fn proglog(text: &str) {
    C::new().s(0, &w(text)).run(code::PROGLOG);
}

pub fn input_state(state: i32) {
    C::new().i(0, state).run(code::INPUT_STATE);
}

pub fn lock_input(on: bool) {
    C::new().i(0, bit(on)).run(code::LOCK_INPUT);
}

pub fn relative_mouse(on: bool) {
    C::new().i(0, bit(on)).run(code::RELATIVE_MOUSE);
}

pub fn flag(which: i32) -> bool {
    C::new().i(0, which).run(code::FLAG) != 0
}

pub fn set_hard(on: bool) {
    C::new().i(0, bit(on)).run(code::SET_HARD);
}

pub fn tinderboxes() -> i32 {
    C::new().i(0, 0).run(code::TINDER) as i32
}

pub fn set_tinderboxes(n: i32) {
    C::new().i(0, 1).i(1, n).run(code::TINDER);
}

pub fn save(kind: i32) {
    C::new().i(0, kind).run(code::SAVE);
}

pub fn has_map() -> bool {
    C::new().run(code::HAS_MAP) != 0
}

pub fn destroy_map() {
    C::new().run(code::DESTROY_MAP);
}

pub fn updater(kind: i32) {
    C::new().i(0, kind).run(code::UPDATER);
}

pub fn custom_clear() {
    C::new().run(code::CUSTOM_CLEAR);
}

pub fn profile(kind: i32) {
    C::new().i(0, kind).run(code::PROFILE);
}

pub fn start_game() {
    C::new().run(code::START_GAME);
}

pub fn draw_menu_screen() {
    C::new().run(code::DRAW_MENU_SCREEN);
}

pub fn zoom_sound(name: &str) {
    C::new().s(0, &w(name)).run(code::ZOOM_SOUND);
}

pub fn map_name() -> String {
    narrow(&C::new().out_str(code::MAP_NAME))
}

pub fn unlock_nope() {
    C::new().run(code::UNLOCK_NOPE);
}

pub fn log_error(text: &str) {
    C::new().s(0, &w(text)).run(code::LOG_ERROR);
}
