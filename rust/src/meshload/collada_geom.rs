//! Collada geometry: `LoadGeometry`, `LoadVertexData`, `FillVertexVec`, `SplitVertices` and
//! the tangent pass.

use super::collada::{
    Ctx, ERROR, Fail, Geometry, Res, Vertex, VtxArray, WARNING, addr, cat, floats, get, ints, sub,
};
use crate::string;
use crate::xml::NodeId;
use std::cell::Cell;

type Vec3 = [f32; 3];

#[derive(Clone, Copy)]
struct Idx {
    vtx: i32,
    norm: i32,
    tex: i32,
}

enum Next {
    Cont,
    Stop,
}

const MAX_VERTICES: i64 = 1 << 27;

unsafe extern "C" {
    fn ohpl_math_create_tri_tangent_vectors(
        dest: *mut f32,
        indices: *const u32,
        index_count: i32,
        vertices: *const f32,
        stride: i32,
        tex: *const f32,
        normals: *const f32,
        vertex_count: i32,
    );
}

fn z_up(v: Vec3) -> Vec3 {
    let [x, y, z] = v;
    [-x, z, y]
}

impl Ctx<'_> {
    pub(super) fn load_geometry(&mut self, lib: NodeId) -> Res<()> {
        let mut e = self.el(lib, "geometry");
        while let Some(g) = e {
            let Some(mesh) = self.el(g, "mesh") else {
                e = self.nx(g, "geometry");
                continue;
            };
            let mut geom = Geometry::default();
            let mut data = Vec::new();
            let r = self.geometry(g, mesh, &mut geom, &mut data);
            for (a, d) in geom.arrays.iter_mut().zip(data) {
                a.data = d;
            }
            self.out.geometries.push(geom);
            if matches!(r?, Next::Stop) {
                return Ok(());
            }
            e = self.nx(g, "geometry");
        }
        Ok(())
    }

    fn geometry(
        &mut self,
        g: NodeId,
        mesh: NodeId,
        geom: &mut Geometry,
        data: &mut Vec<Vec<Vec3>>,
    ) -> Res<Next> {
        geom.id = self.sa(g, "id");
        geom.name = self.sa(g, "name");
        if geom.name.is_empty() {
            geom.name = geom.id.clone();
            let pos = string::last_pos(&geom.name, b"-");
            if pos > 0 {
                geom.name = sub(&geom.name, 0, pos).to_vec();
            }
        }
        let n = &geom.name;
        if sub(n, 0, 4) == b"_ref"
            || sub(n, 0, 3) == b"_bb"
            || sub(n, 0, 4) == b"_ps"
            || sub(n, 0, 6) == b"_sound"
        {
            return Ok(Next::Cont);
        }

        let mut src = self.el(mesh, "source");
        while let Some(s) = src {
            geom.arrays.push(VtxArray {
                id: self.sa(s, "id"),
                ..VtxArray::default()
            });
            let mut v = Vec::new();
            self.vertex_data(s, &mut v)?;
            data.push(v);
            src = self.nx(s, "source");
        }

        let Some(verts) = self.el(mesh, "vertices") else {
            self.log(ERROR, b"Vertices not found!\n".to_vec());
            return Ok(Next::Stop);
        };
        let mut input = self.el(verts, "input");
        while let Some(i) = input {
            let semantic = self.sa(i, "semantic");
            let mut source = self.sa(i, "source");
            addr(&mut source);
            let found = geom.arrays.iter_mut().find(|a| a.id == source);
            if semantic == b"POSITION" {
                if let Some(a) = found {
                    a.id = self.sa(verts, "id");
                }
            } else if let Some(a) = found {
                a.kind = semantic;
                a.in_vertex = true;
            }
            input = self.nx(i, "input");
        }

        let mut tri = self.el(mesh, "triangles");
        let (kind, multi) = if tri.is_none() {
            tri = self.el(mesh, "polylist");
            (&b"polylist"[..], tri)
        } else {
            (&b"triangles"[..], tri)
        };
        if let Some(t) = multi
            && self.sibling_with_value(t, kind)
        {
            let msg = cat(&[
                b"Geometry '",
                &geom.name,
                b"' seem to have multitexturing!\n",
            ]);
            self.log(WARNING, msg);
        }
        let Some(tri) = tri.or_else(|| self.el(mesh, "polygons")) else {
            self.log(ERROR, b"No Polygons found!\n".to_vec());
            return Ok(Next::Stop);
        };

        let tri_count = self.ia(tri, "count", 0);
        geom.material = self.sa(tri, "material");
        addr(&mut geom.material);

        let mut tri_elements = 0i32;
        let mut input = self.el(tri, "input");
        while let Some(i) = input {
            let semantic = self.sa(i, "semantic");
            let mut source = self.sa(i, "source");
            let mut idx = self.ia(i, "idx", -1);
            if idx < 0 {
                idx = self.ia(i, "offset", -1);
            }
            addr(&mut source);
            let arr = geom
                .arrays
                .iter()
                .position(|a| a.id == source)
                .map_or(-1, |p| p as i32);
            match semantic.as_slice() {
                b"VERTEX" => (geom.pos_idx_num, geom.pos_array_idx) = (idx, arr),
                b"NORMAL" => (geom.norm_idx_num, geom.norm_array_idx) = (idx, arr),
                b"TEXCOORD" => (geom.tex_idx_num, geom.tex_array_idx) = (idx, arr),
                _ => {}
            }
            tri_elements = tri_elements.max(idx.wrapping_add(1));
            input = self.nx(i, "input");
        }

        for (i, a) in geom.arrays.iter().enumerate() {
            if a.in_vertex {
                if a.kind == b"NORMAL" {
                    geom.norm_array_idx = i as i32;
                    geom.norm_idx_num = geom.pos_idx_num;
                } else if a.kind == b"TEXCOORD" {
                    geom.tex_array_idx = i as i32;
                    geom.tex_idx_num = geom.pos_idx_num;
                }
            }
        }

        if geom.norm_array_idx < 0 {
            let msg = cat(&[b"No normals for geometry '", &geom.name, b"'\n"]);
            self.log(WARNING, msg);
            return Ok(Next::Cont);
        }
        if geom.tex_array_idx < 0 && geom.name.first() != Some(&b'_') {
            let msg = cat(&[b"No tex coords for geometry '", &geom.name, b"'\n"]);
            self.log(WARNING, msg);
            return Ok(Next::Cont);
        }

        let pos_arr = usize::try_from(geom.pos_array_idx).ok();
        let norm_arr = usize::try_from(geom.norm_array_idx).ok();
        if self.out.z_to_y {
            for arr in [pos_arr, norm_arr] {
                let v = arr
                    .and_then(|a| data.get_mut(a))
                    .ok_or(Fail("geometry without position array"))?;
                v.iter_mut().for_each(|p| *p = z_up(*p));
            }
        }

        let mut indices: Vec<Idx> = Vec::new();
        let mut p = self.el(tri, "p");
        while let Some(pe) = p {
            let Some(text) = self.text(pe)? else {
                self.log(ERROR, b"No tri data found!\n".to_vec());
                return Ok(Next::Stop);
            };
            let ia = ints(text);
            let len = ia.len() as u64;
            if tri_count > 0 && self.nx(pe, "p").is_none() {
                let d = 3 * u64::from(tri_count.unsigned_abs());
                if len.checked_rem(d) == Some(0)
                    && len.checked_div(d).unwrap_or(0) > tri_elements as u64
                {
                    tri_elements = len.checked_div(d).unwrap_or(0) as i32;
                }
            }
            let per = 3 * i64::from(tri_elements);
            let tri_num = (ia.len() as i64).checked_div(per).unwrap_or(0); // aarch64 sdiv by zero yields 0, as in the original build
            for t in 0..tri_num {
                let add = t * per;
                for i in 0..3 {
                    let base = add + i * i64::from(tri_elements);
                    let at = |off: i32| get(&ia, base + i64::from(off), "index outside <p> data");
                    indices.push(Idx {
                        vtx: at(geom.pos_idx_num)?,
                        norm: at(geom.norm_idx_num)?,
                        tex: if geom.tex_idx_num >= 0 {
                            at(geom.tex_idx_num)?
                        } else {
                            0
                        },
                    });
                }
            }
            p = self.nx(pe, "p");
        }

        let pos_arr = pos_arr.ok_or(Fail("geometry without position array"))?;
        if split_vertices(geom, data, pos_arr, &indices)? > 0 {
            let msg = cat(&[
                b"Geometry '",
                &geom.name,
                b"' has normal or texcoord indices outside their source array, using zero\n",
            ]);
            self.log(WARNING, msg);
        }
        data.iter_mut().for_each(Vec::clear);
        tangents(geom)?;
        Ok(Next::Cont)
    }

    fn sibling_with_value(&self, n: NodeId, value: &[u8]) -> bool {
        std::iter::successors(self.d.node(n).next, |&c| self.d.node(c).next)
            .any(|c| crate::xml::c_str(&self.d.node(c).value) == value)
    }

    fn vertex_data(&mut self, src: NodeId, out: &mut Vec<Vec3>) -> Res<()> {
        let Some(tech) = self
            .el(src, "technique_common")
            .or_else(|| self.el(src, "technique"))
        else {
            self.log(
                WARNING,
                b"No technique or technique_common element found!\n".to_vec(),
            );
            return Ok(());
        };
        let Some(acc) = self.el(tech, "accessor") else {
            self.log(
                WARNING,
                b"No accessor element for source data found!\n".to_vec(),
            );
            return Ok(());
        };
        let elements = self.ia(acc, "stride", 0);
        let count = self.ia(acc, "count", 0);
        let Some(data) = self
            .el(src, "float_array")
            .or_else(|| self.el(src, "array"))
        else {
            self.log(WARNING, b"No data found!\n".to_vec());
            return Ok(());
        };
        let Some(text) = self.text(data)? else {
            self.log(WARNING, b"No text found!\n".to_vec());
            return Ok(());
        };
        self.fill_vertex_vec(text, out, elements, count)
    }

    fn fill_vertex_vec(
        &mut self,
        chars: &[u8],
        out: &mut Vec<Vec3>,
        elements: i32,
        count: i32,
    ) -> Res<()> {
        if i64::from(count) > MAX_VERTICES {
            return Err(Fail("vertex count allocates gigabytes"));
        }
        if (out.len() as i64) < i64::from(count) {
            out.resize(count as usize, [0.0; 3]);
        }
        let f = floats(chars);
        let loaded = (f.len() as i32).checked_div(elements).unwrap_or(0); // aarch64 sdiv by zero yields 0, as in the original build
        if loaded != count {
            let msg = cat(&[
                b"Vertex array in does not have correct number of values stored. ",
                (f.len() as i32).to_string().as_bytes(),
                b" instead of ",
                elements.wrapping_mul(count).to_string().as_bytes(),
                b"\n",
            ]);
            self.log(WARNING, msg);
        }
        for i in 0..count {
            if i >= loaded {
                break;
            }
            let base = i64::from(i) * i64::from(elements);
            let [x, y, z] = out.get_mut(i as usize).ok_or(Fail("vertex slot"))?;
            *x = get(&f, base, "vertex data")?;
            if elements > 1 {
                *y = get(&f, base + 1, "vertex data")?;
                if elements > 2 {
                    *z = get(&f, base + 2, "vertex data")?;
                }
            }
        }
        Ok(())
    }
}

/// A normal or texcoord index past its source array reads heap garbage in the original (ASan:
/// heap-buffer-overflow, seen in shipped assets); here it reads as zero and is counted.
fn vertex_of(
    g: &Geometry,
    data: &[Vec<Vec3>],
    pos_arr: usize,
    d: Idx,
    oob: &Cell<u32>,
) -> Res<Vertex> {
    let arr = |a: i32, i: i32| -> Res<Vec3> {
        let v = usize::try_from(a).ok().and_then(|a| data.get(a));
        get(
            v.ok_or(Fail("array index"))?,
            i64::from(i),
            "vertex index outside source array",
        )
    };
    let lenient = |a: i32, i: i32| -> Res<Vec3> {
        arr(a, i).or_else(|e| {
            if a < 0 || usize::try_from(a).is_ok_and(|a| a >= data.len()) {
                return Err(e);
            }
            oob.set(oob.get() + 1);
            Ok([0.0; 3])
        })
    };
    let mut v = Vertex {
        col: [1.0; 4],
        ..Vertex::default()
    };
    v.pos = arr(pos_arr as i32, d.vtx)?;
    if g.norm_array_idx >= 0 {
        v.norm = lenient(g.norm_array_idx, d.norm)?;
    }
    if g.tex_array_idx >= 0 {
        v.tex = lenient(g.tex_array_idx, d.tex)?;
    }
    v.tex[1] = 1.0 - v.tex[1];
    Ok(v)
}

fn split_vertices(g: &mut Geometry, data: &[Vec<Vec3>], pos_arr: usize, idx: &[Idx]) -> Res<u32> {
    let oob = Cell::new(0);
    let size = data.get(pos_arr).map_or(0, Vec::len);
    let mut extra: Vec<Vec<(i32, i32, i32, u32)>> = vec![Vec::new(); size];
    let mut vertices = vec![
        Vertex {
            col: [0.0, 0.0, 0.0, 1.0],
            ..Vertex::default()
        };
        size
    ];
    let mut out = vec![0u32; idx.len()];
    for (d, o) in idx.iter().zip(out.iter_mut()) {
        let slot = usize::try_from(d.vtx)
            .ok()
            .and_then(|v| extra.get_mut(v))
            .ok_or(Fail("position index outside source array"))?;
        let key = (d.vtx, d.norm, d.tex);
        let found = slot.iter().find(|e| (e.0, e.1, e.2) == key).map(|e| e.3);
        let vertex = |g: &Geometry| vertex_of(g, data, pos_arr, *d, &oob);
        *o = if slot.is_empty() {
            slot.push((key.0, key.1, key.2, d.vtx as u32));
            *vertices
                .get_mut(d.vtx as usize)
                .ok_or(Fail("vertex slot"))? = vertex(g)?;
            d.vtx as u32
        } else if let Some(t) = found {
            t
        } else {
            let n = vertices.len() as u32;
            slot.push((key.0, key.1, key.2, n));
            vertices.push(vertex(g)?);
            n
        };
    }
    g.vertices = vertices;
    g.indices = out;
    Ok(oob.get())
}

fn tangents(g: &mut Geometry) -> Res<()> {
    let n = g.vertices.len();
    let mut pos = Vec::with_capacity(n * 4);
    let mut norm = Vec::with_capacity(n * 3);
    let mut tex = Vec::with_capacity(n * 3);
    for v in &g.vertices {
        pos.extend_from_slice(&[v.pos[0], v.pos[1], v.pos[2], 1.0]);
        norm.extend_from_slice(&v.norm);
        tex.extend_from_slice(&v.tex);
    }
    g.tangents = vec![0.0; n * 4];
    // SAFETY: `pos`, `norm`, `tex` and `tangents` hold `n` vertices of stride 4/3/3/4 floats and
    // `indices` holds only values below `n` (split_vertices drew them from `vertices`).
    unsafe {
        ohpl_math_create_tri_tangent_vectors(
            g.tangents.as_mut_ptr(),
            g.indices.as_ptr(),
            g.indices.len() as i32,
            pos.as_ptr(),
            4,
            tex.as_ptr(),
            norm.as_ptr(),
            n as i32,
        );
    }
    Ok(())
}
