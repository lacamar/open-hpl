//! `SomaImGui.cpp`: the immediate-mode UI's layout, focus, navigation, state and fades. The
//! script-owned widget structs, draw recording and script registrations stay C++; plain state
//! structs are edited in place through the layouts below.

use crate::math::clamp;

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct State {
    pub int: i32,
    pub float: f32,
    pub vec: [f32; 3],
    pub col: [f32; 4],
    pub set_int: bool,
    pub set_float: bool,
    pub set_vec: bool,
    pub set_col: bool,
    pub in_focus: bool,
    pub def_int: i32,
    pub def_float: f32,
    pub has_def_int: bool,
    pub has_def_float: bool,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Fade {
    pub ty: i32,
    pub start: [f32; 4],
    pub goal: [f32; 4],
    pub time: f32,
    pub count: f32,
    pub easing: i32,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Layout {
    pub ty: i32,
    pub start: [f32; 3],
    pub cursor: [f32; 3],
    pub size: [f32; 2],
    pub spacing: [f32; 2],
    pub line_max: f32,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Prev {
    pub pressed: bool,
    pub became_pressed: bool,
    pub in_focus: bool,
    pub became_in_focus: bool,
    pub was_in_focus: bool,
    pub mouse_over: bool,
    pub updated: bool,
    pub pos: [f32; 3],
    pub size: [f32; 2],
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct NavEntry {
    pub id: u64,
    pub pos: [i32; 2],
    pub size: [i32; 2],
    pub wrap: i32,
    pub group: i32,
}

/// Keyboard/gamepad and mouse focus bookkeeping of one UI.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct Focus {
    pub focus: u64,
    pub prev_focus: u64,
    pub last_dir: [i32; 2],
    pub last_count: i32,
    pub found_focus: bool,
    pub wrap_mode: i32,
    pub group_flags: i32,
    pub mouse_lock: i32,
}

/// The ten action slots (down / triggered / used by a widget this frame).
pub struct Actions<'a> {
    pub down: &'a mut [bool; 10],
    pub triggered: &'a mut [bool; 10],
    pub used: &'a mut [bool; 10],
}

impl Actions<'_> {
    fn use_action(&mut self, a: i32, check_used: bool) -> Option<usize> {
        let i = usize::try_from(a).ok().filter(|i| *i < 10)?;
        if check_used && self.used.get(i).copied().unwrap_or(false) {
            return None;
        }
        if let Some(u) = self.used.get_mut(i) {
            *u = true;
        }
        Some(i)
    }

    pub fn triggered(&mut self, a: i32, check_used: bool) -> bool {
        self.use_action(a, check_used)
            .is_some_and(|i| self.triggered.get(i).copied().unwrap_or(false))
    }

    pub fn is_down(&mut self, a: i32, check_used: bool) -> bool {
        self.use_action(a, check_used)
            .is_some_and(|i| self.down.get(i).copied().unwrap_or(false))
    }
}

fn fmax(a: f32, b: f32) -> f32 {
    if a < b { b } else { a }
}

/// `cImGui::GetDefaultOrCurrentInt`: a widget's default re-applies whenever the caller changes it.
pub fn default_or_current_int(s: &mut State, def: i32) -> i32 {
    if !s.set_int || (s.has_def_int && s.def_int != def) {
        s.int = def;
        s.set_int = true;
    }
    s.def_int = def;
    s.has_def_int = true;
    s.int
}

pub fn default_or_current_float(s: &mut State, def: f32) -> f32 {
    if !s.set_float || (s.has_def_float && s.def_float != def) {
        s.float = def;
        s.set_float = true;
    }
    s.def_float = def;
    s.has_def_float = true;
    s.float
}

/// Fade bookkeeping of `Begin`: the new count, and the eased fraction input when it has a time.
pub fn fade_advance(f: &mut Fade, dt: f32) -> Option<f32> {
    let c = f.count + dt;
    f.count = if f.time < c { f.time } else { c };
    (f.time > 0.0).then(|| f.count / f.time)
}

/// Blends the fade at eased position `t` into the state it drives.
pub fn fade_apply(f: &Fade, t: f32, s: &mut State) {
    let mut v = [0.0f32; 4];
    for ((v, a), b) in v.iter_mut().zip(&f.start).zip(&f.goal) {
        *v = (b - a).mul_add(t, *a);
    }
    match f.ty {
        0 => {
            s.float = v[0];
            s.set_float = true;
        }
        1 => {
            s.vec = [v[0], v[1], v[2]];
            s.set_vec = true;
        }
        _ => {
            s.col = v;
            s.set_col = true;
        }
    }
}

/// An unfinished fade is kept unless the caller replaces it.
pub fn fade_blocked(existing: Option<&Fade>, replace: bool) -> bool {
    existing.is_some_and(|e| !replace && e.count < e.time)
}

pub fn fade_make(s: &State, ty: i32, goal: [f32; 4], time: f32, easing: i32) -> Fade {
    let start = match ty {
        1 => [s.vec[0], s.vec[1], s.vec[2], 0.0],
        2 => s.col,
        _ => [s.float, 0.0, 0.0, 0.0],
    };
    Fade {
        ty,
        start,
        goal,
        time,
        count: 0.0,
        easing,
    }
}

/// Mouse movement mapped to the UI's virtual space; returns (position, relative, cursor3d).
pub struct MouseIn {
    pub screen: [f32; 2],
    pub virtual_size: [f32; 2],
    pub virtual_offset: [f32; 2],
    pub is_3d: bool,
    pub cursor_3d: [f32; 2],
}

pub fn mouse_map(i: &MouseIn, pos: [i32; 2], rel: [i32; 2]) -> ([f32; 2], [f32; 2], [f32; 2]) {
    let (vs, vv) = (i.screen, i.virtual_size);
    if i.is_3d {
        // screens in the world: the captured mouse moves a cursor across the screen
        let scale = vv[1] / vs[1] * 1.5;
        let r = [rel[0] as f32 * scale, rel[1] as f32 * scale];
        let c = [
            clamp(i.cursor_3d[0] + r[0], 0.0, vv[0]),
            clamp(i.cursor_3d[1] + r[1], 0.0, vv[1]),
        ];
        return (c, r, c);
    }
    let scale = [vv[0] / vs[0], vv[1] / vs[1]];
    (
        [
            (pos[0] as f32).mul_add(scale[0], -i.virtual_offset[0]),
            (pos[1] as f32).mul_add(scale[1], -i.virtual_offset[1]),
        ],
        [rel[0] as f32 * scale[0], rel[1] as f32 * scale[1]],
        i.cursor_3d,
    )
}

/// Fills in the default size and offsets the position by the cursor; `align` names the corner.
pub fn layout(
    pos: &mut [f32; 3],
    size: &mut [f32; 2],
    default_size: [f32; 2],
    origin: [f32; 3],
    do_align: bool,
    align_mode: i32,
) {
    if size[0] < 0.0 {
        size[0] = default_size[0];
    }
    if size[1] < 0.0 {
        size[1] = default_size[1];
    }
    for (p, o) in pos.iter_mut().zip(origin) {
        *p += o;
    }
    if do_align {
        *pos = align(*pos, *size, align_mode);
    }
}

/// The position names the given corner/centre of the widget.
pub fn align(pos: [f32; 3], size: [f32; 2], mode: i32) -> [f32; 3] {
    const TABLE: [[f32; 2]; 9] = [
        [0.0, 0.0],
        [0.0, 1.0],
        [0.0, 0.5],
        [1.0, 0.0],
        [1.0, 1.0],
        [1.0, 0.5],
        [0.5, 0.5],
        [0.5, 0.0],
        [0.5, 1.0],
    ];
    let Some(a) = usize::try_from(mode)
        .ok()
        .filter(|m| *m > 0)
        .and_then(|m| TABLE.get(m))
    else {
        return pos;
    };
    [
        size[0].mul_add(-a[0], pos[0]),
        size[1].mul_add(-a[1], pos[1]),
        pos[2],
    ]
}

/// Moves the layout cursor past the widget just placed.
pub fn advance(l: &mut Layout, pos: [f32; 3], size: [f32; 2]) {
    match l.ty {
        3 => l.cursor[1] = pos[1] + size[1] + l.spacing[1],
        1 => l.cursor[0] = pos[0] + size[0] + l.spacing[0],
        2 => {
            if l.spacing[0] > 0.1 {
                l.cursor[0] = (size[0] / l.spacing[0])
                    .ceil()
                    .mul_add(l.spacing[0], l.cursor[0]);
            }
        }
        _ => {
            l.cursor[0] = pos[0] + size[0] + l.spacing[0];
            l.line_max = fmax(l.line_max, size[1]);
            if l.size[0] > 0.0 && l.cursor[0] > l.start[0] + l.size[0] {
                l.cursor[0] = l.start[0];
                l.cursor[1] += l.line_max + l.spacing[1];
                l.line_max = 0.0;
            }
        }
    }
}

pub fn mouse_over(
    use_input: bool,
    has_focus: bool,
    mouse: [f32; 2],
    pos: [f32; 3],
    size: [f32; 2],
) -> bool {
    if !use_input || !has_focus {
        return false;
    }
    mouse[0] >= pos[0]
        && mouse[1] >= pos[1]
        && mouse[0] < pos[0] + size[0]
        && mouse[1] < pos[1] + size[1]
}

#[repr(C)]
pub struct WidgetIn {
    pub use_input: bool,
    pub show_mouse: bool,
    pub has_focus: bool,
    pub first_run: bool,
    pub use_ui_pos: bool,
    pub mouse_rel: [f32; 2],
    pub exp_hori: [f32; 2],
    pub exp_vert: [f32; 2],
    pub pos: [f32; 3],
    pub size: [f32; 2],
    pub mouse: [f32; 2],
}

#[repr(C)]
pub struct WidgetOut {
    pub in_focus: bool,
    pub mouse_over: bool,
    pub has_nav: bool,
    pub nav: NavEntry,
}

/// `cImGui::DoWidgetBase`: the mouse takes focus only when it moves or clicks, the earliest drawn
/// widget wins. Updates the widget's state and the focus bookkeeping.
pub fn widget_base(
    id: u64,
    i: &WidgetIn,
    f: &mut Focus,
    act: &mut Actions<'_>,
    st: &mut State,
    prev: &mut Prev,
) -> WidgetOut {
    let mut over = false;
    let mut nav = None;
    if i.use_input {
        if !i.show_mouse || f.mouse_lock > 0 || !i.has_focus {
            if f.focus == id {
                f.found_focus = true;
            }
        } else {
            if f.focus == 0 && i.first_run && !f.found_focus {
                f.focus = id;
                f.found_focus = true;
            }
            over = mouse_over(i.use_input, i.has_focus, i.mouse, i.pos, i.size);
            let trig = |a: &Actions<'_>, k: usize| a.triggered.get(k).copied().unwrap_or(false);
            let act_moved =
                i.mouse_rel[0] != 0.0 || i.mouse_rel[1] != 0.0 || trig(act, 1) || trig(act, 0);
            if over && act_moved && !f.found_focus {
                f.focus = id;
                f.found_focus = true;
            } else if !over && act_moved && f.focus == id {
                f.focus = 0;
            }
        }
        if i.use_ui_pos {
            let (h, v) = (i.exp_hori, i.exp_vert);
            nav = Some(NavEntry {
                id,
                pos: [
                    (-h[0]).mul_add(i.size[0], i.pos[0]) as i32,
                    (-v[0]).mul_add(i.size[1], i.pos[1]) as i32,
                ],
                size: [
                    (h[0] + h[1]).mul_add(i.size[0], i.size[0]) as i32,
                    (v[0] + v[1]).mul_add(i.size[1], i.size[1]) as i32,
                ],
                wrap: f.wrap_mode,
                group: f.group_flags,
            });
        }
    }
    let in_focus = i.use_input && f.focus == id;
    prev.was_in_focus = st.in_focus && !in_focus;
    prev.became_in_focus = in_focus && !st.in_focus;
    prev.in_focus = in_focus;
    st.in_focus = in_focus;
    prev.mouse_over = over;
    WidgetOut {
        in_focus,
        mouse_over: over,
        has_nav: nav.is_some(),
        nav: nav.unwrap_or_default(),
    }
}

pub fn became_pressed(act: &mut Actions<'_>, keys: bool, mouse: bool) -> bool {
    let mut b = false;
    if keys {
        b = act.triggered(2, false);
    }
    if mouse {
        b = act.triggered(1, false) || b;
    }
    b
}

/// `cImGui::GetClosestUIPos`: distance along the move axis between top-left corners, lanes must
/// overlap.
fn nav_closest(
    nav: &[NavEntry],
    f: &Focus,
    cur: &NavEntry,
    dir: [i32; 2],
    ahead: bool,
    loose: bool,
) -> u64 {
    let mv = usize::from(dir[0] == 0);
    let perp = 1 - mv;
    let at = |a: &[i32; 2], k: usize| a.get(k).copied().unwrap_or(0);
    let mut best: Option<&NavEntry> = None;
    let mut best_dist = 0i32;
    for e in nav {
        if e.id == f.focus || e.group & cur.group == 0 {
            continue;
        }
        let (lo, ext) = if loose {
            (
                at(&e.pos, perp).wrapping_sub(at(&e.size, perp).wrapping_div(2)),
                at(&e.size, perp).wrapping_mul(2),
            )
        } else {
            (at(&e.pos, perp), at(&e.size, perp))
        };
        let overlap = lo < at(&cur.pos, perp).wrapping_add(at(&cur.size, perp))
            && at(&cur.pos, perp) < lo.wrapping_add(ext);
        if !overlap {
            continue;
        }
        let delta = at(&e.pos, mv).wrapping_sub(at(&cur.pos, mv));
        if (delta.wrapping_mul(at(&dir, mv)) > 0) != ahead {
            continue;
        }
        let dist = delta.wrapping_abs();
        if best.is_none()
            || (if ahead {
                dist < best_dist
            } else {
                dist > best_dist
            })
        {
            best = Some(e);
            best_dist = dist;
        }
    }
    best.map_or(0, |e| e.id)
}

/// `UpdateUIMovement`: moves the focus along the registered nav rects at the end of a frame.
pub fn update_ui_movement(nav: &[NavEntry], f: &mut Focus, act: &mut Actions<'_>) {
    const DIRS: [[i32; 3]; 4] = [[4, 0, -1], [5, 0, 1], [6, 1, 0], [7, -1, 0]];
    let mut dir = [0i32, 0];
    for d in DIRS {
        if act.triggered(d[0], true) && dir[0] == 0 && dir[1] == 0 {
            dir = [d[1], d[2]];
        }
    }
    let confirm = act.triggered(2, true);
    let moved = dir[0] != 0 || dir[1] != 0;
    if (!moved && !confirm) || nav.is_empty() {
        return;
    }
    let find = |id: u64| nav.iter().find(|e| e.id == id);
    let Some(cur) = find(f.focus) else {
        if let Some(first) = nav.first() {
            f.focus = first.id;
            f.prev_focus = first.id;
        }
        f.last_dir = [0, 0];
        f.last_count = 0;
        return;
    };
    if nav.len() < 2 || !moved {
        return;
    }
    let target;
    if nav.len() as i32 == f.last_count
        && f.last_dir[0] == -dir[0]
        && f.last_dir[1] == -dir[1]
        && find(f.prev_focus).is_some()
    {
        target = f.prev_focus;
    } else {
        let hori = dir[0] != 0;
        let wrap = cur.wrap == 3 || (cur.wrap == 1 && hori) || (cur.wrap == 2 && !hori);
        let mut t = nav_closest(nav, f, cur, dir, true, false);
        if t == 0 && wrap {
            t = nav_closest(nav, f, cur, dir, false, false);
        }
        if t == 0 {
            t = nav_closest(nav, f, cur, dir, true, true);
        }
        if t == 0 && wrap {
            t = nav_closest(nav, f, cur, dir, false, true);
        }
        target = t;
    }
    if target != 0 {
        f.prev_focus = f.focus;
        f.focus = target;
        f.last_dir = dir;
        f.last_count = nav.len() as i32;
    }
}
