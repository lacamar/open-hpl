//! L3 differential: the ffi call log the C++ shells made (`ffi_*.txt`) replayed through the Rust
//! resources against a recording GL must reproduce the GL call trace of the original C++
//! (`trace_*.txt`). Both were recorded by `rust/tests/golden/L2/harness` (`record_l3.py`).

mod replay;
mod replay_gen;

use std::sync::Mutex;

use replay::Replay;

/// The program-binding state is process-global.
static LOCK: Mutex<()> = Mutex::new(());

fn check(set: &str, ffi: &str, golden: &str) -> usize {
    let _g = LOCK
        .lock()
        .unwrap_or_else(std::sync::PoisonError::into_inner);
    crate::backend::gl_ctx::tests::fake::reset(set);
    let mut r = Replay::new();
    let calls = r.run(ffi);
    let rust = crate::backend::gl_ctx::tests::fake::take_trace();
    let (got, want): (Vec<&str>, Vec<&str>) = (rust.lines().collect(), golden.lines().collect());
    if let Some(i) = got.iter().zip(&want).position(|(a, b)| a != b) {
        panic!(
            "line {i}: rust {:?} cpp {:?}\ncontext {:?}",
            got.get(i),
            want.get(i),
            &want[i.saturating_sub(4)..i]
        );
    }
    assert_eq!(got.len(), want.len());
    calls
}

macro_rules! golden {
    ($($name:ident: $set:literal $file:literal;)*) => {$(
        #[test]
        fn $name() {
            let calls = check(
                $set,
                include_str!(concat!(env!("CARGO_MANIFEST_DIR"), "/tests/golden/L3/ffi_", $file, ".txt")),
                include_str!(concat!(env!("CARGO_MANIFEST_DIR"), "/tests/golden/L3/trace_", $file, ".txt")),
            );
            assert!(calls > 20, "{calls}");
        }
    )*};
}

include!("tests/golden_list.rs");

#[test]
fn stale_and_zero_handles_do_nothing() {
    let _g = LOCK
        .lock()
        .unwrap_or_else(std::sync::PoisonError::into_inner);
    crate::backend::gl_ctx::tests::fake::reset("A");
    let _r = Replay::new();
    crate::backend::gl_ctx::tests::fake::take_trace();
    super::ffi::ohpl_gltex_free(0);
    super::ffi::ohpl_gltex_generate_handles(0, 3);
    let h = super::ffi::ohpl_gltex_new();
    super::ffi::ohpl_gltex_free(h);
    super::ffi::ohpl_gltex_generate_handles(h, 3);
    assert_eq!(super::ffi::ohpl_gltex_handle_count(h), 0);
    assert_eq!(super::ffi::ohpl_gltex_handle(h, 0.0), 0);
    assert!(crate::backend::gl_ctx::tests::fake::take_trace().is_empty());
}
