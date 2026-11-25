//! `cSomaToneMapping`: exposure and white-cut fades, grading texture blends and layers, the
//! renderer's tone/bloom/grain parameters and the `cPostEffect_ToneMapping` natives. The C++ shell
//! only forwards `Update`/`OnMapLoaded` and implements [`Host`].

use super::tonemap::{self, ExposureArea, ToneState};
use crate::somasound::powf;
use std::cell::RefCell;

/// Engine texture or object pointer; 0 is none.
pub type Ptr = usize;

pub trait Host {
    /// `Create3D` plus clamp/bilinear setup.
    fn grading_tex(&self, name: &[u8]) -> Ptr;
    /// `core_noise2D.dds`, repeat wrapped.
    fn noise_tex(&self) -> Ptr;
    fn tex_destroy(&self, tex: Ptr);
    fn tex_name(&self, tex: Ptr) -> Option<Vec<u8>>;
    fn world_grading_name(&self, world: Ptr) -> Vec<u8>;
    /// Key, exposure and white cut the map file asks for.
    fn world_tone(&self, world: Ptr) -> [f32; 3];
    fn world_areas(&self, world: Ptr) -> Vec<ExposureArea>;
    fn current_world(&self) -> Ptr;
    fn current_viewport(&self) -> Ptr;
    /// The C++ shell, what `GetToneMappingEffect` hands to scripts.
    fn tone_object(&self) -> Ptr;
    fn camera_pos(&self) -> Option<[f32; 3]>;
    fn set_tone(&self, key: f32, scale: f32, white_cut: f32, gamma: f32);
    fn set_layers(&self, layers: &[(Ptr, f32)]);
    fn set_grading(&self, tex: Ptr, target: Ptr, blend: f32);
    fn user_bloom(&self) -> bool;
    fn set_bloom(&self, active: bool, bright_pass: f32, width: f32, tint: [f32; 4]);
    fn set_grain(&self, tex: Ptr, intensity: f32);
    fn set_srgb(&self, on: bool);
}

struct Layer {
    prio: i32,
    name: Vec<u8>,
    tex: Ptr,
    alpha: f32,
}

struct Tone {
    key: f32,
    gamma: f32,
    grain_intensity: f32,
    bright_pass: f32,
    bloom_width: f32,
    bloom_tint: [f32; 4],
    bloom_falloff: f32,
    bloom_active: bool,
    grain_active: bool,
    grading_active: bool,
    srgb: bool,
    s: ToneState,
    world: Ptr,
    grading_tex: Ptr,
    grading_target: Ptr,
    layers: Vec<Layer>,
    noise: Ptr,
}

impl Tone {
    fn new() -> Self {
        let mut s = ToneState {
            world_exposure: 0.0,
            world_white_cut: 3.5,
            world_fade_time: 0.5,
            exposure: 0.0,
            white_cut: 3.5,
            transition_time: 0.5,
            ..ToneState::default()
        };
        s.exposure_fade = tonemap::Fade::default();
        s.white_cut_fade = tonemap::Fade::default();
        Self {
            key: 0.5,
            gamma: 2.2,
            grain_intensity: 1.0,
            bright_pass: 0.75,
            bloom_width: 128.0,
            bloom_tint: [1.0; 4],
            bloom_falloff: 0.5,
            bloom_active: true,
            grain_active: true,
            grading_active: true,
            srgb: false,
            s,
            world: 0,
            grading_tex: 0,
            grading_target: 0,
            layers: Vec::new(),
            noise: 0,
        }
    }
}

thread_local! {
    static TONE: RefCell<Option<Tone>> = const { RefCell::new(None) };
}

fn with<R>(f: impl FnOnce(&mut Tone) -> R) -> Option<R> {
    TONE.with(|t| {
        let mut b = t.try_borrow_mut().ok()?;
        Some(f(b.get_or_insert_with(Tone::new)))
    })
}

pub fn create() {
    TONE.with(|t| {
        if let Ok(mut b) = t.try_borrow_mut() {
            *b = Some(Tone::new());
        }
    });
}

fn take() -> Option<Tone> {
    with(|t| std::mem::replace(t, Tone::new()))
}

fn area_at(h: &dyn Host, world: Ptr) -> Option<ExposureArea> {
    let areas = h.world_areas(world);
    let pos = h.camera_pos()?;
    let i = tonemap::area_at(&areas, tonemap::v(pos))?;
    areas.get(i).copied()
}

pub fn map_loaded(h: &dyn Host, world: Ptr) {
    let Some(mut t) = take() else {
        return;
    };
    t.world = world;
    if t.grading_tex != 0 {
        h.tex_destroy(t.grading_tex);
    }
    if t.grading_target != 0 {
        h.tex_destroy(t.grading_target);
    }
    t.grading_tex = 0;
    t.grading_target = 0;
    let name = h.world_grading_name(world);
    if !name.is_empty() {
        t.grading_tex = h.grading_tex(&name);
    }
    let [key, exposure, white_cut] = h.world_tone(world);
    t.key = key;
    let area = area_at(h, world);
    tonemap::map_loaded(&mut t.s, exposure, white_cut, area.as_ref());
    put(t);
    update(h, 0.0);
}

fn put(t: Tone) {
    let _ = with(|x| *x = t);
}

pub fn update(h: &dyn Host, dt: f32) {
    let Some(mut t) = take() else {
        return;
    };
    let world_current = t.world != 0 && t.world == h.current_world();
    let area = if world_current {
        area_at(h, t.world)
    } else {
        None
    };
    tonemap::update_exposure(&mut t.s, world_current, area.as_ref(), dt);
    h.set_tone(t.key, powf(2.0, t.s.exposure), t.s.white_cut, t.gamma);
    if tonemap::update_grading(&mut t.s, t.grading_target != 0, t.grading_tex != 0, dt)
        .promote_target
    {
        if t.grading_tex != 0 {
            h.tex_destroy(t.grading_tex);
        }
        t.grading_tex = t.grading_target;
        t.grading_target = 0;
    }
    if !t.layers.is_empty() && t.grading_tex == 0 {
        t.grading_tex = h.grading_tex(b"grading_default.dds");
    }
    let layers: Vec<(Ptr, f32)> = t.layers.iter().map(|l| (l.tex, l.alpha)).collect();
    h.set_layers(&layers);
    let on = t.grading_active;
    h.set_grading(
        if on { t.grading_tex } else { 0 },
        if on { t.grading_target } else { 0 },
        t.s.grading_blend,
    );
    h.set_bloom(
        t.bloom_active && h.user_bloom(),
        t.bright_pass,
        t.bloom_width,
        t.bloom_tint,
    );
    if t.noise == 0 && t.grain_active {
        t.noise = h.noise_tex();
    }
    h.set_grain(if t.grain_active { t.noise } else { 0 }, t.grain_intensity);
    h.set_srgb(t.srgb);
    put(t);
}

fn fade_grading(h: &dyn Host, name: &[u8], time: f32) {
    let tex = h.grading_tex(name);
    if tex == 0 {
        return;
    }
    let Some(mut t) = take() else {
        return;
    };
    if t.grading_target != 0 {
        if t.grading_tex != 0 {
            h.tex_destroy(t.grading_tex);
        }
        t.grading_tex = t.grading_target;
    }
    if t.grading_tex == 0 {
        t.grading_tex = h.grading_tex(b"grading_default.dds");
    }
    t.grading_target = tex;
    t.s.grading_blend = 0.0;
    t.s.grading_fade_time = time;
    put(t);
}

fn set_layer(h: &dyn Host, prio: i32, name: &[u8], alpha: f32) {
    let found = with(|t| {
        let hit = t.layers.iter_mut().find(|l| l.name == name);
        hit.map(|l| l.alpha = alpha).is_some()
    });
    if found != Some(false) {
        return;
    }
    let tex = h.grading_tex(name);
    if tex == 0 {
        return;
    }
    let _ = with(|t| {
        let prios: Vec<i32> = t.layers.iter().map(|l| l.prio).collect();
        let at = tonemap::layer_insert_index(&prios, prio);
        t.layers.insert(
            at,
            Layer {
                prio,
                name: name.to_vec(),
                tex,
                alpha,
            },
        );
    });
}

fn remove_layer(h: &dyn Host, name: &[u8]) {
    let gone = with(|t| {
        let i = t.layers.iter().position(|l| l.name == name)?;
        Some(t.layers.remove(i).tex)
    })
    .flatten();
    if let Some(tex) = gone {
        h.tex_destroy(tex);
    }
}

/// `(world exposure, world white cut)` for the save file.
pub fn world_exposure() -> Option<(f32, f32)> {
    with(|t| (t.s.world_exposure, t.s.world_white_cut))
}

pub fn fade_exposure(exposure: f32, white_cut: f32, time: f32) {
    let _ = with(|t| {
        t.s.world_exposure = exposure;
        t.s.world_white_cut = white_cut;
        t.s.world_fade_time = time;
    });
}

pub fn register(e: crate::somascript::s2_as::Engine, h: &'static dyn Host) {
    natives::register(e, h);
}

mod natives {
    use super::{Host, fade_exposure, fade_grading, remove_layer, set_layer, with};
    use crate::somascript::s2_api::{dummy_of, is_dummy};
    use crate::somascript::s2_as::{Engine, Frame, Soma};

    const T: &str = "cPostEffect_ToneMapping";

    fn texture_name(h: &dyn Host, f: &Frame, i: usize) -> Option<Vec<u8>> {
        let p = f.ptr_arg(i);
        if p.is_null() || is_dummy(p) {
            return None;
        }
        h.tex_name(p as usize)
    }

    pub fn register(e: Engine, h: &'static dyn Host) {
        let s = Soma(e);
        s.func("cViewport@ cLux_GetViewport()", move |f| {
            let p = h.current_viewport();
            // Effect handlers init before the first viewport exists
            let p = if p == 0 {
                dummy_of(e, b"cViewport")
            } else {
                p as *mut std::ffi::c_void
            };
            f.ret_ref(p);
        });
        s.plain(
            "cViewport",
            "cPostEffect_ToneMapping@ GetToneMappingEffect()",
            move |f| f.ret_ref(h.tone_object() as *mut std::ffi::c_void),
        );
        s.method(
            T,
            "void FadeExposure(float afExposure, float afWhiteCut, float afTime)",
            |f| fade_exposure(f.arg(0), f.arg(1), f.arg(2)),
        );
        s.method(
            T,
            "void FadeWindowExposure(float afExposure, float afWhiteCut)",
            |f| {
                let time = with(|t| t.s.transition_time).unwrap_or(0.0);
                fade_exposure(f.arg(0), f.arg(1), time);
            },
        );
        s.method(T, "float GetTransitionTime()", |f| {
            f.ret(with(|t| t.s.transition_time).unwrap_or(0.0))
        });
        s.method(T, "void SetColorGradingActive(bool abX)", |f| {
            let _ = with(|t| t.grading_active = f.arg(0));
        });
        s.method(T, "void SetBloomActive(bool abX)", |f| {
            let _ = with(|t| t.bloom_active = f.arg(0));
        });
        s.method(T, "void SetFilmGrainActive(bool abX)", |f| {
            let _ = with(|t| t.grain_active = f.arg(0));
        });
        s.method(T, "bool GetColorGradingActive()", |f| {
            f.ret(with(|t| t.grading_active).unwrap_or(false))
        });
        s.method(T, "bool GetBloomActive()", |f| {
            f.ret(with(|t| t.bloom_active).unwrap_or(false))
        });
        s.method(T, "bool GetFilmGrainActive()", |f| {
            f.ret(with(|t| t.grain_active).unwrap_or(false))
        });
        s.method(T, "void SetSRGBGamma(bool abX)", |f| {
            let _ = with(|t| t.srgb = f.arg(0));
        });
        s.method(T, "float GetExposure()", |f| {
            f.ret(with(|t| t.s.exposure).unwrap_or(0.0))
        });
        s.method(
            T,
            "void GetParams(float &out afKey, float &out afGammaCorrection, float &out afFilmGrainIntensity, float &out afBrightPass, float &out afBloomWidth, cColor &out avBloomTint, float &out afBloomFalloff)",
            |f| {
                let _ = with(|t| {
                    *f.rm::<f32>(0) = t.key;
                    *f.rm::<f32>(1) = t.gamma;
                    *f.rm::<f32>(2) = t.grain_intensity;
                    *f.rm::<f32>(3) = t.bright_pass;
                    *f.rm::<f32>(4) = t.bloom_width;
                    *f.rm::<[f32; 4]>(5) = t.bloom_tint;
                    *f.rm::<f32>(6) = t.bloom_falloff;
                });
            },
        );
        s.method(
            T,
            "void SetParams(float afKey, float afGammaCorrection, float afFilmGrainIntensity, float afBrightPass, float afBloomWidth, const cColor&in avBloomTint, float afBloomFalloff)",
            |f| {
                let _ = with(|t| {
                    t.key = f.arg(0);
                    t.gamma = f.arg(1);
                    t.grain_intensity = f.arg(2);
                    t.bright_pass = f.arg(3);
                    t.bloom_width = f.arg(4);
                    t.bloom_tint = *f.rf::<[f32; 4]>(5);
                    t.bloom_falloff = f.arg(6);
                });
            },
        );

        let w = "cWorld";
        s.method(
            w,
            "void FadeToneMappingExposure(float afX, float afTime)",
            |f| {
                let _ = with(|t| {
                    t.s.world_exposure = f.arg(0);
                    t.s.world_fade_time = f.arg(1);
                });
            },
        );
        s.method(
            w,
            "void FadeToneMappingWhiteCut(float afX, float afTime)",
            |f| {
                let _ = with(|t| {
                    t.s.world_white_cut = f.arg(0);
                    t.s.world_fade_time = f.arg(1);
                });
            },
        );
        s.method(w, "void SetToneMappingKey(float afX)", |f| {
            let _ = with(|t| t.key = f.arg(0));
        });
        s.method(w, "float GetToneMappingExposure()", |f| {
            f.ret(with(|t| t.s.world_exposure).unwrap_or(0.0))
        });
        s.method(w, "float GetToneMappingKey()", |f| {
            f.ret(with(|t| t.key).unwrap_or(0.0))
        });
        s.method(w, "float GetToneMappingWhiteCut()", |f| {
            f.ret(with(|t| t.s.world_white_cut).unwrap_or(0.0))
        });
        s.method(
            w,
            "void FadeGradingTexture(const tString&in asTexture, float afTime)",
            move |f| fade_grading(h, f.str_arg(0), f.arg(1)),
        );
        s.method(
            T,
            "void FadeGradingTexture(iTexture @apGrading, float afTime)",
            move |f| {
                if let Some(n) = texture_name(h, f, 0) {
                    fade_grading(h, &n, f.arg(1));
                }
            },
        );
        s.func(
            "void cScene_FadeGradingTexture(cWorld@ apWorld, iTexture@ apGrading, float afTime)",
            move |f| {
                if let Some(n) = texture_name(h, f, 1) {
                    fade_grading(h, &n, f.arg(2));
                }
            },
        );
        s.func(
            "iTexture@ cResources_CreateTexture3D(const tString&in asName, bool abUseMipMaps)",
            move |f| f.ret_ref(h.grading_tex(f.str_arg(0)) as *mut std::ffi::c_void),
        );
        s.method(
            T,
            "void SetGradingTexture(int alPrio, const tString&in asTexture, float afAlpha)",
            move |f| set_layer(h, f.arg(0), f.str_arg(1), f.arg(2)),
        );
        s.method(
            T,
            "void RemoveGradingTexture(const tString&in asTexture)",
            move |f| remove_layer(h, f.str_arg(0)),
        );
        s.method(w, "float GetToneMappingFadeTime()", |f| {
            f.ret(with(|t| t.s.world_fade_time).unwrap_or(0.0))
        });
    }
}

#[cfg(test)]
#[path = "s8b_tone_tests.rs"]
mod tests;
