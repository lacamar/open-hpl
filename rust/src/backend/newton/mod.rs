//! Newton physics backend (L4), selected with `OHPL_IMPL=rust`.
//!
//! Rust owns the Newton worlds, collisions, bodies and joints and every Newton callback. The
//! `iPhysics*` objects stay C++ and are reached through [`host::Host`]; Newton user data is an
//! arena handle (never a C++ address). Callbacks copy a record out of the registry, drop the lock,
//! and only then call Newton or the host, so no registry borrow lives across a call that can re-enter.

mod body;
mod host;
mod joint;
mod material;
mod reg;
mod shape;
mod world;

#[cfg(test)]
mod tests;

use crate::types::Matrixf;

/// Row-major `cMatrixf` to Newton's transposed layout.
fn transposed(m: &Matrixf) -> [f32; 16] {
    let mut out = [0.0f32; 16];
    for (r, row) in m.m.iter().enumerate() {
        for (c, v) in row.iter().enumerate() {
            if let Some(slot) = out.get_mut(c * 4 + r) {
                *slot = *v;
            }
        }
    }
    out
}
