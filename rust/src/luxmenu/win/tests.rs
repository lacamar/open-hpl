//! Window layout against the widget trees the C++ windows built (golden `created.txt`, recorded
//! with `OHPL_A9_MENUDUMP` before the port) and callback flows against the fake host.

use super::fake::{self, Widget, with};
use super::host::{WStr, lux_op, lux_path, msg, ws};
use super::*;

const GOLDEN: &str = include_str!("../../../tests/golden/A9/created.txt");

const ADAPTER: usize = 0xA0;
const SET: usize = 0x5E7;

#[derive(Debug, Clone)]
struct G {
    depth: usize,
    ty: u32,
    text: String,
    pos: [f32; 3],
    size: [f32; 2],
    nav: [i32; 4],
    id: i32,
}

fn floats<const N: usize>(s: &str) -> [f32; N] {
    let mut out = [0.0; N];
    for (o, v) in out.iter_mut().zip(s.split(',')) {
        *o = v.parse().unwrap_or(0.0);
    }
    out
}

fn parse(line: &str) -> G {
    let depth = line.len() - line.trim_start().len();
    let rest = line.trim_start();
    let id = rest.split(' ').next().unwrap_or("0").parse().unwrap_or(0);
    let q1 = rest.find('\'').unwrap_or(0);
    let q2 = rest.rfind("' p=").unwrap_or(q1);
    let tail = rest.get(q2 + 2..).unwrap_or("");
    let before = rest.get(..q1).unwrap_or("");
    let key = |from: &str, k: &str| -> String {
        from.split(' ')
            .find_map(|t| t.strip_prefix(k))
            .unwrap_or("")
            .to_string()
    };
    let nav: [f32; 4] = floats(&key(tail, "nav="));
    G {
        depth,
        ty: key(before, "t=").parse().unwrap_or(0),
        text: rest.get(q1 + 1..q2).unwrap_or("").to_string(),
        pos: floats(&key(tail, "p=")),
        size: floats(&key(tail, "s=")),
        nav: nav.map(|v| v as i32),
        id,
    }
}

fn golden() -> Vec<G> {
    GOLDEN.lines().map(parse).collect()
}

/// Entries of the top-level window titled `title`, and its descendants.
fn group(all: &[G], title: &str) -> Vec<G> {
    let start = all
        .iter()
        .position(|g| g.depth == 1 && g.ty == 1 && g.text == title)
        .expect("golden window");
    let mut out = vec![all[start].clone()];
    out.extend(all[start + 1..].iter().take_while(|g| g.depth > 1).cloned());
    out
}

fn kind_of(ty: u32) -> i32 {
    match ty {
        1 => 0,
        2 => 1,
        3 => 5,
        4 => 2,
        6 => 6,
        8 => 4,
        9 => 3,
        _ => -1,
    }
}

fn near(a: &[f32], b: &[f32]) -> bool {
    a.iter().zip(b).all(|(x, y)| (x - y).abs() < 0.0011)
}

fn text_of(w: &Widget) -> String {
    let t = String::from_utf16_lossy(&w.text.iter().map(|&c| c as u16).collect::<Vec<_>>());
    t.rsplit('/')
        .next()
        .map_or_else(String::new, str::to_string)
}

fn widgets() -> Vec<Widget> {
    with(|s| s.widgets.clone())
}

fn seed_config(all: &[G]) {
    let prof = group(all, "Profiles");
    let list = prof.iter().find(|g| g.ty == 9).expect("list");
    let enter = group(all, "Create Profile");
    let create = enter
        .iter()
        .find(|g| g.ty == 2 && g.text == "Create")
        .expect("create");
    let label = enter.iter().find(|g| g.ty == 4).expect("label");
    let story = group(all, "");
    let button = story
        .iter()
        .find(|g| g.ty == 2 && g.text == "Continue")
        .expect("button");
    with(|s| {
        s.cfg.insert("Profiles/WindowSize".into(), prof[0].size);
        s.cfg.insert(
            "Profiles/InformationWidth".into(),
            [500.0 - list.pos[0] - list.size[0] - 35.0, 0.0],
        );
        s.cfg
            .insert("Profiles/EnterNameWindowSize".into(), enter[0].size);
        s.cfg.insert(
            "Profiles/EnterNameButtonLength".into(),
            [create.size[0], 0.0],
        );
        s.autosize = label.size;
        s.text_width = button.size[0] - 20.0;
    });
}

fn new_window(kind: i32, story: u64) -> u64 {
    ohpl_a9_win_new(kind, W::from_addr(ADAPTER), W::from_addr(SET), story)
}

fn raw(w: &Widget) -> String {
    w.text.iter().filter_map(|&c| char::from_u32(c)).collect()
}

fn replay(kind: i32, titles: &[&str]) {
    fake::reset();
    let all = golden();
    seed_config(&all);
    let want: Vec<G> = titles.iter().flat_map(|t| group(&all, t)).collect();
    let title = titles;
    let bits = new_window(kind, 0);
    ohpl_a9_win_create_gui(bits);
    let made = widgets();
    let mut used = vec![false; want.len()];
    let mut pair = Vec::new();
    for (n, w) in made.iter().enumerate() {
        let text = raw(w);
        let hit = want.iter().enumerate().position(|(i, g)| {
            !used[i]
                && kind_of(g.ty) == w.kind
                && (text.contains('/') || g.text == text)
                && (g.ty == 1 || near(&g.pos, &w.pos))
                && near(
                    &g.size[..if w.kind == 6 { 1 } else { 2 }],
                    &w.size[..if w.kind == 6 { 1 } else { 2 }],
                )
        });
        let Some(i) = hit else {
            panic!("no golden entry for widget {n} {w:?} in {title:?}");
        };
        used[i] = true;
        pair.push(i);
    }
    for (n, w) in made.iter().enumerate() {
        let g = &want[pair[n]];
        let mut fake_nav = [-1i32; 4];
        for &(dir, target) in &w.nav {
            fake_nav[dir as usize] = pair[target - 1] as i32;
        }
        let golden_nav = g.nav.map(|id| {
            want.iter()
                .position(|x| x.id == id)
                .map_or(-1, |p| p as i32)
        });
        assert_eq!(fake_nav, golden_nav, "nav of widget {n} in {title:?}");
    }
}

#[test]
fn start_layout() {
    replay(2, &["Start New Game"]);
}

fn tok_of(w: &Widget, message: i32) -> Option<usize> {
    w.cbs.iter().find(|c| c.0 == message).map(|c| c.1)
}

fn press_text(text: &str) -> bool {
    let w = widgets();
    let Some((i, x)) = w
        .iter()
        .enumerate()
        .find(|(_, x)| x.kind == 1 && text_of(x) == text)
    else {
        panic!("no widget {text}");
    };
    let obj = tok_of(x, msg::BUTTON_PRESSED).expect("press callback");
    ohpl_a9_cb(obj, W::from_addr(i + 1), 0)
}

fn lux_log() -> Vec<String> {
    with(|s| {
        s.log
            .iter()
            .filter(|l| l.starts_with("lux"))
            .cloned()
            .collect()
    })
}

#[test]
fn start_flow() {
    fake::reset();
    let bits = new_window(2, 0);
    ohpl_a9_win_create_gui(bits);
    assert!(press_text("HardMode"));
    assert_eq!(with(|s| s.lux.get(&0).copied()), Some(1));
    let w = widgets();
    let gold = [232.0 / 255.0, 201.0 / 255.0, 28.0 / 255.0, 1.0];
    let hard = w.iter().find(|x| text_of(x) == "HardMode").expect("hard");
    let normal = w
        .iter()
        .find(|x| text_of(x) == "NormalMode")
        .expect("normal");
    assert_eq!(hard.color, gold);
    assert_eq!(normal.color, [1.0; 4]);
    assert!(press_text("NormalMode"));
    assert_eq!(with(|s| s.lux.get(&0).copied()), Some(0));

    assert!(press_text("Start Game"));
    let popup = widgets().into_iter().find(|x| x.popup).expect("popup");
    assert_eq!(text_of(&popup), "Start a new game?");
    let before = lux_log().len();
    assert!(ohpl_a9_cb(popup.user, W::NULL, 1));
    assert_eq!(lux_log().len(), before);
    assert!(ohpl_a9_cb(popup.user, W::NULL, 0));
    let log = lux_log();
    assert_eq!(&log[before..], ["lux 3 0", "lux 15 2", "lux 14 7"]);

    assert!(press_text("Cancel"));
    assert_eq!(lux_log().last().map(String::as_str), Some("lux 14 7"));
    with(|s| {
        s.lux.insert(lux_op::CUSTOM_STORY_ACTIVE as usize as i32, 1);
    });
    assert!(press_text("Cancel"));
    assert_eq!(lux_log().last().map(String::as_str), Some("lux 14 5"));
}

#[test]
fn ui_press_routes_buttons() {
    fake::reset();
    ohpl_a9_win_create_gui(new_window(2, 0));
    let w = widgets();
    let start = w
        .iter()
        .position(|x| x.kind == 1 && text_of(x) == "Start Game")
        .expect("start");
    let obj = tok_of(&w[start], msg::UI_BUTTON_PRESS).expect("ui callback");
    let target = W::from_addr(start + 1);
    assert!(ohpl_a9_cb(obj, target, 1));
    assert!(widgets().iter().any(|x| x.popup));
    assert!(ohpl_a9_cb(obj, target, 2));
    assert_eq!(lux_log().last().map(String::as_str), Some("lux 14 7"));
    assert!(!ohpl_a9_cb(obj, target, 4));
}

#[test]
fn load_layout() {
    replay(1, &["Load Game"]);
}

#[test]
fn load_flow() {
    fake::reset();
    let day = |y: i32| [0, 0, 0, 1, 1, y, 0, 0];
    with(|s| {
        s.files = ["b.sav", "a.sav", "c.sav"].map(ws).to_vec();
        for (n, y) in [("b.sav", 2020), ("a.sav", 2021), ("c.sav", 2019)] {
            s.dates.insert(ws(n), day(y));
        }
    });
    let bits = new_window(1, 0);
    ohpl_a9_win_create_gui(bits);
    ohpl_a9_win_set_active(bits, true);
    let list = widgets().into_iter().find(|x| x.kind == 3).expect("list");
    let names: Vec<String> = list
        .items
        .iter()
        .map(|i| {
            String::from_utf8_lossy(&i.0.iter().map(|&c| c as u8).collect::<Vec<_>>()).into_owned()
        })
        .collect();
    assert_eq!(names, ["a.sav", "b.sav", "c.sav"]);
    assert!(list.locked);

    assert!(press_text("OK"));
    let popup = widgets().into_iter().find(|x| x.popup).expect("popup");
    assert_eq!(text_of(&popup), "NoGame");

    let idx = widgets().iter().position(|x| x.kind == 3).expect("list");
    with(|s| s.widgets[idx].selected = 1);
    assert!(press_text("OK"));
    let ask = widgets()
        .into_iter()
        .filter(|x| x.popup)
        .nth(1)
        .expect("popup");
    assert_eq!(text_of(&ask), "LoadGameMessage");
    let before = with(|s| s.log.len());
    assert!(ohpl_a9_cb(ask.user, W::NULL, 1));
    assert_eq!(with(|s| s.log.len()), before);
    assert!(ohpl_a9_cb(ask.user, W::NULL, 0));
    let log: Vec<String> = with(|s| s.log[before..].to_vec());
    assert_eq!(
        log.iter()
            .filter(|l| l.starts_with("lux"))
            .cloned()
            .collect::<Vec<_>>(),
        [
            "lux 2 0",
            "lux 14 7",
            "lux 5 0",
            "lux 6 0",
            "lux 7 0",
            "lux_with 2 5"
        ]
    );
}

#[test]
fn profile_layout() {
    replay(0, &["Profiles", "Create Profile"]);
}

fn folder_names() -> Vec<WStr> {
    ["zeta", "alpha"].map(ws).to_vec()
}

#[test]
fn profile_flow() {
    fake::reset();
    let all = golden();
    seed_config(&all);
    with(|s| {
        s.folders = folder_names();
        s.dates.insert(ws("zeta"), [0, 0, 0, 1, 1, 2020, 0, 0]);
        s.dates.insert(ws("alpha"), [0, 0, 0, 1, 1, 2021, 0, 0]);
        s.existing = vec![ws("taken")];
        s.lux_path.insert(lux_path::BASE_SAVE, ws("/s/"));
        s.lux_path.insert(lux_path::BASE_CONFIG, ws("/c/"));
    });
    let bits = new_window(0, 0);
    ohpl_a9_win_create_gui(bits);
    let list_at = |w: &[Widget]| w.iter().position(|x| x.kind == 3).expect("list");
    let w = widgets();
    let names = |w: &Widget| w.items.iter().map(|i| i.0.clone()).collect::<Vec<_>>();
    assert_eq!(names(&w[list_at(&w)]), [ws("zeta"), ws("alpha")]);
    assert_eq!(w[list_at(&w)].selected, 0);

    ohpl_a9_win_set_active(bits, true);
    assert!(widgets()[list_at(&w)].locked);

    assert!(press_text("Create"));
    let text_at = widgets().iter().position(|x| x.kind == 6).expect("textbox");
    assert_eq!(widgets()[text_at].text, ws("MainMenu/New Player"));
    assert!(
        widgets()
            .iter()
            .filter(|x| x.kind == 0)
            .all(|x| x.visible || x.text != ws("MainMenu/Create Profile"))
    );

    with(|s| s.widgets[text_at].text = ws("   "));
    let create = widgets()
        .iter()
        .enumerate()
        .filter(|(_, x)| text_of(x) == "Create")
        .map(|(i, x)| (i, tok_of(x, msg::BUTTON_PRESSED).expect("cb")))
        .next_back()
        .expect("create button");
    assert!(ohpl_a9_cb(create.1, W::from_addr(create.0 + 1), 0));
    let blank = widgets().into_iter().filter(|x| x.popup).count();
    assert_eq!(blank, 1);

    with(|s| s.widgets[text_at].text = ws("  taken "));
    assert!(ohpl_a9_cb(create.1, W::from_addr(create.0 + 1), 0));
    assert_eq!(widgets().into_iter().filter(|x| x.popup).count(), 2);

    with(|s| {
        s.widgets[text_at].text = ws(" Bob ");
        s.folders.push(ws("Bob"));
        s.dates.insert(ws("Bob"), [0, 0, 0, 1, 1, 2022, 0, 0]);
    });
    assert!(ohpl_a9_cb(create.1, W::from_addr(create.0 + 1), 0));
    let w = widgets();
    assert_eq!(w[list_at(&w)].items.len(), 3);
    assert_eq!(w[list_at(&w)].selected, 2);
    let select = widgets()
        .iter()
        .position(|x| x.kind == 1 && text_of(x) == "Select")
        .expect("select");
    assert_eq!(with(|s| s.focused), select + 1);

    assert!(press_text("Delete"));
    let ask = widgets().into_iter().rfind(|x| x.popup).expect("popup");
    assert_eq!(text_of(&ask), "Sure you want to delete profile");
    with(|s| {
        s.folder_ops.clear();
        s.folders.pop();
    });
    assert!(ohpl_a9_cb(ask.user, W::NULL, 0));
    let ops = with(|s| s.folder_ops.clone());
    assert_eq!(ops, [(2, ws("/c/Bob")), (2, ws("/s/Bob"))]);
    let w = widgets();
    assert_eq!(w[list_at(&w)].items.len(), 2);
    assert_eq!(w[list_at(&w)].selected, 1);
}

#[test]
fn profile_close_asks_when_no_user_config() {
    fake::reset();
    seed_config(&golden());
    let bits = new_window(0, 0);
    ohpl_a9_win_create_gui(bits);
    let w = widgets();
    let win = w.iter().position(|x| x.kind == 0).expect("window");
    let obj = tok_of(&w[win], msg::WINDOW_CLOSE).expect("close cb");
    assert!(ohpl_a9_cb(obj, W::from_addr(win + 1), 0));
    let popup = widgets().into_iter().find(|x| x.popup).expect("popup");
    assert_eq!(text_of(&popup), "Sure you want to quit?");
    assert!(ohpl_a9_cb(popup.user, W::NULL, 0));
    assert_eq!(lux_log().last().map(String::as_str), Some("lux 13 0"));
    assert!(ohpl_a9_cb(popup.user, W::NULL, 1));

    with(|s| {
        s.lux.insert(lux_op::USER_CONFIG_LOADED, 1);
    });
    assert!(ohpl_a9_cb(obj, W::from_addr(win + 1), 0));
    assert!(!widgets()[win].visible);
    let log = lux_log();
    assert_eq!(log[log.len() - 2], "lux 14 7");
    assert_eq!(log[log.len() - 1], format!("lux 16 {}", 0.01f32.to_bits()));
}

#[test]
fn story_layout() {
    replay(3, &[""]);
    replay(4, &["Custom Story"]);
}

#[test]
fn story_flow() {
    fake::reset();
    seed_config(&golden());
    with(|s| {
        s.lux.insert(lux_op::USERDIR_RESOURCES, 1);
        s.folders = vec![ws("one")];
        s.stories = vec![
            (ws("First"), ws("Ann"), ws("")),
            (ws(""), ws(""), ws("img.png")),
        ];
    });
    let story = new_window(3, 0);
    let list = new_window(4, story);
    ohpl_a9_win_create_gui(story);
    ohpl_a9_win_create_gui(list);
    ohpl_a9_win_set_active(list, true);
    let w = widgets();
    let at = w.iter().position(|x| x.kind == 3).expect("list");
    assert_eq!(w[at].items.len(), 2);
    assert_eq!(w[at].items[0].0, ws("First"));

    assert!(press_text("OK"));
    assert!(
        widgets()
            .iter()
            .any(|x| x.popup && text_of(x) == "NoValidStory")
    );

    with(|s| s.widgets[at].selected = 1);
    assert!(press_text("OK"));
    assert_eq!(lux_log().last().map(String::as_str), Some("lux 14 5"));
    let w = widgets();
    let title = w
        .iter()
        .find(|x| x.kind == 0 && x.text == ws("CustomStory/NoName"))
        .expect("title");
    assert!(title.visible);
    assert!(
        w.iter()
            .any(|x| x.kind == 2 && x.text == ws("CustomStory/By CustomStory/NoAuthor"))
    );
    assert!(
        w.iter()
            .any(|x| x.kind == 2 && x.text == ws("CustomStoryMain/Description"))
    );
    let pic = w
        .iter()
        .find(|x| x.kind == 4 && !x.image.is_empty())
        .expect("picture");
    assert_eq!(pic.image, ws("img.png"));

    let buttons: Vec<&Widget> = w
        .iter()
        .filter(|x| {
            x.kind == 1
                && matches!(
                    text_of(x).as_str(),
                    "Continue" | "Start" | "Load Game" | "Cancel"
                )
        })
        .take(4)
        .collect();
    let on: Vec<bool> = buttons.iter().map(|b| b.enabled).collect();
    assert_eq!(on, [false, true, false, true]);
    let step = buttons[1].size[1] + 6.0;
    assert!((buttons[3].pos[1] - buttons[1].pos[1] - step).abs() < 1e-4);

    let before = with(|s| s.log.len());
    assert!(press_text("Cancel"));
    let tail: Vec<String> = with(|s| s.log[before..].to_vec());
    assert_eq!(tail, ["lux 3 0", "lux 14 4"]);
    ohpl_a9_win_set_active(list, false);
    assert_eq!(
        with(|s| s.log.iter().filter(|l| *l == "story_free").count()),
        2
    );
}

#[test]
fn stale_handles_resolve_to_nothing() {
    fake::reset();
    let bits = new_window(2, 0);
    ohpl_a9_win_free(bits);
    ohpl_a9_win_create_gui(bits);
    assert!(widgets().is_empty());
    assert_eq!(new_window(9, 0), 0);
    assert!(!ohpl_a9_cb(0, W::NULL, 0));
}

#[test]
fn token_round_trip() {
    fake::reset();
    let bits = new_window(2, 0);
    let win = lookup(bits).expect("window");
    let t = win.base().tok(9);
    assert_eq!((t >> 8) as u64, bits);
    assert_eq!(t & 0xFF, 9);
}
