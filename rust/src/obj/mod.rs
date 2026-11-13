//! Object ownership after the cut-over. See `README.md` for the shell/opaque patterns.

mod arena;
mod foreign;
mod list;

pub use arena::{Arena, Handle};
pub use foreign::{DropFn, Foreign, IdFn, NameFn};
pub use list::{Link, List, Node};
