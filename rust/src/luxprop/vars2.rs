//! `Vars` plus `GetVarVector2f`, which the critter and hand-object loaders read.

use super::vars::Vars;
use crate::types::Vector2f;

pub trait Vars2: Vars {
    fn vec2(&self, name: &str, def: Vector2f) -> Vector2f;
}
