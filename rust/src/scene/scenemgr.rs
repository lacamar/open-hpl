//! `cScene` decisions: what `Render` does per viewport and the draw order of screen GUI sets. Viewports, worlds, GUI sets and the renderer stay C++.

pub const FLAG_WORLD: u32 = 1;
pub const FLAG_GUI: u32 = 2;
pub const FLAG_POST_EFFECTS: u32 = 4;

pub const STEP_SKIP: u32 = 1;
pub const STEP_PRE_CALLBACK: u32 = 2;
pub const STEP_RENDER_WORLD: u32 = 4;
pub const STEP_FALLBACK_TARGET: u32 = 8;
pub const STEP_POST_CALLBACK: u32 = 16;
pub const STEP_GUI_3D: u32 = 32;
pub const STEP_POST_EFFECTS: u32 = 64;
pub const STEP_GUI_SCREEN: u32 = 128;
pub const STEP_POST_FLAG: u32 = 256;

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct ViewportIn {
    pub visible: bool,
    pub has_composite: bool,
    pub has_renderer: bool,
    pub has_world: bool,
    pub has_frustum: bool,
    pub post_effects_active: bool,
}

/// Whether `Render` has to ask the composite for active effects (the answer feeds `plan_viewport`).
pub fn wants_post_effects(flags: u32, has_composite: bool) -> bool {
    flags & FLAG_WORLD != 0 && has_composite && flags & FLAG_POST_EFFECTS != 0
}

/// Steps `cScene::Render` runs for one viewport, in order of the bit values; `STEP_POST_FLAG`
/// is the `bPostEffects` the renderer gets. The C++ asks the
/// renderer for the post-effect texture even without a renderer (null dereference); here the
/// post-effect step needs a renderer.
pub fn plan_viewport(flags: u32, v: &ViewportIn) -> u32 {
    if !v.visible {
        return STEP_SKIP;
    }
    let mut plan = 0;
    let mut post = false;
    if flags & FLAG_WORLD != 0 {
        plan |= STEP_PRE_CALLBACK;
        post = wants_post_effects(flags, v.has_composite) && v.post_effects_active;
        if v.has_renderer && v.has_world && v.has_frustum {
            plan |= STEP_RENDER_WORLD;
        } else {
            plan |= STEP_FALLBACK_TARGET;
        }
        plan |= STEP_POST_CALLBACK;
        plan |= STEP_GUI_3D;
    }
    if post {
        plan |= STEP_POST_FLAG;
        if v.has_renderer {
            plan |= STEP_POST_EFFECTS;
        }
    }
    if flags & FLAG_GUI != 0 {
        plan |= STEP_GUI_SCREEN;
    }
    plan
}

/// Stable order of the screen GUI sets by draw priority (`std::multimap` insertion order).
pub fn gui_order(prios: &[i32], out: &mut [u32]) -> usize {
    let mut idx: Vec<u32> = (0..prios.len().min(out.len()) as u32).collect();
    idx.sort_by_key(|&i| prios.get(i as usize).copied().unwrap_or(0));
    for (o, i) in out.iter_mut().zip(&idx) {
        *o = *i;
    }
    idx.len()
}
