//! `cProgramComboManager` settings: program names and shader variable lists per main mode.

use crate::math::get_flag_bit;

pub const VERTEX_BIT: u32 = 1;
pub const FRAGMENT_BIT: u32 = 2;

#[derive(Debug, Default)]
pub struct Mode {
    pub name: Vec<u8>,
    pub vtx_shader: Vec<u8>,
    pub frag_shader: Vec<u8>,
    /// (shader mask, variable) per feature bit.
    pub features: Vec<(u32, Vec<u8>)>,
    pub defaults: Vec<(Vec<u8>, Vec<u8>)>,
}

#[derive(Debug, Default)]
pub struct ComboSettings {
    pub name: Vec<u8>,
    pub modes: Vec<Mode>,
}

fn has_bit(flags: i32, i: usize) -> bool {
    let bit = i32::try_from(i).map(get_flag_bit).unwrap_or(0);
    flags & bit != 0
}

impl ComboSettings {
    pub fn new(name: &[u8], modes: usize) -> Self {
        ComboSettings {
            name: name.to_vec(),
            modes: (0..modes).map(|_| Mode::default()).collect(),
        }
    }

    pub fn mode(&self, mode: i32) -> Option<&Mode> {
        self.modes.get(usize::try_from(mode).ok()?)
    }

    pub fn mode_mut(&mut self, mode: i32) -> Option<&mut Mode> {
        self.modes.get_mut(usize::try_from(mode).ok()?)
    }

    pub fn program_name(&self, mode: i32, flags: i32) -> Vec<u8> {
        let Some(m) = self.mode(mode) else {
            return Vec::new();
        };
        let mut n = self.name.clone();
        n.push(b'_');
        n.extend_from_slice(&m.name);
        for (i, (_, var)) in m.features.iter().enumerate() {
            if has_bit(flags, i) {
                n.push(b'_');
                n.extend_from_slice(var);
            }
        }
        n
    }

    /// Variables `CreateShaderFromFeatures` adds, in order: enabled features of the shader type, then defaults.
    pub fn shader_vars(&self, mode: i32, flags: i32, shader_type: u32) -> Vec<(&[u8], &[u8])> {
        let Some(m) = self.mode(mode) else {
            return Vec::new();
        };
        let feats = m
            .features
            .iter()
            .enumerate()
            .filter(|(i, (mask, _))| has_bit(flags, *i) && mask & shader_type != 0)
            .map(|(_, (_, v))| (v.as_slice(), &[][..]));
        let defs = m.defaults.iter().map(|(k, v)| (k.as_slice(), v.as_slice()));
        feats.chain(defs).collect()
    }
}
