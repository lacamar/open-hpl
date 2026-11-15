//! `OHPL_S8_DUMP=<file>`: one line per host call, `op args [= results]`, pointers numbered by first
//! appearance. A traced run also uses a fixed 1/60 step, a clock that advances 16 ms per read and
//! a forced Return on the gamma screen after `OHPL_S8_AUTOKEY` key polls, so two dumps of the same
//! session agree. The C++ before the port logged the same lines (`scripts/s8-scenario.sh`).

use super::s8::op;
use crate::luxmenu2::host::{Arg, Res};
use std::collections::HashMap;
use std::io::Write;
use std::sync::{Mutex, OnceLock};

#[derive(Clone, Copy)]
enum R {
    None,
    P,
    B,
    I(usize),
    F(usize),
    Strs,
}

const OPS: &[(i32, &str, R)] = &[
    (op::SET_NEW, "set_new", R::P),
    (op::SET_DRAW_MOUSE, "set_draw_mouse", R::None),
    (op::SET_ACTIVE, "set_active", R::None),
    (op::GUI_FOCUS, "gui_focus", R::None),
    (op::GUI_FOCUSED_IS, "gui_focused_is", R::B),
    (op::VIEWPORT_NEW, "viewport_new", R::P),
    (op::VIEWPORT_ADD_SET, "viewport_add_set", R::None),
    (op::VIEWPORT_ACTIVE, "viewport_active", R::None),
    (op::GFX_RECT, "gfx_rect", R::P),
    (op::GFX_TEX, "gfx_tex", R::P),
    (op::GFX_SIZE, "gfx_size", R::F(2)),
    (op::GFX_DESTROY, "gfx_destroy", R::None),
    (op::DRAW_GFX, "draw_gfx", R::None),
    (op::DRAW_FONT, "draw_font", R::None),
    (op::FONT_NEW, "font_new", R::P),
    (op::FONT_WRAP, "font_wrap", R::Strs),
    (op::CLIP_NEW, "clip_new", R::P),
    (op::CLIP_RECT, "clip_rect", R::None),
    (op::CLIP_GET, "clip_get", R::P),
    (op::CLIP_SET, "clip_set", R::None),
    (op::SLIDER_NEW, "slider_new", R::P),
    (op::BUTTON_NEW, "button_new", R::P),
    (op::SLIDER_SET, "slider_set", R::None),
    (op::SLIDER_GET, "slider_get", R::I(2)),
    (op::DEFAULT_FOCUS_NAV, "default_focus_nav", R::None),
    (op::FOCUSED_WIDGET, "focused_widget", R::None),
    (op::MUSIC, "music", R::None),
    (op::SCREEN, "screen", R::F(2)),
    (op::HUD_IMGUI, "hud_imgui", R::None),
    (op::BASE_SPLASH_DONE, "base_splash_done", R::None),
    (op::BASE_GAMMA_DONE, "base_gamma_done", R::None),
    (op::SAVE_GAMMA, "save_gamma", R::None),
    (op::GAMMA_GET, "gamma_get", R::F(1)),
    (op::GAMMA_SET, "gamma_set", R::None),
    (op::FRAME_BEGIN, "frame_begin", R::None),
    (op::IMGUI_RENDER, "imgui_render", R::None),
    (op::FRAME_END, "frame_end", R::None),
    (op::MOUSE_POLL, "mouse_poll", R::I(6)),
    (op::GUI_MOUSE_POS, "gui_mouse_pos", R::None),
    (op::CLICK, "click", R::None),
];

pub struct Tracer {
    inner: Mutex<Inner>,
    autokey: Option<u32>,
}

struct Inner {
    sink: Box<dyn Write + Send>,
    ids: HashMap<usize, usize>,
    clock: u64,
    key_polls: u32,
}

/// The tracer of `OHPL_S8_DUMP`, if set.
pub fn global() -> Option<&'static Tracer> {
    static T: OnceLock<Option<Tracer>> = OnceLock::new();
    T.get_or_init(|| {
        let f = std::fs::File::create(std::env::var_os("OHPL_S8_DUMP")?).ok()?;
        let auto = std::env::var("OHPL_S8_AUTOKEY").ok()?.parse().ok();
        Some(Tracer::new(Box::new(f), auto))
    })
    .as_ref()
}

fn quote(units: impl Iterator<Item = u32>) -> String {
    let s: String = units
        .map(|u| if u < 0x80 { char::from(u as u8) } else { '?' })
        .collect();
    format!(" \"{s}\"")
}

impl Tracer {
    pub fn new(sink: Box<dyn Write + Send>, autokey: Option<u32>) -> Tracer {
        Tracer {
            inner: Mutex::new(Inner {
                sink,
                ids: HashMap::new(),
                clock: 0,
                key_polls: 0,
            }),
            autokey,
        }
    }

    fn ptr(t: &mut Inner, p: usize) -> String {
        if p == 0 {
            return " p0".into();
        }
        let n = t.ids.len() + 1;
        format!(" p{}", *t.ids.entry(p).or_insert(n))
    }

    fn arg(t: &mut Inner, a: &Arg) -> String {
        match *a {
            Arg::P(p) => Self::ptr(t, p),
            Arg::I(i) => format!(" {i}"),
            Arg::F(f) => format!(" {:08x}", f.to_bits()),
            Arg::B(b) => format!(" {}", u8::from(b)),
            Arg::S(s) => quote(s.iter().map(|&c| u32::from(c))),
            Arg::W(w) => quote(w.iter().copied()),
        }
    }

    fn line(t: &mut Inner, name: &str, args: &[Arg], res: &str) {
        let mut l = String::from(name);
        for a in args {
            l.push_str(&Self::arg(t, a));
        }
        if !res.is_empty() {
            l.push_str(" =");
            l.push_str(res);
        }
        let _ = writeln!(t.sink, "{l}");
        let _ = t.sink.flush();
    }

    /// Ops without results are logged before the call, so what the call triggers (the next screen
    /// being built) follows its line.
    pub fn before(op: i32) -> bool {
        OPS.iter().any(|o| o.0 == op && matches!(o.2, R::None))
    }

    /// A host call and its results; ops outside the table (state dir, clip delete) are not logged.
    pub fn call(&self, op: i32, args: &[Arg], r: &Res) {
        let Some(&(_, name, rk)) = OPS.iter().find(|o| o.0 == op) else {
            return;
        };
        let Ok(mut t) = self.inner.lock() else { return };
        let res = match rk {
            R::None => String::new(),
            R::P => Self::ptr(&mut t, r.p()),
            R::B => format!(" {}", u8::from(r.b())),
            R::I(n) => {
                r.u.iter()
                    .take(n)
                    .fold(String::new(), |s, v| s + &format!(" {}", *v as i32))
            }
            R::F(n) => (0..n).fold(String::new(), |s, i| {
                s + &format!(" {:08x}", r.f(i).to_bits())
            }),
            R::Strs => {
                r.s.iter()
                    .fold(String::new(), |s, w| s + &quote(w.iter().copied()))
            }
        };
        Self::line(&mut t, name, args, &res);
    }

    /// A line for something the traced run decides itself.
    pub fn event(&self, name: &str, args: &[Arg], res: &str) {
        if let Ok(mut t) = self.inner.lock() {
            Self::line(&mut t, name, args, res);
        }
    }

    pub fn now(&self) -> u64 {
        let Ok(mut t) = self.inner.lock() else {
            return 0;
        };
        t.clock += 16;
        let n = t.clock;
        Self::line(&mut t, "now", &[], &format!(" {n}"));
        n
    }

    /// The forced Return of a traced run, or `None` to ask the engine.
    pub fn forced_key(&self) -> Option<i64> {
        let lim = self.autokey?;
        let mut t = self.inner.lock().ok()?;
        t.key_polls += 1;
        (t.key_polls > lim).then_some(super::s8::KEY_RETURN)
    }

    pub fn key(&self, code: i64) {
        self.event("key_poll", &[], &format!(" {code}"));
    }
}

/// The golden replay: a dump line's op id and the results the engine gave.
#[cfg(test)]
pub(super) fn parse_line(line: &str) -> Option<(i32, Res)> {
    let (head, rest) = line.split_once(" =").unwrap_or((line, ""));
    let name = head.split(' ').next()?;
    let &(op, _, rk) = OPS.iter().find(|o| o.1 == name)?;
    let toks: Vec<&str> = rest.split(' ').filter(|t| !t.is_empty()).collect();
    let mut r = Res::default();
    match rk {
        R::None => {}
        R::P => r.u[0] = toks.first()?.trim_start_matches('p').parse().ok()?,
        R::B | R::I(_) => {
            for (i, t) in toks.iter().enumerate() {
                r.u[i] = t.parse::<i32>().ok()? as u32 as u64;
            }
        }
        R::F(_) => {
            for (i, t) in toks.iter().enumerate() {
                r.u[i] = u64::from_str_radix(t, 16).ok()?;
            }
        }
        R::Strs => {
            for s in rest.split('"').skip(1).step_by(2) {
                r.s.push(s.chars().map(u32::from).collect());
            }
        }
    }
    Some((op, r))
}
