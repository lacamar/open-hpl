//! `cLuxMainMenu`: fades, exit flow, window switching and the top menu plan.

use crate::math::clamp;

pub const WIN_LAST: i32 = 7;
pub const EXIT_QUIT_GAME: i32 = 0;
pub const EXIT_RETURN: i32 = 1;
pub const EXIT_START: i32 = 2;
pub const EXIT_CONTINUE: i32 = 3;
pub const EXIT_TO_MENU: i32 = 4;
pub const EXIT_AND_SAVE: i32 = 5;
pub const WIN_CUSTOM: i32 = 5;

#[repr(C)]
#[derive(Clone, Copy)]
pub struct Menu {
    pub fade_alpha: f32,
    pub top_alpha: f32,
    pub desc_alpha: f32,
    pub exiting: bool,
    pub fade_desc: bool,
    pub top_visible: bool,
    pub exit_msg: i32,
    pub window: i32,
}

impl Menu {
    /// C++ left the fade members, `mbFadeInDescription` and `mfDescriptionAlpha` uninitialised.
    pub fn new() -> Menu {
        Menu {
            fade_alpha: 0.0,
            top_alpha: 0.0,
            desc_alpha: 0.0,
            exiting: false,
            fade_desc: false,
            top_visible: false,
            exit_msg: 0,
            window: WIN_LAST,
        }
    }

    pub fn enter(&mut self) {
        self.fade_alpha = 1.0;
        self.exiting = false;
        self.window = WIN_LAST;
    }
}

impl Default for Menu {
    fn default() -> Menu {
        Menu::new()
    }
}

#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct Base {
    pub fov: f32,
    pub set_fov: bool,
    pub exit: bool,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct Times {
    pub fade_in: f32,
    pub out_fast: f32,
    pub out_slow: f32,
    pub fov: f32,
    pub zoomed_fov: f32,
}

pub fn update_base(s: &mut Menu, dt: f32, t: &Times, has_camera: bool) -> Base {
    let mut o = Base::default();
    if s.exiting {
        let zoom = s.exit_msg == EXIT_START || s.exit_msg == EXIT_CONTINUE;
        let speed = if zoom { t.out_slow } else { t.out_fast };
        if zoom && has_camera {
            o.set_fov = true;
            o.fov = t
                .fov
                .mul_add(1.0 - s.fade_alpha, t.zoomed_fov * s.fade_alpha);
        }
        if s.fade_alpha < 1.0 {
            s.fade_alpha = dt.mul_add(1.0 / speed, s.fade_alpha);
            if s.fade_alpha > 1.0 {
                s.fade_alpha = 1.0;
            }
        } else {
            o.exit = true;
        }
    } else if s.fade_alpha > 0.0 {
        s.fade_alpha = (-dt).mul_add(1.0 / t.fade_in, s.fade_alpha);
        if s.fade_alpha < 0.0 {
            s.fade_alpha = 0.0;
        }
    }
    o
}

/// `save_focus`: -1 no Save label, 0 unfocused, 1 focused. Returns whether the label alpha changed.
pub fn update_top(s: &mut Menu, dt: f32, fade_in: f32, fade_out: f32, save_focus: i32) -> bool {
    let mut changed = false;
    if s.top_visible && !s.exiting {
        if s.top_alpha < 1.0 {
            s.top_alpha = dt.mul_add(1.0 / fade_in, s.top_alpha);
            if s.top_alpha > 1.0 {
                s.top_alpha = 1.0;
            }
            changed = true;
        }
    } else if s.top_alpha > 0.0 {
        s.top_alpha = (-dt).mul_add(1.0 / fade_out, s.top_alpha);
        if s.top_alpha < 0.0 {
            s.top_alpha = 0.0;
        }
        changed = true;
    }
    if save_focus >= 0 {
        s.fade_desc = save_focus == 1;
    }
    if s.fade_desc {
        s.desc_alpha += dt;
        if s.desc_alpha > 1.0 {
            s.desc_alpha = 1.0;
        }
    } else {
        s.desc_alpha -= dt;
        if s.desc_alpha < 0.5 {
            s.desc_alpha = 0.5;
        }
    }
    changed
}

/// -2 when `win` is already current, else the window to deactivate (`WIN_LAST` for none).
pub fn win_begin(s: &Menu, win: i32) -> i32 {
    if s.window == win { -2 } else { s.window }
}

/// True when the top menu becomes visible, false when `win` is to be activated.
pub fn win_commit(s: &mut Menu, win: i32) -> bool {
    s.window = win;
    win == WIN_LAST
}

pub fn draw_focus_window(win: i32) -> bool {
    matches!(win, 0..=3 | 6)
}

pub const EXIT_NONE: i32 = 0;
pub const EXIT_RETURN_TO_GAME: i32 = 1;
pub const EXIT_WINDOW: i32 = 2;

pub fn exit_pressed(s: &Menu, map_loaded: bool) -> i32 {
    if s.window == WIN_LAST {
        if map_loaded {
            EXIT_RETURN_TO_GAME
        } else {
            EXIT_NONE
        }
    } else {
        EXIT_WINDOW
    }
}

pub const XM_ZOOM_SOUND: i32 = 1;
pub const XM_ACHIEVEMENT: i32 = 2;

pub fn exit_menu(s: &mut Menu, msg: i32) -> i32 {
    s.exiting = true;
    s.exit_msg = msg;
    let mut f = 0;
    if msg == EXIT_CONTINUE || msg == EXIT_START {
        f |= XM_ZOOM_SOUND;
    }
    if msg == EXIT_TO_MENU || msg == EXIT_AND_SAVE {
        f |= XM_ACHIEVEMENT;
    }
    f
}

pub const OME_NONE: i32 = 0;
pub const OME_DEMO_END: i32 = 1;
pub const OME_ENGINE_EXIT: i32 = 2;
pub const OME_RETURN: i32 = 3;
pub const OME_START: i32 = 4;
pub const OME_CONTINUE: i32 = 5;
pub const OME_TO_MENU: i32 = 6;
pub const OME_AND_SAVE: i32 = 7;

pub fn on_menu_exit(msg: i32, demo_on_all_exit: bool) -> i32 {
    match msg {
        EXIT_QUIT_GAME => {
            if demo_on_all_exit {
                OME_DEMO_END
            } else {
                OME_ENGINE_EXIT
            }
        }
        EXIT_RETURN => OME_RETURN,
        EXIT_START => OME_START,
        EXIT_CONTINUE => OME_CONTINUE,
        EXIT_TO_MENU => OME_TO_MENU,
        EXIT_AND_SAVE => OME_AND_SAVE,
        _ => OME_NONE,
    }
}

pub const UI_PRIMARY: i32 = 1;
pub const UI_SECONDARY: i32 = 2;

/// `TopMenuTextPress`: bit test, primary first. 0 none, 1 press, 2 exit.
pub fn top_press(val: i32) -> i32 {
    if val & UI_PRIMARY != 0 {
        1
    } else if val & UI_SECONDARY != 0 {
        2
    } else {
        0
    }
}

pub const L_CONTINUE: i32 = 0;
pub const L_BACK: i32 = 1;
pub const L_START: i32 = 2;
pub const L_LOAD: i32 = 3;
pub const L_SAVE: i32 = 4;
pub const L_CUSTOM: i32 = 5;
pub const L_OPTIONS: i32 = 6;
pub const L_PROFILE: i32 = 7;
pub const L_EXIT: i32 = 8;
pub const L_TO_MENU: i32 = 9;
pub const L_AND_SAVE: i32 = 10;

pub const P_PTEST: u32 = 1;
pub const P_MAP: u32 = 2;
pub const P_SAVE_EXISTS: u32 = 4;
pub const P_HARD: u32 = 8;
pub const P_EXIT_DIRECT: u32 = 16;
pub const P_DEAD: u32 = 32;
pub const P_NO_CUSTOM: u32 = 64;

pub fn plan(flags: u32) -> Vec<i32> {
    let f = |b: u32| flags & b != 0;
    let (ptest, map, save, hard) = (f(P_PTEST), f(P_MAP), f(P_SAVE_EXISTS), f(P_HARD));
    let mut v = Vec::new();
    if !ptest && !map && save {
        v.push(L_CONTINUE);
    }
    v.push(if map { L_BACK } else { L_START });
    if !ptest && save && !map {
        v.push(L_LOAD);
    }
    if !ptest && hard && map {
        v.push(L_SAVE);
    }
    if !f(P_NO_CUSTOM) && !ptest && !map {
        v.push(L_CUSTOM);
    }
    v.push(L_OPTIONS);
    if !map {
        v.push(L_PROFILE);
    }
    if !map || f(P_EXIT_DIRECT) {
        v.push(L_EXIT);
    } else {
        v.push(L_TO_MENU);
        if !ptest && !f(P_DEAD) && !hard {
            v.push(L_AND_SAVE);
        }
    }
    v
}

/// (font size multiplier, row advance) from the label lengths and their char counts.
pub fn font_fit(lengths: &[f32], chars: &[i32], screen_x: f32, font: [f32; 2]) -> (f32, f32) {
    let mut max_len = 0.0f32;
    let mut longest = 0i32;
    for (l, c) in lengths.iter().zip(chars) {
        if max_len < *l {
            max_len = *l;
            longest = *c;
        }
    }
    let safe = screen_x * 0.35;
    let mut mul = 1.0f32;
    if max_len > safe {
        let char_w = safe / longest as f32;
        mul = char_w / font[0];
        mul = clamp(mul, 0.7, mul);
    }
    (mul, font[1] * mul * 1.3)
}

/// Replaces the first `#` of `text` by `num`. None without a `#` (C++ `replace` at `npos` threw
/// `out_of_range`).
pub fn subst_hash(text: &[u32], num: i64) -> Option<Vec<u32>> {
    let i = text.iter().position(|&c| c == u32::from(b'#'))?;
    let mut out: Vec<u32> = text.iter().take(i).copied().collect();
    out.extend(num.to_string().bytes().map(u32::from));
    out.extend(text.iter().skip(i + 1).copied());
    Some(out)
}

/// `ClickedSaveGamePopup`: tinderboxes left after paying 4, or None when short.
pub fn pay_save(n: i32) -> Option<i32> {
    if n < 4 { None } else { Some((n - 4).max(0)) }
}

pub fn popup_ok(val: i32) -> bool {
    val == 0
}
