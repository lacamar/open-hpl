//! `cGuiGfxElement`: animation stepping and texture-coordinate selection. The element, its
//! images, textures and animation list stay in C++; this works on plain state.

use crate::math::rand_rectl;

pub const ANIM_LOOP: i32 = 0;
pub const ANIM_STOP_AT_END: i32 = 1;
pub const ANIM_OSCILLATE: i32 = 2;
pub const ANIM_RANDOM: i32 = 3;

/// The `cGuiGfxElement` playback members.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct AnimState {
    pub current_frame: f32,
    pub active_image: i32,
    pub forward: bool,
}

impl Default for AnimState {
    fn default() -> Self {
        Self {
            current_frame: 0.0,
            active_image: 0,
            forward: true,
        }
    }
}

/// What `Update` reads besides the playback state.
#[derive(Clone, Copy, Debug)]
pub struct AnimInput {
    pub animation_count: usize,
    pub current_animation: i32,
    pub paused: bool,
    pub ty: i32,
    pub frame_length: f32,
    pub buffer_len: usize,
    pub time_step: f32,
}

/// `cGuiGfxElement::Update`. Returns the buffered image to show when the active one changed.
///
/// UB in C++ not reproduced: a negative current animation, and an image index outside the
/// buffer (empty buffer with stop-at-end, a one-image oscillation, negative time steps) leave the
/// state updated but select no image. Out-of-range float to int casts saturate.
pub fn update(state: &mut AnimState, input: &AnimInput) -> Option<usize> {
    if input.animation_count == 0 || input.paused {
        return None;
    }
    let current = usize::try_from(input.current_animation).ok()?;
    if current >= input.animation_count {
        return None;
    }
    let len = i32::try_from(input.buffer_len).unwrap_or(i32::MAX);
    let advance = 1.0f32 / input.frame_length;
    let mut frame = 0i32;
    if input.ty != ANIM_RANDOM {
        state.current_frame = input.time_step.mul_add(advance, state.current_frame);
        frame = state.current_frame as i32;
        if frame >= len {
            match input.ty {
                ANIM_STOP_AT_END => {
                    frame = len.wrapping_sub(1);
                    state.current_frame = frame as f32;
                }
                ANIM_LOOP => {
                    frame = 0;
                    state.current_frame = 0.0;
                }
                ANIM_OSCILLATE => {
                    frame = 1;
                    state.current_frame = 1.0;
                    state.forward = !state.forward;
                }
                _ => {}
            }
        }
        if !state.forward && input.ty == ANIM_OSCILLATE {
            frame = len.wrapping_sub(1).wrapping_sub(frame);
        }
    } else if len > 1 {
        let prev = state.current_frame;
        state.current_frame = input.time_step.mul_add(advance, state.current_frame);
        frame = state.current_frame as i32;
        if frame != prev as i32 {
            let last = prev as i32;
            loop {
                frame = rand_rectl(0, len.wrapping_sub(1));
                if frame != last {
                    break;
                }
            }
            state.current_frame = frame as f32;
        }
    }
    if frame == state.active_image {
        return None;
    }
    state.active_image = frame;
    usize::try_from(frame)
        .ok()
        .filter(|&i| i < input.buffer_len)
}

/// `cGuiGfxElement::PlayAnimation` after the same-animation early out: back to the first image.
pub fn restart(state: &mut AnimState) {
    *state = AnimState::default();
}

/// `cGuiGfxElement::SetAnimationTime`; `frame_length` is the current animation's, if it has one.
pub fn animation_time(time: f32, frame_length: Option<f32>) -> f32 {
    frame_length.map_or(time, |len| time / len)
}

pub type Tex = [[f32; 2]; 4];

/// `cGuiGfxElement::AddTexture` for the first texture: fills `tex` and returns the image size.
/// Rectangle textures use pixel coordinates.
pub fn texture_uv(
    tex: &mut Tex,
    size: [f32; 2],
    rect: bool,
    start: [f32; 2],
    end: [f32; 2],
) -> [f32; 2] {
    let image_size = [size[0] * (end[0] - start[0]), size[1] * (end[1] - start[1])];
    let (s, e) = if rect {
        (
            [start[0] * size[0], start[1] * size[1]],
            [end[0] * size[0], end[1] * size[1]],
        )
    } else {
        (start, end)
    };
    *tex = [[s[0], s[1]], [e[0], s[1]], [e[0], e[1]], [s[0], e[1]]];
    image_size
}

pub enum FlipSource<'a> {
    /// Texture coordinates of the element's first image.
    Image(&'a Tex),
    Texture,
    Nothing,
}

/// `cGuiGfxElement::SetFlipUvYAxis`. The texture path swaps even when `flip` is false, and the
/// image path's last two assignments are no-ops; both kept.
pub fn set_flip_uv_y(tex: &mut Tex, flip: bool, source: &FlipSource) {
    match source {
        FlipSource::Image(image) => {
            *tex = **image;
            if flip {
                tex[0][1] = tex[2][1];
                tex[1][1] = tex[3][1];
                tex[2][1] = tex[0][1];
                tex[3][1] = tex[1][1];
            }
        }
        FlipSource::Texture => {
            let [y0, y1, y2, y3] = [tex[0][1], tex[1][1], tex[2][1], tex[3][1]];
            if flip {
                tex[2][1] = y1;
                tex[3][1] = y0;
                tex[0][1] = y3;
                tex[1][1] = y2;
            } else {
                tex[0][1] = y3;
                tex[1][1] = y2;
                tex[2][1] = y1;
                tex[3][1] = y0;
            }
        }
        FlipSource::Nothing => {}
    }
}

/// `cGuiGfxElement::Flush`: whether the main image's update count moved; remembers it.
pub fn flush_changed(last_count: &mut i32, count: i32) -> bool {
    let changed = *last_count != count;
    if changed {
        *last_count = count;
    }
    changed
}
