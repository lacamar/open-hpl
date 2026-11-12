//! Quality preset tables, preset comparison and application, and the DetectSettings decision.

use crate::luxbase::cfg::Cfg;

const GRAPHICS: &[u8] = b"Graphics";
pub const SSAO_SAMPLES_DEFAULT: i32 = 8;

const INTS: [(&[u8], i32); 6] = [
    (b"ShadowQuality", 1),
    (b"ShadowResolution", 1),
    (b"TextureQuality", 1),
    (b"TextureFilter", 1),
    (b"SSAOResolution", 1),
    (b"SSAOSamples", SSAO_SAMPLES_DEFAULT),
];
const FLOATS: [(&[u8], f32); 1] = [(b"TextureAnisotropy", 1.0)];
const BOOLS: [(&[u8], bool); 4] = [
    (b"ParallaxEnabled", true),
    (b"WorldReflection", true),
    (b"SSAOActive", true),
    (b"EdgeSmooth", true),
];

/// `cLauncherHelper::EqualsPreset`: missing entries compare as -1/-1.0/false in the file against
/// 0/0.0/true in the preset.
pub fn equals_preset(file: &mut dyn Cfg, preset: &mut dyn Cfg) -> bool {
    INTS.iter()
        .all(|&(n, _)| file.int(GRAPHICS, n, -1) == preset.int(GRAPHICS, n, 0))
        && FLOATS
            .iter()
            .all(|&(n, _)| file.float(GRAPHICS, n, -1.0) == preset.float(GRAPHICS, n, 0.0))
        && BOOLS
            .iter()
            .all(|&(n, _)| file.bool(GRAPHICS, n, false) == preset.bool(GRAPHICS, n, true))
}

/// `GetEqualPresetIndex`; a `None` preset is a NULL file and never equals.
pub fn equal_index(file: Option<&mut dyn Cfg>, presets: &mut [Option<&mut dyn Cfg>]) -> i32 {
    let Some(file) = file else {
        return -1;
    };
    for (i, p) in presets.iter_mut().enumerate() {
        if let Some(p) = p.as_deref_mut()
            && equals_preset(file, p)
        {
            return i as i32;
        }
    }
    -1
}

/// `UpdateConfigFileWithPreset`.
pub fn apply(file: &mut dyn Cfg, preset: &mut dyn Cfg) {
    for &(n, d) in &INTS {
        let v = preset.int(GRAPHICS, n, d);
        file.set_int(GRAPHICS, n, v);
    }
    for &(n, d) in &FLOATS {
        let v = preset.float(GRAPHICS, n, d);
        file.set_float(GRAPHICS, n, v);
    }
    for &(n, d) in &BOOLS {
        let v = preset.bool(GRAPHICS, n, d);
        file.set_bool(GRAPHICS, n, v);
    }
}

#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct Detect {
    pub show_closest: bool,
    pub show_unknown: bool,
    pub show_unsupported: bool,
    /// Index into the preset list (low, medium, high) to apply.
    pub preset: Option<usize>,
}

/// `DetectSettings` before any widget work. A rating outside -1..=3 indexed past the preset
/// vector in C++; here it applies no preset.
pub fn detect(rating: i32, closest_nonempty: bool) -> Detect {
    let unknown = rating == super::quality::UNKNOWN;
    let rating = if unknown { 1 } else { rating };
    let unsupported = rating == super::quality::UNSUPPORTED;
    Detect {
        show_closest: closest_nonempty,
        show_unknown: unknown,
        show_unsupported: unsupported,
        preset: (!unsupported && (0..=2).contains(&rating)).then_some(rating as usize),
    }
}

/// Selected row: the first menu entry whose value equals `wanted`, else 0.
pub fn menu_index(values: &[i32], wanted: i32) -> i32 {
    values
        .iter()
        .position(|&v| v == wanted)
        .map_or(0, |p| p as i32)
}

#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct Initial {
    pub fullscreen: i32,
    pub shadow_quality: i32,
    pub shadow_resolution: i32,
    pub reflection: i32,
    pub ssao: i32,
    pub ssao_resolution: i32,
    pub ssao_samples: i32,
    pub texture_quality: i32,
    pub texture_filter: i32,
    pub anisotropy: i32,
    pub edge_smooth: i32,
    pub parallax: i32,
}

/// `SetInitialValues` reads, in the same order.
pub fn initial(c: &mut dyn Cfg) -> Initial {
    Initial {
        fullscreen: i32::from(c.bool(b"Screen", b"FullScreen", true)),
        shadow_quality: c.int(GRAPHICS, b"ShadowQuality", 1),
        shadow_resolution: c.int(GRAPHICS, b"ShadowResolution", 1),
        reflection: i32::from(c.bool(GRAPHICS, b"WorldReflection", true)),
        ssao: i32::from(c.bool(GRAPHICS, b"SSAOActive", true)),
        ssao_resolution: c.int(GRAPHICS, b"SSAOResolution", 0),
        ssao_samples: c.int(GRAPHICS, b"SSAOSamples", SSAO_SAMPLES_DEFAULT),
        texture_quality: c.int(GRAPHICS, b"TextureQuality", 1),
        texture_filter: c.int(GRAPHICS, b"TextureFilter", 1),
        anisotropy: c.float(GRAPHICS, b"TextureAnisotropy", 1.0) as i32,
        edge_smooth: i32::from(c.bool(GRAPHICS, b"EdgeSmooth", false)),
        parallax: i32::from(c.bool(GRAPHICS, b"ParallaxEnabled", true)),
    }
}

/// Texture quality menu row: the menu is stored high to low with a trailing sentinel item.
pub fn texture_quality_row(menu_size: i32, quality: i32) -> i32 {
    menu_size.wrapping_sub(2).wrapping_sub(quality)
}
