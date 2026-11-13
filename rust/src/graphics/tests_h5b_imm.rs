use std::fmt::Write as _;
use std::path::PathBuf;

use super::h5b_gfx::{Draw, Imm};
use super::h5b_imm::Batch;
use super::h5b_scn;
use super::h5b_trace::{fnv, put};

const SEEDS: &[u64] = &[1, 2, 3, 4, 5, 6];
const CALLS: u32 = 100;

fn bytes(v: &[f32]) -> Vec<u8> {
    v.iter().flat_map(|x| x.to_ne_bytes()).collect()
}

/// Records the immediate-mode primitive stream and the batch flushes.
#[derive(Default)]
struct PrimRec {
    out: String,
    batch: Batch,
}

impl PrimRec {
    fn line(&mut self, s: &str) {
        self.out.push_str(s);
        self.out.push('\n');
    }

    fn flush(&mut self, kind: &str, flags: u32, auto: bool) {
        let b = &self.batch;
        let idx: Vec<u8> = b.indices.iter().flat_map(|x| x.to_ne_bytes()).collect();
        let mut l = format!(
            "flush {kind} {flags} {} v={}:{:016x} i={}:{:016x}",
            u8::from(auto),
            b.vertices().len(),
            fnv(&bytes(b.vertices())),
            b.indices.len(),
            fnv(&idx)
        );
        for (u, t) in b
            .texcoords
            .iter()
            .enumerate()
            .filter(|(_, t)| !t.is_empty())
        {
            let _ = write!(l, " tc{u}={}:{:016x}", t.len(), fnv(&bytes(t)));
        }
        self.line(&l);
        if auto {
            self.batch.clear_all();
        }
    }
}

impl Imm for PrimRec {
    fn im_begin(&mut self, m: u32) {
        self.line(&format!("begin {m}"));
    }
    fn im_end(&mut self) {
        self.line("end");
    }
    fn im_vertex3(&mut self, x: f32, y: f32, z: f32) {
        self.line(&format!("v3 {} {} {}", put::f(x), put::f(y), put::f(z)));
    }
    fn im_color4(&mut self, r: f32, g: f32, b: f32, a: f32) {
        self.line(&format!(
            "c4 {} {} {} {}",
            put::f(r),
            put::f(g),
            put::f(b),
            put::f(a)
        ));
    }
    fn im_texcoord2(&mut self, s: f32, t: f32) {
        self.line(&format!("t2 {} {}", put::f(s), put::f(t)));
    }
    fn im_texcoord3(&mut self, s: f32, t: f32, r: f32) {
        self.line(&format!("t3 {} {} {}", put::f(s), put::f(t), put::f(r)));
    }
    fn im_multitexcoord2(&mut self, u: u32, s: f32, t: f32) {
        self.line(&format!("mt2 {u} {} {}", put::f(s), put::f(t)));
    }
    fn im_multitexcoord3(&mut self, u: u32, s: f32, t: f32, r: f32) {
        self.line(&format!(
            "mt3 {u} {} {} {}",
            put::f(s),
            put::f(t),
            put::f(r)
        ));
    }
    fn im_clear_texture0(&mut self) {
        self.line("settex0");
    }
    fn im_blend_off(&mut self) {
        self.line("blend0");
    }
}

impl Draw for PrimRec {
    fn batch(&mut self) -> &mut Batch {
        &mut self.batch
    }
    fn set_batch_texture_unit_active(&mut self, unit: u32, active: bool) {
        self.line(&format!("unit {unit} {}", u8::from(active)));
    }
    fn flush_tri_batch(&mut self, flags: u32, auto: bool) {
        self.flush("tri", flags, auto);
    }
    fn flush_quad_batch(&mut self, flags: u32, auto: bool) {
        self.flush("quad", flags, auto);
    }
}

fn run(seed: u64) -> String {
    let mut g = PrimRec::default();
    h5b_scn::run_draws(&mut g, seed, CALLS);
    g.out
}

fn golden() -> PathBuf {
    PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("tests/golden/H5")
}

#[test]
#[ignore = "writes the Rust primitive streams to $H5B_DUMP"]
fn dump_imm() {
    let dir = PathBuf::from(std::env::var("H5B_DUMP").unwrap_or_default());
    for &s in SEEDS {
        std::fs::write(dir.join(format!("imm_{s:03}.rs.txt")), run(s)).unwrap_or_default();
    }
}

#[test]
fn imm_matches_cpp_inl() {
    for &s in SEEDS {
        let want =
            std::fs::read_to_string(golden().join(format!("imm_{s:03}.txt"))).unwrap_or_default();
        assert!(!want.is_empty(), "missing golden for seed {s}");
        assert_eq!(run(s), want, "imm seed {s}");
    }
}
