//! H9: `cRendererDeferred::LoadData` program-combo setup (feature tables, default defines, variable ids).
//! C ABI declared in `graphics/RustRendererH9.h`.

use std::ffi::c_char;

const VERTEX: u32 = 1;
const FRAGMENT: u32 = 2;
const FRAG_VERT: u32 = 3;
const GOBO: u32 = 1 << 4;

pub struct Feature {
    pub name: &'static [u8],
    pub stages: u32,
    pub needed: u32,
}

const fn feat(name: &'static [u8], stages: u32, needed: u32) -> Feature {
    Feature {
        name,
        stages,
        needed,
    }
}

pub const LIGHT_FEATURES: [Feature; 13] = [
    feat(b"UseDeferredLightShapes\0", VERTEX, 0),
    feat(b"UseBatching\0", FRAG_VERT, 0),
    feat(b"UseSpecular\0", FRAGMENT, 0),
    feat(b"LightType_Spot\0", FRAG_VERT, 0),
    feat(b"UseGobo\0", FRAGMENT, 0),
    feat(b"DivideInFrag\0", FRAG_VERT, 0),
    feat(b"UseShadowMap\0", FRAGMENT, 0),
    feat(b"BoxMask\0", FRAGMENT, 0),
    feat(b"GoboSpecFlag\0", FRAGMENT, GOBO),
    feat(b"GoboType_Specular\0", FRAGMENT, GOBO),
    feat(b"UseTranslucency\0", FRAGMENT, 0),
    feat(b"UseUnderwaterFog\0", FRAGMENT, 0),
    feat(b"LightType_Directional\0", FRAGMENT, 0),
];

pub const FOG_FEATURES: [Feature; 9] = [
    feat(b"OutsideBox\0", FRAG_VERT, 0),
    feat(b"UseBackside\0", FRAG_VERT, 0),
    feat(b"UseUnderwaterFog\0", FRAGMENT, 0),
    feat(b"UseNoise\0", FRAGMENT, 0),
    feat(b"UseSkybox\0", FRAGMENT, 0),
    feat(b"UseSecondaryFog\0", FRAGMENT, 0),
    feat(b"FogArea\0", FRAGMENT, 0),
    feat(b"Lighten\0", FRAGMENT, 0),
    feat(b"UseSeparateFogSkybox\0", FRAGMENT, 0),
];

pub const FOG_VARS: [(&[u8], i32); 23] = [
    (b"afNegFarPlane\0", 3),
    (b"avFogSettings\0", 95),
    (b"avSecondFogSettings\0", 96),
    (b"avCameraPos\0", 97),
    (b"alFogType\0", 98),
    (b"a_mtxSkybox\0", 99),
    (b"afSkyboxMipNum\0", 100),
    (b"avFogStartAndLength\0", 14),
    (b"avFogColor\0", 15),
    (b"avRayCastStart\0", 16),
    (b"a_mtxBoxInvViewModelRotation\0", 17),
    (b"avNegPlaneDistNeg\0", 18),
    (b"avNegPlaneDistPos\0", 19),
    (b"afFalloffExp\0", 20),
    (b"avScreenToFarPlane\0", 23),
    (b"avInvScreenSize\0", 24),
    (b"a_mtxNoise\0", 70),
    (b"afInvNoiseScale\0", 71),
    (b"afNoiseStrength\0", 72),
    (b"avSkyboxColor\0", 73),
    (b"avSecondFogColor\0", 74),
    (b"avSecondFogStartAndLength\0", 75),
    (b"afSecondFalloffExp\0", 76),
];

pub const LIGHT_VARS: [(&[u8], i32); 58] = [
    (b"avLightPos\0", 0),
    (b"avLightColor\0", 1),
    (b"afInvLightRadius\0", 2),
    (b"afNegFarPlane\0", 3),
    (b"afOneMinusCosHalfSpotFOV\0", 4),
    (b"avLightForward\0", 5),
    (b"a_mtxSpotViewProj\0", 6),
    (b"a_mtxInvViewRotation\0", 7),
    (b"avShadowMapOffsetMul\0", 8),
    (b"avShadowParams\0", 110),
    (b"avShadowMapSize\0", 111),
    (b"avScreenToFarPlane\0", 23),
    (b"avInvScreenSize\0", 24),
    (b"a_mtxLightViewProj\0", 25),
    (b"a_mtxLightViewProj0\0", 87),
    (b"a_mtxLightViewProj1\0", 88),
    (b"a_mtxLightViewProj2\0", 89),
    (b"a_mtxLightViewProj3\0", 90),
    (b"afShadowDistance\0", 112),
    (b"alSliceNum\0", 113),
    (b"afSliceDiv\0", 114),
    (b"a_mtxLightViewProj[0]\0", 115),
    (b"a_mtxLightViewProj[1]\0", 116),
    (b"a_mtxLightViewProj[2]\0", 117),
    (b"a_mtxLightViewProj[3]\0", 118),
    (b"avSplitsNear\0", 91),
    (b"avSplitsFar\0", 92),
    (b"avSplitOffsetMul\0", 93),
    (b"afSpotNearClip\0", 52),
    (b"afFalloffPow\0", 26),
    (b"afTranslucencyScale\0", 69),
    (b"afSpotFalloffPow\0", 27),
    (b"a_mtxInvView\0", 32),
    (b"avMaskCenter\0", 33),
    (b"avMaskExtent\0", 34),
    (b"afLightSourceRadius\0", 42),
    (b"avLightUp\0", 43),
    (b"avLightRight\0", 44),
    (b"avFogColor\0", 15),
    (b"avFogStartAndLength\0", 14),
    (b"afFogFalloffExp\0", 77),
    (b"avLightDirection\0", 78),
    (b"avAmbientColorSky\0", 35),
    (b"avAmbientColorGround\0", 36),
    (b"avViewSpaceUp\0", 94),
    (b"px_vLightColor\0", 1),
    (b"px_fFalloff\0", 26),
    (b"px_fSpotFalloffPow\0", 27),
    (b"px_fTranslucenyMul\0", 69),
    (b"px_fLightSourceRadius\0", 42),
    (b"px_mtxLightViewProj\0", 25),
    (b"px_vLightForward\0", 5),
    (b"px_vLightUp\0", 43),
    (b"px_vLightRight\0", 44),
    (b"px_vMaskCenter\0", 33),
    (b"px_vMaskExtent\0", 34),
    (b"px_vLightPosScale\0", 94),
    (b"afFarPlane\0", 10),
];

#[derive(Clone, Copy, Debug, PartialEq)]
pub enum Value {
    Flag,
    Int(i32),
    Float(f32),
}

pub struct Define {
    pub name: &'static [u8],
    pub value: Value,
}

const fn def(name: &'static [u8], value: Value) -> Define {
    Define { name, value }
}

const QUALITY_LOW: i32 = 0;
const QUALITY_MEDIUM: i32 = 1;
const QUALITY_HIGH: i32 = 2;

pub fn light_defaults(
    quality: i32,
    gbuffer_64bit: bool,
    targets4: bool,
    jitter_size: i32,
    jitter_samples: i32,
) -> Vec<Define> {
    let mut v = vec![
        def(
            b"ShadowJitterLookupMul\0",
            Value::Float(1.0 / jitter_size as f32),
        ),
        def(
            b"ShadowJitterSamplesDiv2\0",
            Value::Int(jitter_samples.wrapping_div(2)),
        ),
        def(b"ShadowJitterSamples\0", Value::Int(jitter_samples)),
    ];
    match quality {
        QUALITY_HIGH => v.push(def(b"ShadowMapQuality_High\0", Value::Flag)),
        QUALITY_MEDIUM => v.push(def(b"ShadowMapQuality_Medium\0", Value::Flag)),
        QUALITY_LOW => v.push(def(b"ShadowMapQuality_Low\0", Value::Flag)),
        _ => {}
    }
    v.push(def(b"DeferredLight\0", Value::Flag));
    v.push(if gbuffer_64bit {
        def(b"Deferred_64bit\0", Value::Flag)
    } else {
        def(b"Deferred_32bit\0", Value::Flag)
    });
    v.push(if targets4 {
        def(b"RenderTargets_4\0", Value::Flag)
    } else {
        def(b"RenderTargets_3\0", Value::Flag)
    });
    v
}

pub fn fog_defaults(gbuffer_64bit: bool) -> Vec<Define> {
    if gbuffer_64bit {
        Vec::new()
    } else {
        vec![def(b"PackedDepth\0", Value::Flag)]
    }
}

#[repr(C)]
pub struct CDefine {
    pub name: *const c_char,
    pub kind: i32,
    pub ival: i32,
    pub fval: f32,
}

fn vars(table: i32) -> &'static [(&'static [u8], i32)] {
    match table {
        0 => &FOG_VARS,
        1 => &LIGHT_VARS,
        _ => &[],
    }
}

fn features(table: i32) -> &'static [Feature] {
    match table {
        0 => &FOG_FEATURES,
        1 => &LIGHT_FEATURES,
        _ => &[],
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_h9_var_count(table: i32) -> usize {
    vars(table).len()
}

/// # Safety
/// `id` is null or valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h9_var(table: i32, i: usize, id: *mut i32) -> *const c_char {
    let Some(&(name, v)) = vars(table).get(i) else {
        return std::ptr::null();
    };
    // SAFETY: by the caller's contract.
    if let Some(id) = unsafe { id.as_mut() } {
        *id = v;
    }
    name.as_ptr().cast()
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_h9_feature_count(table: i32) -> usize {
    features(table).len()
}

/// # Safety
/// `stages` and `needed` are null or valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h9_feature(
    table: i32,
    i: usize,
    stages: *mut u32,
    needed: *mut u32,
) -> *const c_char {
    let Some(f) = features(table).get(i) else {
        return std::ptr::null();
    };
    // SAFETY: by the caller's contract.
    if let Some(s) = unsafe { stages.as_mut() } {
        *s = f.stages;
    }
    // SAFETY: by the caller's contract.
    if let Some(n) = unsafe { needed.as_mut() } {
        *n = f.needed;
    }
    f.name.as_ptr().cast()
}

fn fill(src: &[Define], out: *mut CDefine, cap: usize) -> usize {
    if out.is_null() {
        return src.len();
    }
    for (i, d) in src.iter().take(cap).enumerate() {
        let (kind, ival, fval) = match d.value {
            Value::Flag => (0, 0, 0.0),
            Value::Int(n) => (1, n, 0.0),
            Value::Float(f) => (2, 0, f),
        };
        // SAFETY: `out` has `cap` elements and `i < cap`.
        unsafe {
            out.add(i).write(CDefine {
                name: d.name.as_ptr().cast(),
                kind,
                ival,
                fval,
            });
        }
    }
    src.len()
}

/// Returns the define count; writes at most `cap` entries when `out` is non-null.
///
/// # Safety
/// `out` is null or points to `cap` elements.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h9_light_defaults(
    quality: i32,
    gbuffer_64bit: bool,
    targets4: bool,
    jitter_size: i32,
    jitter_samples: i32,
    out: *mut CDefine,
    cap: usize,
) -> usize {
    fill(
        &light_defaults(
            quality,
            gbuffer_64bit,
            targets4,
            jitter_size,
            jitter_samples,
        ),
        out,
        cap,
    )
}

/// # Safety
/// `out` is null or points to `cap` elements.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h9_fog_defaults(
    gbuffer_64bit: bool,
    out: *mut CDefine,
    cap: usize,
) -> usize {
    fill(&fog_defaults(gbuffer_64bit), out, cap)
}
