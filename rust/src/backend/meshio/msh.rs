//! `cMeshLoaderMSH::LoadMesh` / `LoadAnimation`: read, parse, then drive the builder.

use super::abi::{self, Array, Bone, Collider, Pair, Sink, VbCfg};
use crate::meshload::{self, Anim, Mesh, MshError, Reason};
use crate::string;
use std::ffi::OsStr;
use std::os::unix::ffi::OsStrExt;

pub const IDENTITY_SCALE: [f32; 3] = [1.0, 1.0, 1.0];

pub fn text(parts: &[&[u8]]) -> Vec<u8> {
    parts.concat()
}

/// `cBinaryBuffer::Load` with its log lines.
pub fn read_binary(sink: Sink, path: &[u8]) -> Option<Vec<u8>> {
    match std::fs::File::open(OsStr::from_bytes(path)) {
        Err(_) => {
            sink.log(
                abi::ERROR,
                &text(&[b"Could not open binary file '", path, b"'\n"]),
            );
            None
        }
        Ok(mut f) => {
            let mut data = Vec::new();
            let ok = std::io::Read::read_to_end(&mut f, &mut data).is_ok() && !data.is_empty();
            if !ok {
                sink.log(
                    abi::ERROR,
                    &text(&[b"Could not read from binary file '", path, b"'\n"]),
                );
                return None;
            }
            Some(data)
        }
    }
}

enum Parsed<T> {
    Ok(T),
    Failed,
}

fn parse_failure(sink: Sink, path: &[u8], e: MshError, anim: bool) {
    match e {
        MshError::BadMagic => sink.log(
            abi::ERROR,
            &text(&[
                b"File '",
                path,
                b"' does not have right MSH magic number! Invalid header!\n",
            ]),
        ),
        MshError::BadVersion => sink.log(
            if anim { abi::WARNING } else { abi::ERROR },
            &text(&[b"File '", path, b"' does not have right MSH version!\n"]),
        ),
        MshError::Unsupported(reason) => {
            let why = Reason::ALL
                .iter()
                .find(|(r, _)| *r == reason)
                .map_or(c"unsupported", |(_, t)| t);
            sink.log(
                abi::ERROR,
                &text(&[
                    b"File '",
                    path,
                    b"' has MSH data that cannot be loaded: ",
                    why.to_bytes(),
                    b"\n",
                ]),
            );
        }
    }
}

fn parse_mesh(sink: Sink, path: &[u8], cfg: &abi::Cfg) -> Parsed<Mesh> {
    let Some(data) = read_binary(sink, path) else {
        sink.log(
            abi::ERROR,
            &text(&[b"Could not load file '", path, b"' in MSH loader."]),
        );
        return Parsed::Failed;
    };
    match meshload::parse_mesh(&data, cfg.bone_unit_scale != 0, cfg.keep_hpl3_anims != 0) {
        Ok(m) => Parsed::Ok(m),
        Err(e) => {
            parse_failure(sink, path, e, false);
            Parsed::Failed
        }
    }
}

pub fn emit_anim(sink: Sink, anim: &Anim, path: &[u8], add_to_mesh: bool) {
    sink.anim_new(
        path,
        string::file_name(path),
        None,
        anim.length,
        anim.tracks.len(),
    );
    for t in &anim.tracks {
        sink.anim_track(&t.name, i32::from(t.flags), &t.keys);
    }
    sink.anim_end(add_to_mesh);
}

fn emit_bones(sink: Sink, mesh: &Mesh) {
    let Some(bones) = &mesh.bones else { return };
    sink.skeleton_begin();
    for b in bones {
        sink.bone(&Bone {
            name: abi::Str::new(&b.name),
            sid: abi::Str::new(&b.sid),
            m: b.matrix,
            inv_bind: [0.0; 16],
            unit_scale: b.unit_scale,
            parent: b.parent,
            value: 0,
            has_unit_scale: 1,
            has_inv_bind: 0,
        });
    }
    sink.skeleton_end(mesh.hpl3);
}

fn emit_sub(sink: Sink, cfg: &abi::Cfg, path: &[u8], hpl3: bool, s: &meshload::SubMesh) {
    sink.sub_new(
        &s.name,
        Some(&s.matrix),
        Some(if hpl3 { &IDENTITY_SCALE } else { &s.scale }),
        Some(s.collide_shape),
    );
    let mut material = 0;
    if !s.material.is_empty() {
        // SAFETY: the config strings live for the call.
        let fast = unsafe { cfg.fastload_material.bytes() };
        let load = if cfg.use_fastload != 0 && !fast.is_empty() {
            fast
        } else {
            &s.material
        };
        material = sink.material_create(load);
    }
    sink.sub_material(&s.material, material);
    for c in &s.colliders {
        sink.sub_collider(&Collider {
            m: c.matrix,
            size: c.size,
            kind: i32::from(c.kind),
            char_collider: i32::from(c.char_collider),
        });
    }
    if !s.bone_pairs.is_empty() {
        let pairs: Vec<Pair> = s
            .bone_pairs
            .iter()
            .map(|p| Pair {
                vtx: p.vtx,
                bone: p.bone,
                weight: p.weight,
            })
            .collect();
        sink.sub_pairs(&pairs);
    }
    sink.sub_vb_new(&VbCfg {
        draw_type: 0,
        usage_type: 0,
        reserve_vtx: 0,
        reserve_idx: 0,
    });
    for k in &s.skipped {
        sink.log(
            abi::WARNING,
            &text(&[
                b"Skipping HPL3 vertex array ",
                k.to_string().as_bytes(),
                b" in '",
                path,
                b"'\n",
            ]),
        );
    }
    for a in &s.arrays {
        sink.sub_vb_array(&Array {
            kind: a.kind,
            format: a.format,
            prog_var: a.prog_var,
            elem_num: a.elem_num,
            repeats: a.repeats,
            count: a.count,
            data: a.data.as_ptr(),
            bytes: a.data.len(),
        });
    }
    sink.sub_vb_indices(&s.indices);
    sink.sub_vb_compile();
    sink.sub_vb_attach();
    sink.sub_compile();
}

pub fn load_mesh(cfg: &abi::Cfg, sink: Sink, path: &[u8]) -> i32 {
    let Parsed::Ok(mesh) = parse_mesh(sink, path, cfg) else {
        return 1;
    };
    sink.mesh_new(string::file_name(path), path);
    emit_bones(sink, &mesh);
    for n in &mesh.nodes {
        sink.node(n.parent, &n.name, &n.matrix, n.flags);
    }
    for s in &mesh.submeshes {
        emit_sub(sink, cfg, path, mesh.hpl3, s);
    }
    sink.mesh_compile();
    for a in &mesh.animations {
        emit_anim(sink, a, path, true);
    }
    0
}

pub fn load_anim(sink: Sink, path: &[u8]) -> i32 {
    let Some(data) = read_binary(sink, path) else {
        sink.log(
            abi::ERROR,
            &text(&[b"Could not load file '", path, b"' in MSH loader."]),
        );
        return 1;
    };
    match meshload::parse_animation(&data) {
        Ok(a) => {
            emit_anim(sink, &a, path, false);
            0
        }
        Err(e) => {
            parse_failure(sink, path, e, true);
            1
        }
    }
}
