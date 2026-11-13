//! The generated registrations (`s1_gen.rs`) against the compiler-verified C++ table
//! (`tests/golden/S1/bindings.txt`).

use super::s1_gen;
use super::s2_as::Engine;

#[test]
fn generated_registrations_match_cpp_table() {
    s1_gen::seen::CALLS.with(|c| c.borrow_mut().clear());
    s1_gen::register(Engine(std::ptr::null_mut()));
    let text = std::fs::read_to_string(concat!(
        env!("CARGO_MANIFEST_DIR"),
        "/tests/golden/S1/bindings.txt"
    ))
    .unwrap_or_default();
    let want: Vec<(&str, &str)> = text.lines().filter_map(|l| l.split_once('\t')).collect();
    let got = s1_gen::seen::CALLS.with(|c| c.borrow().clone());
    assert!(want.len() > 2000);
    assert_eq!(got.len(), want.len());
    for (i, ((ty, decl, op), (wty, wdecl))) in got.iter().zip(&want).enumerate() {
        assert_eq!((ty.as_str(), decl.as_str(), *op), (*wty, *wdecl, i));
    }
}
