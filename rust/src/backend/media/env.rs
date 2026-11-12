//! `cOpenALSoundEnvironment`: the reverb parameter set, its XML file and the fade blend.

use crate::string::{token_to_float, token_to_int};
use crate::xml::{DOC, Doc, NodeId};

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EnvParams {
    pub density: f32,
    pub diffusion: f32,
    pub gain: f32,
    pub gain_hf: f32,
    pub gain_lf: f32,
    pub decay_time: f32,
    pub decay_hf_ratio: f32,
    pub decay_lf_ratio: f32,
    pub reflections_gain: f32,
    pub reflections_delay: f32,
    pub reflections_pan: [f32; 3],
    pub late_reverb_gain: f32,
    pub late_reverb_delay: f32,
    pub late_reverb_pan: [f32; 3],
    pub echo_time: f32,
    pub echo_depth: f32,
    pub modulation_time: f32,
    pub modulation_depth: f32,
    pub air_absorption_gain_hf: f32,
    pub hf_reference: f32,
    pub lf_reference: f32,
    pub room_rolloff_factor: f32,
    pub decay_hf_limit: i32,
}

pub struct Loaded {
    pub name: Vec<u8>,
    pub params: EnvParams,
}

fn float(doc: &Doc, node: NodeId, name: &[u8]) -> f32 {
    doc.attribute(node, name).map_or(0.0, token_to_float)
}

/// `cString::ToVector3f(attr, 0)`: three numbers or zeros.
fn vec3(doc: &Doc, node: NodeId, name: &[u8]) -> [f32; 3] {
    let mut v = [0.0; 3];
    if let Some(s) = doc.attribute(node, name)
        && !crate::string::floats_exact(s, &mut v)
    {
        v = [0.0; 3];
    }
    v
}

/// `CreateFromFile`: the file bytes in, `None` for what the original rejected. A document
/// without the `SoundEnvironment` root dereferenced null there; it is rejected here.
pub fn parse(data: &[u8]) -> Option<Loaded> {
    let mut doc = Doc::new();
    if !doc.load(data, true) {
        return None;
    }
    let root = doc.first_child_element(DOC, Some(b"SoundEnvironment"))?;
    let main = doc.first_child_element(root, Some(b"Main"));
    let (ty, name) = main.map_or((Vec::new(), Vec::new()), |m| {
        (
            doc.attribute(m, b"Type").unwrap_or_default().to_vec(),
            doc.attribute(m, b"Name").unwrap_or_default().to_vec(),
        )
    });
    let p = doc.first_child_element(root, Some(b"Parameters"))?;
    if ty != b"OpenAL" {
        return None;
    }
    let f = |n: &[u8]| float(&doc, p, n);
    let params = EnvParams {
        density: f(b"Density"),
        diffusion: f(b"Diffusion"),
        gain: f(b"Gain"),
        gain_hf: f(b"GainHF"),
        gain_lf: f(b"GainLF"),
        decay_time: f(b"DecayTime"),
        decay_hf_ratio: f(b"DecayHFRatio"),
        decay_lf_ratio: f(b"DecayLFRatio"),
        reflections_gain: f(b"ReflectionsGain"),
        reflections_delay: f(b"ReflectionsDelay"),
        reflections_pan: vec3(&doc, p, b"ReflectionsPan"),
        late_reverb_gain: f(b"LateReverbGain"),
        late_reverb_delay: f(b"LateReverbDelay"),
        late_reverb_pan: vec3(&doc, p, b"LateReverbPan"),
        echo_time: f(b"EchoTime"),
        echo_depth: f(b"EchoDepth"),
        modulation_time: f(b"ModulationTime"),
        modulation_depth: f(b"ModulationDepth"),
        air_absorption_gain_hf: f(b"AirAbsorptionGainHF"),
        hf_reference: f(b"HFReference"),
        lf_reference: f(b"LFReference"),
        room_rolloff_factor: f(b"RoomRolloffFactor"),
        decay_hf_limit: doc.attribute(p, b"DecayHFLimit").map_or(0, token_to_int),
    };
    Some(Loaded { name, params })
}

/// Only the half of the blend the original computed: `FadeSoundEnvironment` never set
/// `DecayHFRatio` or the pans, so those stay zero here (stack garbage there).
pub fn blend(src: Option<&EnvParams>, dst: Option<&EnvParams>, t: f32) -> Option<EnvParams> {
    let one_minus = 1.0 - t;
    let mut e = EnvParams::default();
    macro_rules! each {
        ($(($f:ident, $mix:expr)),* $(,)?) => {
            $( e.$f = $mix(src.map(|s| s.$f), dst.map(|d| d.$f)); )*
        };
    }
    let mix = |s: Option<f32>, d: Option<f32>| match (s, d) {
        (None, Some(d)) => d * t,
        (Some(s), None) => s * one_minus,
        (Some(s), Some(d)) => s.mul_add(one_minus, d * t),
        (None, None) => 0.0,
    };
    if src.is_none() && dst.is_none() {
        return None;
    }
    each!(
        (density, mix),
        (diffusion, mix),
        (gain, mix),
        (gain_hf, mix),
        (gain_lf, mix),
        (decay_time, mix),
        (decay_lf_ratio, mix),
        (reflections_gain, mix),
        (reflections_delay, mix),
        (late_reverb_gain, mix),
        (late_reverb_delay, mix),
        (echo_time, mix),
        (echo_depth, mix),
        (modulation_time, mix),
        (modulation_depth, mix),
        (air_absorption_gain_hf, mix),
        (hf_reference, mix),
        (lf_reference, mix),
        (room_rolloff_factor, mix),
    );
    e.decay_hf_limit = dst.or(src).map_or(0, |x| x.decay_hf_limit);
    Some(e)
}

const _: () = assert!(size_of::<EnvParams>() == 108);
