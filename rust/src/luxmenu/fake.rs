//! In-memory host for the tests: a widget tree with deterministic sizes and canned engine data.

use super::host::{Args, op};
use super::options_win::Vals;
use std::cell::RefCell;

#[derive(Default, Clone)]
pub struct Node {
    pub kind: i32,
    pub parent: usize,
    pub children: Vec<usize>,
    pub pos: [f32; 3],
    pub size: [f32; 2],
    pub text: Vec<u32>,
    pub visible: bool,
    pub enabled: bool,
    pub user_data: usize,
    pub user_value: i32,
    pub nav: [usize; 4],
    pub color: [f32; 4],
    pub font: [f32; 2],
    pub checked: bool,
    pub slider: [i32; 3],
    pub items: Vec<Vec<u32>>,
    pub selected: i32,
    pub callbacks: Vec<(i32, usize)>,
    pub tabs: Vec<usize>,
    pub label: usize,
    pub tab_frame: usize,
}

#[derive(Default)]
pub struct Fake {
    pub nodes: Vec<Node>,
    pub log: Vec<String>,
    pub out: Vec<u32>,
    pub vals: Vals,
    pub applied: Option<Vals>,
    pub popup: bool,
    pub lists: [Vec<Vec<u32>>; 6],
    pub action_ids: Vec<(i32, i32)>,
}

thread_local! {
    pub static FAKE: RefCell<Fake> = RefCell::new(Fake::default());
}

pub fn reset() {
    FAKE.with(|f| {
        let mut f = f.borrow_mut();
        *f = Fake::default();
        f.nodes.push(Node::default());
        f.vals.show_hints = 1;
        f.vals.res_x = 1920;
        f.vals.res_y = 1080;
        f.vals.gui_scale = 2;
        f.vals.aniso = 4.0;
        f.vals.ssao_samples = 8;
        f.vals.mouse_sens = 1.0;
        f.vals.pad_sens = 1.5;
        f.vals.volume = 0.5;
        f.vals.gamma = 1.0;
        f.vals.paper_white = 200.0;
        f.vals.peak_nits = 1000.0;
        f.vals.lang = super::options_win::Str::from_text("english.lang");
        f.vals.renderer = super::options_win::Str::from_text("Vulkan");
        f.vals.color = super::options_win::Str::from_text("HDR10");
    });
}

fn wide(s: &str) -> Vec<u32> {
    s.chars().map(u32::from).collect()
}

fn text_of(a: &Args, i: usize) -> Vec<u32> {
    let (Some(&p), Some(&n)) = (a.s.get(i), a.n.get(i)) else {
        return Vec::new();
    };
    if p.is_null() || n == 0 {
        return Vec::new();
    }
    // SAFETY: the caller keeps the buffer alive for the call.
    unsafe { std::slice::from_raw_parts(p, n) }.to_vec()
}

fn out(f: &mut Fake, a: &mut Args, s: Vec<u32>) {
    f.out = s;
    a.s[0] = f.out.as_ptr();
    a.n[0] = f.out.len();
}

fn node(f: &mut Fake, id: usize) -> &mut Node {
    if id == 0 || id >= f.nodes.len() {
        return &mut f.nodes[0];
    }
    &mut f.nodes[id]
}

fn create(f: &mut Fake, code: i32, a: &Args) -> usize {
    let mut n = Node {
        kind: code,
        parent: a.p[1],
        pos: [a.f[0], a.f[1], a.f[2]],
        size: [a.f[3], a.f[4]],
        text: text_of(a, 0),
        visible: true,
        enabled: true,
        color: [1.0; 4],
        font: [14.0, 14.0],
        selected: -1,
        ..Node::default()
    };
    if n.size[0] < 0.0 || (code == op::LABEL && n.size == [0.0, 0.0]) {
        n.size = [n.text.len() as f32 * 7.0, 16.0];
    }
    if code == op::CHECKBOX && n.size[0] <= 0.0 {
        n.size = [20.0 + n.text.len() as f32 * 7.0, 20.0];
    }
    if code == op::IMAGE {
        n.size = [64.0, 32.0];
    }
    if code == op::SLIDER {
        n.slider = [0, 10, 1];
    }
    let id = f.nodes.len();
    f.nodes.push(n);
    if a.p[1] != 0 {
        node(f, a.p[1]).children.push(id);
    }
    id
}

pub fn call(code: i32, a: &mut Args) -> usize {
    let r = FAKE.with(|f| call_in(&mut f.borrow_mut(), code, a));
    if code == op::SET_TEXT {
        let tokens: Vec<usize> = FAKE.with(|f| {
            let f = f.borrow();
            f.nodes.get(a.p[0]).map_or_else(Vec::new, |n| {
                n.callbacks
                    .iter()
                    .filter(|c| c.0 == super::host::msg::TEXT_CHANGE)
                    .map(|c| c.1)
                    .collect()
            })
        });
        let data = super::store::CbData {
            msg: super::host::msg::TEXT_CHANGE,
            ..Default::default()
        };
        for t in tokens {
            // SAFETY: `data` is a live record.
            unsafe { super::ffi_win::ohpl_a10a_cb(t, a.p[0], &data) };
        }
    }
    r
}

fn call_in(f: &mut Fake, code: i32, a: &mut Args) -> usize {
    let w = a.p[0];
    match code {
        op::WINDOW..=op::FRAME => {
            let id = create(f, code, a);
            if code == op::TABFRAME {
                f.nodes[id].size[0] = a.f[3];
            }
            id
        }
        op::SIZE => {
            let s = node(f, w).size;
            a.f[0] = s[0];
            a.f[1] = s[1];
            0
        }
        op::LOCAL_POS | op::GLOBAL_POS => {
            let p = node(f, w).pos;
            a.f[..3].copy_from_slice(&p);
            0
        }
        op::SET_SIZE => {
            node(f, w).size = [a.f[0], a.f[1]];
            0
        }
        op::SET_POS => {
            node(f, w).pos = [a.f[0], a.f[1], a.f[2]];
            0
        }
        op::SET_TEXT => {
            node(f, w).text = text_of(a, 0);
            0
        }
        op::GET_TEXT => {
            let t = node(f, w).text.clone();
            out(f, a, t);
            0
        }
        op::SET_VISIBLE => {
            node(f, w).visible = a.i[0] != 0;
            0
        }
        op::SET_ENABLED => {
            node(f, w).enabled = a.i[0] != 0;
            0
        }
        op::IS_VISIBLE => usize::from(node(f, w).visible),
        op::SET_USER_DATA => {
            node(f, w).user_data = a.p[1];
            0
        }
        op::GET_USER_DATA => node(f, w).user_data,
        op::SET_USER_VALUE => {
            node(f, w).user_value = a.i[0];
            0
        }
        op::GET_USER_VALUE => node(f, w).user_value as usize,
        op::SET_NAV => {
            node(f, w).nav[(a.i[0] & 3) as usize] = a.p[1];
            0
        }
        op::FONT_COLOR => {
            let c = node(f, w).color;
            a.f[..4].copy_from_slice(&c);
            0
        }
        op::SET_FONT_COLOR => {
            node(f, w).color = [a.f[0], a.f[1], a.f[2], a.f[3]];
            0
        }
        op::FONT_SIZE => {
            let s = node(f, w).font;
            a.f[..2].copy_from_slice(&s);
            0
        }
        op::SET_FONT_SIZE => {
            node(f, w).font = [a.f[0], a.f[1]];
            0
        }
        op::TEXT_LENGTH => {
            a.f[0] = node(f, w).text.len() as f32 * 7.0;
            0
        }
        op::PARENT => node(f, w).parent,
        op::ADD_CALLBACK => {
            node(f, w).callbacks.push((a.i[0], a.p[1]));
            0
        }
        op::SET_CHECKED => {
            node(f, w).checked = a.i[0] != 0;
            0
        }
        op::IS_CHECKED => usize::from(node(f, w).checked),
        op::COMBO_ADD => {
            let t = text_of(a, 0);
            node(f, w).items.push(t);
            0
        }
        op::COMBO_CLEAR => {
            let n = node(f, w);
            n.items.clear();
            n.selected = -1;
            0
        }
        op::COMBO_SELECT => {
            node(f, w).selected = a.i[0];
            0
        }
        op::COMBO_SELECTED => node(f, w).selected as usize,
        op::COMBO_NUM => node(f, w).items.len(),
        op::SLIDER_MAX => {
            node(f, w).slider[1] = a.i[0];
            0
        }
        op::SLIDER_BAR => {
            node(f, w).slider[2] = a.i[0];
            0
        }
        op::SLIDER_SET => {
            node(f, w).slider[0] = a.i[0];
            0
        }
        op::SLIDER_GET => node(f, w).slider[0] as usize,
        op::SLIDER_GET_MAX => node(f, w).slider[1] as usize,
        op::TAB_ADD => {
            let mut t = Node {
                kind: 99,
                parent: w,
                visible: true,
                enabled: true,
                ..Node::default()
            };
            t.text = text_of(a, 0);
            let tid = f.nodes.len();
            f.nodes.push(t);
            let lid = f.nodes.len();
            f.nodes.push(Node {
                kind: 98,
                parent: tid,
                visible: true,
                ..Node::default()
            });
            f.nodes[tid].label = lid;
            f.nodes[tid].tab_frame = w;
            f.nodes[w].tabs.push(tid);
            tid
        }
        op::TAB_NUM => node(f, w).tabs.len(),
        op::TAB_GET => node(f, w).tabs.get(a.i[0] as usize).copied().unwrap_or(0),
        op::TAB_TOP => node(f, w).tabs.first().copied().unwrap_or(0),
        op::TAB_LABEL => node(f, w).label,
        op::TAB_FRAME => node(f, w).tab_frame,
        op::FRAME_OF => usize::from(node(f, w).kind == op::FRAME) * w,
        op::FRAME_SCROLL => 0,
        op::POPUP_ACTIVE => usize::from(f.popup),
        op::SET_ATTENTION
        | op::SET_FOCUSED
        | op::SET_DEFAULT_NAV
        | op::PUSH_ATTENTION
        | op::POP_ATTENTION
        | op::PUSH_FOCUSED
        | op::POP_FOCUSED
        | op::PUSH_DEFAULT_NAV
        | op::SET_MOUSE_MOVE
        | op::SET_DRAW_MOUSE => {
            f.log.push(format!("set {code} {}", a.p[1]));
            0
        }
        op::MSGBOX => {
            f.log.push(format!("msgbox {}", a.p[1]));
            0
        }
        op::SET_WINDOW_ACTIVE => {
            f.log.push(format!("window {}", a.i[0]));
            0
        }
        op::TRANSLATE => {
            let mut t = text_of(a, 0);
            t.push(u32::from(b'.'));
            t.extend(text_of(a, 1));
            out(f, a, t);
            0
        }
        op::OPT_READ => {
            // SAFETY: the test passes a live `Vals`.
            unsafe { (a.p[0] as *mut Vals).write(f.vals) };
            0
        }
        op::OPT_APPLY => {
            // SAFETY: the test passes a live `Vals`.
            f.applied = Some(unsafe { *(a.p[0] as *const Vals) });
            0
        }
        op::SAVE_CONFIG | op::NEEDS_RESTART | op::PLAY_SANITY => {
            f.log.push(format!("op {code}"));
            0
        }
        op::RESTART_WARNING => {
            f.log.push(format!("restart-warning {}", a.p[1]));
            0
        }
        op::SET_GAMMA | op::SET_MOUSE_SENS | op::SET_PAD_SENS | op::SET_VOLUME => {
            f.log.push(format!("op {code} {}", a.f[0]));
            0
        }
        op::GAMEPAD_PRESENT | op::SHOW_COMMENTARY_CFG => 1,
        op::LANG_FILES => {
            f.lists[0] = vec![wide("english.lang"), wide("norwegian.lang")];
            2
        }
        op::LIST_STR => {
            let l = f
                .lists
                .get(a.i[0] as usize)
                .and_then(|l| l.get(a.i[1] as usize))
                .cloned();
            out(f, a, l.unwrap_or_default());
            0
        }
        op::VIDEO_MODES => 3,
        op::VIDEO_MODE => {
            let m = [(0, 0, 0, 0), (0, 1280, 720, 60), (0, 1920, 1080, 60)];
            let (d, x, y, r) = m.get(a.i[0] as usize).copied().unwrap_or_default();
            a.i = [d, x, y, r];
            0
        }
        op::DISPLAY_NAME => {
            out(f, a, wide("Display"));
            0
        }
        op::SND_DEVICES => {
            f.lists[1] = vec![wide("dev0"), wide("dev1")];
            f.lists[5] = vec![wide("dev1")];
            2
        }
        op::SND_DEVICE => {
            a.i[1] = i32::from(a.i[0] == 1);
            a.i[0] += 10;
            0
        }
        op::ACTIONS => {
            let cat = a.i[0];
            f.lists[2] = (0..2).map(|i| wide(&format!("Act{cat}{i}"))).collect();
            f.action_ids = (0..2)
                .map(|i| (cat * 10 + i, i32::from(!(cat == 3 && i == 1))))
                .collect();
            2
        }
        op::ACTION => {
            let (id, c) = f
                .action_ids
                .get(a.i[0] as usize)
                .copied()
                .unwrap_or_default();
            a.i[0] = id;
            a.i[1] = c;
            0
        }
        op::DEFAULT_INPUTS => {
            f.lists[3] = vec![wide("Keyboard")];
            1
        }
        op::DEFAULT_INPUT => 25,
        op::INITIAL_INPUTS => {
            let letter = char::from(b'A' + u8::try_from(a.i[0]).unwrap_or(0));
            f.lists[4] = vec![wide(&format!("Keyboard.{letter}"))];
            1
        }
        op::ACTION_CLEAR => {
            f.log.push(format!("clear {}", a.i[0]));
            0
        }
        op::ACTION_ADD => {
            f.log
                .push(format!("add {} {} {} {}", a.i[0], a.i[1], a.i[2], a.i[3]));
            0
        }
        op::ACTION_NAME => {
            out(f, a, wide("Act"));
            0
        }
        op::GUI_MOUSE => a.i[0] as usize,
        op::AXIS_RANGE => 0,
        _ => 0,
    }
}
