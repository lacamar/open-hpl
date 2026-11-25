//! C ABI of the engine loop (`RustEngineH22.h`): every host operation goes through one
//! `call(ctx, op, a0..a3)` entry so the C++ shell stays a switch.

use super::ffi::{VisitFn, visitor};
use super::frame::Fps;
use super::frame::Loop;
use super::h22_engine::{self as eng, Module, Native, RunLog, RunState, Step, Timing};
use super::updater::{self, Updater};
use crate::backend::sdl::{delay_ms, ohpl_l1_ticks};
use crate::system::logic_timer::LogicTimer;
use std::ffi::c_void;
use std::sync::OnceLock;
use std::sync::atomic::{AtomicBool, Ordering};
use std::time::Instant;

pub type CallFn = unsafe extern "C" fn(*mut c_void, i32, f64, f64, f64, f64) -> f64;

/// Numbers are the original `OHPL_H22_OP_*` values; gaps are ops the loop now does itself.
pub mod op {
    pub const LOG: i32 = 0;
    pub const LOG_MEDIUM: i32 = 1;
    pub const WINDOW_INPUT_FOCUS: i32 = 11;
    pub const INPUT_UPDATE: i32 = 13;
    pub const HC_LOGIC_STEP: i32 = 16;
    pub const HC_UPDATE: i32 = 17;
    pub const QUIT_POSTED: i32 = 18;
    pub const RESET_QUIT: i32 = 19;
    pub const WINDOW_FOCUS_BITS: i32 = 20;
    pub const TAKE_PLUGGED: i32 = 21;
    pub const TAKE_REMOVED: i32 = 22;
    pub const UPDATE_LOG_ACTIVE: i32 = 23;
    pub const CLEAR_UPDATE_LOG: i32 = 24;
    pub const SWAP: i32 = 27;
    pub const TIMING: i32 = 28;
    pub const FT_START: i32 = 29;
    pub const FT_STOP: i32 = 30;
    pub const FT_SEC: i32 = 31;
    pub const SCENE_RENDER: i32 = 32;
    pub const DEVHUD_RENDER: i32 = 33;
    pub const FLUSH: i32 = 34;
    pub const DIAG_END_FRAME: i32 = 35;
    pub const DIAG_ADD_TIMING: i32 = 36;
    /// Init/exit step: a0 = step code, a1 = module.
    pub const STEP: i32 = 40;
}

pub struct CHost {
    call: CallFn,
    ctx: *mut c_void,
}

impl CHost {
    #[cfg(test)]
    pub fn new(call: CallFn, ctx: *mut c_void) -> Self {
        Self { call, ctx }
    }

    fn c(&self, op: i32, a: [f64; 4]) -> f64 {
        // SAFETY: host contract for `call`/`ctx`.
        unsafe { (self.call)(self.ctx, op, a[0], a[1], a[2], a[3]) }
    }

    fn q(&self, op: i32) -> bool {
        self.c(op, [0.0; 4]) != 0.0
    }

    fn v(&self, op: i32) {
        self.c(op, [0.0; 4]);
    }
}

impl CHost {
    pub(super) fn log(&self, line: RunLog) {
        let i = match line {
            RunLog::RuleBlank => 0.0,
            RunLog::GameRunning => 1.0,
            RunLog::Rule => 2.0,
            RunLog::Statistics => 3.0,
            RunLog::UserExit => 4.0,
        };
        self.c(op::LOG, [i, 0.0, 0.0, 0.0]);
    }
    pub(super) fn log_medium_framerate(&self, fps: f64) {
        self.c(op::LOG_MEDIUM, [fps, 0.0, 0.0, 0.0]);
    }
    pub(super) fn window_input_focus(&self) -> bool {
        self.q(op::WINDOW_INPUT_FOCUS)
    }
    pub(super) fn input_update(&self, dt: f32) {
        self.c(op::INPUT_UPDATE, [f64::from(dt), 0.0, 0.0, 0.0]);
    }
    pub(super) fn headless_logic_step(&self) {
        self.v(op::HC_LOGIC_STEP);
    }
    pub(super) fn headless_update(&self) {
        self.v(op::HC_UPDATE);
    }
    pub(super) fn quit_posted(&self) -> bool {
        self.q(op::QUIT_POSTED)
    }
    pub(super) fn reset_quit_posted(&self) {
        self.v(op::RESET_QUIT);
    }
    pub(super) fn window_focus_bits(&self) -> u32 {
        self.c(op::WINDOW_FOCUS_BITS, [0.0; 4]) as u32
    }
    pub(super) fn take_device_plugged(&self) -> bool {
        self.q(op::TAKE_PLUGGED)
    }
    pub(super) fn take_device_removed(&self) -> bool {
        self.q(op::TAKE_REMOVED)
    }
    pub(super) fn update_log_active(&self) -> bool {
        self.q(op::UPDATE_LOG_ACTIVE)
    }
    pub(super) fn clear_update_log(&self) {
        self.v(op::CLEAR_UPDATE_LOG);
    }
    pub(super) fn swap_buffers(&self) {
        self.v(op::SWAP);
    }
    pub(super) fn timing(&self, t: Timing, start: bool) {
        let tag = match t {
            Timing::WaitAndFinishRendering => 0.0,
            Timing::SwapBuffers => 1.0,
            Timing::OnDraw => 2.0,
            Timing::RenderAll => 3.0,
            Timing::PostRender => 4.0,
            Timing::FlushRender => 5.0,
        };
        self.c(op::TIMING, [tag, f64::from(u8::from(start)), 0.0, 0.0]);
    }
    pub(super) fn frame_timer_start(&self) {
        self.v(op::FT_START);
    }
    pub(super) fn frame_timer_stop(&self) {
        self.v(op::FT_STOP);
    }
    pub(super) fn frame_timer_sec(&self) -> f64 {
        self.c(op::FT_SEC, [0.0; 4])
    }
    pub(super) fn scene_render(&self, frame_time: f32) {
        self.c(op::SCENE_RENDER, [f64::from(frame_time), 0.0, 0.0, 0.0]);
    }
    pub(super) fn dev_hud_render(&self) {
        self.v(op::DEVHUD_RENDER);
    }
    pub(super) fn flush_rendering(&self) {
        self.v(op::FLUSH);
    }
    pub(super) fn diag_end_frame(&self) {
        self.v(op::DIAG_END_FRAME);
    }
    pub(super) fn diag_add_frame_timing(
        &self,
        logic_ms: f64,
        steps: i32,
        render_ms: f64,
        swap_ms: f64,
    ) {
        self.c(
            op::DIAG_ADD_TIMING,
            [logic_ms, f64::from(steps), render_ms, swap_ms],
        );
    }
}

pub fn module_code(m: Module) -> i32 {
    match m {
        Module::Input => 0,
        Module::Physics => 1,
        Module::Scene => 2,
        Module::Graphics => 3,
        Module::Sound => 4,
        Module::Ai => 5,
        Module::Gui => 6,
        Module::Resources => 7,
        Module::System => 8,
    }
}

pub fn step_code(s: Step) -> (i32, i32) {
    match s {
        Step::LogCreatingModules => (0, 0),
        Step::LogRule => (1, 0),
        Step::LogRuleBlank => (2, 0),
        Step::CreateGraphics => (3, 0),
        Step::CreateSystem => (4, 0),
        Step::CreateResources => (5, 0),
        Step::CreateInput => (6, 0),
        Step::CreateSound => (7, 0),
        Step::CreatePhysics => (8, 0),
        Step::CreateAi => (9, 0),
        Step::CreateGui => (10, 0),
        Step::CreateScene => (11, 0),
        Step::InitResources => (12, 0),
        Step::InitGraphics => (13, 0),
        Step::InitSound => (14, 0),
        Step::InitPhysics => (15, 0),
        Step::InitAi => (16, 0),
        Step::InitGui => (17, 0),
        Step::LogInitGame => (18, 0),
        Step::LogAddingUpdates => (19, 0),
        Step::CreateUpdater => (20, 0),
        Step::AddGlobalUpdate(m) => (21, module_code(m)),
        Step::DefaultContainer => (22, 0),
        Step::CreateLogicTimer => (23, 0),
        Step::LogScriptFuncs => (24, 0),
        Step::InitScriptFuncs => (25, 0),
        Step::CreateMutex => (26, 0),
        Step::InitFlags => (27, 0),
        Step::CreateFpsCounter => (28, 0),
        Step::CreateFrameTimer => (29, 0),
        Step::LogUserInit => (30, 0),
        Step::LogExit => (31, 0),
        Step::DeleteHeadless => (32, 0),
        Step::DeleteDevHud => (33, 0),
        Step::DeleteLogicTimer => (34, 0),
        Step::DeleteFpsCounter => (35, 0),
        Step::DeleteFrameTimer => (36, 0),
        Step::DeleteMutex => (37, 0),
        Step::DeleteUpdater => (38, 0),
        Step::DeleteModule(m) => (39, module_code(m)),
        Step::LogDeleteSetup => (40, 0),
        Step::DeleteSetup => (41, 0),
        Step::LogExitOk => (42, 0),
    }
}

impl CHost {
    pub(super) fn step(&self, s: Step) {
        let (code, module) = step_code(s);
        self.c(op::STEP, [f64::from(code), f64::from(module), 0.0, 0.0]);
    }
}

/// Engine members the loop reads and drives itself (`OhplH22Refs` in RustEngineH22.h).
#[repr(C)]
pub struct Refs {
    pub updater: *mut Updater,
    pub visit: VisitFn,
    pub timer: *mut LogicTimer,
    pub fps: *mut Fps,
    pub game_done: *const bool,
    pub paused: *const bool,
    pub render_once: *const bool,
    pub limit_fps: *const bool,
    pub wait_focus: *const bool,
    pub headless: *const *const c_void,
}

fn ticks_ms() -> u64 {
    u64::from(ohpl_l1_ticks())
}

impl Refs {
    fn flag(p: *const bool) -> bool {
        // SAFETY: the shell passes live engine members; `Exit`/`SetPaused` write them from other
        // threads, so the read is atomic.
        unsafe { AtomicBool::from_ptr(p.cast_mut()).load(Ordering::Acquire) }
    }

    fn visit(&self, f: impl FnOnce(&mut dyn FnMut(usize, updater::Visit<'_>))) {
        f(&mut visitor(self.visit, std::ptr::null_mut()));
    }
}

impl Native for Refs {
    fn broadcast(&self, msg: i32) {
        // SAFETY: live updater; visits re-enter through the same pointer.
        self.visit(|cb| unsafe { updater::broadcast(self.updater, msg, 0.0, cb) });
    }
    fn run_message(&self, msg: i32, x: f32) {
        // SAFETY: as above.
        self.visit(|cb| unsafe { updater::run_message(self.updater, msg, x, cb) });
    }
    fn current_container_is_default(&self) -> bool {
        // SAFETY: live updater, short borrow.
        unsafe { (*self.updater).current_name() == b"Default" }
    }
    fn logic_timer_reset(&self) {
        // SAFETY: live timer, short borrow.
        unsafe { (*self.timer).reset(ticks_ms()) }
    }
    fn want_update(&self) -> bool {
        // SAFETY: as above.
        unsafe { (*self.timer).want_update(ticks_ms()) }
    }
    fn end_update_loop(&self) {
        // SAFETY: as above.
        unsafe { (*self.timer).end_update_loop(ticks_ms()) }
    }
    fn step_size(&self) -> f32 {
        // SAFETY: as above.
        unsafe { (*self.timer).step_size() }
    }
    fn game_is_done(&self) -> bool {
        Self::flag(self.game_done)
    }
    fn wait_if_out_of_focus(&self) -> bool {
        Self::flag(self.wait_focus)
    }
    fn sleep_ms(&self, ms: u32) {
        delay_ms(ms);
    }
    fn paused(&self) -> bool {
        Self::flag(self.paused)
    }
    fn has_headless(&self) -> bool {
        // SAFETY: points at the shell's member.
        unsafe { !(*self.headless).is_null() }
    }
    fn render_once(&self) -> bool {
        Self::flag(self.render_once)
    }
    fn limit_fps(&self) -> bool {
        Self::flag(self.limit_fps)
    }
    fn fps_add_frame(&self) {
        // SAFETY: live counter, short borrow.
        unsafe { (*self.fps).add_frame(ticks_ms()) }
    }
    fn app_time_ms(&self) -> u64 {
        ticks_ms()
    }
    fn steady_ns(&self) -> i64 {
        static BASE: OnceLock<Instant> = OnceLock::new();
        BASE.get_or_init(Instant::now).elapsed().as_nanos() as i64
    }
}

/// # Safety
/// `call`/`ctx` are a valid host; `refs`, `lp`, `frame_time` and `focus_bits` point at engine
/// members that outlive the call.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h22_engine_run(
    call: CallFn,
    ctx: *mut c_void,
    refs: *const Refs,
    lp: *mut Loop,
    frame_time: *mut f32,
    focus_bits: *mut u32,
) {
    let host = CHost { call, ctx };
    eng::run(
        &host,
        // SAFETY: caller contract.
        unsafe { &*refs },
        &RunState {
            lp,
            frame_time,
            focus_bits,
        },
    );
}

/// # Safety
/// `call`/`ctx` are a valid host.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h22_engine_init(call: CallFn, ctx: *mut c_void) {
    let host = CHost { call, ctx };
    for s in eng::init_steps() {
        host.step(s);
    }
}

/// # Safety
/// `call`/`ctx` are a valid host.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h22_engine_exit(call: CallFn, ctx: *mut c_void) {
    let host = CHost { call, ctx };
    for s in eng::exit_steps() {
        host.step(s);
    }
}
