//! `DoWindowStart` geometry of `SomaImGui.cpp`.

fn fmax(a: f32, b: f32) -> f32 {
    if a < b { b } else { a }
}

const ALIGN_RIGHT: i32 = 1;
const ALIGN_CENTER: i32 = 2;

/// Script-owned window struct values the layout reads; paddings are top, right, bottom, left.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct WindowIn {
    pub pos: [f32; 3],
    pub size: [f32; 2],
    pub use_header: bool,
    pub header_type: i32,
    pub label_offset: [f32; 3],
    pub label_pad: [f32; 4],
    pub label_min_width: f32,
    pub font_size: [f32; 2],
    pub caption_length: f32,
    pub font_align: i32,
    pub pad: [f32; 4],
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct WindowOut {
    pub has_header: bool,
    pub header_pos: [f32; 3],
    pub header_size: [f32; 2],
    pub text_pos: [f32; 3],
    pub group_pos: [f32; 3],
    pub group_size: [f32; 2],
}

pub fn window(i: &WindowIn) -> WindowOut {
    let mut o = WindowOut::default();
    let mut header = 0.0f32;
    if i.use_header {
        let [pad_t, pad_r, pad_b, pad_l] = i.label_pad;
        let off = i.label_offset;
        let font = i.font_size;
        let pos = [i.pos[0] + off[0], i.pos[1] + off[1], i.pos[2] + 0.1];
        let mut size = [font[0], pad_t + pad_b + font[1]];
        if i.header_type == 1 {
            size[0] = fmax(i.caption_length + pad_r + pad_l, i.label_min_width);
        } else if i.header_type == 0 {
            size[0] = (-2.0f32).mul_add(off[0], i.size[0]);
        }
        let mut text = [pos[0] + pad_l, pos[1] + pad_t, pos[2]];
        let free = size[0] - free_inner(off[0], pad_r, pad_l);
        if free > 0.0 {
            text[1] += (size[1] - (pad_t + pad_b + off[1]) - font[1]) * 0.5;
            text[2] += 0.2;
            if i.font_align == ALIGN_RIGHT {
                text[0] += free;
            } else if i.font_align == ALIGN_CENTER {
                text[0] += free * 0.5;
            }
        }
        o.has_header = true;
        o.header_pos = pos;
        o.header_size = size;
        o.text_pos = text;
        header = size[1];
    }
    let [top, right, bottom, left] = i.pad;
    o.group_pos = [i.pos[0] + left, i.pos[1] + (header + top), i.pos[2] + 0.3];
    o.group_size = [
        i.size[0] - (left + right),
        i.size[1] - (top + bottom + header),
    ];
    o
}

fn free_inner(off_x: f32, pad_r: f32, pad_l: f32) -> f32 {
    2.0f32.mul_add(off_x, pad_r) + pad_l
}
