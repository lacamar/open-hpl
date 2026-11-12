//! A builder that records every call as a line of text, for the tests.

use super::{Array, Bone, Builder, Collider, Ctx, Pair, Str, VbCfg};
use crate::meshload::Key;
use std::fmt::Write;

#[derive(Default)]
pub struct Recorder {
    pub lines: Vec<String>,
    pub existing: Vec<String>,
}

fn rec<'a>(ctx: Ctx) -> &'a mut Recorder {
    // SAFETY: tests pass a live `Recorder` as the context.
    unsafe { &mut *ctx.cast::<Recorder>() }
}

fn s(v: Str) -> String {
    // SAFETY: strings come from the loader and live for the call.
    String::from_utf8_lossy(unsafe { v.bytes() }).into_owned()
}

fn hash(bytes: &[u8]) -> u64 {
    bytes.iter().fold(0xcbf2_9ce4_8422_2325u64, |h, &b| {
        (h ^ u64::from(b)).wrapping_mul(0x0100_0000_01b3)
    })
}

fn floats(p: *const f32, n: usize) -> String {
    // SAFETY: the loader passes `n` floats.
    let v = unsafe { std::slice::from_raw_parts(p, n) };
    v.iter().fold(String::new(), |mut o, f| {
        let _ = write!(o, "{:08x},", f.to_bits());
        o
    })
}

macro_rules! line {
    ($ctx:expr, $($arg:tt)*) => {
        rec($ctx).lines.push(format!($($arg)*))
    };
}

extern "C" fn log(ctx: Ctx, level: i32, m: Str) {
    line!(ctx, "log {level} {}", s(m).trim_end());
}
extern "C" fn file_exists(ctx: Ctx, p: Str) -> bool {
    rec(ctx).existing.contains(&s(p))
}
extern "C" fn file_date(_: Ctx, _: Str) -> i64 {
    0
}
extern "C" fn cache_file(_: Ctx, p: Str, e: Str) -> Str {
    let _ = (p, e);
    Str::NONE
}
extern "C" fn relative_dir(_: Ctx, _: Str) -> Str {
    Str::NONE
}
extern "C" fn save_mesh(ctx: Ctx, p: Str) -> bool {
    line!(ctx, "save_mesh {}", s(p));
    true
}
extern "C" fn save_anim(ctx: Ctx, p: Str) -> bool {
    line!(ctx, "save_anim {}", s(p));
    true
}
extern "C" fn mesh_new(ctx: Ctx, n: Str, p: Str) {
    line!(ctx, "mesh_new {} {}", s(n), s(p));
}
extern "C" fn mesh_set_path(ctx: Ctx, p: Str) {
    line!(ctx, "mesh_set_path {}", s(p));
}
extern "C" fn mesh_drop(ctx: Ctx) {
    line!(ctx, "mesh_drop");
}
extern "C" fn mesh_compile(ctx: Ctx) {
    line!(ctx, "mesh_compile");
}
extern "C" fn skeleton_begin(ctx: Ctx) {
    line!(ctx, "skeleton_begin");
}
extern "C" fn bone(ctx: Ctx, b: *const Bone) {
    // SAFETY: the loader passes a live bone.
    let b = unsafe { &*b };
    line!(
        ctx,
        "bone {} {} parent={} m={} unit={:?} inv={:?} value={}",
        s(b.name),
        s(b.sid),
        b.parent,
        floats(b.m.as_ptr(), 16),
        (b.has_unit_scale != 0).then_some(b.unit_scale.to_bits()),
        (b.has_inv_bind != 0).then(|| floats(b.inv_bind.as_ptr(), 16)),
        b.value
    );
}
extern "C" fn skeleton_end(ctx: Ctx, f: i32) {
    line!(ctx, "skeleton_end {f}");
}
extern "C" fn node(ctx: Ctx, parent: i32, n: Str, m: *const f32, flags: i32) {
    line!(ctx, "node {parent} {} {} {flags}", s(n), floats(m, 16));
}
extern "C" fn sub_select(ctx: Ctx, i: i32) {
    line!(ctx, "sub_select {i}");
}
extern "C" fn sub_new(ctx: Ctx, n: Str, m: *const f32, sc: *const f32, c: i32) {
    let m = if m.is_null() {
        String::from("-")
    } else {
        floats(m, 16)
    };
    let sc = if sc.is_null() {
        String::from("-")
    } else {
        floats(sc, 3)
    };
    line!(ctx, "sub_new {} {m} {sc} {c}", s(n));
}
extern "C" fn material_create(ctx: Ctx, n: Str) -> usize {
    line!(ctx, "material_create {}", s(n));
    1
}
extern "C" fn sub_material(ctx: Ctx, n: Str, m: usize) {
    line!(ctx, "sub_material {} {m}", s(n));
}
extern "C" fn sub_collider(ctx: Ctx, c: *const Collider) {
    // SAFETY: the loader passes a live collider.
    let c = unsafe { &*c };
    line!(
        ctx,
        "sub_collider kind={} char={} size={} m={}",
        c.kind,
        c.char_collider,
        floats(c.size.as_ptr(), 3),
        floats(c.m.as_ptr(), 16)
    );
}
extern "C" fn sub_pairs(ctx: Ctx, p: *const Pair, n: usize) {
    // SAFETY: the loader passes `n` pairs.
    let v = unsafe { std::slice::from_raw_parts(p, n) };
    let text: String = v
        .iter()
        .map(|p| format!("{}:{}:{:08x},", p.vtx, p.bone, p.weight.to_bits()))
        .collect();
    line!(ctx, "sub_pairs {n} {text}");
}
extern "C" fn sub_vb_new(ctx: Ctx, c: *const VbCfg) {
    // SAFETY: the loader passes a live config.
    let c = unsafe { &*c };
    line!(
        ctx,
        "sub_vb_new {} {} {} {}",
        c.draw_type,
        c.usage_type,
        c.reserve_vtx,
        c.reserve_idx
    );
}
extern "C" fn sub_vb_array(ctx: Ctx, a: *const Array) {
    // SAFETY: the loader passes a live array with `bytes` readable bytes.
    let (a, data) = unsafe { (&*a, std::slice::from_raw_parts((*a).data, (*a).bytes)) };
    line!(
        ctx,
        "sub_vb_array kind={} fmt={} prog={} elem={} repeats={} count={} data={:016x}",
        a.kind,
        a.format,
        a.prog_var,
        a.elem_num,
        a.repeats,
        a.count,
        hash(data)
    );
}
extern "C" fn sub_vb_indices(ctx: Ctx, p: *const u32, n: usize) {
    // SAFETY: the loader passes `n` indices.
    let v = unsafe { std::slice::from_raw_parts(p, n) };
    let bytes: Vec<u8> = v.iter().flat_map(|i| i.to_le_bytes()).collect();
    line!(ctx, "sub_vb_indices {n} {:016x}", hash(&bytes));
}
extern "C" fn sub_vb_transform(ctx: Ctx, m: *const f32) {
    line!(ctx, "sub_vb_transform {}", floats(m, 16));
}
extern "C" fn sub_vb_compile(ctx: Ctx) {
    line!(ctx, "sub_vb_compile");
}
extern "C" fn sub_vb_attach(ctx: Ctx) {
    line!(ctx, "sub_vb_attach");
}
extern "C" fn sub_compile(ctx: Ctx) {
    line!(ctx, "sub_compile");
}
extern "C" fn anim_new(ctx: Ctx, n: Str, f: Str, a: Str, len: f32, tracks: usize) {
    let a = if a.p.is_null() {
        String::from("-")
    } else {
        s(a)
    };
    line!(
        ctx,
        "anim_new {} {} {a} {:08x} {tracks}",
        s(n),
        s(f),
        len.to_bits()
    );
}
extern "C" fn anim_track(ctx: Ctx, n: Str, flags: i32, k: *const Key, c: usize) {
    // SAFETY: the loader passes `c` keys.
    let keys = unsafe { std::slice::from_raw_parts(k, c) };
    let text: String = keys
        .iter()
        .map(|k| {
            let all: Vec<u32> = std::iter::once(k.time)
                .chain(k.trans)
                .chain(k.rot)
                .map(f32::to_bits)
                .collect();
            format!("{all:08x?};")
        })
        .collect();
    line!(ctx, "anim_track {} {flags} {c} {text}", s(n));
}
extern "C" fn anim_unit_scale(ctx: Ctx, v: f32) {
    line!(ctx, "anim_unit_scale {:08x}", v.to_bits());
}
extern "C" fn anim_set_path(ctx: Ctx, p: Str) {
    line!(ctx, "anim_set_path {}", s(p));
}
extern "C" fn anim_end(ctx: Ctx, add: i32) {
    line!(ctx, "anim_end {add}");
}

pub fn builder(r: &mut Recorder) -> Builder {
    Builder {
        ctx: std::ptr::from_mut(r).cast(),
        log,
        file_exists,
        file_date,
        cache_file,
        relative_dir,
        save_mesh,
        save_anim,
        mesh_new,
        mesh_set_path,
        mesh_drop,
        mesh_compile,
        skeleton_begin,
        bone,
        skeleton_end,
        node,
        sub_select,
        sub_new,
        material_create,
        sub_material,
        sub_collider,
        sub_pairs,
        sub_vb_new,
        sub_vb_array,
        sub_vb_indices,
        sub_vb_transform,
        sub_vb_compile,
        sub_vb_attach,
        sub_compile,
        anim_new,
        anim_track,
        anim_unit_scale,
        anim_set_path,
        anim_end,
    }
}
