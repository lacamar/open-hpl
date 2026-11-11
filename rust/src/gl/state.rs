//! `cLowLevelGraphicsSDL` redundant-state filters and framebuffer rectangle arithmetic.
//! A filter returns true when the C++ caller must issue the `gl*` call.

use super::c;
use crate::types::Vector2l;

pub const MAX_TEXTURE_UNITS: usize = 16;
const FIXED_UNITS: u32 = 8;

#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct GlState {
    pub color_write: [bool; 4],
    pub depth_write: bool,
    pub depth_test: bool,
    pub alpha_test: bool,
    pub scissor: bool,
    pub blend: bool,
    pub cull_active: bool,
    pub depth_func: i32,
    pub alpha_func: i32,
    pub cull_mode: i32,
    pub active_unit: i32,
    pub alpha_ref: f32,
    pub target: [u32; MAX_TEXTURE_UNITS],
}

/// Texture-unit work for one `SetTexture`, in execution order.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct TexPlan {
    /// Unit for `glActiveTexture`, -1 when unchanged.
    pub active_unit: i32,
    /// Target to `glDisable`, 0 for none (runs before the bind).
    pub disable: u32,
    pub bind: bool,
    /// Target to `glEnable`, 0 for none (runs after the bind).
    pub enable: u32,
}

impl GlState {
    /// Constructor state: members the C++ constructor leaves to `SetupGL` are zeroed.
    pub fn new() -> Self {
        Self {
            color_write: [false; 4],
            depth_write: false,
            depth_test: false,
            alpha_test: false,
            scissor: false,
            blend: false,
            cull_active: false,
            depth_func: 0,
            alpha_func: 0,
            cull_mode: 0,
            active_unit: -1,
            alpha_ref: 0.0,
            target: [0; MAX_TEXTURE_UNITS],
        }
    }

    /// The tracked values `SetupGL` assigns; texture unit tracking is untouched.
    pub fn setup(&mut self) {
        self.color_write = [true; 4];
        self.depth_write = true;
        self.cull_active = true;
        self.cull_mode = 1;
        self.depth_test = true;
        self.depth_func = 2;
        self.alpha_test = false;
        self.alpha_func = 4;
        self.alpha_ref = 0.6;
        self.scissor = false;
        self.blend = false;
    }

    pub fn set_color_write(&mut self, r: bool, g: bool, b: bool, a: bool) -> bool {
        let next = [r, g, b, a];
        if self.color_write == next {
            return false;
        }
        self.color_write = next;
        true
    }

    pub fn set_depth_write(&mut self, on: bool) -> bool {
        replace(&mut self.depth_write, on)
    }

    pub fn set_depth_test(&mut self, on: bool) -> bool {
        replace(&mut self.depth_test, on)
    }

    pub fn set_depth_func(&mut self, f: i32) -> bool {
        replace(&mut self.depth_func, f)
    }

    pub fn set_alpha_test(&mut self, on: bool) -> bool {
        replace(&mut self.alpha_test, on)
    }

    #[expect(clippy::float_cmp, reason = "C++ compares the reference exactly")]
    pub fn set_alpha_func(&mut self, f: i32, reference: f32) -> bool {
        if self.alpha_func == f && self.alpha_ref == reference {
            return false;
        }
        self.alpha_func = f;
        self.alpha_ref = reference;
        true
    }

    pub fn set_scissor(&mut self, on: bool) -> bool {
        replace(&mut self.scissor, on)
    }

    pub fn set_blend(&mut self, on: bool) -> bool {
        replace(&mut self.blend, on)
    }

    pub fn set_active_unit(&mut self, unit: u32) -> bool {
        let unit = i32::try_from(unit).unwrap_or(i32::MAX);
        replace(&mut self.active_unit, unit)
    }

    /// `new_target` is 0 for a NULL texture. Units past the tracking array read as untracked and
    /// are not stored (C++ indexed out of bounds).
    pub fn set_texture(
        &mut self,
        unit: u32,
        new_target: u32,
        has_texture: bool,
        multitexture: bool,
    ) -> TexPlan {
        let mut plan = TexPlan {
            active_unit: -1,
            ..TexPlan::default()
        };
        let slot = usize::try_from(unit)
            .ok()
            .filter(|&u| u < MAX_TEXTURE_UNITS);
        let last = slot.and_then(|s| self.target.get(s)).copied().unwrap_or(0);
        let fixed = unit < FIXED_UNITS;
        let signed = i32::try_from(unit).unwrap_or(i32::MAX);
        if multitexture && self.active_unit != signed {
            plan.active_unit = signed;
            self.active_unit = signed;
        }
        if !has_texture {
            if last != 0 && fixed {
                plan.disable = last;
            }
        } else {
            if last != 0 && new_target != last && fixed {
                plan.disable = last;
            }
            plan.bind = true;
            if new_target != last && fixed {
                plan.enable = new_target;
            }
        }
        if let Some(t) = slot.and_then(|s| self.target.get_mut(s)) {
            *t = new_target;
        }
        plan
    }
}

impl Default for GlState {
    fn default() -> Self {
        Self::new()
    }
}

fn replace<T: PartialEq>(slot: &mut T, v: T) -> bool {
    if *slot == v {
        return false;
    }
    *slot = v;
    true
}

/// `SetFrameBufferDrawTargets`: attachment enums for the requested targets. Returns how many
/// were written (the buffer length caps the count).
pub fn draw_targets(targets: &[i32], out: &mut [u32]) -> usize {
    let mut n = 0;
    for (slot, t) in out.iter_mut().zip(targets) {
        *slot = c::COLOR_ATTACHMENT0_EXT.wrapping_add_signed(*t);
        n += 1;
    }
    n
}

/// `glScissor` rectangle for the target of `size` height `target_h`.
pub fn scissor_rect(target_h: i32, pos: Vector2l, size: Vector2l) -> [i32; 4] {
    [
        pos.x,
        target_h.wrapping_sub(pos.y).wrapping_sub(size.y),
        size.x,
        size.y,
    ]
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Viewport {
    pub total: Vector2l,
    pub size: Vector2l,
    pub pos: Vector2l,
    pub gl: [i32; 4],
}

/// `SetCurrentFrameBuffer` size and viewport arithmetic; `target` is the bound framebuffer size
/// or the screen size.
pub fn viewport(target: Vector2l, pos: Vector2l, size: Vector2l) -> Viewport {
    let size = if size.x < 0 || size.y < 0 {
        target
    } else {
        size
    };
    let gl_y = target.y.wrapping_sub(size.y).wrapping_sub(pos.y);
    Viewport {
        total: target,
        size,
        pos,
        gl: [pos.x, gl_y, size.x, size.y],
    }
}

fn default_size(size: Vector2l, fb: Vector2l) -> Vector2l {
    Vector2l {
        x: if size.x <= 0 { fb.x } else { size.x },
        y: if size.y <= 0 { fb.y } else { size.y },
    }
}

/// `CopyFrameBufferToTexure`: `[tex_x, tex_y, x, y, w, h]`.
pub fn copy_to_texture(
    fb_pos: Vector2l,
    fb_size: Vector2l,
    fb_total: Vector2l,
    pos: Vector2l,
    size: Vector2l,
    tex_offset: Vector2l,
    tex_height: i32,
) -> [i32; 6] {
    let size = default_size(size, fb_size);
    let y = fb_total
        .y
        .wrapping_sub(size.y)
        .wrapping_sub(fb_pos.y.wrapping_add(pos.y));
    let tex_y = tex_height.wrapping_sub(size.y).wrapping_sub(tex_offset.y);
    [
        tex_offset.x,
        tex_y,
        fb_pos.x.wrapping_add(pos.x),
        y,
        size.x,
        size.y,
    ]
}

/// `CopyFrameBufferToBitmap`: `[x, y, w, h]`.
pub fn read_rect(
    fb_pos: Vector2l,
    fb_size: Vector2l,
    fb_total: Vector2l,
    pos: Vector2l,
    size: Vector2l,
) -> [i32; 4] {
    let size = default_size(size, fb_size);
    let y = fb_total
        .y
        .wrapping_sub(size.y)
        .wrapping_sub(fb_pos.y.wrapping_add(pos.y));
    [fb_pos.x.wrapping_add(pos.x), y, size.x, size.y]
}
