//! Replays the H21 cassettes (rust/tests/golden/H21/cassette_*.txt: the host traffic of the Rust
//! popups driving the real C++ widgets over seeded input, recorded by `harness/h21_popups` with
//! `H21_LOG`) against a mock host that answers from the recording. Every command the popups issue
//! must equal the recorded one, in order, and every entry point must return what it returned.

use super::h21_log::{Ids, fmt_cmd};
use super::h21_popup::{self as pop, Cmd, Host, Hx, OP_NAMES, Popup, Ret};
use super::set::Msg;
use std::cell::RefCell;
use std::collections::HashMap;
use std::path::Path;
use std::rc::Rc;

struct Mock {
    lines: Vec<String>,
    st: RefCell<St>,
}

struct St {
    pos: usize,
    ids: Ids,
    popups: HashMap<usize, Rc<Popup>>,
    strs: Vec<Box<[u32]>>,
}

fn parse_hex(s: &str) -> Vec<u32> {
    s.split(',')
        .filter(|x| !x.is_empty())
        .map(|x| u32::from_str_radix(x, 16).unwrap_or(0))
        .collect()
}

fn kv<'a>(line: &'a str, key: &str) -> &'a str {
    line.split(' ')
        .find_map(|t| t.strip_prefix(key).and_then(|r| r.strip_prefix('=')))
        .unwrap_or("")
}

fn num(line: &str, key: &str) -> usize {
    kv(line, key).parse().unwrap_or(0)
}

impl Mock {
    fn line(&self) -> String {
        let pos = self.st.borrow().pos;
        self.lines.get(pos).cloned().unwrap_or_default()
    }

    fn advance(&self) {
        self.st.borrow_mut().pos += 1;
    }

    fn id(&self, raw: usize) -> usize {
        let k = self.st.borrow_mut().ids.id(raw);
        assert_eq!(k, raw, "id order diverged");
        k
    }

    fn ids_of(&self, line: &str, keys: &[&str]) {
        for t in line.split(' ') {
            for k in keys {
                if let Some(v) = t.strip_prefix(k).and_then(|r| r.strip_prefix('=')) {
                    for x in v.split(',') {
                        self.id(x.parse().unwrap_or(0));
                    }
                }
            }
        }
    }

    fn popup(&self, k: usize) -> Rc<Popup> {
        self.st.borrow().popups.get(&k).cloned().expect("popup")
    }

    fn parse_ret(&self, line: &str) -> Ret {
        let mut r = Ret::default();
        for t in line.split(' ').skip(1) {
            if let Some(v) = t.strip_prefix("w=") {
                r.w = self.id(v.parse().unwrap_or(0));
            } else if let Some(v) = t.strip_prefix("i=") {
                r.i = v.parse().unwrap_or(0);
            } else if let Some(v) = t.strip_prefix("f=") {
                for (k, x) in v.split(',').enumerate() {
                    if let Some(d) = r.f.get_mut(k) {
                        *d = f32::from_bits(u32::from_str_radix(x, 16).unwrap_or(0));
                    }
                }
            } else if let Some(v) = t.strip_prefix("s=") {
                for (k, x) in v.split(';').enumerate() {
                    if x == "-" {
                        continue;
                    }
                    let b: Box<[u32]> = parse_hex(x).into_boxed_slice();
                    if let (Some(p), Some(n)) = (r.s.get_mut(k), r.n.get_mut(k)) {
                        *p = b.as_ptr();
                        *n = b.len();
                    }
                    self.st.borrow_mut().strs.push(b);
                }
            }
        }
        r
    }

    fn expect_result(&self, got: &str) {
        let line = self.line();
        assert_eq!(line, format!("= {got}"), "entry result");
        self.advance();
    }

    /// Runs the entry point at the cursor, with its nested host traffic.
    fn entry(&self) {
        let line = self.line();
        self.advance();
        let name = line.split(' ').nth(1).unwrap_or("").to_owned();
        self.ids_of(&line, &["target", "dest", "cb", "w", "p"]);
        let h = Hx(self);
        let cb = || {
            let v: Vec<usize> = kv(&line, "cb")
                .split(',')
                .map(|x| x.parse().unwrap_or(0))
                .collect();
            [
                v.first().copied().unwrap_or(0),
                v.get(1).copied().unwrap_or(0),
            ]
        };
        match name.as_str() {
            "msgbox_new" => {
                let p = pop::msgbox_new(
                    &h,
                    &parse_hex(kv(&line, "l")),
                    &parse_hex(kv(&line, "t")),
                    &parse_hex(kv(&line, "b1")),
                    &parse_hex(kv(&line, "b2")),
                    cb(),
                );
                self.new_result(p);
            }
            "color_new" => {
                let p = pop::color_new(&h, num(&line, "dest"), cb());
                self.new_result(p);
            }
            "fp_new" => {
                let kind = kv(&line, "kind").parse().unwrap_or(0);
                let p = pop::fp_new(
                    &h,
                    kind,
                    &parse_hex(kv(&line, "path")),
                    &parse_hex(kv(&line, "name")),
                    cb(),
                );
                self.new_result(p);
            }
            "kb_new" => {
                let p = pop::kb_new(&h, num(&line, "target"));
                self.new_result(p);
            }
            "event" => {
                let p = self.popup(num(&line, "p"));
                let d = Msg::with_val(kv(&line, "val").parse().unwrap_or(0));
                let r = p.event(
                    &h,
                    kv(&line, "tag").parse().unwrap_or(0),
                    num(&line, "w"),
                    &d,
                );
                self.expect_result(&format!("ret={}", u8::from(r)));
            }
            "destroy" => {
                let k = num(&line, "p");
                self.popup(k).destroy(&h);
                self.st.borrow_mut().popups.remove(&k);
                self.expect_result("ok");
            }
            "set_destroy_callback" => {
                let c = cb();
                self.popup(num(&line, "p")).set_destroy_callback(c[0], c[1]);
                self.expect_result("ok");
            }
            "set_next_focus" => {
                self.popup(num(&line, "p")).set_next_focus(num(&line, "w"));
                self.expect_result("ok");
            }
            "set_kill_on_escape" => {
                self.popup(num(&line, "p"))
                    .set_kill_on_escape(&h, num(&line, "x") != 0);
                self.expect_result("ok");
            }
            "kill_on_escape" => {
                let r = self.popup(num(&line, "p")).kill_on_escape(&h);
                self.expect_result(&format!("ret={}", u8::from(r)));
            }
            "fp_init" => {
                self.popup(num(&line, "p")).fp_init(&h);
                self.expect_result("ok");
            }
            "fp_set_save_dest" => {
                self.popup(num(&line, "p"))
                    .fp_set_save_dest(&h, &parse_hex(kv(&line, "name")));
                self.expect_result("ok");
            }
            "fp_on_add_filter" => {
                self.popup(num(&line, "p")).fp_on_add_filter(&h);
                self.expect_result("ok");
            }
            "fp_on_navigate" => {
                self.popup(num(&line, "p")).fp_on_navigate(&h);
                self.expect_result("ok");
            }
            other => panic!("unknown entry {other}"),
        }
    }

    fn new_result(&self, p: Popup) {
        let line = self.line();
        self.advance();
        let k: usize = line
            .strip_prefix("= p=")
            .and_then(|r| r.parse().ok())
            .expect("p id");
        self.id(k);
        self.st.borrow_mut().popups.insert(k, Rc::new(p));
    }
}

impl Host for Mock {
    fn exec(&self, c: &Cmd) -> Ret {
        let want = self.line();
        let got = fmt_cmd(c, &mut self.st.borrow_mut().ids);
        assert_eq!(got, want, "command {}", self.st.borrow().pos);
        self.advance();
        while self.line().starts_with("ev ") {
            self.entry();
        }
        let line = self.line();
        assert!(line.starts_with("< "), "expected a return, got {line}");
        self.advance();
        self.parse_ret(&line)
    }
}

fn replay(path: &Path) -> usize {
    let text = std::fs::read_to_string(path).expect("cassette");
    let mock = Mock {
        lines: text.lines().map(str::to_owned).collect(),
        st: RefCell::new(St {
            pos: 0,
            ids: Ids::default(),
            popups: HashMap::new(),
            strs: Vec::new(),
        }),
    };
    let mut entries = 0;
    while !mock.line().is_empty() {
        assert!(mock.line().starts_with("ev "), "stray line {}", mock.line());
        mock.entry();
        entries += 1;
    }
    entries
}

fn cassettes() -> Vec<std::path::PathBuf> {
    let dir = Path::new(env!("CARGO_MANIFEST_DIR")).join("tests/golden/H21");
    let mut v: Vec<_> = std::fs::read_dir(dir)
        .expect("golden dir")
        .filter_map(Result::ok)
        .map(|e| e.path())
        .filter(|p| {
            p.file_name()
                .and_then(|n| n.to_str())
                .is_some_and(|n| n.starts_with("cassette_"))
        })
        .collect();
    v.sort();
    v
}

#[test]
fn cassettes_replay() {
    let all = cassettes();
    assert!(all.len() >= 5, "missing cassettes");
    let entries: usize = all.iter().map(|p| replay(p)).sum();
    assert!(entries > 100, "only {entries} entry points replayed");
}

#[test]
fn abi_layout() {
    assert_eq!(std::mem::size_of::<Cmd>(), 136);
    assert_eq!(std::mem::size_of::<Ret>(), 80);
    assert_eq!(std::mem::align_of::<Cmd>(), 8);
}

#[test]
fn header_op_list_matches() {
    let h = Path::new(env!("CARGO_MANIFEST_DIR")).join("../HPL2/core/include/gui/RustGuiH21.h");
    let text = std::fs::read_to_string(h).expect("header");
    let body = text
        .split("enum OhplH21Op {")
        .nth(1)
        .and_then(|r| r.split('}').next())
        .expect("enum");
    let names: Vec<&str> = body
        .split(',')
        .filter_map(|n| n.trim().strip_prefix("kOhplH21"))
        .filter(|n| *n != "OpCount")
        .collect();
    assert_eq!(names, OP_NAMES);
}
