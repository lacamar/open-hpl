//! `cSetupVarContainer`: string map, first insert wins, typed getters over the stored text.

use crate::string::{float_to_string, int_to_string, to_bool, token_to_float, token_to_int};
use std::collections::BTreeMap;

#[derive(Default)]
pub struct SetupVars {
    vars: BTreeMap<Vec<u8>, Vec<u8>>,
}

impl SetupVars {
    pub fn add_string(&mut self, name: &[u8], value: &[u8]) {
        self.vars
            .entry(name.to_vec())
            .or_insert_with(|| value.to_vec());
    }

    pub fn add_int(&mut self, name: &[u8], v: i32) {
        self.add_string(name, &int_to_string(v, 0));
    }

    pub fn add_float(&mut self, name: &[u8], v: f32) {
        self.add_string(name, &float_to_string(v, 20, false));
    }

    pub fn add_bool(&mut self, name: &[u8], v: bool) {
        self.add_string(name, if v { b"true" } else { b"false" });
    }

    pub fn get_string(&self, name: &[u8]) -> &[u8] {
        self.vars.get(name).map_or(&[], Vec::as_slice)
    }

    pub fn get_float(&self, name: &[u8], default: f32) -> f32 {
        match self.get_string(name) {
            [] => default,
            s => token_to_float(s),
        }
    }

    pub fn get_int(&self, name: &[u8], default: i32) -> i32 {
        match self.get_string(name) {
            [] => default,
            s => token_to_int(s),
        }
    }

    pub fn get_bool(&self, name: &[u8], default: bool) -> bool {
        match self.get_string(name) {
            [] => default,
            s => to_bool(s),
        }
    }
}
