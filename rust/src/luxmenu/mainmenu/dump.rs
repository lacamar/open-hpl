//! The A10a and A9 gate dumps (OHPL_A10A_DUMP, OHPL_A9_MENUDUMP) of the main menu, read straight
//! from the Rust-owned widgets; the lines are the ones the C++ hooks wrote.

use super::{Menu, WIN_KEY_CONFIG, WIN_OPTIONS, h};
use crate::gui::gw_h20::{self, P};
use crate::gui::gw_kinds::{self as gk, Kind};
use crate::gui::{direct as d, g1_set, gw, gw_items as gi, h20_list};
use crate::luxmenu::win::host as wh;
use crate::scene::h10::CamState;
use crate::somascript::cfmt::fmt_g9;
use std::collections::HashMap;
use std::ffi::{c_char, c_int};
use std::fs::OpenOptions;
use std::io::Write;

unsafe extern "C" {
    fn snprintf(buf: *mut c_char, n: usize, fmt: *const c_char, ...) -> c_int;
    fn wcstombs(dst: *mut u8, src: *const u32, n: usize) -> usize;
}

const TYPE_WINDOW: i32 = 1;
const TYPE_BUTTON: i32 = 2;
const TYPE_LABEL: i32 = 4;
const TYPE_TEXTBOX: i32 = 6;
const TYPE_IMAGE: i32 = 8;
const TYPE_LISTBOX: i32 = 9;
const SELECT_TOGGLE: i32 = 1;
const MSG_BUTTON_PRESSED: i32 = 11;
const MSG_MOUSE_DOWN: i32 = 1;
const ARROWS: [i32; 4] = [0, 1, 2, 3];

fn fixed(v: f32, digits: usize) -> String {
    let mut buf = [0u8; 48];
    let fmt: &[u8] = match digits {
        3 => b"%.3f\0",
        4 => b"%.4f\0",
        _ => b"%.6f\0",
    };
    // SAFETY: the buffer holds 48 bytes and the format takes one double.
    let n = unsafe {
        snprintf(
            buf.as_mut_ptr().cast(),
            buf.len(),
            fmt.as_ptr().cast(),
            f64::from(v),
        )
    };
    let n = usize::try_from(n).unwrap_or(0).min(buf.len() - 1);
    String::from_utf8_lossy(buf.get(..n).unwrap_or_default()).into_owned()
}

fn g9(v: f32) -> Vec<u8> {
    fmt_g9(v)
}

/// `cString::To8Char`: empty when the string does not convert.
fn narrow(wide: &[u32]) -> Vec<u8> {
    let src: Vec<u32> = wide.iter().copied().chain([0]).collect();
    // SAFETY: `src` is NUL-terminated; `dst` has the bytes asked for plus the NUL slot.
    unsafe {
        let n = wcstombs(std::ptr::null_mut(), src.as_ptr(), 0);
        if n == usize::MAX {
            return Vec::new();
        }
        let mut dst = vec![0u8; n + 1];
        wcstombs(dst.as_mut_ptr(), src.as_ptr(), n);
        dst.truncate(n);
        dst
    }
}

fn wide(s: &str) -> Vec<u32> {
    s.chars().map(u32::from).collect()
}

fn children(w: usize) -> Vec<usize> {
    gw::read(w, |g| g.children.clone())
}

fn number(w: usize, ids: &mut HashMap<usize, i32>) {
    let id = i32::try_from(ids.len()).unwrap_or(i32::MAX);
    ids.insert(w, id);
    for c in children(w) {
        number(c, ids);
    }
}

fn nav(w: usize, ids: &HashMap<usize, i32>) -> [i32; 4] {
    ARROWS.map(|a| {
        let n = gw::focus_navigation(w, 1 << a);
        if n != 0 { ids.get(&n).copied() } else { None }.unwrap_or(-1)
    })
}

fn put(out: &mut Vec<u8>, s: impl AsRef<[u8]>) {
    out.extend_from_slice(s.as_ref());
}

fn item_text(it: *mut gi::Item) -> Vec<u32> {
    let mut n = 0usize;
    let p = gi::ohpl_gwi_text(it, &mut n);
    if p.is_null() {
        Vec::new()
    } else {
        // SAFETY: the item text is readable for `n` units until its next change.
        unsafe { std::slice::from_raw_parts(p, n) }.to_vec()
    }
}

fn type_of(w: usize) -> i32 {
    gw::read(w, |g| g.ty)
}

fn dump_widget(out: &mut Vec<u8>, w: usize, depth: usize, ids: &HashMap<usize, i32>) {
    let id = ids.get(&w).copied().unwrap_or(0);
    let name = gw::read(w, |g| g.name.clone());
    let name = name.split(|&b| b == 0).next().unwrap_or_default();
    put(out, " ".repeat(depth * 2));
    put(out, format!("#{id} type={} name=", type_of(w)));
    put(out, name);
    let (p, s) = (d::widget_get_local_pos(w), d::widget_get_size(w));
    let list = |v: &[f32]| {
        let mut o = Vec::new();
        for (i, x) in v.iter().enumerate() {
            if i > 0 {
                o.push(b',');
            }
            o.extend(g9(*x));
        }
        o
    };
    put(out, " pos=");
    put(out, list(&p));
    put(out, " size=");
    put(out, list(&s));
    put(out, " text=\"");
    put(out, narrow(&gw::text(w)));
    let (uv, ud) = gw::read(w, |g| (g.user_value, !g.user_data.is_null()));
    put(
        out,
        format!(
            "\" vis={} en={} uv={uv} ud={}",
            i32::from(gw::is_visible(w)),
            i32::from(gw::is_enabled(w)),
            i32::from(ud)
        ),
    );
    let (c, fs) = gw::read(w, |g| (g.p.font_color, g.p.font_size));
    put(out, " col=");
    put(out, list(&[c.r, c.g, c.b, c.a]));
    put(out, " fs=");
    put(out, list(&[fs.x, fs.y]));
    let n = nav(w, ids);
    put(out, format!(" nav={},{},{},{}", n[0], n[1], n[2], n[3]));
    let kind = gw::read(w, |g| match &g.kind {
        Kind::Check(_) => 1,
        Kind::Slider(_) => 2,
        Kind::Combo(_) => 3,
        Kind::Label(_) => 4,
        _ => 0,
    });
    match kind {
        1 => put(
            out,
            format!(" checked={}", i32::from(gk::ohpl_gw_check_checked(w))),
        ),
        2 => put(
            out,
            format!(" slider={}/{}", d::slider_get(w), d::slider_get_max(w)),
        ),
        3 => {
            let sel = gw::read(w, |g| match &g.kind {
                Kind::Combo(c) => c.combo.selected,
                _ => 0,
            });
            put(out, format!(" combo={sel}["));
            for i in 0..gi::num(w) {
                if i > 0 {
                    put(out, ",");
                }
                put(out, "\"");
                put(out, narrow(&item_text(gi::get(w, i as i32))));
                put(out, "\"");
            }
            put(out, "]");
        }
        4 => put(
            out,
            format!(
                " align={} wrap={}",
                gk::ohpl_gw_label_align(w),
                i32::from(gk::ohpl_gw_label_word_wrap(w))
            ),
        ),
        _ => {}
    }
    put(out, "\n");
    for c in children(w) {
        dump_widget(out, c, depth + 1, ids);
    }
}

fn append(path: &std::ffi::OsStr) -> Option<std::fs::File> {
    OpenOptions::new().create(true).append(true).open(path).ok()
}

fn print(out: &mut Vec<u8>, w: usize, ids: &HashMap<usize, i32>, depth: usize) {
    let id = |w: usize| ids.get(&w).copied().unwrap_or(-1);
    let (p, s) = (d::widget_get_local_pos(w), d::widget_get_size(w));
    let c = gw::read(w, |g| g.p.font_color);
    let ty = type_of(w);
    let n = ARROWS.map(|a| id(gw::focus_navigation(w, 1 << a)));
    put(out, format!("{}{} t={ty} '", " ".repeat(depth), id(w),));
    put(out, narrow(&gw::text(w)));
    put(
        out,
        format!(
            "' p={},{},{} s={},{} v={} e={} c={},{},{},{} nav={},{},{},{}",
            fixed(p[0], 3),
            fixed(p[1], 3),
            fixed(p[2], 3),
            fixed(s[0], 3),
            fixed(s[1], 3),
            i32::from(gw::is_visible(w)),
            i32::from(gw::is_enabled(w)),
            fixed(c.r, 3),
            fixed(c.g, 3),
            fixed(c.b, 3),
            fixed(c.a, 3),
            n[0],
            n[1],
            n[2],
            n[3]
        ),
    );
    if ty == TYPE_LISTBOX
        && let Some(P::List(lb)) = gw_h20::ptr(w)
    {
        {
            // SAFETY: the list state is live while the widget is.
            let (sel, locked) =
                unsafe { (h20_list::selected_item(lb), i32::from((*lb).list.locked)) };
            put(out, format!(" sel={sel} lock={locked} items="));
            for i in 0..gi::num(w) {
                put(out, "[");
                put(out, narrow(&item_text(gi::get(w, i as i32))));
                put(out, "]");
            }
        }
    }
    if ty == TYPE_IMAGE {
        let has = !gk::ohpl_gw_image(w).is_null();
        put(out, format!(" gfx={}", i32::from(has)));
    }
    put(out, "\n");
    for ch in children(w) {
        print(out, ch, ids, depth + 1);
    }
}

fn find(w: usize, ty: i32, text: Option<&[u32]>, skip: &mut i32) -> usize {
    if type_of(w) == ty && text.is_none_or(|t| gw::text(w) == t) {
        let hit = *skip == 0;
        *skip -= 1;
        if hit {
            return w;
        }
    }
    for c in children(w) {
        let r = find(c, ty, text, skip);
        if r != 0 {
            return r;
        }
    }
    0
}

struct Step {
    name: &'static str,
    window: i32,
    scope: Option<(&'static str, &'static str)>,
    ty: i32,
    cat: &'static str,
    key: &'static str,
}

const fn st(
    name: &'static str,
    window: i32,
    scope: Option<(&'static str, &'static str)>,
    ty: i32,
    cat: &'static str,
    key: &'static str,
) -> Step {
    Step {
        name,
        window,
        scope,
        ty,
        cat,
        key,
    }
}

const START: Option<(&str, &str)> = Some(("MainMenu", "Start Game"));
const LOAD: Option<(&str, &str)> = Some(("LoadGame", "LoadGameTitle"));
const STORIES: Option<(&str, &str)> = Some(("MainMenu", "Custom Map"));
const PROFILES: Option<(&str, &str)> = Some(("MainMenu", "Profiles"));
const NAME: Option<(&str, &str)> = Some(("MainMenu", "Create Profile"));
const POPUP: Option<(&str, &str)> = Some(("", ""));
const STORY_BACK: Option<(&str, &str)> = Some(("=", "A9 Story"));
const BTN: i32 = TYPE_BUTTON;
const LBL: i32 = TYPE_LABEL;
const LAST: i32 = 7;
const PRESS: i32 = -1;
const TEXT: i32 = -2;
const CREATED: i32 = -3;
const SELECT: i32 = -4;
const CLICK: i32 = -5;
const TICK: i32 = -6;

fn steps() -> Vec<Step> {
    let none = None;
    vec![
        st("created", CREATED, none, LBL, "", ""),
        st("start", 6, none, LBL, "", ""),
        st("hard", PRESS, START, BTN, "MainMenu", "HardMode"),
        st("normal", PRESS, START, BTN, "MainMenu", "NormalMode"),
        st("startpress", PRESS, START, BTN, "MainMenu", "Start Game"),
        st("startno", PRESS, POPUP, BTN, "MainMenu", "No"),
        st("startcancel", PRESS, START, BTN, "Global", "Cancel"),
        st("load", 3, none, LBL, "", ""),
        st("loadok", PRESS, LOAD, BTN, "Global", "OK"),
        st("loadpopup", PRESS, POPUP, BTN, "Global", "OK"),
        st("loadcancel", PRESS, LOAD, BTN, "Global", "Cancel"),
        st("stories", 4, none, LBL, "", ""),
        st("storyok0", PRESS, STORIES, BTN, "Global", "OK"),
        st("storypopup", PRESS, POPUP, BTN, "Global", "OK"),
        st("storysel", SELECT, STORIES, TYPE_LISTBOX, "", ""),
        st("storyok", PRESS, STORIES, BTN, "Global", "OK"),
        st("storyback", PRESS, STORY_BACK, BTN, "Global", "Cancel"),
        st("storycancel", PRESS, STORIES, BTN, "Global", "Cancel"),
        st("profiles", 0, none, LBL, "", ""),
        st("profcreate", PRESS, PROFILES, BTN, "MainMenu", "Create"),
        st("profname", TEXT, NAME, TYPE_TEXTBOX, "", ""),
        st("profconfirm", PRESS, NAME, BTN, "MainMenu", "Create"),
        st("profdelete", PRESS, PROFILES, BTN, "MainMenu", "Delete"),
        st("profno", PRESS, POPUP, BTN, "MainMenu", "No"),
        st("top", LAST, none, LBL, "", ""),
        st("tick", TICK, none, LBL, "", ""),
        st("press_options", CLICK, none, LBL, "MainMenu", "Options"),
        st("top2", LAST, none, LBL, "", ""),
        st(
            "press_profile",
            CLICK,
            none,
            LBL,
            "MainMenu",
            "Change Profile",
        ),
        st("top3", LAST, none, LBL, "", ""),
        st("press_custom", CLICK, none, LBL, "MainMenu", "Custom Map"),
        st("top4", LAST, none, LBL, "", ""),
        st("press_start", CLICK, none, LBL, "MainMenu", "Start Game"),
        st("press_start_no", PRESS, POPUP, BTN, "MainMenu", "No"),
        st("top5", LAST, none, LBL, "", ""),
        st("press_exit", CLICK, none, LBL, "MainMenu", "Exit"),
        st("exit_tick", TICK, none, LBL, "", ""),
        st("exit_no", PRESS, POPUP, BTN, "MainMenu", "No"),
        st("exit_tick2", TICK, none, LBL, "", ""),
    ]
}

fn press(w: usize, message: i32) {
    gw::process_message(w, message, &crate::gui::set::Msg::zero(), false, false);
}

impl Menu {
    pub fn dump_a10a(&self) {
        let Some(path) = std::env::var_os("OHPL_A10A_DUMP") else {
            return;
        };
        for (name, win) in [("options", WIN_OPTIONS), ("keyconfig", WIN_KEY_CONFIG)] {
            let root = self.window_dump(win, true);
            if root != 0
                && let Some(mut f) = append(&path)
            {
                let mut ids = HashMap::new();
                number(root, &mut ids);
                let mut out = format!("== {name}\n").into_bytes();
                dump_widget(&mut out, root, 0, &ids);
                let _ = f.write_all(&out);
            }
            self.window_dump(win, false);
        }
    }

    pub fn dump_a9(&self) {
        let Some(path) = std::env::var_os("OHPL_A9_MENUDUMP") else {
            return;
        };
        self.load_fonts();
        self.create_gui();
        let Some(mut f) = append(&path) else {
            return;
        };
        let info = self.dump_info();
        let set = info.set as *mut g1_set::Set;
        let mut root = info.first_label;
        while root != 0 && gw::read(root, |g| g.base.parent) != 0 {
            root = gw::read(root, |g| g.base.parent);
        }
        if root == 0 {
            return;
        }
        let story_dir = {
            let mut d = narrow(&wh::lux_str(wh::lux_path::USER_STORY_ROOT));
            d.extend_from_slice(b"A9story/");
            std::path::PathBuf::from(std::ffi::OsString::from(
                String::from_utf8_lossy(&d).into_owned(),
            ))
        };
        let _ = std::fs::create_dir_all(story_dir.join("maps"));
        let _ = std::fs::File::create(story_dir.join("maps/start.hpm"));
        let _ = std::fs::write(
            story_dir.join("custom_story_settings.cfg"),
            "<Main\n\tName = \"A9 Story\"\n\tAuthor = \"Tester\"\n\tStartMap = \"start.hpm\"\n\tMapsFolder = \"maps\"\n/>\n",
        );
        for s in steps() {
            let mut scope = root;
            if let Some((cat, key)) = s.scope {
                if cat.is_empty() {
                    scope = children(root).last().copied().unwrap_or(0);
                } else {
                    let title = if cat == "=" {
                        wide(key)
                    } else {
                        wh::tr(cat, key)
                    };
                    scope = find(root, TYPE_WINDOW, Some(&title), &mut 0);
                }
            }
            match s.window {
                w if w >= 0 => self.set_window_active(w),
                TICK => {
                    for _ in 0..20 {
                        self.update(1.0 / 60.0);
                    }
                }
                CLICK | PRESS => {
                    let text = wh::tr(s.cat, s.key);
                    let from = if s.window == CLICK { root } else { scope };
                    let w = if from != 0 {
                        find(from, s.ty, Some(&text), &mut 0)
                    } else {
                        0
                    };
                    if w != 0 {
                        press(
                            w,
                            if s.window == CLICK {
                                MSG_MOUSE_DOWN
                            } else {
                                MSG_BUTTON_PRESSED
                            },
                        );
                    } else {
                        let _ = writeln!(f, "!! no widget {}", s.name);
                    }
                }
                SELECT => {
                    let w = if scope != 0 {
                        find(scope, TYPE_LISTBOX, None, &mut 0)
                    } else {
                        0
                    };
                    if let Some(P::List(lb)) = gw_h20::ptr(w).filter(|_| w != 0) {
                        // SAFETY: the list state is live while the widget is.
                        unsafe {
                            crate::gui::h20_ffi::ohpl_h20_lb_set_selected_item(
                                gw_h20::vt(),
                                lb,
                                0,
                                false,
                                true,
                                SELECT_TOGGLE,
                                true,
                            );
                        }
                    }
                }
                TEXT => {
                    let w = if scope != 0 {
                        find(scope, TYPE_TEXTBOX, None, &mut 0)
                    } else {
                        0
                    };
                    if w != 0 {
                        gw::set_text(w, &wide("A9dump"));
                    }
                }
                _ => {}
            }
            let mut ids = HashMap::new();
            number(root, &mut ids);
            let info = self.dump_info();
            let id = |w: usize| ids.get(&w).copied().unwrap_or(-1);
            let hard = i32::from(h::flag(h::flag::HARD));
            let line = format!(
                "== {} win={} hard={hard} focus={} attn={} drawfocus={} popup={} topvis={} ta={} fa={}\n",
                s.name,
                info.window,
                id(g1_set::focused_widget(set)),
                id(g1_set::attention_widget(set)),
                i32::from(g1_set::draw_focus(set)),
                // SAFETY: the set is live.
                i32::from(unsafe { g1_set::ohpl_g1_set_popup_is_active(set) }),
                i32::from(info.top_visible),
                fixed(info.top_alpha, 6),
                fixed(info.fade_alpha, 6),
            );
            let _ = f.write_all(line.as_bytes());
            let mut out = Vec::new();
            print(&mut out, root, &ids, 0);
            let _ = f.write_all(&out);
        }
        self.set_window_active(LAST);
        self.dump_background();
        let info = self.dump_info();
        let _ = writeln!(
            f,
            "== background world={} camera={}",
            i32::from(info.world != 0),
            i32::from(info.camera != 0)
        );
        if info.camera != 0 {
            // SAFETY: `mS` is the first member of a live cCamera.
            let cam = unsafe { &mut *(info.camera as *mut CamState) };
            let _ = writeln!(
                f,
                "cam fov={} aspect={} pos={},{},{}",
                fixed(cam.fov, 6),
                fixed(cam.aspect, 6),
                fixed(cam.position.x, 4),
                fixed(cam.position.y, 4),
                fixed(cam.position.z, 4)
            );
            let m = cam.view_matrix().m;
            for r in m {
                let _ = writeln!(
                    f,
                    "view {} {} {} {}",
                    fixed(r[0], 4),
                    fixed(r[1], 4),
                    fixed(r[2], 4),
                    fixed(r[3], 4)
                );
            }
        }
        let _ = writeln!(
            f,
            "logo={} logosize={},{}",
            i32::from(info.logo != 0),
            fixed(info.logo_size[0], 4),
            fixed(info.logo_size[1], 4)
        );
        let _ = std::fs::remove_dir_all(&story_dir);
        let mut save = wh::lux_str(wh::lux_path::BASE_SAVE);
        save.extend(wide("A9dump"));
        wh::remove_folder(&save);
        self.dump_end();
    }
}
