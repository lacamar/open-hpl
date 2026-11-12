//! L2 differential: the ffi call log the C++ shell made (`ffi_*.txt`) replayed through the Rust
//! context against a recording GL must reproduce the GL and SDL call trace of the original C++
//! (`trace_*.txt`). Both were recorded by `rust/tests/golden/L2/harness`.

pub(crate) mod fake;
pub(crate) mod fakegl;
mod replay;
mod replay_gen;

use super::caps::Caps;
use super::ctx::Ctx;
use super::oq::Query;
use replay::Replay;

/// The Rust path reads the extension string once more in `ohpl_glctx_load`; the C++ got it from GLEW.
fn calls(trace: &str) -> Vec<&str> {
    trace
        .lines()
        .filter(|l| *l != "glGetString(7939)")
        .collect()
}

fn check(set: &str, ffi: &str, golden: &str) {
    fake::reset(set);
    let mut r = Replay::new();
    r.run(ffi);
    let rust = fake::take_trace();
    let (got, want) = (calls(&rust), calls(golden));
    let at = got.iter().zip(&want).position(|(a, b)| a != b);
    if let Some(i) = at {
        panic!("line {i}: rust {:?} cpp {:?}", got.get(i), want.get(i));
    }
    assert_eq!(got.len(), want.len());
    assert!(got.len() > 700);
}

#[test]
fn replay_matches_cpp_trace_full_extensions() {
    check(
        "A",
        include_str!("../../../tests/golden/L2/ffi_A.txt"),
        include_str!("../../../tests/golden/L2/trace_A.txt"),
    );
}

#[test]
fn replay_matches_cpp_trace_limited_extensions() {
    check(
        "B",
        include_str!("../../../tests/golden/L2/ffi_B.txt"),
        include_str!("../../../tests/golden/L2/trace_B.txt"),
    );
}

fn caps_of(set: &str) -> Ctx {
    fake::reset(set);
    let mut c = Ctx::with_table(Box::leak(Box::new(fakegl::table())));
    // SAFETY: the fake resolves names and returns null or a function pointer.
    c.derive_caps(|n| {
        let name = std::ffi::CString::new(n).unwrap_or_default();
        !unsafe { fake::SDL_GL_GetProcAddress(name.as_ptr()) }.is_null()
    });
    c
}

fn check_caps(set: &str, golden: &str) {
    let c = caps_of(set);
    let line = golden.lines().find_map(|l| l.strip_prefix("CAPS "));
    assert_eq!(line, Some(c.caps.line().as_str()));
    let values: Vec<i32> = golden
        .lines()
        .filter_map(|l| l.strip_prefix("cap")?.split_once('=')?.1.parse().ok())
        .collect();
    assert_eq!(values.len(), 22);
    for (kind, want) in (0..).zip(values) {
        assert_eq!(c.get_caps(kind, false), want, "cap{kind}");
    }
}

#[test]
fn caps_match_glew_flags() {
    check_caps("A", include_str!("../../../tests/golden/L2/caps_A.txt"));
    check_caps("B", include_str!("../../../tests/golden/L2/caps_B.txt"));
}

#[test]
fn caps_need_listed_extension_and_every_entry_point() {
    let ext = "GL_EXT_framebuffer_object GL_ARB_multitexture";
    let all = Caps::from_extensions(ext, &|_| true);
    assert!(all.framebuffer_object && all.multitexture && !all.s3tc);
    let broken = Caps::from_extensions(ext, &|n| n != "glGenerateMipmapEXT");
    assert!(!broken.framebuffer_object && broken.multitexture);
    assert_eq!(Caps::from_extensions("", &|_| true), Caps::default());
}

#[test]
fn shader_model_flags_honour_the_force_off() {
    let c = caps_of("B");
    assert_eq!(c.get_caps(18, false), 1);
    assert_eq!(c.get_caps(18, true), 0);
}

#[test]
fn occlusion_query_follows_the_cpp_sequence() {
    fake::reset("A");
    let gl = Box::leak(Box::new(fakegl::table()));
    let mut q = Query::create(gl);
    assert_eq!((q.id, q.fetched), (1, true));
    q.begin(gl);
    assert!(!q.fetched);
    q.end(gl);
    assert!(q.fetch(gl));
    assert_eq!((q.last_samples, q.fetched), (77, true));
    assert!(q.fetch(gl));
    q.destroy(gl);
    let t = fake::take_trace();
    assert_eq!(
        t.lines().collect::<Vec<_>>(),
        [
            "glGenQueriesARB(1,p)",
            "glBeginQueryARB(35092,1)",
            "glEndQueryARB(35092)",
            "glIsQuery(1)",
            "glGetQueryObjectivARB(1,34919,p)",
            "glGetQueryObjectivARB(1,34918,p)",
            "glDeleteQueriesARB(1,p)",
        ]
    );
}
