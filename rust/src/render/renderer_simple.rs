//! `cRendererSimple::RenderObjects` and `cRendererWireFrame::RenderObjects`: the exact sequence
//! of render-state and draw calls, as ops the C++ shim replays against `iRenderFunctions`.
//!
//! Objects are named by `(list, index)`; the shim resolves materials, matrices and vertex buffers.

pub mod list {
    pub const Z: i32 = 0;
    pub const DIFFUSE: i32 = 1;
    pub const DECAL: i32 = 2;
    pub const TRANSLUCENT: i32 = 3;
}

pub mod kind {
    pub const DEPTH_FUNC: u32 = 0;
    pub const DEPTH_TEST: u32 = 1;
    pub const DEPTH_WRITE: u32 = 2;
    pub const BLEND_NONE: u32 = 3;
    pub const ALPHA_MODE: u32 = 4;
    pub const CHANNEL_MODE: u32 = 5;
    pub const TEXTURE_RANGE_NONE: u32 = 6;
    pub const PROGRAM: u32 = 7;
    pub const OBJ_TEXTURE: u32 = 8;
    pub const OBJ_MATRIX: u32 = 9;
    pub const OBJ_VERTEX_BUFFER: u32 = 10;
    pub const OBJ_BLEND: u32 = 11;
    pub const OBJ_UPDATE_GRAPHICS: u32 = 12;
    pub const OBJ_MATRIX_FRUSTUM: u32 = 13;
    pub const DRAW: u32 = 14;
    pub const CALLBACK: u32 = 15;
}

pub mod arg {
    pub const FUNC_LESS_OR_EQUAL: i32 = 0;
    pub const FUNC_EQUAL: i32 = 1;
    pub const ALPHA_SOLID: i32 = 0;
    pub const ALPHA_TRANS: i32 = 1;
    pub const CHANNEL_NONE: i32 = 0;
    pub const CHANNEL_RGBA: i32 = 1;
    pub const PROGRAM_FLAT: i32 = 0;
    pub const PROGRAM_DIFFUSE: i32 = 1;
    pub const TEXTURE_ALPHA: i32 = 0;
    pub const TEXTURE_DIFFUSE: i32 = 1;
    pub const DRAW_DEFAULT: i32 = 0;
    pub const DRAW_LINE_STRIP: i32 = 1;
    pub const MESSAGE_POST_SOLID: i32 = 0;
    pub const MESSAGE_POST_TRANSLUCENT: i32 = 1;
}

/// `a` is the first argument, `list`/`index` name the object for `OBJ_*` ops.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct Op {
    pub kind: u32,
    pub a: i32,
    pub list: i32,
    pub index: u32,
}

fn op(kind: u32, a: i32) -> Op {
    Op {
        kind,
        a,
        list: 0,
        index: 0,
    }
}

fn obj(kind: u32, a: i32, list: i32, index: usize) -> Op {
    Op {
        kind,
        a,
        list,
        index: index as u32,
    }
}

/// Sizes of the four render lists and which Z-list objects have an alpha texture.
pub struct Lists<'a> {
    pub z_has_alpha: &'a [bool],
    pub diffuse: usize,
    pub decal: usize,
    pub translucent: usize,
}

/// `cRendererSimple::RenderObjects`.
pub fn simple_ops(l: &Lists, use_shaders: bool) -> Vec<Op> {
    use arg::*;
    use kind::*;
    let mut v = vec![
        op(DEPTH_FUNC, FUNC_LESS_OR_EQUAL),
        op(DEPTH_TEST, 1),
        op(DEPTH_WRITE, 1),
        op(BLEND_NONE, 0),
        op(ALPHA_MODE, ALPHA_TRANS),
        op(CHANNEL_MODE, CHANNEL_NONE),
        op(TEXTURE_RANGE_NONE, 0),
    ];
    for (i, &alpha) in l.z_has_alpha.iter().enumerate() {
        if use_shaders {
            v.push(op(
                PROGRAM,
                if alpha { PROGRAM_DIFFUSE } else { PROGRAM_FLAT },
            ));
        }
        v.push(obj(OBJ_TEXTURE, TEXTURE_ALPHA, list::Z, i));
        v.push(obj(OBJ_MATRIX, 0, list::Z, i));
        v.push(obj(OBJ_VERTEX_BUFFER, 0, list::Z, i));
        v.push(op(DRAW, DRAW_DEFAULT));
    }

    v.push(op(DEPTH_FUNC, FUNC_EQUAL));
    v.push(op(DEPTH_WRITE, 0));
    v.push(op(CHANNEL_MODE, CHANNEL_RGBA));
    v.push(op(ALPHA_MODE, ALPHA_SOLID));
    v.push(op(TEXTURE_RANGE_NONE, 0));
    if use_shaders {
        v.push(op(PROGRAM, PROGRAM_DIFFUSE));
    }
    for i in 0..l.diffuse {
        v.push(obj(OBJ_TEXTURE, TEXTURE_DIFFUSE, list::DIFFUSE, i));
        v.push(obj(OBJ_MATRIX, 0, list::DIFFUSE, i));
        v.push(obj(OBJ_VERTEX_BUFFER, 0, list::DIFFUSE, i));
        v.push(op(DRAW, DRAW_DEFAULT));
    }

    v.push(op(DEPTH_FUNC, FUNC_LESS_OR_EQUAL));
    v.push(op(DEPTH_WRITE, 0));
    if use_shaders {
        v.push(op(PROGRAM, PROGRAM_DIFFUSE));
    }
    for i in 0..l.decal {
        v.push(obj(OBJ_BLEND, 0, list::DECAL, i));
        v.push(obj(OBJ_TEXTURE, TEXTURE_DIFFUSE, list::DECAL, i));
        v.push(obj(OBJ_MATRIX, 0, list::DECAL, i));
        v.push(obj(OBJ_VERTEX_BUFFER, 0, list::DECAL, i));
        v.push(op(DRAW, DRAW_DEFAULT));
    }
    v.push(op(CALLBACK, MESSAGE_POST_SOLID));

    v.push(op(DEPTH_FUNC, FUNC_LESS_OR_EQUAL));
    v.push(op(DEPTH_WRITE, 0));
    if use_shaders {
        v.push(op(PROGRAM, PROGRAM_DIFFUSE));
    }
    translucent(&mut v, l.translucent, DRAW_DEFAULT);
    v.push(op(CALLBACK, MESSAGE_POST_TRANSLUCENT));
    v
}

fn translucent(v: &mut Vec<Op>, count: usize, draw: i32) {
    use arg::*;
    use kind::*;
    for i in 0..count {
        v.push(obj(OBJ_UPDATE_GRAPHICS, 0, list::TRANSLUCENT, i));
        v.push(obj(OBJ_BLEND, 0, list::TRANSLUCENT, i));
        v.push(obj(OBJ_TEXTURE, TEXTURE_DIFFUSE, list::TRANSLUCENT, i));
        v.push(obj(OBJ_MATRIX_FRUSTUM, 0, list::TRANSLUCENT, i));
        v.push(obj(OBJ_VERTEX_BUFFER, 0, list::TRANSLUCENT, i));
        v.push(op(DRAW, draw));
    }
}

/// `cRendererWireFrame::RenderObjects`.
pub fn wireframe_ops(l: &Lists) -> Vec<Op> {
    use arg::*;
    use kind::*;
    let mut v = vec![
        op(DEPTH_TEST, 1),
        op(DEPTH_WRITE, 1),
        op(BLEND_NONE, 0),
        op(ALPHA_MODE, ALPHA_SOLID),
        op(CHANNEL_MODE, CHANNEL_RGBA),
        op(TEXTURE_RANGE_NONE, 0),
    ];
    for i in 0..l.diffuse {
        v.push(obj(OBJ_TEXTURE, TEXTURE_DIFFUSE, list::DIFFUSE, i));
        v.push(obj(OBJ_MATRIX, 0, list::DIFFUSE, i));
        v.push(obj(OBJ_VERTEX_BUFFER, 0, list::DIFFUSE, i));
        v.push(op(DRAW, DRAW_LINE_STRIP));
    }

    v.push(op(DEPTH_WRITE, 0));
    for i in 0..l.decal {
        v.push(obj(OBJ_BLEND, 0, list::DECAL, i));
        v.push(obj(OBJ_TEXTURE, TEXTURE_DIFFUSE, list::DECAL, i));
        v.push(obj(OBJ_MATRIX, 0, list::DECAL, i));
        v.push(obj(OBJ_VERTEX_BUFFER, 0, list::DECAL, i));
        v.push(op(DRAW, DRAW_LINE_STRIP));
    }
    v.push(op(CALLBACK, MESSAGE_POST_SOLID));

    v.push(op(DEPTH_WRITE, 0));
    translucent(&mut v, l.translucent, DRAW_LINE_STRIP);
    v.push(op(CALLBACK, MESSAGE_POST_TRANSLUCENT));
    v
}

/// # Safety
/// `z_has_alpha` points at `z` bools; `out` has room for the returned count (query with
/// `out` null first).
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_rn_simple_ops(
    z_has_alpha: *const bool,
    z: usize,
    diffuse: usize,
    decal: usize,
    translucent: usize,
    use_shaders: bool,
    wireframe: bool,
    out: *mut Op,
    cap: usize,
) -> usize {
    let alpha: &[bool] = if z_has_alpha.is_null() || z == 0 {
        &[]
    } else {
        // SAFETY: caller contract.
        unsafe { std::slice::from_raw_parts(z_has_alpha, z) }
    };
    let lists = Lists {
        z_has_alpha: alpha,
        diffuse,
        decal,
        translucent,
    };
    let ops = if wireframe {
        wireframe_ops(&lists)
    } else {
        simple_ops(&lists, use_shaders)
    };
    if !out.is_null() {
        for (i, o) in ops.iter().enumerate().take(cap) {
            // SAFETY: caller contract, `i < cap`.
            unsafe { out.add(i).write(*o) };
        }
    }
    ops.len()
}

const _: () = assert!(std::mem::size_of::<Op>() == 16);

#[cfg(test)]
mod tests {
    use super::*;

    fn lists(z: &[bool]) -> Lists<'_> {
        Lists {
            z_has_alpha: z,
            diffuse: 2,
            decal: 1,
            translucent: 1,
        }
    }

    #[test]
    fn simple_prepass_picks_program_by_alpha_texture() {
        let ops = simple_ops(&lists(&[true, false]), true);
        let programs: Vec<i32> = ops
            .iter()
            .take_while(|o| !(o.kind == kind::DEPTH_FUNC && o.a == arg::FUNC_EQUAL))
            .filter(|o| o.kind == kind::PROGRAM)
            .map(|o| o.a)
            .collect();
        assert_eq!(programs, vec![arg::PROGRAM_DIFFUSE, arg::PROGRAM_FLAT]);
        let no_shaders = simple_ops(&lists(&[true, false]), false);
        assert!(no_shaders.iter().all(|o| o.kind != kind::PROGRAM));
    }

    #[test]
    fn passes_run_in_order_with_callbacks_between() {
        let ops = simple_ops(&lists(&[]), true);
        let cbs: Vec<(usize, i32)> = ops
            .iter()
            .enumerate()
            .filter(|(_, o)| o.kind == kind::CALLBACK)
            .map(|(i, o)| (i, o.a))
            .collect();
        assert_eq!(cbs.len(), 2);
        assert_eq!(cbs[0].1, arg::MESSAGE_POST_SOLID);
        assert_eq!(cbs[1].1, arg::MESSAGE_POST_TRANSLUCENT);
        assert_eq!(ops.last().map(|o| o.kind), Some(kind::CALLBACK));
        let draws = ops.iter().filter(|o| o.kind == kind::DRAW).count();
        assert_eq!(draws, 2 + 1 + 1);
    }

    #[test]
    fn wireframe_draws_line_strips_and_skips_z_pass() {
        let ops = wireframe_ops(&lists(&[true, true, true]));
        assert!(
            ops.iter()
                .filter(|o| o.kind == kind::DRAW)
                .all(|o| o.a == arg::DRAW_LINE_STRIP)
        );
        assert!(
            ops.iter()
                .filter(|o| (kind::OBJ_TEXTURE..=kind::OBJ_MATRIX_FRUSTUM).contains(&o.kind))
                .all(|o| o.list != list::Z)
        );
        assert!(ops.iter().all(|o| o.kind != kind::PROGRAM));
    }
}
