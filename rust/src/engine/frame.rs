//! `cFPSCounter` and the decision state of `cEngine::Run`. The host reads the clocks and does the
//! work; this decides what runs next. Times are the application time in milliseconds.

pub const MSG_APP_GOT_INPUT_FOCUS: i32 = 11;
pub const MSG_APP_GOT_MOUSE_FOCUS: i32 = 12;
pub const MSG_APP_GOT_VISIBILITY: i32 = 13;
pub const MSG_APP_LOST_INPUT_FOCUS: i32 = 14;
pub const MSG_APP_LOST_MOUSE_FOCUS: i32 = 15;
pub const MSG_APP_LOST_VISIBILITY: i32 = 16;

fn secs(now: u64) -> f32 {
    now as f32 / 1000.0
}

#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq)]
pub struct Fps {
    pub fps: f32,
    pub update_rate: f32,
    pub frame_counter: i32,
    pub frametime_start: f32,
    pub frametime: f32,
}

impl Fps {
    pub fn new(now: u64) -> Self {
        Self {
            fps: 60.0,
            update_rate: 1.0,
            frame_counter: 0,
            frametime_start: secs(now),
            frametime: 0.0,
        }
    }

    pub fn add_frame(&mut self, now: u64) {
        self.frame_counter = self.frame_counter.wrapping_add(1);
        self.frametime = secs(now) - self.frametime_start;
        if self.frametime >= self.update_rate {
            self.fps = self.frame_counter as f32 / self.frametime;
            self.frame_counter = 0;
            self.frametime_start = secs(now);
        }
    }

    pub fn avg_frame_ms(&self) -> f32 {
        (1.0 / self.fps) * 1000.0
    }
}

#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq)]
pub struct Loop {
    pub game_time: f64,
    pub num_of_times: f64,
    pub logic_ms: f64,
    pub swap_ms: f64,
    pub steps: i32,
    pub clear_check: i32,
    pub is_updated: bool,
    pub buffer_swap: bool,
    pub swapped_once: bool,
}

#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq)]
pub struct FrameTiming {
    pub logic_ms: f64,
    pub swap_ms: f64,
    pub steps: i32,
}

impl Default for Loop {
    fn default() -> Self {
        Self {
            game_time: 0.0,
            num_of_times: 0.0,
            logic_ms: 0.0,
            swap_ms: 0.0,
            steps: 0,
            clear_check: 0,
            is_updated: true,
            buffer_swap: false,
            swapped_once: false,
        }
    }
}

impl Loop {
    /// Run-local state is reset; game time and the update-log counter persist across runs.
    pub fn begin_run(&mut self) {
        *self = Self {
            game_time: self.game_time,
            clear_check: self.clear_check,
            ..Self::default()
        };
    }

    pub fn step_begin(&mut self) {
        self.steps = self.steps.wrapping_add(1);
    }

    pub fn step_end(&mut self) {
        self.is_updated = true;
    }

    pub fn pause_tick(&mut self) {
        self.is_updated = true;
    }

    pub fn advance_game_time(&mut self, step: f32) {
        self.game_time += f64::from(step);
    }

    /// True on every 20th update-log tick; the caller then checks for the "Default" container.
    pub fn update_log_tick(&mut self) -> bool {
        self.clear_check = self.clear_check.wrapping_add(1);
        self.clear_check.rem_euclid(20) == 0
    }

    pub fn add_logic_ms(&mut self, ms: f64) {
        self.logic_ms += ms;
    }

    pub fn add_swap_ms(&mut self, ms: f64) {
        self.swap_ms += ms;
    }

    pub fn skip_render_once(&self, render_once: bool) -> bool {
        render_once && self.swapped_once
    }

    /// True when a swap is due; the pending flag is consumed.
    pub fn take_swap(&mut self) -> bool {
        std::mem::take(&mut self.buffer_swap)
    }

    /// True when the loop must `continue` after the swap.
    pub fn swap_done(&mut self, render_once: bool) -> bool {
        self.swapped_once = true;
        render_once
    }

    pub fn want_render(&self, limit_fps: bool) -> bool {
        !limit_fps || self.is_updated
    }

    pub fn frame_rendered(&mut self) -> FrameTiming {
        let t = FrameTiming {
            logic_ms: self.logic_ms,
            swap_ms: self.swap_ms,
            steps: self.steps,
        };
        self.logic_ms = 0.0;
        self.swap_ms = 0.0;
        self.steps = 0;
        self.num_of_times += 1.0;
        self.is_updated = false;
        self.buffer_swap = true;
        t
    }

    pub fn mean_framerate(&self, elapsed_ms: u64) -> f64 {
        self.num_of_times / (elapsed_ms as f64 / 1000.0)
    }
}

pub fn frame_time(timer_sec: f64) -> f32 {
    timer_sec as f32
}

/// Messages for focus/visibility transitions in `CheckAndBroadcastFocusChange` order.
/// `had` and `now` are (input focus, mouse focus, visible).
pub fn focus_messages(had: [bool; 3], now: [bool; 3]) -> Vec<i32> {
    const MSGS: [(i32, i32); 3] = [
        (MSG_APP_LOST_INPUT_FOCUS, MSG_APP_GOT_INPUT_FOCUS),
        (MSG_APP_LOST_MOUSE_FOCUS, MSG_APP_GOT_MOUSE_FOCUS),
        (MSG_APP_LOST_VISIBILITY, MSG_APP_GOT_VISIBILITY),
    ];
    let mut out = Vec::new();
    for ((h, n), (lost, got)) in had.into_iter().zip(now).zip(MSGS) {
        if h && !n {
            out.push(lost);
        }
        if !h && n {
            out.push(got);
        }
    }
    out
}

/// `SetDevHudActive`: the argument or `OPENHPL_DEV_HUD=1...`.
pub fn dev_hud_active(requested: bool, env_first: Option<u8>) -> bool {
    requested || env_first == Some(b'1')
}

pub const VARIABLE_TYPES: [&str; 8] = [
    "Int", "Float", "Vector2", "Vector3", "Color", "String", "Enum", "Bool",
];

/// `GetEngineTypeFromString`: index in the table, or `eVariableType_LastEnum`.
pub fn engine_type_from_string(s: &[u8]) -> i32 {
    VARIABLE_TYPES
        .iter()
        .position(|t| t.as_bytes() == s)
        .map_or(VARIABLE_TYPES.len() as i32, |i| i as i32)
}
