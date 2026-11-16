use super::*;
use std::cell::{Cell, RefCell};

#[derive(Default)]
struct Fake {
    log: RefCell<Vec<(u32, String)>>,
    next: Cell<u64>,
    xml: RefCell<Vec<u8>>,
}

impl Fake {
    fn count(&self, name: &str) -> usize {
        let o = op(name);
        self.log.borrow().iter().filter(|(x, _)| *x == o).count()
    }

    fn names(&self) -> Vec<&'static str> {
        self.log
            .borrow()
            .iter()
            .filter_map(|(o, _)| OPS.get(usize::try_from(*o - OP_BASE).ok()?).copied())
            .collect()
    }
}

impl Gpu for Fake {
    fn call(&self, o: u32, io: &mut Io) {
        let text = if io.s.is_null() {
            String::new()
        } else {
            String::from_utf8_lossy(&io.text()).into_owned()
        };
        let text = if o == op("TrailAlpha") {
            io.f[0].to_string()
        } else {
            text
        };
        self.log.borrow_mut().push((o, text));
        let id = self.next.get() + 100;
        self.next.set(id);
        match o {
            _ if o == op("NewShell")
                || o == op("ProgramFromShaders")
                || o == op("MatCreate")
                || o == op("EntNew") =>
            {
                io.r = id;
            }
            _ if o == op("CreateTarget") => {
                io.r = id;
                io.p[0] = id + 1;
            }
            _ if o == op("TexSize") => {
                io.i[..2].copy_from_slice(&[1280, 720]);
                io.f[..2].copy_from_slice(&[1280.0, 720.0]);
            }
            _ if o == op("ScreenSize") => {
                io.i[..2].copy_from_slice(&[1280, 720]);
                io.f[..2].copy_from_slice(&[1280.0, 720.0]);
            }
            _ if o == op("FrameTime") => io.f[0] = 1.0 / 60.0,
            _ if o == op("XmlTentacle") => {
                let x = self.xml.borrow();
                io.s = x.as_ptr();
                io.n = x.len();
                io.r = 1;
            }
            _ if o == op("EntAnimIndex") => io.i[0] = 2,
            _ => {}
        }
    }
}

#[test]
fn op_numbers_match_the_header() {
    let h = include_str!("../../../soma/src/game/RustSomaS8b.h");
    let body = h
        .split("enum eOhplS8cOp")
        .nth(1)
        .and_then(|b| b.split('{').nth(1))
        .and_then(|b| b.split('}').next())
        .unwrap_or("");
    let names: Vec<&str> = body
        .split(',')
        .filter_map(|n| n.trim().strip_prefix("kS8c_"))
        .map(|n| n.split_whitespace().next().unwrap_or(""))
        .collect();
    assert_eq!(names[..OPS.len()], OPS);
    assert_eq!(names[OPS.len()..], ["DumpOn", "Log", "LogEnd"]);
    assert!(body.contains(&format!("= {OP_BASE}")));
}

#[test]
fn trail_resets_then_decays() {
    let f = Fake::default();
    let g = Gx(&f);
    let s = create(&g, TRAIL);
    for _ in 0..2 {
        assert_eq!(render(&g, s, 7, 8), 7);
    }
    let alphas: Vec<String> = f
        .log
        .borrow()
        .iter()
        .filter(|(o, _)| *o == op("TrailAlpha"))
        .map(|(_, t)| t.clone())
        .collect();
    assert_eq!(
        alphas,
        [
            "1".to_string(),
            postfx::trail_alpha(0.3, 1.0 / 60.0, false).to_string()
        ]
    );
    assert_eq!(f.count("DrawQuad"), 0);
}

#[test]
fn blur_draws_what_the_plan_says() {
    let f = Fake::default();
    let g = Gx(&f);
    let s = create(&g, BLUR);
    set(&g, s, &[1.5]);
    let sizes = postfx::blur_levels([1280, 720]);
    let plan = postfx::blur_plan(1.5, 720, &sizes);
    assert!(plan.n_ops > 0);
    render(&g, s, 7, 8);
    assert_eq!(f.count("DrawQuad"), plan.n_ops);
    // one render target pair per level but the first, which only has the B pair
    assert_eq!(f.count("CreateTarget"), 2 * sizes.len() - 1);
    assert_eq!(f.count("ProgramFromShaders"), 4);
    render(&g, s, 7, 8);
    assert_eq!(f.count("CreateTarget"), 2 * sizes.len() - 1);
    assert_eq!(f.count("ProgramFromShaders"), 4);
    destroy(&g, s);
    assert_eq!(f.count("DestroyFb"), 2 * sizes.len() - 1);
    assert_eq!(f.count("DeleteShell"), 1);
}

#[test]
fn inactive_blur_passes_the_input_through() {
    let f = Fake::default();
    let g = Gx(&f);
    let s = create(&g, BLUR);
    assert_eq!(render(&g, s, 7, 8), 7);
    assert!(f.names().iter().all(|n| *n != "DrawQuad"));
}

#[test]
fn plain_effect_sets_program_before_uniforms() {
    let f = Fake::default();
    let g = Gx(&f);
    let s = create(&g, RADIAL);
    render(&g, s, 7, 8);
    let n = f.names();
    let at = |name: &str| n.iter().position(|x| *x == name);
    assert!(at("SetProgram") < at("UniF"));
    assert!(at("UniF") < at("DrawQuad"));
    assert!(at("SetFinalFb") < at("SetProgram"));
    assert_eq!(f.count("TexBilinear"), 0);
    assert_eq!(f.count("ProgramFromShaders"), 1);
}

#[test]
fn tentacle_setup_creates_groups_and_animations() {
    let f = Fake::default();
    *f.xml.borrow_mut() = b"1\tpf.msh\nloop\tloop.anm\t3f800000\n".to_vec();
    let g = Gx(&f);
    let s = create(&g, TENT);
    // SAFETY-free path: the natives are exercised through the setup helpers of the registration module
    natives_setup(&g, s, 6, 3);
    assert_eq!(f.count("EntNew"), 3);
    assert_eq!(f.count("EntAnim"), 3);
    assert_eq!(with_fx(s, |e| e.tent.tentacles.len()), Some(6));
    assert_eq!(with_fx(s, |e| e.tent.groups.len()), Some(3));
}

fn natives_setup(g: &Gx, s: Ptr, num: i32, groups: i32) {
    natives::setup_entity(g, s, b"pf_tentacle.ent", num, groups);
}
