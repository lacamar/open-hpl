//! `cMaterialManager::LoadFromFile`: the `.mat` XML is read here; material objects, texture
//! creation and material-type queries are host calls.

use super::h3_ops::{Create, Flow, LOG, Str, WStr, c, k, null, wide8};
use crate::resmgr::mat;
use crate::string;
use crate::xml::{DOC, Doc, NodeId};
use std::collections::BTreeMap;
use std::ffi::c_void;

/// `MAT_SET` codes (`Create::i[0]`).
pub mod set {
    pub const DEPTH_TEST: i32 = 0;
    pub const FORCE_FULL_SCALE: i32 = 1;
    pub const PHYSICS: i32 = 2;
    pub const UV_SUBDIVS: i32 = 3;
    pub const BLEND: i32 = 4;
}

/// `MAT_TYPE_Q` queries (`call` argument).
pub mod q {
    pub const TRANSLUCENT: i64 = 0;
    pub const USED_NUM: i64 = 1;
    pub const USED_BASE: i64 = 2;
}

const ILLUMINATION: i64 = 5;

fn attr<'a>(doc: &'a Doc, id: NodeId, name: &[u8]) -> Option<&'a [u8]> {
    doc.attribute(id, name)
}

fn attr_str(doc: &Doc, id: NodeId, name: &[u8]) -> Vec<u8> {
    attr(doc, id, name).unwrap_or_default().to_vec()
}

fn attr_bool(doc: &Doc, id: NodeId, name: &[u8], default: bool) -> bool {
    attr(doc, id, name).map_or(default, string::to_bool)
}

fn attr_float(doc: &Doc, id: NodeId, name: &[u8], default: f32) -> f32 {
    attr(doc, id, name).map_or(default, string::token_to_float)
}

impl Flow<'_> {
    /// `iXmlDocument::CreateFromFile` on a Rust document, with its failure log line.
    fn xml_load(&self, path: &[u32]) -> Option<Doc> {
        let bytes = self.ops.text(c::FILE_BYTES, null(), path);
        let (state, data) = bytes.split_first().map_or((0, &[][..]), |(s, d)| (*s, d));
        let mut doc = Doc::new();
        let loaded = state != 0 && doc.load(data, state == 2);
        let (row, col, desc) = if !loaded {
            let e = doc.error_info();
            (e.row, e.col, e.desc.clone().into_bytes())
        } else if doc.first_child_element(DOC, None).is_none() {
            (0, 0, b"No root element".to_vec())
        } else {
            return Some(doc);
        };
        let msg = [
            b"Failed parsing of XML document ".as_slice(),
            &wide8(path),
            b" in line ",
            row.to_string().as_bytes(),
            b", column ",
            col.to_string().as_bytes(),
            b": ",
            &desc,
            b"\n",
        ]
        .concat();
        self.ops.log(LOG, &msg);
        None
    }

    fn mat_set(&self, mat: *mut c_void, code: i32, a: i32, b: i32, text: &[u8]) {
        let mut cr = Create::new(k::MAT_SET);
        cr.ptr = mat;
        cr.i = [code, a, b, 0];
        cr.name = Str::of(text);
        self.ops.create(&mut cr);
    }

    /// `LoadFromFile`; null when the file or a required texture is bad.
    pub fn material_load(&mut self, name: &[u8], path: &[u32]) -> *mut c_void {
        let Some(doc) = self.xml_load(path) else {
            return null();
        };
        let Some(root) = doc.first_child_element(DOC, None) else {
            return null();
        };
        let Some(main) = doc.first_child_element(root, Some(b"Main")) else {
            self.ops.err(&[b"Main child not found.\n"]);
            return null();
        };
        let ty = attr_str(&doc, main, b"Type");
        if ty.is_empty() {
            self.ops.err(&[b"Type not found.\n"]);
            return null();
        }
        let depth_test = attr_bool(&doc, main, b"DepthTest", true);
        let physics = attr(&doc, main, b"PhysicsMaterial")
            .unwrap_or(b"Default")
            .to_vec();
        let blend = attr(&doc, main, b"BlendMode").unwrap_or(b"Add").to_vec();

        let mut cr = Create::new(k::MAT_NEW);
        cr.name = Str::of(name);
        cr.path = WStr::of(path);
        if self.m().mat_blank {
            cr.i[0] = 1;
            let mat = self.ops.create(&mut cr);
            self.mat_set(mat, set::PHYSICS, 0, 0, &physics);
            return mat;
        }
        let mut tq = Create::new(k::MAT_TYPE);
        tq.name = Str::of(&ty);
        let mat_type = self.ops.create(&mut tq);
        if mat_type.is_null() {
            self.ops.err(&[b"Invalid material type '", &ty, b"'!\n"]);
            return null();
        }
        cr.ptr = mat_type;
        let mat = self.ops.create(&mut cr);
        self.mat_set(mat, set::DEPTH_TEST, i32::from(depth_test), 0, b"");
        let full = attr_bool(&doc, main, b"ForceFullScaleTextures", false);
        self.mat_set(mat, set::FORCE_FULL_SCALE, i32::from(full), 0, b"");
        self.mat_set(mat, set::PHYSICS, 0, 0, &physics);
        let mut sub = [1.0f32; 2];
        if !attr(&doc, main, b"UVSubDivs").is_some_and(|v| string::floats_exact(v, &mut sub)) {
            sub = [1.0; 2];
        }
        self.mat_set(mat, set::UV_SUBDIVS, sub[0] as i32, sub[1] as i32, b"");
        if self.ops.call(c::MAT_TYPE_Q, mat_type, q::TRANSLUCENT) != 0 {
            let (mode, warn) = mat::blend_mode(&blend);
            if warn {
                self.ops.log(
                    super::h3_ops::WARNING,
                    &[
                        b"Material BlendMode '".as_slice(),
                        &blend,
                        b"' does not exist!\n",
                    ]
                    .concat(),
                );
            }
            self.mat_set(mat, set::BLEND, mode, 0, b"");
        }

        let Some(tex_root) = doc.first_child_element(root, Some(b"TextureUnits")) else {
            self.ops.err(&[b"TextureUnits child not found.\n"]);
            return null();
        };
        let used = self.ops.call(c::MAT_TYPE_Q, mat_type, q::USED_NUM);
        for i in 0..used {
            let used_type = self.ops.call(c::MAT_TYPE_Q, mat_type, q::USED_BASE + i);
            let tex_name = mat::texture_string(used_type as i32);
            let child = doc
                .first_child_element(tex_root, Some(&tex_name))
                .or_else(|| {
                    doc.first_child_element(
                        tex_root,
                        Some(&[tex_name.as_slice(), b"Side"].concat()),
                    )
                });
            let Some(child) = child else {
                continue;
            };
            let tex_type = mat::texture_type(&attr_str(&doc, child, b"Type"));
            let mut file = attr_str(&doc, child, b"File");
            let mips = attr_bool(&doc, child, b"MipMaps", true);
            let wrap = mat::wrap(&attr_str(&doc, child, b"Wrap"));
            let anim = mat::anim_mode(attr(&doc, child, b"AnimMode").unwrap_or(b"None"));
            let frame_time = attr_float(&doc, child, b"AnimFrameTime", 1.0);
            if file.is_empty() {
                continue;
            }
            file = mat::texture_file(&file, path);
            let kind = match mat::create_kind(anim, tex_type) {
                mat::Create::Anim => 0,
                mat::Create::Tex1D => 1,
                mat::Create::Tex2D => 2,
                mat::Create::Tex3D => 3,
                mat::Create::Cube => 4,
                mat::Create::None => -1,
            };
            let mut tc = Create::new(k::MAT_TEXTURE);
            tc.name = Str::of(&file);
            tc.i = [i32::from(mips), tex_type, kind, used_type as i32];
            let tex = self.ops.create(&mut tc);
            if tex.is_null() {
                if used_type == ILLUMINATION {
                    continue;
                }
                self.ops.destroy(mat);
                return null();
            }
            let mut ts = Create::new(k::MAT_TEX_SETUP);
            ts.ptr = tex;
            ts.ptr2 = mat;
            ts.i = [used_type as i32, anim, wrap, 0];
            ts.f[0] = frame_time;
            self.ops.create(&mut ts);
        }

        if let Some(uv) = doc.first_child_element(root, Some(b"UvAnimations")) {
            let mut e = doc.first_child_element(uv, None);
            while let Some(a) = e {
                let ty = attr_str(&doc, a, b"Type");
                let (ty_v, ty_err) = mat::uv_anim_type(Some(&ty));
                if ty_err {
                    self.ops.err(&[b"Invalid uv animation type ", &ty, b"\n"]);
                }
                let axis = attr_str(&doc, a, b"Axis");
                let (axis_v, axis_err) = mat::anim_axis(Some(&axis));
                if axis_err {
                    self.ops.err(&[b"Invalid animation axis ", &axis, b"\n"]);
                }
                let mut ua = Create::new(k::MAT_ADD_UV);
                ua.ptr = mat;
                ua.i = [ty_v, axis_v, 0, 0];
                ua.f = [
                    attr_float(&doc, a, b"Speed", 0.0),
                    attr_float(&doc, a, b"Amplitude", 0.0),
                    0.0,
                    0.0,
                ];
                self.ops.create(&mut ua);
                e = doc.next_sibling_element(a, None);
            }
        }

        let mut clear = Create::new(k::MAT_VAR);
        clear.i[0] = 1;
        self.ops.create(&mut clear);
        if let Some(vars) = doc.first_child_element(root, Some(b"SpecificVariables")) {
            let mut seen = BTreeMap::new();
            let mut e = doc.first_child_element(vars, None);
            while let Some(v) = e {
                seen.entry(attr_str(&doc, v, b"Name"))
                    .or_insert_with(|| attr_str(&doc, v, b"Value"));
                e = doc.next_sibling_element(v, None);
            }
            for (n, v) in &seen {
                let mut vc = Create::new(k::MAT_VAR);
                vc.name = Str::of(n);
                vc.name2 = Str::of(v);
                self.ops.create(&mut vc);
            }
        }
        let mut apply = Create::new(k::MAT_VARS_APPLY);
        apply.ptr = mat_type;
        apply.ptr2 = mat;
        self.ops.create(&mut apply);
        self.ops.call(c::MAT_COMPILE, mat, 0);
        mat
    }
}
