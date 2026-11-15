use super::hint_handler::{Hint, HintHandler};
use super::host::{Hud, Slot, Ui};
use super::msg_handler::{MessageHandler, Msg};
use crate::obj::Arena;
use std::cell::RefCell;

fn w(s: &str) -> Vec<u32> {
    s.chars().map(u32::from).collect()
}

fn s(t: &[u32]) -> String {
    t.iter().filter_map(|&c| char::from_u32(c)).collect()
}

/// Monospace font of 10 units per glyph; wraps on spaces at `width`.
#[derive(Default)]
struct Fake {
    log: RefCell<Vec<String>>,
    chinese: bool,
}

impl Fake {
    fn push(&self, l: String) {
        self.log.borrow_mut().push(l);
    }

    fn take(&self) -> Vec<String> {
        std::mem::take(&mut *self.log.borrow_mut())
    }
}

impl Ui for Fake {
    fn menu_str(&self, _: &[u8], k: &[u8], _: &[u8]) -> Vec<u8> {
        match k {
            b"QuestAddedSound" => b"quest.snt".to_vec(),
            b"QuestAddedIcon" => b"quest.png".to_vec(),
            _ => Vec::new(),
        }
    }
    fn menu_f32(&self, _: &[u8], k: &[u8], _: f32) -> f32 {
        match k {
            b"YPos" => 40.0,
            _ => 2.0,
        }
    }
    fn menu_vec2(&self, _: &[u8], _: &[u8], _: f32) -> [f32; 2] {
        [15.0, 15.0]
    }
    fn user_bool(&self, _: &[u8], k: &[u8], d: bool) -> bool {
        k != b"ShowEffectSubtitles" && d
    }
    fn set_user_bool(&self, _: &[u8], k: &[u8], v: bool) {
        self.push(format!(
            "set {} {v}",
            s(&k.iter().map(|&c| u32::from(c)).collect::<Vec<_>>())
        ));
    }
    fn gfx_rect(&self, _: [f32; 4]) -> usize {
        1
    }
    fn gfx_image(&self, _: &[u8]) -> usize {
        2
    }
    fn gfx_texture(&self, f: &[u8]) -> usize {
        100 + f.len()
    }
    fn gfx_size(&self, _: usize, _: i32) -> [f32; 2] {
        [40.0, 20.0]
    }
    fn wrap_rows(&self, _: usize, width: f32, _: f32, _: [f32; 2], text: &[u32]) -> Vec<Vec<u32>> {
        let mut rows: Vec<Vec<u32>> = Vec::new();
        for word in s(text).split(' ') {
            let fits = rows
                .last()
                .is_some_and(|r| (r.len() + 1 + word.len()) as f32 * 10.0 <= width);
            match rows.last_mut() {
                Some(r) if fits => {
                    r.push(32);
                    r.extend(w(word));
                }
                _ => rows.push(w(word)),
            }
        }
        rows
    }
    fn font_len(&self, _: usize, _: [f32; 2], t: &[u32]) -> f32 {
        t.len() as f32 * 10.0
    }
    fn font_range(&self, _: usize) -> (u32, u32) {
        (32, 126)
    }
    fn font_glyph(&self, _: usize, _: u32) -> Option<([f32; 2], f32)> {
        Some(([1.0, 1.0], 0.5))
    }
    fn parse_string(&self, t: &[u32]) -> Vec<u32> {
        t.to_vec()
    }
    fn string_duration(&self, t: &[u32]) -> f32 {
        t.len() as f32 * 0.1
    }
    fn gui_sound(&self, n: &[u8]) {
        self.push(format!("snd {}", String::from_utf8_lossy(n)));
    }
    fn set_update_active(&self, on: bool) {
        self.push(format!("upd {}", u8::from(on)));
    }
    fn input_name(&self, a: &[u8]) -> Vec<u32> {
        w(&String::from_utf8_lossy(a))
    }
    fn hud(&self) -> Hud {
        Hud {
            center: [800.0, 600.0],
            start: [0.0, 0.0, 0.0],
        }
    }
    fn ptest(&self) -> bool {
        false
    }
    fn draw_gfx(
        &self,
        set: usize,
        g: usize,
        p: [f32; 3],
        sz: Option<[f32; 2]>,
        c: [f32; 4],
        m: i32,
    ) {
        self.push(format!("G {set} {g} {p:?} {sz:?} {c:?} {m}"));
    }
    fn draw_font(
        &self,
        set: usize,
        f: usize,
        p: [f32; 3],
        sz: [f32; 2],
        c: [f32; 4],
        a: i32,
        m: i32,
        t: &[u32],
        sp: bool,
    ) {
        self.push(format!(
            "F {set} {f} {p:?} {sz:?} {c:?} {a} {m} {}|{sp}",
            s(t)
        ));
    }
    fn translate(&self, c: &[u8], k: &[u8]) -> Vec<u32> {
        w(&format!(
            "<{}:{}>",
            String::from_utf8_lossy(c),
            String::from_utf8_lossy(k)
        ))
    }
    fn chinese(&self) -> bool {
        self.chinese
    }
    fn callback(&self, cb: usize, yes: bool) {
        self.push(format!("cb {cb} {}", u8::from(yes)));
    }
}

fn hint_with(ui: &Fake) -> (RefCell<Arena<HintHandler>>, crate::obj::Handle<HintHandler>) {
    let mut a = Arena::default();
    let mut h = HintHandler::new(ui);
    h.font = 9;
    let id = a.insert(h);
    (RefCell::new(a), id.expect("slot"))
}

#[test]
fn hint_queue_rules_and_names() {
    let ui = Fake::default();
    let (arena, h) = hint_with(&ui);
    let x = Hint {
        ui: &ui,
        c: Slot { arena: &arena, h },
    };
    assert!(x.add(b"a", &w("one"), 2.0));
    assert!(!x.add(b"b", &w("two"), 2.0), "one hint at a time");
    x.c.with(|s| s.s.show_count = 0.0);
    x.c.with(|s| s.block(b"b"));
    assert!(!x.add(b"b", &w("two"), 2.0));
    x.c.with(|s| s.unblock(b"b"));
    assert!(!x.add(b"a", &w("one"), 2.0), "already given");
    assert!(x.add(b"b", &w("two"), 2.0));
    assert_eq!(
        x.c.with(|s| s.names(false)),
        Some(vec![b"a".to_vec(), b"b".to_vec()])
    );
    assert_eq!(x.c.with(|s| s.remove(b"a")), Some(true));
    assert_eq!(x.c.with(|s| s.remove(b"a")), Some(false));
    x.c.with(|s| s.reset());
    assert_eq!(x.c.with(|s| s.names(false)), Some(vec![]));
}

#[test]
fn hint_draw_rows_and_icons() {
    let ui = Fake::default();
    let (arena, h) = hint_with(&ui);
    let x = Hint {
        ui: &ui,
        c: Slot { arena: &arena, h },
    };
    x.c.with(|s| s.s.active = 1);
    assert!(x.add(b"i", &w("go $GamepadButton.Button A$ now"), 3.0));
    x.c.with(|s| s.s.alpha = 0.5);
    ui.take();
    x.draw(7);
    let log = ui.take();
    assert_eq!(
        log[0],
        "F 7 9 [400.0, 40.0, 20.0] [15.0, 15.0] [1.0, 0.0, 0.0, 0.5] 2 2 <Hints:HINT:>|true"
    );
    assert!(log[1].ends_with("2 2 go      now|true"), "{}", log[1]);
    let g = log.last().expect("icon draw");
    assert!(g.starts_with("G 7 113 [370.0, 58.0, 20.0]"), "{g}");
    assert!(g.contains("Some([30.0, 15.0])"), "{g}");
}

#[test]
fn hint_chinese_and_missing_icons() {
    let ui = Fake {
        chinese: true,
        ..Fake::default()
    };
    let (arena, h) = hint_with(&ui);
    let x = Hint {
        ui: &ui,
        c: Slot { arena: &arena, h },
    };
    assert!(x.add(
        b"i",
        &w("$GamepadButton.Button Guide$|$GamepadButton.Nope$|$GamepadButton.Button A$"),
        1.0
    ));
    let t = x.c.with(|s| s.s.show_count);
    assert_eq!(t, Some(1.0));
    ui.take();
    x.c.with(|s| s.s.active = 1);
    x.c.with(|s| s.s.alpha = 1.0);
    x.draw(0);
    let row = ui.take();
    assert!(
        row[1].contains("<ButtonNames:Button Guide>| |"),
        "{}",
        row[1]
    );
    assert!(
        row[1].contains(&format!("|{}|", " ".repeat(16))),
        "{}",
        row[1]
    );
}

fn msg_with(
    ui: &Fake,
) -> (
    RefCell<Arena<MessageHandler>>,
    crate::obj::Handle<MessageHandler>,
) {
    let mut a = Arena::default();
    let mut h = MessageHandler::new(ui);
    h.font = 9;
    let id = a.insert(h);
    (RefCell::new(a), id.expect("slot"))
}

#[test]
fn message_pause_flow() {
    let ui = Fake::default();
    let (arena, h) = msg_with(&ui);
    let x = Msg {
        ui: &ui,
        c: Slot { arena: &arena, h },
    };
    x.start_pause(&w("a b"), true, 5);
    assert_eq!(ui.take(), ["upd 0"]);
    x.do_action(0, true);
    assert_eq!(ui.take(), ["upd 1", "cb 5 1"]);
    x.start_pause(&w("a b"), true, 0);
    x.do_action(1, true);
    assert_eq!(ui.take(), ["upd 0", "upd 1"]);
    x.start_pause(&w("a"), false, 5);
    x.do_action(0, false);
    x.do_action(0, true);
    assert_eq!(ui.take(), ["upd 0", "upd 1"]);
}

#[test]
fn message_draw_rows_and_quest() {
    let ui = Fake::default();
    let (arena, h) = msg_with(&ui);
    let x = Msg {
        ui: &ui,
        c: Slot { arena: &arena, h },
    };
    x.set_message(&w("hello"), 2.0);
    x.start_pause(&w("p"), true, 0);
    ui.take();
    for _ in 0..40 {
        x.update(0.05);
    }
    ui.take();
    x.draw();
    let log = ui.take();
    assert!(
        log[0].starts_with("F 0 9 [400.0, 345.0, 4.0]"),
        "{}",
        log[0]
    );
    assert!(log.iter().any(|l| l.contains("(Interact)|false")));
    assert!(
        log.iter()
            .any(|l| l.starts_with("G 0 1 [0.0, 0.0, 3.0] Some([800.0, 600.0])"))
    );
    x.c.with(MessageHandler::start_quest_added);
    for _ in 0..4 {
        x.update(0.05);
    }
    assert!(ui.take().contains(&"snd quest.snt".to_string()));
    x.draw();
    let q: Vec<_> = ui
        .take()
        .into_iter()
        .filter(|l| l.starts_with("G 0 2 "))
        .collect();
    assert_eq!(q.len(), 3);
    assert!(q[0].ends_with(" 6"), "{}", q[0]);
    assert!(q[1].ends_with(" 3"), "{}", q[1]);
}

#[test]
fn message_user_config() {
    let ui = Fake::default();
    let (arena, h) = msg_with(&ui);
    let x = Msg {
        ui: &ui,
        c: Slot { arena: &arena, h },
    };
    x.c.with(|m| m.load_user_config(&ui));
    assert_eq!(
        x.c.with(|m| (m.s.show_subtitles, m.s.show_effect_subtitles)),
        Some((1, 0))
    );
    x.c.with(|m| m.save_user_config(&ui));
    assert_eq!(
        ui.take(),
        ["set ShowSubtitles true", "set ShowEffectSubtitles false"]
    );
}
