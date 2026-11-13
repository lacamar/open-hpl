//! Replays the H18 golden scripts (recorded from the C++ `cGuiSet`, rust/tests/golden/H18) through
//! `set::Core` with a model of the harness' dummy widgets, and compares every printed line.

use super::set::{self, Core, Host, Msg, SetState, W};
use std::fmt::Write;
use std::path::Path;

const ROOT: W = usize::MAX;
const PROB: [u64; 24] = [
    30, 20, 20, 20, 40, 20, 50, 40, 30, 0, 0, 0, 0, 0, 25, 25, 25, 30, 30, 30, 30, 30, 40, 40,
];

struct Wd {
    parent: Option<usize>,
    vis: bool,
    en: bool,
    conn: bool,
    tip: bool,
    gkl: bool,
    gul: bool,
    ptr: usize,
    nav: [i64; 4],
    pos: [f32; 3],
    size: [f32; 2],
    over: bool,
}

struct Mock {
    w: Vec<Wd>,
    order: Vec<usize>,
    cur: usize,
    cb: u64,
    log: String,
    sort_pending: bool,
}

fn idx(w: W) -> i64 {
    if w == 0 || w == ROOT {
        -1
    } else {
        w as i64 - 1
    }
}

impl Mock {
    fn chain(&self, w: W, f: &impl Fn(&Wd) -> bool) -> bool {
        let wd = &self.w[w - 1];
        wd.parent.is_none_or(|p| self.chain(p + 1, f)) && f(wd)
    }

    fn walk(&self, w: W, other: W, start: bool) -> bool {
        let wd = &self.w[w - 1];
        if !start && !wd.conn {
            return false;
        }
        if other == 0 {
            return false;
        }
        if other == w {
            return true;
        }
        match wd.parent {
            Some(p) => self.walk(p + 1, other, false),
            None => false,
        }
    }

    fn local_z(&self, w: usize, z: f32) -> f32 {
        z - self.w[w].parent.map_or(0.0, |p| self.w[p].pos[2])
    }

    fn move_to(&mut self, w: usize, p: [f32; 3]) {
        if self.local_z(w, self.w[w].pos[2]) != self.local_z(w, p[2]) {
            self.sort_pending = true;
        }
        self.w[w].pos = p;
    }

    fn callback(&mut self, w: W, msg: i32, d: &Msg) -> bool {
        let m = msg as usize;
        let p = PROB[m];
        if p == 0 || w == self.w.len() {
            return false;
        }
        self.cb = self
            .cb
            .wrapping_mul(6364136223846793005)
            .wrapping_add(1442695040888963407);
        let ret = (self.cb >> 33) % 100 < p;
        let _ = write!(self.log, "ev w{} m{}", idx(w), msg);
        if msg != 22 && msg != 23 {
            let pos = d.pos();
            let _ = write!(
                self.log,
                " pos={:08x},{:08x}",
                pos[0].to_bits(),
                pos[1].to_bits()
            );
        }
        if msg <= 5 || msg == 16 || (17..=21).contains(&msg) {
            let _ = write!(self.log, " val={}", d.val());
        }
        if msg == 14 || msg == 15 {
            let _ = write!(self.log, " key={},{}", d.key(), d.modifier());
        }
        let _ = writeln!(self.log, " ret={}", u8::from(ret));
        ret
    }
}

impl Host for Mock {
    fn is_visible(&mut self, w: W) -> bool {
        self.chain(w, &|x| x.vis)
    }
    fn is_enabled(&mut self, w: W) -> bool {
        self.chain(w, &|x| x.en)
    }
    fn is_connected_to(&mut self, w: W, other: W) -> bool {
        self.walk(w, other, true)
    }
    fn point_is_inside(&mut self, w: W, p: [f32; 2]) -> bool {
        let x = &self.w[w - 1];
        !(p[0] < x.pos[0]
            || p[0] > x.pos[0] + x.size[0]
            || p[1] < x.pos[1]
            || p[1] > x.pos[1] + x.size[1])
    }
    fn mouse_is_over(&mut self, w: W) -> bool {
        self.w[w - 1].over
    }
    fn set_mouse_is_over(&mut self, w: W, v: bool) {
        self.w[w - 1].over = v;
    }
    fn process_message(&mut self, w: W, msg: i32, d: &Msg, skip_vis: bool, skip_en: bool) -> bool {
        if (!skip_vis && !self.is_visible(w)) || (!skip_en && !self.is_enabled(w)) {
            return false;
        }
        let mut ret = (msg == set::MSG_GOT_FOCUS || msg == 7) && self.w[w - 1].en;
        if self.callback(w, msg, d) {
            ret = true;
        }
        ret
    }
    fn get_focus(&mut self, w: W, d: &Msg) -> bool {
        self.process_message(w, set::MSG_GOT_FOCUS, d, false, false)
    }
    fn tooltip_enabled(&mut self, w: W) -> bool {
        self.w[w - 1].tip
    }
    fn has_focus_nav(&mut self, w: W) -> bool {
        self.w[w - 1].nav.iter().any(|n| *n >= 0)
    }
    fn parent(&mut self, w: W) -> W {
        self.w[w - 1].parent.map_or(ROOT, |p| p + 1)
    }
    fn widget_set(&mut self, _w: W) -> usize {
        1
    }
    fn pointer_gfx(&mut self, w: W) -> usize {
        self.w[w - 1].ptr
    }
    fn focus_nav(&mut self, w: W, dir: i32) -> W {
        let slot = match dir {
            1 => 0,
            2 => 1,
            4 => 2,
            8 => 3,
            _ => return 0,
        };
        let n = self.w[w - 1].nav[slot];
        if n < 0 { 0 } else { n as usize + 1 }
    }
    fn global_key_listener(&mut self, w: W) -> bool {
        self.w[w - 1].gkl
    }
    fn global_ui_listener(&mut self, w: W) -> bool {
        self.w[w - 1].gul
    }
    fn got_tab_focus(&mut self, w: W, _d: &Msg) -> bool {
        self.w[w - 1].en
    }
    fn is_valid(&mut self, w: W) -> bool {
        w == 0 || self.order.contains(&(w - 1))
    }
    fn list_begin(&mut self) -> usize {
        0
    }
    fn list_end(&mut self, it: usize) -> bool {
        it >= self.order.len()
    }
    fn list_get(&mut self, it: usize) -> W {
        self.order.get(it).map_or(0, |i| i + 1)
    }
    fn list_next(&mut self, it: usize) -> usize {
        it + 1
    }
    fn sort_widgets(&mut self) {
        if self.sort_pending {
            self.sort_pending = false;
            let w = &self.w;
            self.order.sort_by(|a, b| {
                let (za, zb) = (w[*a].pos[2], w[*b].pos[2]);
                if za > zb {
                    std::cmp::Ordering::Less
                } else if zb > za {
                    std::cmp::Ordering::Greater
                } else {
                    std::cmp::Ordering::Equal
                }
            });
        }
    }
    fn current_pointer(&mut self) -> usize {
        self.cur
    }
    fn set_current_pointer(&mut self, g: usize) {
        self.cur = g;
    }
    fn skin_normal_pointer(&mut self) -> usize {
        0
    }
    fn exec_shortcut(&mut self, _d: &Msg) -> bool {
        false
    }
    fn global_pos(&mut self, w: W) -> [f32; 3] {
        self.w[w - 1].pos
    }
    fn set_global_pos(&mut self, w: W, p: [f32; 3]) {
        self.move_to(w - 1, p);
    }
}

fn hex(s: &str) -> f32 {
    f32::from_bits(u32::from_str_radix(s, 16).unwrap_or(0))
}

fn num(s: &str) -> i64 {
    s.parse().unwrap_or(-1)
}

fn field<'a>(line: &'a str, key: &str) -> &'a str {
    line.split(' ')
        .find_map(|t| t.strip_prefix(key).and_then(|r| r.strip_prefix('=')))
        .unwrap_or("")
}

fn handle(w: i64) -> W {
    if w < 0 { 0 } else { w as usize + 1 }
}

fn state_line(m: &Mock, s: &SetState, ret: bool) -> String {
    let mut o = format!(
        "= ret={} foc={} att={} def={} tab={} tip={} tmr={:08x} mouse={:08x},{:08x} cur={} over=",
        u8::from(ret),
        idx(s.focused),
        idx(s.attention),
        idx(s.default_nav),
        idx(s.tab_widget),
        idx(s.tooltip_widget),
        s.tooltip_timer.to_bits(),
        s.mouse_pos[0].to_bits(),
        s.mouse_pos[1].to_bits(),
        m.cur,
    );
    let n = m.w.len() - 1;
    for w in &m.w[..n] {
        o.push(if w.over { '1' } else { '0' });
    }
    o.push_str(" down=");
    for d in s.mouse_down {
        o.push(if d { '1' } else { '0' });
    }
    o.push_str(" ord=");
    for i in &m.order {
        let _ = write!(o, "{i},");
    }
    o.push_str(" gz=");
    for w in &m.w[..n] {
        let _ = write!(o, "{:08x},", w.pos[2].to_bits());
    }
    o
}

fn replay(text: &str) -> Result<(), String> {
    let mut m = Mock {
        w: Vec::new(),
        order: Vec::new(),
        cur: 0,
        cb: 0,
        log: String::new(),
        sort_pending: false,
    };
    let mut n = 0usize;
    let mut out: Vec<String> = Vec::new();
    let mut st = SetState {
        focused: 0,
        attention: 0,
        default_nav: 0,
        tab_widget: 0,
        tooltip_widget: 0,
        tooltip_timer: 0.0,
        mouse_pos: [0.0; 2],
        window_z: 0.0,
        mouse_down: [false; 9],
        mouse_movement: true,
        lists: set::lists_new(),
    };
    // SAFETY: `st` outlives `core`; `lists` is valid until freed below.
    let core = unsafe { Core::new(&mut st) };
    let mut result = Ok(());
    let mut expect: Vec<String> = Vec::new();
    for line in text.lines() {
        expect.push(line.to_string());
    }

    for line in &expect {
        let tok: Vec<&str> = line.split(' ').collect();
        match tok.first().copied().unwrap_or("") {
            "seed" => {
                let seed: u64 = tok.get(1).and_then(|s| s.parse().ok()).unwrap_or(0);
                m.cb = seed * 104729 + 3;
                out.push(line.clone());
            }
            "n" => {
                n = tok.get(1).and_then(|s| s.parse().ok()).unwrap_or(0);
                out.push(line.clone());
            }
            "w" => {
                let nav: Vec<i64> = field(line, "nav")
                    .split(',')
                    .filter(|x| !x.is_empty())
                    .map(num)
                    .collect();
                let p: Vec<f32> = field(line, "pos").split(',').map(hex).collect();
                let sz: Vec<f32> = field(line, "size").split(',').map(hex).collect();
                let par = num(field(line, "par"));
                m.w.push(Wd {
                    parent: (par >= 0).then_some(par as usize),
                    vis: field(line, "vis") == "1",
                    en: field(line, "en") == "1",
                    conn: field(line, "conn") == "1",
                    tip: field(line, "tip") == "1",
                    gkl: field(line, "gkl") == "1",
                    gul: field(line, "gul") == "1",
                    ptr: field(line, "ptr").parse().unwrap_or(0),
                    nav: [nav[0], nav[1], nav[2], nav[3]],
                    pos: [p[0], p[1], p[2]],
                    size: [sz[0], sz[1]],
                    over: false,
                });
                out.push(line.clone());
                if m.w.len() == n {
                    m.w.push(Wd {
                        parent: None,
                        vis: true,
                        en: true,
                        conn: true,
                        tip: false,
                        gkl: false,
                        gul: false,
                        ptr: 0,
                        nav: [-1; 4],
                        pos: [0.0; 3],
                        size: [0.0; 2],
                        over: false,
                    });
                }
            }
            t if t.starts_with("ord=") => {
                m.sort_pending = (0..n).any(|i| m.local_z(i, m.w[i].pos[2]) != 0.0);
                m.order = t["ord=".len()..]
                    .split(',')
                    .filter(|x| !x.is_empty())
                    .map(|x| x.parse().unwrap_or(0))
                    .collect();
                out.push(line.clone());
            }
            "=" => {
                // the very first state line follows the setup, later ones are produced by ops
                if out.last().is_some_and(|l| l.starts_with("ord=")) {
                    out.push(state_line(&m, &st, false));
                }
            }
            "op" => {
                out.push(line.clone());
                m.log.clear();
                let a = |i: usize| tok.get(i).copied().unwrap_or("");
                let wi = num(a(2));
                let leaf = |m: &Mock, w: i64| {
                    w >= 0 && (w as usize) < n && !m.w.iter().any(|x| x.parent == Some(w as usize))
                };
                let mut ret = false;
                let mut d = Msg::zero();
                let h = &mut m;
                let send = |h: &mut Mock, msg: i32, d: &Msg| core.send_message(h, msg, d);
                match a(1) {
                    "move" => {
                        d.set_pos([hex(a(2)), hex(a(3))]);
                        ret = send(h, 0, &d);
                    }
                    "down" | "up" | "dbl" => {
                        d.set_val(1 << num(a(2)));
                        let msg = match a(1) {
                            "down" => 1,
                            "up" => 2,
                            _ => 3,
                        };
                        ret = send(h, msg, &d);
                    }
                    "keypress" | "keyrel" => {
                        d.set_key(num(a(2)) as i32, num(a(3)) as i32);
                        ret = send(h, if a(1) == "keypress" { 14 } else { 15 }, &d);
                    }
                    "pad" => ret = send(h, 16, &d),
                    "arrowpress" | "arrowrel" | "btnpress" | "btnrel" | "btndbl" => {
                        d.set_val(num(a(2)) as i32);
                        let msg = match a(1) {
                            "arrowpress" => 17,
                            "arrowrel" => 18,
                            "btnpress" => 19,
                            "btnrel" => 20,
                            _ => 21,
                        };
                        ret = send(h, msg, &d);
                    }
                    "focus" => core.set_focused_widget(h, handle(wi), a(3) == "1"),
                    "attn" => core.set_attention_widget(h, handle(wi), a(3) == "1", a(4) == "1"),
                    "pushf" => core.push_focused(),
                    "popf" => core.pop_focused(h),
                    "pusha" => core.push_attention(),
                    "popa" => core.pop_attention(h, a(2) == "1"),
                    "def" => core.set_default_focus_nav(h, handle(wi), a(3) == "1"),
                    "pushd" => core.push_default_focus_nav(),
                    "popd" => core.pop_default_focus_nav(h),
                    "tabadd" => {
                        if wi >= 0 {
                            core.add_to_tab_order(handle(wi));
                        }
                    }
                    "tabclr" => core.clear_tab_order(),
                    "winadd" => core.add_window(handle(wi)),
                    "wintop" => {
                        if leaf(h, wi) {
                            core.set_window_on_top(h, handle(wi));
                        }
                    }
                    "winrm" => {
                        if wi >= 0 {
                            core.remove_window(handle(wi));
                        }
                    }
                    "vis" | "en" | "over" if wi >= 0 && (wi as usize) < n => {
                        let v = a(3) == "1";
                        if let Some(x) = h.w.get_mut(wi as usize) {
                            match a(1) {
                                "vis" => x.vis = v,
                                "en" => x.en = v,
                                _ => x.over = v,
                            }
                        }
                    }
                    "pos" => {
                        if wi >= 0 {
                            h.move_to(wi as usize, [hex(a(3)), hex(a(4)), hex(a(5))]);
                        }
                    }
                    "sort" => h.sort_pending = true,
                    "mm" => st.mouse_movement = a(2) == "1",
                    _ => {}
                }
                out.extend(m.log.lines().map(str::to_string));
                out.push(state_line(&m, &st, ret));
            }
            _ => {}
        }
    }
    // SAFETY: `lists` came from `lists_new` and `core` is no longer used.
    unsafe { set::lists_free(st.lists) };
    let want: Vec<&str> = expect.iter().map(String::as_str).collect();
    let got: Vec<&str> = out.iter().map(String::as_str).collect();
    for (i, (w, g)) in want.iter().zip(&got).enumerate() {
        if w != g {
            result = Err(format!("line {i}\n want {w}\n got  {g}"));
            break;
        }
    }
    if result.is_ok() && want.len() != got.len() {
        result = Err(format!("length want {} got {}", want.len(), got.len()));
    }
    result
}

#[test]
fn replays_h18_set_golden() {
    let dir = Path::new(env!("CARGO_MANIFEST_DIR")).join("tests/golden/H18");
    let mut files: Vec<_> = std::fs::read_dir(&dir)
        .map(|r| r.filter_map(Result::ok).map(|e| e.path()).collect())
        .unwrap_or_default();
    files.retain(|p| {
        p.file_name()
            .and_then(|n| n.to_str())
            .is_some_and(|n| n.starts_with("set_"))
    });
    files.sort();
    assert!(files.len() >= 20, "golden files missing");
    let mut events = 0;
    for f in &files {
        let text = std::fs::read_to_string(f).unwrap_or_default();
        events += text.lines().filter(|l| l.starts_with("ev ")).count();
        if let Err(e) = replay(&text) {
            panic!("{}: {e}", f.display());
        }
    }
    assert!(events > 300);
}

fn skin_dump(s: &super::skin::Skin) -> String {
    let bits = |v: &[f32]| {
        v.iter()
            .map(|f| format!("{:08x}", f.to_bits()))
            .collect::<Vec<_>>()
            .join(",")
    };
    let name = |b: &[u8]| String::from_utf8_lossy(b).into_owned();
    let mut o = String::new();
    for a in &s.attributes {
        let _ = writeln!(o, "attr {} {}", name(&a.type_name), bits(&a.value));
    }
    for f in &s.fonts {
        let _ = writeln!(
            o,
            "font {} {} {} {}",
            name(&f.type_name),
            name(&f.file),
            bits(&f.size),
            bits(&f.color)
        );
    }
    for g in &s.gfx {
        let _ = writeln!(
            o,
            "gfx {} {} off={} act={} col={} mat={} warn={:?} anim={} ft={:08x} mode={} awarn={:?}",
            name(&g.type_name),
            name(&g.file),
            bits(&g.offset),
            bits(&g.active_size),
            bits(&g.color),
            g.material,
            g.material_warn.as_deref().map(name),
            u8::from(g.animated),
            g.frame_time.to_bits(),
            g.anim_mode,
            g.anim_warn.as_deref().map(name),
        );
    }
    o
}

const SKINS: [(&str, &str); 7] = [
    (
        "amnesia_default",
        "Amnesia The Dark Descent/gui/default/gui_default.skin",
    ),
    (
        "amnesia_main_menu",
        "Amnesia The Dark Descent/gui/main_menu/gui_main_menu.skin",
    ),
    ("rebirth_default", "Amnesia Rebirth/gui/gui_default.skin"),
    ("bunker_default", "Amnesia The Bunker/gui/gui_default.skin"),
    ("soma_default", "SOMA/gui/gui_default.skin"),
    (
        "mfp_default",
        "Machine for Pigs/gui/default/gui_default.skin",
    ),
    (
        "mfp_main_menu",
        "Machine for Pigs/gui/main_menu/gui_main_menu.skin",
    ),
];

/// H18_GEN_SKIN=1 rewrites rust/tests/golden/H18/skin_*.txt from the Steam data; otherwise the
/// golden dumps are compared against the parse of every real `.skin` that is installed.
#[test]
fn parses_real_skins_like_golden() {
    let golden = Path::new(env!("CARGO_MANIFEST_DIR")).join("tests/golden/H18");
    let steam = std::env::var_os("HOME")
        .map(|h| Path::new(&h).join(".local/share/Steam/steamapps/common"))
        .unwrap_or_default();
    let gen_mode = std::env::var_os("H18_GEN_SKIN").is_some();
    let mut compared = 0;
    for (name, rel) in SKINS {
        let want = golden.join(format!("skin_{name}.txt"));
        if gen_mode {
            let data = std::fs::read(steam.join(rel)).unwrap_or_default();
            let skin = super::skin::parse(&data, true);
            assert!(skin.is_ok(), "{rel}");
            let _ = std::fs::write(&want, skin_dump(&skin.unwrap_or_default()));
        }
        let expected = std::fs::read_to_string(&want).unwrap_or_default();
        assert!(expected.lines().count() > 10, "{name}: golden missing");
        if let Ok(data) = std::fs::read(steam.join(rel)) {
            let skin = super::skin::parse(&data, true);
            assert_eq!(skin.map(|s| skin_dump(&s)), Ok(expected), "{name}");
            compared += 1;
        }
    }
    eprintln!(
        "skins compared against game data: {compared}/{}",
        SKINS.len()
    );
}
