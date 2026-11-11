//! `SomaToneMapping.cpp`: exposure / white-cut fades, exposure areas and grading-texture blends.

use super::hw;
use crate::math::{matrix_mul_vec, v3};
use crate::types::{Matrixf, Vector3f};

#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct Fade {
    pub goal: f32,
    pub speed: f32,
}

impl Default for Fade {
    fn default() -> Self {
        Self {
            goal: 1e30,
            speed: 0.0,
        }
    }
}

#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct ExposureArea {
    pub inv_transform: Matrixf,
    pub half_size: [f32; 3],
    pub exposure: f32,
    pub white_cut: f32,
    pub transition_time: f32,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct ToneState {
    pub world_exposure: f32,
    pub world_white_cut: f32,
    pub world_fade_time: f32,
    pub exposure: f32,
    pub white_cut: f32,
    pub transition_time: f32,
    pub grading_blend: f32,
    pub grading_fade_time: f32,
    pub exposure_fade: Fade,
    pub white_cut_fade: Fade,
}

/// Index of the first area containing `pos`.
pub fn area_at(areas: &[ExposureArea], pos: Vector3f) -> Option<usize> {
    areas.iter().position(|a| {
        let local = matrix_mul_vec(&a.inv_transform, pos);
        local.x.abs() <= a.half_size[0]
            && local.y.abs() <= a.half_size[1]
            && local.z.abs() <= a.half_size[2]
    })
}

pub fn step(value: &mut f32, fade: &mut Fade, goal: f32, time: f32, dt: f32) {
    if goal != fade.goal {
        fade.goal = goal;
        fade.speed = if time > 0.0 {
            (goal - *value).abs() / time
        } else {
            1e30
        };
    }
    let step = hw::mul(fade.speed, dt);
    if (goal - *value).abs() <= step {
        *value = goal;
    } else {
        let sel = if goal > *value { step } else { -step };
        *value = hw::add(sel, *value);
    }
}

/// `OnMapLoaded`'s state reset.
pub fn map_loaded(s: &mut ToneState, exposure: f32, white_cut: f32, area: Option<&ExposureArea>) {
    s.world_exposure = exposure;
    s.exposure = exposure;
    s.world_white_cut = white_cut;
    s.white_cut = white_cut;
    s.world_fade_time = 0.5;
    if let Some(a) = area {
        s.exposure = a.exposure;
        s.white_cut = a.white_cut;
    }
    s.exposure_fade = Fade::default();
    s.white_cut_fade = Fade::default();
}

/// Exposure and white-cut stepping while the tone-mapped world is the current one.
pub fn update_exposure(
    s: &mut ToneState,
    world_current: bool,
    area: Option<&ExposureArea>,
    dt: f32,
) {
    if !world_current {
        return;
    }
    let (mut goal_exp, mut goal_white) = (s.world_exposure, s.world_white_cut);
    s.transition_time = s.world_fade_time;
    if let Some(a) = area {
        goal_exp = a.exposure;
        goal_white = a.white_cut;
        s.transition_time = a.transition_time;
    }
    let (mut exposure, mut white_cut) = (s.exposure, s.white_cut);
    let mut ef = s.exposure_fade;
    let mut wf = s.white_cut_fade;
    step(&mut exposure, &mut ef, goal_exp, s.transition_time, dt);
    step(&mut white_cut, &mut wf, goal_white, s.transition_time, dt);
    s.exposure = exposure;
    s.white_cut = white_cut;
    s.exposure_fade = ef;
    s.white_cut_fade = wf;
}

/// What C++ does with the grading textures after a blend step.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct GradingStep {
    /// Destroy the current texture (when present) and make the target current.
    pub promote_target: bool,
}

pub fn update_grading(
    s: &mut ToneState,
    has_target: bool,
    has_texture: bool,
    dt: f32,
) -> GradingStep {
    if !has_target {
        return GradingStep {
            promote_target: false,
        };
    }
    s.grading_blend += if s.grading_fade_time > 0.0 {
        dt / s.grading_fade_time
    } else {
        1.0
    };
    GradingStep {
        promote_target: s.grading_blend >= 1.0 || !has_texture,
    }
}

/// Insert position for a layer of `prio`: after every layer whose priority is not above it.
pub fn layer_insert_index(prios: &[i32], prio: i32) -> usize {
    prios.iter().take_while(|p| **p <= prio).count()
}

pub fn v(a: [f32; 3]) -> Vector3f {
    v3(a[0], a[1], a[2])
}
