//! `.ent` object loader parse (`cEntityLoader_Object::Load`), driving an engine-side [`Builder`].

mod builder;
mod ffi;
mod parse;

pub use builder::{
    AnimEventType, BodyProps, Builder, Flags, Handle, JointKind, JointProps, MeshInfo, Params,
    WorldEntity,
};
pub use parse::parse;

#[cfg(test)]
mod golden_tests;
#[cfg(test)]
mod tests;
