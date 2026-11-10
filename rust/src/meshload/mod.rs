//! Mesh loaders: parse model files into plain data that the C++ side turns into engine objects.

mod msh;
mod msh_ffi;
#[cfg(test)]
mod msh_tests;

pub use msh::{
    Anim, Array, Bone, BonePair, Collider, Key, Mesh, MshError, Node, Reason, SubMesh, Track,
    parse_animation, parse_mesh,
};
