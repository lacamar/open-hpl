//! H20 host table and header consistency.

use super::h20_host::OP_TABLE;

#[test]
fn host_ops_match_header() {
    let header = include_str!("../../../HPL2/core/include/gui/RustGuiH20.h");
    for (name, v) in OP_TABLE {
        let line = format!("kOhplH20Op_{name} = {v},");
        assert!(header.contains(&line), "{line}");
    }
    let n = header.matches("kOhplH20Op_").count();
    assert_eq!(n, OP_TABLE.len());
}
