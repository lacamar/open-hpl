//! `cEngine`: module init/exit order and the frame loop. The host performs every module call; this
//! decides the order and the timing branches. Host calls re-enter the engine (updatables, game
//! code), so shared state is only touched through short raw-pointer scopes.

use super::frame::{self, Loop};

pub const MSG_ON_POST_BUFFER_SWAP: i32 = 0;
pub const MSG_ON_START: i32 = 1;
pub const MSG_ON_DRAW: i32 = 2;
pub const MSG_ON_POST_RENDER: i32 = 3;
pub const MSG_PRE_UPDATE: i32 = 4;
pub const MSG_UPDATE: i32 = 5;
pub const MSG_POST_UPDATE: i32 = 6;
pub const MSG_ON_QUIT: i32 = 7;
pub const MSG_ON_EXIT: i32 = 8;
pub const MSG_ON_PAUSE_UPDATE: i32 = 10;
pub const MSG_DEVICE_PLUGGED: i32 = 17;
pub const MSG_DEVICE_REMOVED: i32 = 18;

pub const PAUSE_DT: f32 = 1.0 / 100.0;
pub const FOCUS_WAIT_DT: f32 = 1.0 / 10.0;

/// Fixed log lines of `Run`.
#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum RunLog {
    /// Dashes and a blank line.
    RuleBlank,
    GameRunning,
    /// Dashes.
    Rule,
    Statistics,
    UserExit,
}

/// The `START_TIMING` pairs of `Run`.
#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum Timing {
    WaitAndFinishRendering,
    SwapBuffers,
    OnDraw,
    RenderAll,
    PostRender,
    FlushRender,
}

pub trait EngineHost {
    fn log(&self, line: RunLog);
    fn log_medium_framerate(&self, fps: f64);
    fn broadcast(&self, msg: i32);
    fn run_message(&self, msg: i32, x: f32);
    fn current_container_is_default(&self) -> bool;
    fn logic_timer_reset(&self);
    fn want_update(&self) -> bool;
    fn end_update_loop(&self);
    fn step_size(&self) -> f32;
    fn game_is_done(&self) -> bool;
    fn wait_if_out_of_focus(&self) -> bool;
    fn window_input_focus(&self) -> bool;
    fn sleep_ms(&self, ms: u32);
    fn input_update(&self, dt: f32);
    fn paused(&self) -> bool;
    fn has_headless(&self) -> bool;
    fn headless_logic_step(&self);
    fn headless_update(&self);
    fn quit_posted(&self) -> bool;
    fn reset_quit_posted(&self);
    /// Bit 1 input focus, 2 mouse focus, 4 visible.
    fn window_focus_bits(&self) -> u32;
    fn take_device_plugged(&self) -> bool;
    fn take_device_removed(&self) -> bool;
    fn update_log_active(&self) -> bool;
    fn clear_update_log(&self);
    fn render_once(&self) -> bool;
    fn limit_fps(&self) -> bool;
    fn swap_buffers(&self);
    fn timing(&self, t: Timing, start: bool);
    fn frame_timer_start(&self);
    fn frame_timer_stop(&self);
    fn frame_timer_sec(&self) -> f64;
    fn scene_render(&self, frame_time: f32);
    fn dev_hud_render(&self);
    fn flush_rendering(&self);
    fn diag_end_frame(&self);
    fn diag_add_frame_timing(&self, logic_ms: f64, steps: i32, render_ms: f64, swap_ms: f64);
    fn fps_add_frame(&self);
    fn app_time_ms(&self) -> u64;
    fn steady_ns(&self) -> i64;
}

/// Engine members `Run` shares with the C++ shell.
pub struct RunState {
    pub lp: *mut Loop,
    pub frame_time: *mut f32,
    pub focus_bits: *mut u32,
}

fn ms_since(host: &dyn EngineHost, t: i64) -> f64 {
    host.steady_ns().wrapping_sub(t) as f64 / 1_000_000.0
}

impl RunState {
    fn with_loop<R>(&self, f: impl FnOnce(&mut Loop) -> R) -> R {
        // SAFETY: the shell passes a live Loop; the borrow ends before any host call.
        f(unsafe { &mut *self.lp })
    }
}

/// `cEngine::CheckAndBroadcastFocusChange`.
fn check_focus_change(host: &dyn EngineHost, st: &RunState) {
    if host.has_headless() {
        return;
    }
    // SAFETY: shell-owned word, short scope.
    let had = unsafe { *st.focus_bits };
    let now = host.window_focus_bits();
    // SAFETY: as above.
    unsafe { *st.focus_bits = now };
    let bits = |v: u32| [v & 1 != 0, v & 2 != 0, v & 4 != 0];
    for m in frame::focus_messages(bits(had), bits(now)) {
        host.run_message(m, 0.0);
    }
}

fn check_device_change(host: &dyn EngineHost) {
    if host.take_device_plugged() {
        host.run_message(MSG_DEVICE_PLUGGED, 0.0);
    }
    if host.take_device_removed() {
        host.run_message(MSG_DEVICE_REMOVED, 0.0);
    }
}

fn wait_for_focus(host: &dyn EngineHost) {
    while !host.window_input_focus() {
        host.sleep_ms(100);
        host.input_update(FOCUS_WAIT_DT);
    }
}

/// `cEngine::Run`.
pub fn run(host: &dyn EngineHost, st: &RunState) {
    host.log(RunLog::RuleBlank);
    host.broadcast(MSG_ON_START);
    host.logic_timer_reset();
    let start_time = host.app_time_ms();
    host.log(RunLog::GameRunning);
    host.log(RunLog::Rule);
    // SAFETY: shell-owned float, short scope.
    unsafe { *st.frame_time = 0.0 };
    host.frame_timer_start();
    st.with_loop(Loop::begin_run);

    while !host.game_is_done() {
        if host.wait_if_out_of_focus() {
            wait_for_focus(host);
        }

        if host.paused() {
            host.sleep_ms(10);
            host.input_update(PAUSE_DT);
            if host.has_headless() {
                host.headless_logic_step();
            }
            st.with_loop(Loop::pause_tick);
            host.run_message(MSG_ON_PAUSE_UPDATE, PAUSE_DT);
        } else {
            let t_logic = host.steady_ns();
            while host.want_update() && !host.game_is_done() {
                st.with_loop(Loop::step_begin);
                host.run_message(MSG_PRE_UPDATE, host.step_size());
                host.run_message(MSG_UPDATE, host.step_size());
                host.run_message(MSG_POST_UPDATE, host.step_size());
                if host.has_headless() {
                    host.headless_logic_step();
                }
                st.with_loop(Loop::step_end);

                if host.quit_posted() {
                    host.run_message(MSG_ON_QUIT, 0.0);
                    host.reset_quit_posted();
                }

                check_focus_change(host, st);
                check_device_change(host);

                if host.update_log_active()
                    && st.with_loop(Loop::update_log_tick)
                    && host.current_container_is_default()
                {
                    host.clear_update_log();
                }

                let step = host.step_size();
                st.with_loop(|l| l.advance_game_time(step));
            }
            host.end_update_loop();
            let ms = ms_since(host, t_logic);
            st.with_loop(|l| l.add_logic_ms(ms));
        }

        if host.has_headless() {
            host.headless_update();
        }

        let render_once = host.render_once();
        if st.with_loop(|l| l.skip_render_once(render_once)) {
            continue;
        }

        if st.with_loop(Loop::take_swap) {
            host.timing(Timing::WaitAndFinishRendering, true);
            host.timing(Timing::WaitAndFinishRendering, false);

            host.timing(Timing::SwapBuffers, true);
            let t_swap = host.steady_ns();
            host.swap_buffers();
            let ms = ms_since(host, t_swap);
            st.with_loop(|l| l.add_swap_ms(ms));
            host.timing(Timing::SwapBuffers, false);

            host.run_message(MSG_ON_POST_BUFFER_SWAP, 0.0);
            let render_once = host.render_once();
            if st.with_loop(|l| l.swap_done(render_once)) {
                continue;
            }
        }

        let limit = host.limit_fps();
        if st.with_loop(|l| l.want_render(limit)) {
            host.frame_timer_stop();
            let ft = frame::frame_time(host.frame_timer_sec());
            // SAFETY: shell-owned float, short scope.
            unsafe { *st.frame_time = ft };
            host.frame_timer_start();
            let t_render = host.steady_ns();

            host.timing(Timing::OnDraw, true);
            host.run_message(MSG_ON_DRAW, ft);
            host.timing(Timing::OnDraw, false);

            host.timing(Timing::RenderAll, true);
            host.scene_render(ft);
            host.timing(Timing::RenderAll, false);

            host.timing(Timing::PostRender, true);
            host.run_message(MSG_ON_POST_RENDER, ft);
            host.timing(Timing::PostRender, false);

            host.dev_hud_render();

            host.timing(Timing::FlushRender, true);
            host.flush_rendering();
            host.timing(Timing::FlushRender, false);

            host.diag_end_frame();
            let t = st.with_loop(Loop::frame_rendered);
            host.diag_add_frame_timing(t.logic_ms, t.steps, ms_since(host, t_render), t.swap_ms);
            host.fps_add_frame();
        } else {
            host.sleep_ms(1);
        }
    }
    host.log(RunLog::RuleBlank);
    host.log(RunLog::Statistics);
    host.log(RunLog::Rule);
    let elapsed = host.app_time_ms().wrapping_sub(start_time);
    let fps = st.with_loop(|l| l.mean_framerate(elapsed));
    host.log_medium_framerate(fps);
    host.log(RunLog::RuleBlank);
    host.log(RunLog::UserExit);
    host.log(RunLog::Rule);
    host.broadcast(MSG_ON_EXIT);
}

/// Steps of `GameInit` and the destructor, in order.
#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum Step {
    LogCreatingModules,
    LogRule,
    LogRuleBlank,
    CreateGraphics,
    CreateSystem,
    CreateResources,
    CreateInput,
    CreateSound,
    CreatePhysics,
    CreateAi,
    CreateGui,
    CreateScene,
    InitResources,
    InitGraphics,
    InitSound,
    InitPhysics,
    InitAi,
    InitGui,
    LogInitGame,
    LogAddingUpdates,
    CreateUpdater,
    AddGlobalUpdate(Module),
    DefaultContainer,
    CreateLogicTimer,
    LogScriptFuncs,
    InitScriptFuncs,
    CreateMutex,
    InitFlags,
    CreateFpsCounter,
    CreateFrameTimer,
    LogUserInit,
    /// Destructor.
    LogExit,
    DeleteHeadless,
    DeleteDevHud,
    DeleteLogicTimer,
    DeleteFpsCounter,
    DeleteFrameTimer,
    DeleteMutex,
    DeleteUpdater,
    DeleteModule(Module),
    LogDeleteSetup,
    DeleteSetup,
    LogExitOk,
}

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum Module {
    Input,
    Physics,
    Scene,
    Graphics,
    Sound,
    Ai,
    Gui,
    Resources,
    System,
}

/// `cEngine::GameInit` order. The `Create*` steps log their " Creating ... module" line first.
pub fn init_steps() -> Vec<Step> {
    use Module::*;
    use Step::*;
    let mut v = vec![
        LogCreatingModules,
        LogRule,
        CreateGraphics,
        CreateSystem,
        CreateResources,
        CreateInput,
        CreateSound,
        CreatePhysics,
        CreateAi,
        CreateGui,
        CreateScene,
        LogRuleBlank,
        InitResources,
        InitGraphics,
        InitSound,
        InitPhysics,
        InitAi,
        InitGui,
        LogInitGame,
        LogRule,
        LogAddingUpdates,
        CreateUpdater,
    ];
    for m in [Input, Physics, Scene, Graphics, Sound, Ai, Gui, Resources] {
        v.push(AddGlobalUpdate(m));
    }
    v.extend([
        DefaultContainer,
        CreateLogicTimer,
        LogScriptFuncs,
        InitScriptFuncs,
        CreateMutex,
        InitFlags,
        CreateFpsCounter,
        CreateFrameTimer,
        LogRuleBlank,
        LogUserInit,
        LogRule,
    ]);
    v
}

/// `cEngine::~cEngine` order.
pub fn exit_steps() -> Vec<Step> {
    use Module::*;
    use Step::*;
    let mut v = vec![
        LogRuleBlank,
        DeleteHeadless,
        DeleteDevHud,
        DeleteLogicTimer,
        DeleteFpsCounter,
        DeleteFrameTimer,
        DeleteMutex,
        DeleteUpdater,
    ];
    for m in [
        Gui, Scene, Input, Sound, Graphics, Resources, Physics, Ai, System,
    ] {
        v.push(DeleteModule(m));
    }
    v.extend([LogDeleteSetup, DeleteSetup, LogExitOk]);
    v
}

pub fn run_steps(host: &dyn StepHost, steps: &[Step]) {
    for s in steps {
        host.step(*s);
    }
}

pub trait StepHost {
    fn step(&self, s: Step);
}
