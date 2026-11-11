//! `GetVar*` of the entity loader and of the map's instance variables; the value strings are
//! parsed on the C++ side. Strings leave through `put` into numbered slots.

use crate::types::{Color, Vector3f};

pub trait Vars {
    fn string(&self, name: &str, def: &[u8]) -> Vec<u8>;
    fn boolean(&self, name: &str, def: bool) -> bool;
    fn float(&self, name: &str, def: f32) -> f32;
    fn int(&self, name: &str, def: i32) -> i32;
    fn vec3(&self, name: &str, def: Vector3f) -> Vector3f;
    fn color(&self, name: &str, def: Color) -> Color;
    fn put(&self, slot: i32, s: &[u8]);
}

pub(crate) fn put_var(v: &dyn Vars, slot: i32, name: &str) {
    v.put(slot, &v.string(name, b""));
}

pub(crate) fn zero3() -> Vector3f {
    Vector3f {
        x: 0.0,
        y: 0.0,
        z: 0.0,
    }
}
