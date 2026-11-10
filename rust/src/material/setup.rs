//! What a material type's `LoadData` tells its `cProgramComboManager`s.

use std::ffi::CStr;

/// `cProgramComboFeature`
#[derive(Clone, Copy, Debug)]
pub struct Feature {
    pub name: &'static CStr,
    pub stage: u32,
    pub required: u32,
}

pub(crate) const fn feature(name: &'static CStr, stage: u32, required: u32) -> Feature {
    Feature {
        name,
        stage,
        required,
    }
}

/// The `cParserVarContainer` / `cProgramComboManager` calls of a `LoadData`, in order.
/// `manager` is a `types::manager` code.
pub trait SetupSink {
    fn clear_defines(&mut self);
    fn define(&mut self, name: &str);
    fn define_value(&mut self, name: &str, value: f32);
    fn setup_program(
        &mut self,
        manager: i32,
        mode: i32,
        name: &str,
        vtx: &str,
        frag: &str,
        features: &[Feature],
    );
    fn var_id(&mut self, manager: i32, name: &str, id: i32, mode: i32);
}
