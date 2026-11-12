//! Exported entry points, mirrored in `HPL2/core/include/impl/RustL6.h`.

use super::abi::{Builder, Cfg, Str};
use super::{assimp, collada, msh};

/// # Safety
/// `cfg` and `builder` are valid for the call; `path` is valid for its length.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_l6_msh_mesh(
    cfg: *const Cfg,
    builder: *const Builder,
    path: Str,
) -> i32 {
    // SAFETY: caller contract.
    let (cfg, builder, path) = unsafe { (&*cfg, &*builder, path.bytes()) };
    msh::load_mesh(cfg, super::abi::Sink(builder), path)
}

/// # Safety
/// As `ohpl_l6_msh_mesh`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_l6_msh_anim(
    _cfg: *const Cfg,
    builder: *const Builder,
    path: Str,
) -> i32 {
    // SAFETY: caller contract.
    let (builder, path) = unsafe { (&*builder, path.bytes()) };
    msh::load_anim(super::abi::Sink(builder), path)
}

/// # Safety
/// As `ohpl_l6_msh_mesh`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_l6_collada_mesh(
    cfg: *const Cfg,
    builder: *const Builder,
    path: Str,
) -> i32 {
    // SAFETY: caller contract.
    let (cfg, builder, path) = unsafe { (&*cfg, &*builder, path.bytes()) };
    collada::load_mesh(cfg, super::abi::Sink(builder), path)
}

/// # Safety
/// As `ohpl_l6_msh_mesh`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_l6_collada_anim(
    cfg: *const Cfg,
    builder: *const Builder,
    path: Str,
) -> i32 {
    // SAFETY: caller contract.
    let (cfg, builder, path) = unsafe { (&*cfg, &*builder, path.bytes()) };
    collada::load_anim(cfg, super::abi::Sink(builder), path)
}

/// # Safety
/// As `ohpl_l6_msh_mesh`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_l6_assimp_mesh(
    cfg: *const Cfg,
    builder: *const Builder,
    path: Str,
) -> i32 {
    // SAFETY: caller contract.
    let (cfg, builder, path) = unsafe { (&*cfg, &*builder, path.bytes()) };
    assimp::load_mesh(cfg, super::abi::Sink(builder), path)
}

/// # Safety
/// As `ohpl_l6_msh_mesh`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_l6_assimp_anim(
    cfg: *const Cfg,
    builder: *const Builder,
    path: Str,
) -> i32 {
    // SAFETY: caller contract.
    let (cfg, builder, path) = unsafe { (&*cfg, &*builder, path.bytes()) };
    assimp::load_anim(cfg, super::abi::Sink(builder), path)
}
