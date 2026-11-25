//! Test double for the C++ host: a recording widget tree and game state.

use super::host::{Sink, Str, W, WStr, ws};
use crate::luxmap::mhost::slice_of as slice;
use std::cell::RefCell;
use std::collections::HashMap;
use std::ffi::c_void;

#[derive(Clone, Default, Debug)]
pub struct Widget {
    pub kind: i32,
    pub text: WStr,
    pub pos: [f32; 3],
    pub size: [f32; 2],
    pub visible: bool,
    pub enabled: bool,
    pub color: [f32; 4],
    pub nav: Vec<(i32, usize)>,
    pub cbs: Vec<(i32, usize)>,
    pub items: Vec<(WStr, usize)>,
    pub selected: i32,
    pub locked: bool,
    pub user: usize,
    pub popup: bool,
    pub image: WStr,
}

#[derive(Default)]
pub struct State {
    pub widgets: Vec<Widget>,
    pub log: Vec<String>,
    pub lux: HashMap<i32, usize>,
    pub focused: usize,
    pub default_focus: usize,
    pub attention: usize,
    pub draw_focus: bool,
    pub files: Vec<WStr>,
    pub folders: Vec<WStr>,
    pub dates: HashMap<WStr, [i32; 8]>,
    pub stories: Vec<(WStr, WStr, WStr)>,
    pub lux_path: HashMap<i32, WStr>,
    pub folder_ops: Vec<(i32, WStr)>,
    pub existing: Vec<WStr>,
    pub cfg: HashMap<String, [f32; 2]>,
    pub autosize: [f32; 2],
    pub text_width: f32,
    pub story_next: usize,
}

thread_local! {
    pub static ST: RefCell<State> = RefCell::new(State::default());
}

pub fn with<R>(f: impl FnOnce(&mut State) -> R) -> R {
    ST.with(|s| f(&mut s.borrow_mut()))
}

pub fn reset() {
    with(|s| *s = State::default());
}

fn wid(w: W) -> usize {
    w.addr()
}

fn id(i: usize) -> W {
    W::from_addr(i)
}

fn wd(s: &mut State, w: W) -> Option<&mut Widget> {
    s.widgets.get_mut(wid(w).wrapping_sub(1))
}

fn emit(sink: Sink, ctx: *mut c_void, t: &[u32]) {
    // SAFETY: the sink is one of the host module's collectors and `ctx` its target.
    unsafe { sink(ctx, t.as_ptr(), t.len()) };
}

#[allow(clippy::too_many_arguments)]
pub(super) unsafe fn ohpl_a9h_new(
    _set: W,
    kind: i32,
    _parent: W,
    pos: *const f32,
    size: *const f32,
    _flags: i32,
    t: *const u32,
    n: usize,
) -> W {
    let (p, sz) = (slice(pos, 3), slice(size, 2));
    with(|s| {
        s.widgets.push(Widget {
            kind,
            text: slice(t, n).to_vec(),
            pos: [p[0], p[1], p[2]],
            size: if sz[0] < 0.0 {
                s.autosize
            } else {
                [sz[0], sz[1]]
            },
            visible: true,
            enabled: true,
            selected: -1,
            ..Widget::default()
        });
        s.log.push(format!("new {kind}"));
        id(s.widgets.len())
    })
}

pub(super) unsafe fn ohpl_a9h_popup(
    _set: W,
    st: *const Str,
    obj: usize,
    _kill: i32,
    _copy: bool,
) -> W {
    let strs = slice(st, 4);
    with(|s| {
        s.widgets.push(Widget {
            kind: 1,
            text: slice(strs[1].p, strs[1].n).to_vec(),
            visible: true,
            enabled: true,
            popup: true,
            user: obj,
            ..Widget::default()
        });
        s.log.push("popup".into());
        id(s.widgets.len())
    })
}

pub(super) unsafe fn ohpl_a9h_set(_set: W, op: i32, w: W, arg: usize) -> usize {
    with(|s| {
        s.log.push(format!("set {op} {} {arg}", wid(w)));
        match op {
            0 => s.draw_focus = arg != 0,
            1 => return usize::from(s.draw_focus),
            2 => s.focused = wid(w),
            3 => s.default_focus = wid(w),
            4 => return s.widgets.iter().filter(|x| x.popup).count(),
            11 => s.attention = wid(w),
            12 => return s.attention,
            _ => {}
        }
        0
    })
}

pub(super) unsafe fn ohpl_a9h_w(w: W, op: i32, arg: usize) -> usize {
    with(|s| {
        let Some(x) = wd(s, w) else { return 0 };
        match op {
            0 => x.enabled = arg != 0,
            1 => x.visible = arg != 0,
            2 => return usize::from(x.visible),
            6 => x.user = arg,
            16 => {
                x.items.clear();
                x.selected = -1;
            }
            17 => return x.items.len(),
            18 => return x.selected as usize,
            19 => x.selected = arg as i32,
            20 => x.locked = arg != 0,
            21 => return usize::from(x.locked),
            23 => return x.items.get(arg).map_or(0, |i| i.1),
            _ => {}
        }
        0
    })
}

pub(super) unsafe fn ohpl_a9h_w_set_f(w: W, op: i32, v: *const f32) {
    with(|s| {
        if let Some(x) = wd(s, w) {
            let v = slice(v, 4);
            match op {
                0 => x.pos = [v[0], v[1], v[2]],
                1 => x.size = [v[0], v[1]],
                2 => x.color = [v[0], v[1], v[2], v[3]],
                _ => {}
            }
        }
    })
}

pub(super) unsafe fn ohpl_a9h_w_get_f(w: W, op: i32, out: *mut f32) {
    with(|s| {
        if let Some(x) = wd(s, w) {
            let o: &mut [f32] = if op == 0 {
                &mut x.pos[..]
            } else {
                &mut x.size[..]
            };
            // SAFETY: the caller passes room for 3 (position) or 2 (size) floats.
            let dst = unsafe { std::slice::from_raw_parts_mut(out, o.len()) };
            dst.copy_from_slice(o);
        }
    })
}

pub(super) unsafe fn ohpl_a9h_w_str(w: W, op: i32, t: *const u32, n: usize, arg: usize) -> usize {
    with(|s| {
        if let Some(x) = wd(s, w) {
            match op {
                0 => x.text = slice(t, n).to_vec(),
                2 => x.items.push((slice(t, n).to_vec(), arg)),
                _ => {}
            }
        }
        0
    })
}

pub(super) unsafe fn ohpl_a9h_w_get_str(w: W, op: i32, idx: i32, sink: Sink, ctx: *mut c_void) {
    let t = with(|s| {
        wd(s, w).map(|x| {
            if op == 0 {
                x.text.clone()
            } else {
                x.items
                    .get(idx as usize)
                    .map(|i| i.0.clone())
                    .unwrap_or_default()
            }
        })
    });
    emit(sink, ctx, &t.unwrap_or_default());
}

pub(super) unsafe fn ohpl_a9h_w_cb(w: W, msg: i32, obj: usize) {
    with(|s| {
        if let Some(x) = wd(s, w) {
            x.cbs.push((msg, obj));
        }
    })
}

pub(super) unsafe fn ohpl_a9h_w_nav(w: W, dir: i32, target: W) {
    with(|s| {
        if let Some(x) = wd(s, w) {
            x.nav.push((dir, wid(target)));
        }
    })
}

pub(super) unsafe fn ohpl_a9h_text_width(_w: W, _t: *const u32, _n: usize) -> f32 {
    with(|s| s.text_width)
}

pub(super) unsafe fn ohpl_a9h_image_file(w: W, t: *const u32, n: usize) {
    with(|s| {
        if let Some(x) = wd(s, w) {
            x.image = slice(t, n).to_vec();
        }
    })
}

pub(super) unsafe fn ohpl_a9h_lux(op: i32, arg: usize) -> usize {
    with(|s| {
        s.log.push(format!("lux {op} {arg}"));
        if op == 1 {
            s.lux.insert(0, arg);
        }
        s.lux.get(&op).copied().unwrap_or(0)
    })
}

pub(super) unsafe fn ohpl_a9h_lux_str(op: i32, sink: Sink, ctx: *mut c_void) {
    let t = with(|s| s.lux_path.get(&op).cloned().unwrap_or_default());
    emit(sink, ctx, &t);
}

pub(super) unsafe fn ohpl_a9h_lux_with(op: i32, t: *const u32, n: usize) -> usize {
    let name = slice(t, n).to_vec();
    with(|s| {
        s.log.push(format!("lux_with {op} {}", name.len()));
        usize::from(op == 0 && !s.existing.contains(&name))
    })
}

pub(super) unsafe fn ohpl_a9h_lux_with_str(
    _op: i32,
    t: *const u32,
    n: usize,
    sink: Sink,
    ctx: *mut c_void,
) {
    emit(sink, ctx, slice(t, n));
}

pub(super) unsafe fn ohpl_a9h_plat_list(
    kind: i32,
    _dir: *const u32,
    _dir_n: usize,
    _mask: *const u32,
    _mask_n: usize,
    sink: Sink,
    ctx: *mut c_void,
) {
    let l = with(|s| {
        if kind == 0 {
            s.files.clone()
        } else {
            s.folders.clone()
        }
    });
    for t in &l {
        emit(sink, ctx, t);
    }
}

pub(super) unsafe fn ohpl_a9h_plat_date(_kind: i32, path: *const u32, n: usize, out: *mut i32) {
    let p = slice(path, n);
    let d = with(|s| {
        s.dates
            .iter()
            .find(|(k, _)| p.ends_with(k))
            .map(|(_, v)| *v)
            .unwrap_or_default()
    });
    // SAFETY: the caller passes room for 8 ints.
    unsafe { std::slice::from_raw_parts_mut(out, 8) }.copy_from_slice(&d);
}

pub(super) unsafe fn ohpl_a9h_plat_folder(op: i32, path: *const u32, n: usize) -> usize {
    let p = slice(path, n).to_vec();
    with(|s| {
        s.folder_ops.push((op, p.clone()));
        usize::from(match op {
            0 => s.existing.contains(&p),
            _ => true,
        })
    })
}

pub(super) unsafe fn ohpl_a9h_cfg(
    op: i32,
    sec: *const u8,
    sec_n: usize,
    key: *const u8,
    key_n: usize,
    out: *mut f32,
) -> usize {
    let k = format!(
        "{}/{}",
        String::from_utf8_lossy(slice(sec, sec_n)),
        String::from_utf8_lossy(slice(key, key_n))
    );
    let v = with(|s| s.cfg.get(&k).copied().unwrap_or_default());
    // SAFETY: the caller passes room for 2 floats.
    let o = unsafe { std::slice::from_raw_parts_mut(out, 2) };
    o[0] = v[0];
    o[1] = if op == 1 { v[1] } else { 0.0 };
    1
}

pub(super) unsafe fn ohpl_a9h_tr(
    cat: *const u8,
    cat_n: usize,
    key: *const u8,
    key_n: usize,
    sink: Sink,
    ctx: *mut c_void,
) {
    let t = format!(
        "{}/{}",
        String::from_utf8_lossy(slice(cat, cat_n)),
        String::from_utf8_lossy(slice(key, key_n))
    );
    emit(sink, ctx, &ws(&t));
}

pub(super) unsafe fn ohpl_a9h_story_new(_path: *const u32, _n: usize) -> W {
    with(|s| {
        s.log.push("story_new".into());
        s.story_next += 1;
        if s.story_next <= s.stories.len() {
            id(s.story_next)
        } else {
            W::NULL
        }
    })
}

pub(super) unsafe fn ohpl_a9h_story_str(story: W, op: i32, sink: Sink, ctx: *mut c_void) {
    let t = with(|s| {
        s.stories.get(wid(story).wrapping_sub(1)).map(|x| match op {
            0 => x.0.clone(),
            1 => x.1.clone(),
            _ => x.2.clone(),
        })
    });
    emit(sink, ctx, &t.unwrap_or_default());
}

pub(super) unsafe fn ohpl_a9h_story_activate(_story: W) {
    with(|s| s.log.push("story_activate".into()));
}

pub(super) unsafe fn ohpl_a9h_story_free(_story: W) {
    with(|s| s.log.push("story_free".into()));
}
