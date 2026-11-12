//! `cConfigFile` as the Rust side sees it: every read carries its level, key and default.

pub trait Cfg {
    fn bool(&mut self, level: &[u8], key: &[u8], def: bool) -> bool;
    fn int(&mut self, level: &[u8], key: &[u8], def: i32) -> i32;
    fn float(&mut self, level: &[u8], key: &[u8], def: f32) -> f32;
    fn string(&mut self, level: &[u8], key: &[u8], def: &[u8]) -> Vec<u8>;
    fn string_w(&mut self, level: &[u8], key: &[u8], def: &[u32]) -> Vec<u32>;
    fn set_bool(&mut self, level: &[u8], key: &[u8], v: bool);
    fn set_int(&mut self, level: &[u8], key: &[u8], v: i32);
    fn set_float(&mut self, level: &[u8], key: &[u8], v: f32);
    fn set_string(&mut self, level: &[u8], key: &[u8], v: &[u8]);
}
