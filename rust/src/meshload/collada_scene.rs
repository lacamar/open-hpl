//! Collada scene tree, animations and skin controllers (`LoadColladaScene`, `LoadAnimations`,
//! `LoadControllers` and the scene header of `FillStructures`).

use super::collada::{
    AnimSource, Animation, Bytes, Channel, Controller, Ctx, ERROR, Fail, Node, Res, Sampler,
    Transform, WARNING, addr, cat, flat16, floats, get, identity, ints, lower, mat_from, mul,
    rotation_matrix, scale_matrix, sub, translate_matrix,
};
use crate::string;
use crate::types::Vector3f;
use crate::xml::{Kind, NodeId};

const MAX_RESERVE: i32 = 1 << 28;

fn v3(v: &[f32], z_to_y: bool) -> Res<Vector3f> {
    let at = |i| get(v, i, "transform has too few values");
    let (x, y, z) = (at(0)?, at(1)?, at(2)?);
    Ok(if z_to_y {
        Vector3f { x, y: z, z: y }
    } else {
        Vector3f { x, y, z }
    })
}

impl Ctx<'_> {
    pub(super) fn load_scene(&mut self, root: NodeId) -> Res<()> {
        let mut scene = self.el(root, "library_visual_scenes");
        if let Some(s) = scene {
            scene = self.el(s, "visual_scene");
            if scene.is_none() {
                self.log(WARNING, b"No visual scene element found!\n".to_vec());
            }
        }
        let Some(scene) = scene.or_else(|| self.el(root, "scene")) else {
            self.log(WARNING, b"No scene element found!\n".to_vec());
            return Ok(());
        };

        if let Some(extra) = self.el(scene, "extra") {
            let mut tech = self.el(extra, "technique");
            while let Some(t) = tech {
                let profile = self.sa(t, "profile");
                if profile == b"MAYA" || profile == b"FCOLLADA" {
                    let mut param = self.any(t);
                    while let Some(p) = param {
                        let mut name = self.sa(p, "name");
                        if name.is_empty() {
                            name = self.value(p).to_vec();
                        }
                        let value = string::token_to_float(self.text_req(p)?);
                        match lower(&name).as_slice() {
                            b"start_time" => self.out.scene.start = value,
                            b"end_time" => self.out.scene.end = value,
                            _ => {}
                        }
                        param = self.nx_any(p);
                    }
                }
                self.out.scene.delta = self.out.scene.end - self.out.scene.start;
                tech = self.nx(t, "technique");
            }
        } else {
            self.out.scene.delta = -1.0;
            self.log(WARNING, b"No 'extra scene' element found!\n".to_vec());
        }

        let mut node = self.el(scene, "node");
        while let Some(n) = node {
            self.scene_node(n, -1)?;
            node = self.nx(n, "node");
        }
        Ok(())
    }

    fn scene_node(&mut self, ne: NodeId, parent: i32) -> Res<()> {
        let index = self.out.scene.nodes.len();
        let id = self.sa(ne, "id");
        let mut node = Node {
            name: self.sa(ne, "name"),
            sid: self
                .at(ne, "sid")
                .map_or_else(|| id.clone(), <[u8]>::to_vec),
            kind: self.sa(ne, "type"),
            id,
            instance_material: Bytes::new(),
            source: Bytes::new(),
            source_is_file: false,
            source_is_light: false,
            mtx: identity(),
            mtx_world: identity(),
            scale: [1.0; 3],
            parent,
            transforms: Vec::new(),
        };

        let inst = self
            .el(ne, "instance_geometry")
            .or_else(|| self.el(ne, "instance_light"))
            .or_else(|| self.el(ne, "instance_controller"))
            .or_else(|| self.el(ne, "instance"));
        if let Some(inst) = inst {
            let mut source = self.sa(inst, "url");
            node.source_is_file = source.first() != Some(&b'#');
            addr(&mut source);
            node.source = source;
            node.source_is_light = self.value(inst) == b"instance_light";
            let material = self
                .el(inst, "bind_material")
                .and_then(|b| self.el(b, "technique_common"))
                .and_then(|t| self.el(t, "instance_material"));
            if let Some(m) = material {
                node.instance_material = self.sa(m, "target");
                addr(&mut node.instance_material);
            }
        }

        let z_to_y = self.out.z_to_y;
        let mut m = identity();
        let mut t = self.any(ne);
        while let Some(te) = t {
            let next = self.nx_any(te);
            t = next;
            let kind = self.value(te).to_vec();
            let sid = self.sa(te, "sid");
            let Some(first) = self.d.node(te).first else {
                continue;
            };
            let first = self.d.node(first);
            if !matches!(first.kind, Kind::Text { .. }) {
                continue;
            }
            let vals = floats(crate::xml::c_str(&first.value));
            match kind.as_slice() {
                b"translate" => m = mul(&m, &translate_matrix(v3(&vals, z_to_y)?)),
                b"rotate" => {
                    let axis = v3(&vals, false)?;
                    let angle = get(&vals, 3, "rotate has fewer than 4 values")?;
                    m = mul(&m, &rotation_matrix(angle, axis));
                }
                b"scale" => {
                    let s = v3(&vals, z_to_y)?;
                    node.scale = [s.x, s.y, s.z];
                    let name = &node.name;
                    let is_light = self.o.lights && self.light_exists(&node.source);
                    let no_scale = is_light
                        || lower(sub(name, 1, 8)) == b"collider"
                        || lower(sub(name, 1, 12)) == b"charcollider"
                        || lower(sub(name, 1, 4)) == b"area";
                    if !no_scale {
                        m = mul(&m, &scale_matrix(s));
                    }
                }
                b"matrix" => m = mul(&m, &mat_from(&vals)?),
                _ => {}
            }
            node.transforms.push(Transform {
                sid,
                kind,
                values: vals,
            });
        }
        let parent_world = usize::try_from(parent)
            .ok()
            .and_then(|p| self.out.scene.nodes.get(p))
            .map_or_else(identity, |p| p.mtx_world);
        node.mtx = m;
        node.mtx_world = mul(&parent_world, &m);
        self.out.scene.nodes.push(node);

        let mut child = self.el(ne, "node");
        while let Some(c) = child {
            self.scene_node(c, index as i32)?;
            child = self.nx(c, "node");
        }
        Ok(())
    }

    pub(super) fn load_animations(&mut self, lib: NodeId) -> Res<()> {
        let mut e = self.el(lib, "animation");
        while let Some(ae) = e {
            e = self.nx(ae, "animation");
            if self.el(ae, "animation").is_some() {
                self.load_animations(ae)?;
                continue;
            }
            let Some(tc) = self.el(ae, "channel") else {
                self.log(WARNING, b"Animation missing channel!\n".to_vec());
                continue;
            };
            let target = self.sa(tc, "target");
            let node = string::split(&target, Some(b"/"), true)
                .next()
                .ok_or(Fail("empty channel target"))?
                .to_vec();
            let pos = match self
                .out
                .animations
                .iter()
                .position(|a| a.target_node == node)
            {
                Some(p) => p,
                None => {
                    self.out.animations.push(Animation {
                        target_node: node,
                        ..Animation::default()
                    });
                    self.out.animations.len() - 1
                }
            };
            let slot = self
                .out
                .animations
                .get_mut(pos)
                .ok_or(Fail("animation slot"))?;
            let mut anim = std::mem::take(slot);
            let r = self.animation(ae, tc, &mut anim);
            if let Some(slot) = self.out.animations.get_mut(pos) {
                *slot = anim;
            }
            r?;
        }
        Ok(())
    }

    fn animation(&mut self, ae: NodeId, first_channel: NodeId, anim: &mut Animation) -> Res<()> {
        anim.id = self.sa(ae, "id");
        let mut ch = Some(first_channel);
        while let Some(c) = ch {
            let mut source = self.sa(c, "source");
            addr(&mut source);
            anim.channels.push(Channel {
                id: self.sa(c, "id"),
                source,
                target: self.sa(c, "target"),
            });
            ch = self.nx(c, "channel");
        }

        let mut sampler = self.el(ae, "sampler");
        while let Some(s) = sampler {
            let mut sm = Sampler {
                id: self.sa(s, "id"),
                ..Sampler::default()
            };
            let mut input = self.el(s, "input");
            while let Some(i) = input {
                let semantic = self.sa(i, "semantic");
                let mut source = self.sa(i, "source");
                addr(&mut source);
                match semantic.as_slice() {
                    b"INPUT" => sm.time_array = source,
                    b"OUTPUT" => sm.value_array = source,
                    _ => {}
                }
                input = self.nx(i, "input");
            }
            anim.samplers.push(sm);
            sampler = self.nx(s, "sampler");
        }

        for c in &anim.channels {
            for s in anim.samplers.iter_mut().filter(|s| s.id == c.source) {
                s.target = c.target.clone();
            }
        }

        let mut src = self.el(ae, "source");
        while let Some(s) = src {
            let mut source = AnimSource {
                id: self.sa(s, "id"),
                values: Vec::new(),
            };
            if let Some(fa) = self.el(s, "float_array") {
                let count = self.ia(fa, "count", 0);
                if !(0..=MAX_RESERVE).contains(&count) {
                    return Err(Fail("float_array count reserves gigabytes"));
                }
                if count > 0 {
                    source.values = floats(self.text_req(fa)?);
                }
            }
            anim.sources.push(source);
            src = self.nx(s, "source");
        }
        Ok(())
    }

    pub(super) fn load_controllers(&mut self, lib: NodeId) -> Res<()> {
        let mut e = self.el(lib, "controller");
        while let Some(ce) = e {
            let mut c = Controller {
                id: self.sa(ce, "id"),
                ..Controller::default()
            };
            let r = self.controller(ce, &mut c);
            self.out.controllers.push(c);
            if r? {
                return Ok(());
            }
            e = self.nx(ce, "controller");
        }
        Ok(())
    }

    /// `true` where the original returns from `LoadControllers`.
    fn controller(&mut self, ce: NodeId, c: &mut Controller) -> Res<bool> {
        let Some(skin) = self.el(ce, "skin") else {
            self.log(ERROR, b"No Skin found in controller!\n".to_vec());
            return Ok(false);
        };
        c.target = self.sa(skin, "source");
        addr(&mut c.target);

        if let Some(b) = self.el(skin, "bind_shape_matrix") {
            c.bind = Some(flat16(&mat_from(&floats(self.text_req(b)?))?));
        } else {
            let msg = cat(&[
                b"No bind matrix in controller '",
                &c.id,
                b"' using identity\n",
            ]);
            self.log(WARNING, msg);
            c.bind = Some(flat16(&identity()));
        }

        let (mut name_src, mut weight_src, mut matrix_src) =
            (Bytes::new(), Bytes::new(), Bytes::new());
        let (mut joint_off, mut weight_off) = (-1i32, -1i32);

        let Some(joints) = self.el(skin, "joints") else {
            self.log(
                WARNING,
                b"Couldn't find joint element for controller!\n".to_vec(),
            );
            return Ok(false);
        };
        let mut input = self.el(joints, "input");
        while let Some(i) = input {
            let semantic = self.sa(i, "semantic");
            let mut source = self.sa(i, "source");
            addr(&mut source);
            match semantic.as_slice() {
                b"JOINT" => name_src = source,
                b"INV_BIND_MATRIX" => matrix_src = source,
                _ => {}
            }
            input = self.nx(i, "input");
        }

        let Some(vw) = self.el(skin, "vertex_weights") else {
            self.log(
                WARNING,
                b"Couldn't find vertex_weights element for controller!\n".to_vec(),
            );
            return Ok(false);
        };
        let mut input = self.el(vw, "input");
        while let Some(i) = input {
            let semantic = self.sa(i, "semantic");
            let mut source = self.sa(i, "source");
            let offset = self.ia(i, "offset", -1);
            addr(&mut source);
            match semantic.as_slice() {
                b"JOINT" => joint_off = offset,
                b"WEIGHT" => {
                    weight_off = offset;
                    weight_src = source;
                }
                _ => {}
            }
            input = self.nx(i, "input");
        }

        let mut src = self.el(skin, "source");
        while let Some(s) = src {
            let id = self.sa(s, "id");
            if id == name_src {
                let na = self
                    .el(s, "Name_array")
                    .ok_or(Fail("source without Name_array loops forever"))?;
                let count = self.ia(na, "count", 0);
                if !(0..=MAX_RESERVE).contains(&count) {
                    return Err(Fail("Name_array count reserves gigabytes"));
                }
                let text = self
                    .text(na)?
                    .ok_or(Fail("Name_array without text loops forever"))?;
                let names: Vec<&[u8]> = string::split(text, None, true).collect();
                if names.len() as u64 != count as u64 {
                    let root = *names.first().ok_or(Fail("empty Name_array"))?;
                    let mut joint = root.to_vec();
                    for part in names.iter().skip(1) {
                        if *part == root {
                            c.joints.push(std::mem::replace(&mut joint, root.to_vec()));
                        } else {
                            joint.push(b' ');
                            joint.extend_from_slice(part);
                        }
                    }
                    if joint != root {
                        c.joints.push(joint);
                    }
                } else {
                    c.joints.extend(names.iter().map(|n| n.to_vec()));
                }
            } else if id == weight_src || id == matrix_src {
                let fa = self
                    .el(s, "float_array")
                    .ok_or(Fail("source without float_array loops forever"))?;
                let count = self.ia(fa, "count", 0);
                if !(0..=MAX_RESERVE).contains(&count) {
                    return Err(Fail("float_array count reserves gigabytes"));
                }
                let Some(text) = self.text(fa)? else {
                    self.log(ERROR, b"No value data found!\n".to_vec());
                    return Ok(true);
                };
                let vals = floats(text);
                if id == weight_src {
                    c.weights = vals;
                } else {
                    for k in 0..count.checked_div(16).unwrap_or(0) {
                        let mut flat = [0.0f32; 16];
                        for (j, d) in flat.iter_mut().enumerate() {
                            *d = get(
                                &vals,
                                i64::from(k) * 16 + j as i64,
                                "matrix source has too few values",
                            )?;
                        }
                        c.matrices.push(flat);
                    }
                }
            }
            src = self.nx(s, "source");
        }

        let Some(vcount) = self.el(vw, "vcount") else {
            self.log(
                WARNING,
                b"Couldn't find vertex_weights vcount element for controller!\n".to_vec(),
            );
            return Ok(false);
        };
        let Some(vcount_text) = self.text(vcount)? else {
            self.log(ERROR, b"No value data found!\n".to_vec());
            return Ok(false);
        };
        let counts = ints(vcount_text);
        let Some(v) = self.el(vw, "v") else {
            self.log(
                WARNING,
                b"Couldn't find vertex_weights v element for controller!\n".to_vec(),
            );
            return Ok(false);
        };
        let Some(v_text) = self.text(v)? else {
            self.log(ERROR, b"No value data found!\n".to_vec());
            return Ok(false);
        };
        let v = ints(v_text);

        c.pairs = vec![Vec::new(); counts.len()];
        let (mut vtx, mut in_vtx) = (0usize, 0i32);
        for pair in 0..v.len().div_euclid(2) as i64 {
            let joint = get(
                &v,
                pair * 2 + i64::from(joint_off),
                "joint offset outside <v>",
            )?;
            let weight = get(
                &v,
                pair * 2 + i64::from(weight_off),
                "weight offset outside <v>",
            )?;
            c.pairs
                .get_mut(vtx)
                .ok_or(Fail("more weight pairs than vertices"))?
                .push((joint, weight));
            in_vtx += 1;
            let need = *counts.get(vtx).ok_or(Fail("vcount"))?;
            if in_vtx >= need {
                vtx += 1;
                in_vtx = 0;
            }
        }
        Ok(false)
    }
}
