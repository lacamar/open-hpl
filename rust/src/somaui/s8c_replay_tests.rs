//! Replays the dump of the pre-port C++ (`rust/tests/golden/S8c`, one line per rendered effect: the
//! engine-call stream) through the Rust effects and a host that prints the same vocabulary.

use super::*;
use std::cell::{Cell, RefCell};
use std::collections::HashMap;

const GOLDEN: [&str; 3] = [
    include_str!("../../tests/golden/S8c/s8c-soma-cpp.txt"),
    include_str!("../../tests/golden/S8c/s8c-rebirth-cpp.txt"),
    include_str!("../../tests/golden/S8c/s8c-bunker-cpp.txt"),
];

#[derive(Default)]
struct Replay {
    line: RefCell<Vec<String>>,
    done: RefCell<Vec<String>>,
    texs: RefCell<HashMap<u64, (String, i32, i32)>>,
    progs: RefCell<HashMap<u64, String>>,
    next: Cell<u64>,
    dt: Cell<f32>,
    mask: Cell<bool>,
}

fn hexes(v: &[f32]) -> String {
    v.iter()
        .map(|f| format!("{:08x}", f.to_bits()))
        .collect::<Vec<_>>()
        .join(",")
}

impl Replay {
    fn id(&self) -> u64 {
        self.next.set(self.next.get() + 1);
        self.next.get() + 10
    }

    fn add(&self, s: String) {
        self.line.borrow_mut().push(s);
    }

    fn tex(&self, p: u64) -> (String, i32, i32) {
        self.texs
            .borrow()
            .get(&p)
            .cloned()
            .unwrap_or(("-".into(), 0, 0))
    }

    fn uniform(&self, name: &str, values: String, plain: bool) {
        let masked = self.mask.get() && name != "avScreenSize";
        self.add(format!(
            "{name}={}",
            if masked {
                "rnd".to_string()
            } else if plain {
                values
            } else {
                values + ","
            }
        ));
    }
}

impl Gpu for Replay {
    fn call(&self, o: u32, io: &mut Io) {
        let text = if io.s.is_null() {
            String::new()
        } else {
            String::from_utf8_lossy(&io.text()).into_owned()
        };
        if o == OP_LOG {
            self.mask.set(io.i[0] != 0);
            self.add(text);
            return;
        }
        if o == OP_LOG_END {
            self.mask.set(false);
            let l = self
                .line
                .borrow_mut()
                .drain(..)
                .collect::<Vec<_>>()
                .join(" | ");
            self.done.borrow_mut().push(l);
            return;
        }
        if o == OP_DUMP_ON {
            io.r = 1;
            return;
        }
        let Some(name) = OPS.get(usize::try_from(o - OP_BASE).unwrap_or(usize::MAX)) else {
            return;
        };
        let f = &io.f;
        match *name {
            "NewShell" => io.r = self.id(),
            "FrameTime" => io.f[0] = self.dt.get(),
            "ProgramFromShaders" => {
                let id = self.id();
                self.progs
                    .borrow_mut()
                    .insert(id, text.lines().next().unwrap_or("").to_string());
                io.r = id;
            }
            "UniF" => self.uniform(&text, hexes(&f[..1]), true),
            "UniV2" => self.uniform(&text, hexes(&f[..2]), true),
            "UniV3" => self.uniform(&text, hexes(&f[..3]), true),
            "UniV4" => self.uniform(&text, hexes(&f[..4]), true),
            "UniArr" => self.uniform(
                &text,
                hexes(&f[..usize::try_from(io.i[0]).unwrap_or(0)]),
                false,
            ),
            "UniMat" => self.uniform(&text, hexes(&f[..16]), false),
            "SetFlat" => self.add("flat".into()),
            "SetBlend" => self.add(format!("blend={}", if io.i[0] != 0 { 3 } else { 0 })),
            "SetChannel" => self.add("chan=0".into()),
            "SetRange" => self.add(format!("range={}", io.i[0])),
            "SetTexture" => {
                let (n, w, h) = self.tex(io.p[0]);
                self.add(format!("tex{}={n}:{w}x{h}", io.i[0]));
            }
            "SetProgram" => {
                let n = self
                    .progs
                    .borrow()
                    .get(&io.p[0])
                    .cloned()
                    .unwrap_or("-".into());
                self.add(format!("prog={n}"));
            }
            "SetFinalFb" => self.add("final".into()),
            "SetFb" => {
                let (_, w, h) = self.tex(io.p[1]);
                self.add(format!("fb={w}x{h}"));
            }
            "DrawQuad" => self.add("quad".into()),
            "FbResult" => io.r = 3,
            "TexSize" => {
                let (_, w, h) = self.tex(io.p[0]);
                io.i[..2].copy_from_slice(&[w, h]);
                io.f[..2].copy_from_slice(&[w as f32, h as f32]);
            }
            "ScreenSize" => {
                io.i[..2].copy_from_slice(&[1280, 720]);
                io.f[..2].copy_from_slice(&[1280.0, 720.0]);
            }
            "CreateTarget" => {
                let (tex, fb) = (self.id(), self.id());
                self.texs.borrow_mut().insert(tex, (text, io.i[0], io.i[1]));
                io.r = tex;
                io.p[0] = fb;
            }
            _ => {}
        }
    }
}

fn field<'a>(line: &'a str, key: &str) -> Option<&'a str> {
    line.split(' ').find_map(|t| t.strip_prefix(key))
}

#[test]
fn rust_effects_reproduce_the_cpp_engine_call_stream() {
    let mut replayed = 0;
    for golden in GOLDEN {
        for line in golden.lines() {
            let name = field(line, "fx=").unwrap_or("");
            let Some(ty) = TYPES.iter().position(|t| t.0 == name) else {
                continue;
            };
            if ty == FADE || ty == TENT {
                continue;
            }
            forget_programs();
            let r = Replay::default();
            let g = Gx(&r);
            let shell = create(&g, ty);
            let params: Vec<f32> = field(line, "p=")
                .unwrap_or("")
                .split(',')
                .filter(|p| !p.is_empty())
                .map(|p| f32::from_bits(u32::from_str_radix(p, 16).unwrap_or(0)))
                .collect();
            let dt = field(line, "dt=")
                .and_then(|d| u32::from_str_radix(d, 16).ok())
                .unwrap_or(0);
            r.dt.set(f32::from_bits(dt));
            let _ = with_fx(shell, |e| {
                e.clear = field(line, "clear=") == Some("1");
                if ty != DISTORT {
                    for (d, s) in e.params.iter_mut().zip(&params) {
                        *d = *s;
                    }
                }
            });
            let input = line
                .split(" | ")
                .find_map(|t| t.strip_prefix("tex0="))
                .and_then(|t| {
                    let (n, dims) = t.rsplit_once(':')?;
                    let (w, h) = dims.split_once('x')?;
                    Some((n.to_string(), w.parse().ok()?, h.parse().ok()?))
                })
                .unwrap_or(("TempBufferTexture1280x720:5".into(), 1280, 720));
            r.texs.borrow_mut().insert(1, input);
            render(&g, shell, 1, 2);
            let got = r.done.borrow().last().cloned().unwrap_or_default();
            assert_eq!(got, line);
            replayed += 1;
        }
    }
    assert!(replayed > 20, "{replayed}");
}
