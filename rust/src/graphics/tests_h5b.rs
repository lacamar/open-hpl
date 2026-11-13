use std::path::PathBuf;

use super::h5b_ops;
use super::h5b_scn;
use super::h5b_trace::TraceGfx;

const SEEDS: &[u64] = &[1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12];
const ROUNDS: u32 = 3;

fn golden() -> PathBuf {
    PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("tests/golden/H5")
}

fn rust_run(seed: u64) -> (String, String) {
    let mut g = TraceGfx::default();
    let log = h5b_scn::run(&mut g, seed, ROUNDS);
    (g.text().to_string(), log)
}

#[test]
fn ops_match_header() {
    let h = include_str!("../../../HPL2/core/include/graphics/RustGraphicsH5b.h");
    let names: Vec<String> = h
        .lines()
        .filter_map(|l| l.trim().strip_prefix("kOhplH5b_"))
        .map(|l| l.trim_end_matches(',').to_string())
        .filter(|n| n != "Count")
        .collect();
    assert_eq!(names, h5b_ops::NAMES);
}

#[test]
#[ignore = "writes the Rust trace and log of every seed to $H5B_DUMP"]
fn dump() {
    let dir = PathBuf::from(std::env::var("H5B_DUMP").unwrap_or_default());
    for &s in SEEDS {
        let (t, l) = rust_run(s);
        std::fs::write(dir.join(format!("scn_{s:03}.rs.trace")), t).unwrap_or_default();
        std::fs::write(dir.join(format!("scn_{s:03}.rs.res")), l).unwrap_or_default();
    }
}

#[test]
fn trace_matches_cpp_recorder() {
    for &s in SEEDS {
        let (t, l) = rust_run(s);
        let dir = golden();
        let want_t =
            std::fs::read_to_string(dir.join(format!("scn_{s:03}.trace"))).unwrap_or_default();
        let want_l =
            std::fs::read_to_string(dir.join(format!("scn_{s:03}.res"))).unwrap_or_default();
        assert!(!want_t.is_empty(), "missing golden for seed {s}");
        assert_eq!(t, want_t, "trace seed {s}");
        assert_eq!(l, want_l, "log seed {s}");
    }
}

#[test]
fn gl_gfx_keeps_state_calls_off_the_inner_device() {
    use super::h5b_gl::GlGfx;
    use crate::backend::gl_ctx::ctx::Ctx;
    let mut ctx = Ctx::new();
    // SAFETY: `ctx` outlives the device and the test is single threaded.
    let mut g = unsafe { GlGfx::new(&mut ctx, TraceGfx::default()) };
    let log = h5b_scn::run(&mut g, 3, 1);
    assert!(log.contains("actions="));
    let t = g.inner().text();
    assert!(t.contains("CreateTexture("));
    for direct in [
        "SetBlendActive(",
        "SetMatrix(",
        "ClearFrameBuffer(",
        "DrawQuad(",
        "FlushTriBatch(",
    ] {
        assert!(!t.contains(direct), "{direct} reached the inner device");
    }
}
