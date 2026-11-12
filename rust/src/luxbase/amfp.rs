//! `amfp/src/game` scaffold (`AmfpBase`, `Main`): command line, the three init config keys and the
//! test map constants. Not part of the shipped AMFP launcher, which runs `Amnesia.bin`.

use super::base::w;
use super::cfg::Cfg;
use crate::string;

pub struct Vars {
    pub resource_config: Vec<u8>,
    pub material_config: Vec<u8>,
    pub game_name: Vec<u8>,
}

/// `ParseCommandLine`.
pub fn init_config_file(cmd: &[u8]) -> Vec<u32> {
    let f = string::utf8_to_wide(cmd);
    if f.is_empty() {
        w("config/main_init.cfg")
    } else {
        f
    }
}

/// `InitMainConfig`: `Err` is the error message for a missing init file.
pub fn load_vars(loaded: bool, file: &[u32], c: &mut dyn Cfg) -> Result<Vars, Vec<u32>> {
    if !loaded {
        return Err([w("Could not load main init file: ").as_slice(), file].concat());
    }
    Ok(Vars {
        resource_config: c.string(b"ConfigFiles", b"Resources", b"resources.cfg"),
        material_config: c.string(b"ConfigFiles", b"Materials", b"materials.cfg"),
        game_name: c.string(b"Variables", b"GameName", b"Amnesia - A Machine For Pigs"),
    })
}

pub fn window_caption(game_name: &[u8]) -> Vec<u8> {
    [game_name, b" (Phase 0)"].concat()
}

pub fn banner(game_name: &[u8]) -> Vec<u8> {
    [
        b"AMFP game module - Phase 0 scaffolding (".as_slice(),
        game_name,
        b")\n",
    ]
    .concat()
}
