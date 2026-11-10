//! `iRenderFunctions` state-change dedup: the cached GPU state and the decision of which backend
//! call each setter makes. The backend (`Gpu`) is the C++ `iLowLevelGraphics` behind callbacks.

use crate::types::{Vector2f, Vector2l};

pub const TEXTURE_UNITS: usize = 16;

const CULL_CLOCKWISE: u32 = 0;
const CULL_COUNTER_CLOCKWISE: u32 = 1;
const DEPTH_FUNC_LESS_OR_EQUAL: u32 = 2;
const CHANNEL_RGBA: u32 = 0;
const CHANNEL_RGB: u32 = 1;
const CHANNEL_A: u32 = 2;
const CHANNEL_NONE: u32 = 3;
const ALPHA_SOLID: u32 = 0;
const ALPHA_LAST: u32 = 2;
const BLEND_NONE: u32 = 0;
const BLEND_ADD: u32 = 1;
const BLEND_MUL: u32 = 2;
const BLEND_MULX2: u32 = 3;
const BLEND_ALPHA: u32 = 4;
const BLEND_PREMUL_ALPHA: u32 = 5;
const BLEND_MODULATE_SOURCE: u32 = 6;
const BLEND_LAST: u32 = 7;

const BLEND_FUNC_ZERO: u32 = 0;
const BLEND_FUNC_ONE: u32 = 1;
const BLEND_FUNC_SRC_COLOR: u32 = 2;
const BLEND_FUNC_DEST_COLOR: u32 = 4;
const BLEND_FUNC_SRC_ALPHA: u32 = 6;
const BLEND_FUNC_ONE_MINUS_SRC_ALPHA: u32 = 7;
const BLEND_FUNC_DEST_ALPHA: u32 = 8;

/// Mirrors the state members of `iRenderFunctions` (`sRenderFunctionsState`). Pointers are
/// opaque handles compared by address. Layout is asserted in `RustRenderFunctions.h`.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct State {
    pub projection: usize,
    pub render_target_size: Vector2l,
    pub current_frame_buffer_size: Vector2l,
    pub screen_size: Vector2l,
    pub screen_size_float: Vector2f,
    pub log: bool,
    pub invert_cull: bool,
    pub use_global_scissor: bool,
    pub global_scissor_active: bool,
    pub global_scissor_pos: Vector2l,
    pub global_scissor_size: Vector2l,
    pub depth_test: bool,
    pub depth_write: bool,
    pub stencil_active: bool,
    pub scissor_pos: Vector2l,
    pub scissor_size: Vector2l,
    pub scissor_active: bool,
    pub depth_func: u32,
    pub cull_active: bool,
    pub cull_mode: u32,
    pub channel_mode: u32,
    pub alpha_mode: u32,
    pub alpha_limit: f32,
    pub blend_mode: u32,
    pub textures: [usize; TEXTURE_UNITS],
    pub program: usize,
    pub vtx_buffer: usize,
    pub matrix: usize,
    pub material: usize,
    pub material_type: usize,
}

impl Default for State {
    fn default() -> Self {
        Self {
            projection: 0,
            render_target_size: Vector2l::default(),
            current_frame_buffer_size: Vector2l::default(),
            screen_size: Vector2l::default(),
            screen_size_float: Vector2f::default(),
            log: false,
            invert_cull: false,
            use_global_scissor: false,
            global_scissor_active: false,
            global_scissor_pos: Vector2l::default(),
            global_scissor_size: Vector2l::default(),
            depth_test: true,
            depth_write: true,
            stencil_active: false,
            scissor_pos: Vector2l::default(),
            scissor_size: v2(-1, -1),
            scissor_active: false,
            depth_func: DEPTH_FUNC_LESS_OR_EQUAL,
            cull_active: true,
            cull_mode: CULL_COUNTER_CLOCKWISE,
            channel_mode: CHANNEL_RGBA,
            alpha_mode: ALPHA_LAST,
            alpha_limit: -1.0,
            blend_mode: BLEND_LAST,
            textures: [0; TEXTURE_UNITS],
            program: 0,
            vtx_buffer: 0,
            matrix: 0,
            material: 0,
            material_type: 0,
        }
    }
}

const fn v2(x: i32, y: i32) -> Vector2l {
    Vector2l { x, y }
}

fn add(a: Vector2l, b: Vector2l) -> Vector2l {
    v2(a.x.wrapping_add(b.x), a.y.wrapping_add(b.y))
}

/// Debug-log events; `a..d` and `f` carry the values the C++ formats.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
#[repr(i32)]
pub enum Log {
    Projection = 0,
    DepthTest,
    DepthWrite,
    DepthFunc,
    CullActive,
    CullMode,
    StencilActive,
    ScissorActive,
    ScissorRect,
    ChannelMode,
    AlphaMode,
    AlphaLimit,
    BlendMode,
    Program,
    ProgramNull,
    Texture,
    VertexBuffer,
    VertexBufferNull,
    ModelMatrix,
    ModelMatrixNull,
    InvertCull,
    FrameBufferPos,
    FrameBufferDefault,
    ClearPortionGlobal,
    ClearPortionTarget,
    ClearAll,
}

pub trait Gpu {
    fn depth_test(&mut self, on: bool);
    fn depth_write(&mut self, on: bool);
    fn depth_func(&mut self, func: u32);
    fn cull_active(&mut self, on: bool);
    fn cull_mode(&mut self, mode: u32);
    fn stencil_active(&mut self, on: bool);
    fn scissor_active(&mut self, on: bool);
    fn scissor_rect(&mut self, pos: Vector2l, size: Vector2l);
    fn color_write(&mut self, r: bool, g: bool, b: bool, a: bool);
    fn alpha_test_active(&mut self, on: bool);
    fn alpha_test_greater_or_equal(&mut self, limit: f32);
    fn blend_active(&mut self, on: bool);
    fn blend_func(&mut self, src: u32, dst: u32);
    fn program_bind(&mut self, program: usize);
    fn program_unbind(&mut self, program: usize);
    fn texture(&mut self, unit: i32, texture: usize);
    fn vertex_buffer_unbind(&mut self, vb: usize);
    fn vertex_buffer_bind(&mut self, vb: usize);
    fn set_projection(&mut self, matrix: usize);
    fn set_model_matrix(&mut self, matrix: usize);
    fn set_frame_buffer(&mut self, fb: usize, use_pos_size: bool, pos: Vector2l, size: Vector2l);
    fn frame_buffer_size(&mut self, fb: usize) -> Vector2l;
    fn clear_frame_buffer(&mut self, flags: u32);
    fn log(&mut self, event: Log, a: i64, b: i64, c: i64, d: i64, f: f32);
}

impl State {
    /// `InitAndResetRenderFunctions` after the C++ resolved the frame buffer size.
    #[expect(clippy::too_many_arguments, reason = "mirrors the C++ call")]
    pub fn init(
        &mut self,
        null_matrix: usize,
        log: bool,
        use_global_scissor: bool,
        global_pos: Vector2l,
        global_size: Vector2l,
        screen: Vector2l,
        screen_float: Vector2f,
        frame_buffer_size: Vector2l,
        target_size: Vector2l,
    ) {
        let (screen_size, screen_size_float, pos, size) =
            (screen, screen_float, global_pos, global_size);
        *self = Self {
            log,
            screen_size,
            screen_size_float,
            matrix: null_matrix,
            render_target_size: v2(
                if target_size.x < 0 {
                    frame_buffer_size.x
                } else {
                    target_size.x
                },
                if target_size.y < 0 {
                    frame_buffer_size.y
                } else {
                    target_size.y
                },
            ),
            current_frame_buffer_size: frame_buffer_size,
            use_global_scissor,
            global_scissor_pos: pos,
            global_scissor_size: size,
            ..Self::default()
        };
    }

    pub fn exit(&mut self, gpu: &mut impl Gpu) {
        if self.use_global_scissor {
            self.use_global_scissor = false;
            if self.log {
                gpu.log(Log::ScissorActive, 0, 0, 0, 0, 0.0);
            }
            gpu.scissor_active(false);
        }
    }

    /// After the C++ set an ortho projection and identity model view.
    pub fn flat_projection_set(&mut self, gpu: &mut impl Gpu, null_matrix: usize) {
        self.matrix = null_matrix;
        self.projection = 0;
        self.set_invert_cull(gpu, false);
    }

    pub fn set_projection_matrix(&mut self, gpu: &mut impl Gpu, null_matrix: usize, matrix: usize) {
        if self.projection == matrix {
            return;
        }
        if self.log {
            gpu.log(Log::Projection, matrix as i64, 0, 0, 0, 0.0);
        }
        gpu.set_projection(matrix);
        self.matrix = null_matrix;
        self.projection = matrix;
    }

    pub fn set_depth_test(&mut self, gpu: &mut impl Gpu, on: bool) -> bool {
        if self.depth_test == on {
            return false;
        }
        if self.log {
            gpu.log(Log::DepthTest, on.into(), 0, 0, 0, 0.0);
        }
        gpu.depth_test(on);
        self.depth_test = on;
        true
    }

    pub fn set_depth_write(&mut self, gpu: &mut impl Gpu, on: bool) -> bool {
        if self.depth_write == on {
            return false;
        }
        if self.log {
            gpu.log(Log::DepthWrite, on.into(), 0, 0, 0, 0.0);
        }
        gpu.depth_write(on);
        self.depth_write = on;
        true
    }

    pub fn set_depth_test_func(&mut self, gpu: &mut impl Gpu, func: u32) -> bool {
        if self.depth_func == func {
            return false;
        }
        if self.log {
            gpu.log(Log::DepthFunc, i64::from(func), 0, 0, 0, 0.0);
        }
        self.depth_func = func;
        gpu.depth_func(func);
        true
    }

    pub fn set_cull_active(&mut self, gpu: &mut impl Gpu, on: bool) -> bool {
        if self.cull_active == on {
            return false;
        }
        if self.log {
            gpu.log(Log::CullActive, on.into(), 0, 0, 0, 0.0);
        }
        self.cull_active = on;
        gpu.cull_active(on);
        true
    }

    pub fn set_cull_mode(
        &mut self,
        gpu: &mut impl Gpu,
        mut mode: u32,
        check_inverted: bool,
    ) -> bool {
        if check_inverted && self.invert_cull {
            mode = if mode == CULL_CLOCKWISE {
                CULL_COUNTER_CLOCKWISE
            } else {
                CULL_CLOCKWISE
            };
        }
        if self.cull_mode == mode {
            return false;
        }
        if self.log {
            gpu.log(Log::CullMode, i64::from(mode), 0, 0, 0, 0.0);
        }
        self.cull_mode = mode;
        gpu.cull_mode(mode);
        true
    }

    pub fn set_stencil_active(&mut self, gpu: &mut impl Gpu, on: bool) -> bool {
        if self.stencil_active == on {
            return false;
        }
        if self.log {
            gpu.log(Log::StencilActive, on.into(), 0, 0, 0, 0.0);
        }
        gpu.stencil_active(on);
        self.stencil_active = on;
        true
    }

    pub fn set_scissor_active(
        &mut self,
        gpu: &mut impl Gpu,
        on: bool,
        target_pos: Vector2l,
    ) -> bool {
        if self.scissor_active == on {
            return false;
        }
        if self.use_global_scissor {
            if self.scissor_active {
                if self.scissor_size.x > -1 {
                    self.set_scissor_rect(
                        gpu,
                        self.scissor_pos,
                        self.scissor_size,
                        false,
                        target_pos,
                    );
                }
            } else {
                self.set_scissor_rect(
                    gpu,
                    self.global_scissor_pos,
                    self.global_scissor_size,
                    false,
                    target_pos,
                );
            }
            return false;
        }
        if self.log {
            gpu.log(Log::ScissorActive, on.into(), 0, 0, 0, 0.0);
        }
        gpu.scissor_active(on);
        self.scissor_active = on;
        true
    }

    pub fn set_scissor_rect(
        &mut self,
        gpu: &mut impl Gpu,
        pos: Vector2l,
        size: Vector2l,
        auto_enable: bool,
        target_pos: Vector2l,
    ) -> bool {
        let mut final_pos = add(pos, target_pos);
        let mut final_size = size;
        let min = if self.use_global_scissor {
            self.global_scissor_pos
        } else {
            target_pos
        };
        let max = if self.use_global_scissor {
            add(self.global_scissor_size, min)
        } else {
            add(self.render_target_size, min)
        };
        if final_pos.x < min.x {
            final_pos.x = min.x;
        }
        if final_pos.y < min.y {
            final_pos.y = min.y;
        }
        if final_pos.x.wrapping_add(final_size.x) > max.x {
            final_size.x = max.x.wrapping_sub(final_pos.x);
        }
        if final_pos.y.wrapping_add(final_size.y) > max.y {
            final_size.y = max.y.wrapping_sub(final_pos.y);
        }
        if self.scissor_pos == final_pos && self.scissor_size == final_size {
            return false;
        }
        self.scissor_pos = final_pos;
        self.scissor_size = final_size;
        if !self.use_global_scissor
            && final_pos.x == 0
            && final_pos.y == 0
            && final_size.x == self.render_target_size.x
            && final_size.y == self.render_target_size.y
        {
            if auto_enable {
                self.set_scissor_active(gpu, false, target_pos);
            }
            false
        } else {
            if auto_enable {
                self.set_scissor_active(gpu, true, target_pos);
            }
            if self.log {
                gpu.log(
                    Log::ScissorRect,
                    final_pos.x.into(),
                    final_pos.y.into(),
                    final_size.x.into(),
                    final_size.y.into(),
                    0.0,
                );
            }
            gpu.scissor_rect(self.scissor_pos, self.scissor_size);
            true
        }
    }

    pub fn set_channel_mode(&mut self, gpu: &mut impl Gpu, mode: u32) -> bool {
        if self.channel_mode == mode {
            return false;
        }
        let write = match mode {
            CHANNEL_RGBA => Some((true, true, true, true)),
            CHANNEL_RGB => Some((true, true, true, false)),
            CHANNEL_A => Some((false, false, false, true)),
            CHANNEL_NONE => Some((false, false, false, false)),
            _ => None,
        };
        if let Some((r, g, b, a)) = write {
            if self.log {
                gpu.log(Log::ChannelMode, i64::from(mode), 0, 0, 0, 0.0);
            }
            gpu.color_write(r, g, b, a);
        }
        self.channel_mode = mode;
        true
    }

    pub fn set_alpha_mode(&mut self, gpu: &mut impl Gpu, mode: u32) -> bool {
        if self.alpha_mode == mode {
            return false;
        }
        if self.log {
            gpu.log(Log::AlphaMode, i64::from(mode == ALPHA_SOLID), 0, 0, 0, 0.0);
        }
        gpu.alpha_test_active(mode != ALPHA_SOLID);
        self.alpha_mode = mode;
        true
    }

    pub fn set_alpha_limit(&mut self, gpu: &mut impl Gpu, limit: f32) -> bool {
        if limit == self.alpha_limit {
            return false;
        }
        self.alpha_limit = limit;
        if self.log {
            gpu.log(Log::AlphaLimit, 0, 0, 0, 0, limit);
        }
        gpu.alpha_test_greater_or_equal(limit);
        true
    }

    pub fn set_blend_mode(&mut self, gpu: &mut impl Gpu, mode: u32) -> bool {
        if self.blend_mode == mode {
            return false;
        }
        if mode == BLEND_NONE {
            if self.log {
                gpu.log(Log::BlendMode, i64::from(mode), 0, 0, 0, 0.0);
            }
            gpu.blend_active(false);
        } else {
            gpu.blend_active(true);
            let func = match mode {
                BLEND_ADD => Some((BLEND_FUNC_ONE, BLEND_FUNC_ONE)),
                BLEND_MUL => Some((BLEND_FUNC_ZERO, BLEND_FUNC_SRC_COLOR)),
                BLEND_MULX2 => Some((BLEND_FUNC_DEST_COLOR, BLEND_FUNC_SRC_COLOR)),
                BLEND_ALPHA => Some((BLEND_FUNC_SRC_ALPHA, BLEND_FUNC_ONE_MINUS_SRC_ALPHA)),
                BLEND_PREMUL_ALPHA => Some((BLEND_FUNC_ONE, BLEND_FUNC_ONE_MINUS_SRC_ALPHA)),
                BLEND_MODULATE_SOURCE => Some((BLEND_FUNC_DEST_ALPHA, BLEND_FUNC_ONE)),
                _ => None,
            };
            if let Some((src, dst)) = func {
                if self.log {
                    gpu.log(Log::BlendMode, i64::from(mode), 0, 0, 0, 0.0);
                }
                gpu.blend_func(src, dst);
            }
        }
        self.blend_mode = mode;
        true
    }

    pub fn set_program(&mut self, gpu: &mut impl Gpu, program: usize) -> bool {
        if self.program == program {
            return false;
        }
        if program != 0 {
            if self.log {
                gpu.log(Log::Program, program as i64, 0, 0, 0, 0.0);
            }
            gpu.program_bind(program);
        } else {
            if self.log {
                gpu.log(Log::ProgramNull, 0, 0, 0, 0, 0.0);
            }
            gpu.program_unbind(self.program);
        }
        self.program = program;
        self.material_type = 0;
        self.material = 0;
        true
    }

    fn texture_log(&self, gpu: &mut impl Gpu, unit: i32, texture: usize) {
        if self.log {
            gpu.log(Log::Texture, i64::from(unit), texture as i64, 0, 0, 0.0);
        }
    }

    pub fn set_texture(&mut self, gpu: &mut impl Gpu, unit: i32, texture: usize) {
        let Some(slot) = usize::try_from(unit)
            .ok()
            .and_then(|u| self.textures.get_mut(u))
        else {
            return;
        };
        if *slot == texture {
            return;
        }
        *slot = texture;
        self.texture_log(gpu, unit, texture);
        gpu.texture(unit, texture);
    }

    pub fn set_texture_range(&mut self, gpu: &mut impl Gpu, texture: usize, first: i32, last: i32) {
        for unit in first..=last {
            let Some(slot) = usize::try_from(unit)
                .ok()
                .and_then(|u| self.textures.get_mut(u))
            else {
                continue;
            };
            if *slot != texture {
                *slot = texture;
                self.texture_log(gpu, unit, texture);
                gpu.texture(unit, texture);
            }
        }
    }

    pub fn set_vertex_buffer(&mut self, gpu: &mut impl Gpu, vb: usize) {
        if self.vtx_buffer == vb {
            return;
        }
        if self.log {
            let event = if vb != 0 {
                Log::VertexBuffer
            } else {
                Log::VertexBufferNull
            };
            gpu.log(event, vb as i64, 0, 0, 0, 0.0);
        }
        if self.vtx_buffer != 0 {
            gpu.vertex_buffer_unbind(self.vtx_buffer);
        }
        if vb != 0 {
            gpu.vertex_buffer_bind(vb);
        }
        self.vtx_buffer = vb;
    }

    pub fn set_matrix(&mut self, gpu: &mut impl Gpu, matrix: usize) {
        if self.matrix == matrix {
            return;
        }
        if self.log {
            let event = if matrix != 0 {
                Log::ModelMatrix
            } else {
                Log::ModelMatrixNull
            };
            gpu.log(event, matrix as i64, 0, 0, 0, 0.0);
        }
        gpu.set_model_matrix(matrix);
        self.matrix = matrix;
    }

    pub fn set_invert_cull(&mut self, gpu: &mut impl Gpu, on: bool) {
        if self.invert_cull == on {
            return;
        }
        self.invert_cull = on;
        if self.log {
            gpu.log(Log::InvertCull, 0, 0, 0, 0, 0.0);
        }
        let flipped = if self.cull_mode == CULL_CLOCKWISE {
            CULL_COUNTER_CLOCKWISE
        } else {
            CULL_CLOCKWISE
        };
        self.set_cull_mode(gpu, flipped, false);
    }

    pub fn set_frame_buffer(
        &mut self,
        gpu: &mut impl Gpu,
        fb: usize,
        use_pos_size: bool,
        use_global_scissor: bool,
        target_pos: Vector2l,
    ) {
        if use_pos_size {
            if self.log {
                gpu.log(
                    Log::FrameBufferPos,
                    fb as i64,
                    target_pos.x.into(),
                    target_pos.y.into(),
                    0,
                    0.0,
                );
            }
            gpu.set_frame_buffer(fb, true, target_pos, self.render_target_size);
        } else {
            if self.log {
                gpu.log(Log::FrameBufferDefault, fb as i64, 0, 0, 0, 0.0);
            }
            gpu.set_frame_buffer(fb, false, v2(0, 0), v2(-1, -1));
        }

        let prev = self.current_frame_buffer_size;
        self.current_frame_buffer_size = if fb != 0 {
            gpu.frame_buffer_size(fb)
        } else {
            self.screen_size
        };

        if !self.use_global_scissor {
            return;
        }
        if use_global_scissor {
            if !self.global_scissor_active {
                if self.log {
                    gpu.log(Log::ScissorActive, 1, 0, 0, 0, 0.0);
                }
                gpu.scissor_active(true);
            }
            if !self.global_scissor_active || self.current_frame_buffer_size != prev {
                if self.log {
                    gpu.log(
                        Log::ScissorRect,
                        self.global_scissor_pos.x.into(),
                        self.global_scissor_pos.y.into(),
                        self.global_scissor_size.x.into(),
                        self.global_scissor_size.y.into(),
                        0.0,
                    );
                }
                gpu.scissor_rect(self.global_scissor_pos, self.global_scissor_size);
            }
            self.global_scissor_active = true;
        } else if self.global_scissor_active {
            if self.log {
                gpu.log(Log::ScissorActive, 0, 0, 0, 0, 0.0);
            }
            gpu.scissor_active(false);
            self.global_scissor_active = false;
        }
    }

    pub fn clear_frame_buffer(
        &mut self,
        gpu: &mut impl Gpu,
        flags: u32,
        use_pos_size: bool,
        target_pos: Vector2l,
    ) {
        let mut scissor_used = false;
        if use_pos_size
            && (target_pos != v2(0, 0)
                || self.render_target_size != self.current_frame_buffer_size
                || self.use_global_scissor)
        {
            if self.use_global_scissor {
                if self.log {
                    gpu.log(
                        Log::ClearPortionGlobal,
                        self.global_scissor_pos.x.into(),
                        self.global_scissor_pos.y.into(),
                        self.global_scissor_size.x.into(),
                        self.global_scissor_size.y.into(),
                        0.0,
                    );
                }
            } else {
                if self.log {
                    gpu.log(
                        Log::ClearPortionTarget,
                        target_pos.x.into(),
                        target_pos.y.into(),
                        self.render_target_size.x.into(),
                        self.render_target_size.y.into(),
                        0.0,
                    );
                }
                gpu.scissor_active(true);
                gpu.scissor_rect(target_pos, self.render_target_size);
                scissor_used = true;
            }
        } else if self.log {
            gpu.log(Log::ClearAll, 0, 0, 0, 0, 0.0);
        }
        gpu.clear_frame_buffer(flags);
        if scissor_used {
            gpu.scissor_active(false);
        }
    }
}
