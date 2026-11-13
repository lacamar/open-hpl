//! Renderable container kernels: the static `cRenderableContainer_BoxTree` compile and the split
//! predicates the dynamic tree shares. Renderables stay C++ objects; they cross as indices into
//! the bounding-volume array passed to `compile`.

use crate::graphics::stdsort;
use crate::math::{EPSILON, fma, len3, sub3, v3};
use crate::types::Vector3f;
use std::mem::take;

mod animstate;
mod beam;
mod billboard;
pub mod boxtree_dyn;
pub(crate) mod boxtree_dyn_ffi;
#[cfg(test)]
pub(crate) mod boxtree_dyn_tests;
mod camera;
mod ffi;
mod ffi_h10;
mod ffi_h11;
mod ffi_h13;
mod ffi_light;
mod ffi_meshentity;
mod ffi_node;
mod ffi_particles;
mod ffi_world;
mod h10;
mod h11;
mod h12_psload;
mod h12b_ent;
mod h12b_psys;
mod h13_world;
mod lensflare;
mod light;
mod meshentity;
mod node;
mod particles;
mod rope;
mod scenemgr;
mod skinning;
mod soundentity;
#[cfg(test)]
mod tests;
#[cfg(test)]
mod tests_h10;
#[cfg(test)]
mod tests_h11;
#[cfg(test)]
mod tests_h12;
#[cfg(test)]
mod tests_h12b;
#[cfg(test)]
mod tests_h13;
#[cfg(test)]
mod tests_light;
#[cfg(test)]
mod tests_meshentity;
#[cfg(test)]
mod tests_node;
#[cfg(test)]
mod tests_particles;
#[cfg(test)]
mod tests_world;
mod world;

const BIG: f32 = 100_000.0;

/// World-space bounding volume of one renderable: AABB and `GetWorldCenter()`.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Object {
    pub min: Vector3f,
    pub max: Vector3f,
    pub center: Vector3f,
}

#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct Params {
    pub min_leaf_objects: i32,
    pub min_side_length: f32,
    pub max_side_length: f32,
    pub max_intersection_amount: f32,
    pub max_volume_calc_objects: i32,
    pub min_force_intersection_relative_size: f32,
}

pub const NO_PARENT: u32 = u32::MAX;

/// One compiled node; nodes are in pre-order so a parent precedes its children, and children of
/// a parent appear in list order. Only leaves own objects.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Node {
    pub min: Vector3f,
    pub max: Vector3f,
    pub center: Vector3f,
    pub radius: f32,
    pub parent: u32,
    pub first_object: u32,
    pub object_count: u32,
}

#[derive(Debug, Default)]
pub struct Tree {
    pub nodes: Vec<Node>,
    pub objects: Vec<u32>,
}

fn axis_of(v: Vector3f, axis: i32) -> f32 {
    match axis {
        0 => v.x,
        1 => v.y,
        2 => v.z,
        _ => 0.0,
    }
}

/// 0 = above, 1 = below, 2 = intersecting.
pub fn split_group(
    min: Vector3f,
    max: Vector3f,
    plane: f32,
    axis: i32,
    node_size: Vector3f,
    max_intersection: f32,
) -> i32 {
    let min_val = axis_of(min, axis);
    let max_val = axis_of(max, axis);
    if min_val >= plane {
        return 0;
    }
    if max_val <= plane {
        return 1;
    }
    let above = max_val - plane;
    let below = plane - min_val;
    let size = axis_of(node_size, axis);
    let mut min_dist = above;
    let mut dest = 1;
    if below < min_dist {
        min_dist = below;
        dest = 0;
    }
    if min_dist / size <= max_intersection {
        return dest;
    }
    2
}

pub fn at_edge(min: Vector3f, max: Vector3f, node_min: Vector3f, node_max: Vector3f) -> bool {
    (min.x - node_min.x).abs() < EPSILON
        || (min.y - node_min.y).abs() < EPSILON
        || (min.z - node_min.z).abs() < EPSILON
        || (max.x - node_max.x).abs() < EPSILON
        || (max.y - node_max.y).abs() < EPSILON
        || (max.z - node_max.z).abs() < EPSILON
}

/// Axis of the longest side, earlier axes winning ties.
pub fn longest_axis(size: Vector3f) -> i32 {
    let mut longest = size.x;
    let mut axis = 0;
    if longest < size.y {
        longest = size.y;
        axis = 1;
    }
    if longest < size.z {
        axis = 2;
    }
    axis
}

fn longest_side(size: Vector3f) -> f32 {
    let mut longest = size.x;
    if longest < size.y {
        longest = size.y;
    }
    if longest < size.z {
        longest = size.z;
    }
    longest
}

#[derive(Clone, Copy)]
struct Bounds {
    min: Vector3f,
    max: Vector3f,
    count: u32,
}

impl Bounds {
    fn new() -> Self {
        Self {
            min: v3(BIG, BIG, BIG),
            max: v3(-BIG, -BIG, -BIG),
            count: 0,
        }
    }

    fn add(&mut self, o: &Object) {
        self.count += 1;
        if self.min.x > o.min.x {
            self.min.x = o.min.x;
        }
        if self.min.y > o.min.y {
            self.min.y = o.min.y;
        }
        if self.min.z > o.min.z {
            self.min.z = o.min.z;
        }
        if self.max.x < o.max.x {
            self.max.x = o.max.x;
        }
        if self.max.y < o.max.y {
            self.max.y = o.max.y;
        }
        if self.max.z < o.max.z {
            self.max.z = o.max.z;
        }
    }

    fn size(&self) -> Vector3f {
        sub3(self.max, self.min)
    }

    fn volume(&self) -> f32 {
        if self.count == 0 {
            return 0.0;
        }
        let s = self.size();
        s.x * s.y * s.z
    }
}

#[derive(Default)]
struct Temp {
    axis_count: i32,
    children: Vec<Temp>,
    objects: Vec<u32>,
}

struct Compiler<'a> {
    objs: &'a [Object],
    p: &'a Params,
}

impl Compiler<'_> {
    fn ob(&self, i: u32) -> Object {
        self.objs.get(i as usize).copied().unwrap_or_default()
    }

    fn bounds(&self, list: &[u32]) -> Bounds {
        let mut b = Bounds::new();
        for &i in list {
            b.add(&self.ob(i));
        }
        b
    }

    fn group(&self, i: u32, plane: f32, axis: i32, node_size: Vector3f) -> i32 {
        let o = self.ob(i);
        split_group(
            o.min,
            o.max,
            plane,
            axis,
            node_size,
            self.p.max_intersection_amount,
        )
    }

    fn best_cut_plane(&self, list: &[u32], axis: i32, node_size: Vector3f) -> f32 {
        let mut best_plane = 0.0;
        let mut best_h = -1.0f32;
        for &split in list {
            let plane = axis_of(self.ob(split).min, axis);
            let mut groups = [Bounds::new(); 3];
            for &t in list {
                let g = self.group(t, plane, axis, node_size);
                if let Some(b) = groups.get_mut(g as usize) {
                    b.add(&self.ob(t));
                }
            }
            let [g0, g1, g2] = groups;
            // gcc contracts the sum of products as fma(c2*v2, fma(c0*v0, c1*v1)).
            let h = fma(
                g2.count as f32,
                g2.volume(),
                fma(g0.count as f32, g0.volume(), g1.count as f32 * g1.volume()),
            );
            if h < best_h || best_h < 0.0 {
                best_h = h;
                best_plane = plane;
            }
        }
        best_plane
    }

    fn compile(&self, node: &mut Temp, is_root: bool, split_axis: i32) {
        let p = self.p;
        let b = self.bounds(&node.objects);
        let node_size = b.size();
        let longest = longest_side(node_size);
        if longest < p.min_side_length {
            return;
        }
        let num = node.objects.len() as i32;
        if num < p.min_leaf_objects.wrapping_mul(2) && !is_root && longest < p.max_side_length {
            return;
        }
        let mut axis = split_axis;
        if axis < 0 {
            if longest == node_size.x {
                axis = 0;
            } else if longest == node_size.y {
                axis = 1;
            } else if longest == node_size.z {
                axis = 2;
            }
        }

        let mut sorted = node.objects.clone();
        let cut = if num < p.max_volume_calc_objects {
            self.best_cut_plane(&node.objects, axis, node_size)
        } else {
            let median = (num >> 1) as usize;
            // The C++ indexes its comparator table with the axis, which is -1 only when the
            // node width is NaN (unreachable: node bounds never hold NaN); fall back to x.
            let ca = axis.max(0);
            stdsort::sort(&mut sorted, |&a, &b| {
                axis_of(self.ob(a).center, ca) < axis_of(self.ob(b).center, ca)
            });
            axis_of(
                self.ob(sorted.get(median).copied().unwrap_or_default()).min,
                axis,
            )
        };

        let mut high: Option<Temp> = None;
        let mut low: Option<Temp> = None;
        let mut inter: Option<Temp> = None;
        let mut order: Vec<u8> = Vec::with_capacity(3);
        for &i in &sorted {
            let g = self.group(i, cut, axis, node_size);
            let (slot, id) = match g {
                0 => (&mut high, 0u8),
                1 => (&mut low, 1u8),
                _ => (&mut inter, 2u8),
            };
            if slot.is_none() {
                order.push(id);
            }
            slot.get_or_insert_with(Temp::default).objects.push(i);
        }

        let side = |t: &Option<Temp>| {
            t.as_ref()
                .map_or(0.0, |t| longest_side(self.bounds(&t.objects).size()))
        };
        let high_side = side(&high);
        let low_side = side(&low);
        let inter_side = side(&inter);

        let min_leaf = p.min_leaf_objects;
        let max_side = p.max_side_length;
        let small = |t: &Option<Temp>| {
            t.as_ref()
                .is_some_and(|t| (t.objects.len() as i32) < min_leaf)
        };

        if small(&inter) && inter_side < max_side {
            let mut kept = Vec::new();
            let mut skipped = false;
            let moving = inter
                .as_mut()
                .map(|t| take(&mut t.objects))
                .unwrap_or_default();
            for i in moving {
                let o = self.ob(i);
                let min_val = axis_of(o.min, axis);
                let max_val = axis_of(o.max, axis);
                let size = axis_of(node_size, axis);
                let obj_size = max_val - min_val;
                let above = max_val - cut;
                let below = cut - min_val;
                if obj_size > size * p.min_force_intersection_relative_size {
                    skipped = true;
                    kept.push(i);
                } else if high.is_some() && (above > below || low.is_none()) {
                    if let Some(h) = high.as_mut() {
                        h.objects.push(i);
                    }
                } else if let Some(l) = low.as_mut() {
                    l.objects.push(i);
                } else {
                    skipped = true;
                    kept.push(i);
                }
            }
            if skipped {
                if let Some(t) = inter.as_mut() {
                    t.objects = kept;
                }
            } else {
                inter = None;
                order.retain(|&id| id != 2);
            }
        }
        if small(&high) && inter.is_some() && high_side < max_side {
            if let (Some(h), Some(t)) = (high.take(), inter.as_mut()) {
                t.objects.extend(h.objects);
            }
            order.retain(|&id| id != 0);
        }
        if small(&low) && inter.is_some() && low_side < max_side {
            if let (Some(l), Some(t)) = (low.take(), inter.as_mut()) {
                t.objects.extend(l.objects);
            }
            order.retain(|&id| id != 1);
        }

        if order.len() <= 1 {
            node.axis_count += 1;
            if node.axis_count < 3 {
                let mut next = axis + 1;
                if next > 2 {
                    next = 0;
                }
                self.compile(node, is_root, next);
            }
            return;
        }

        for slot in [&mut high, &mut low, &mut inter] {
            if let Some(t) = slot.as_mut() {
                self.compile(t, false, -1);
            }
        }
        for id in order {
            let slot = match id {
                0 => high.take(),
                1 => low.take(),
                _ => inter.take(),
            };
            if let Some(t) = slot {
                node.children.push(t);
            }
        }
    }

    fn build(&self, t: &Temp, parent: u32, tree: &mut Tree) {
        let b = self.bounds(&t.objects);
        let index = tree.nodes.len() as u32;
        let mut node = Node {
            min: b.min,
            max: b.max,
            center: v3(
                (b.max.x + b.min.x) * 0.5,
                (b.max.y + b.min.y) * 0.5,
                (b.max.z + b.min.z) * 0.5,
            ),
            radius: len3(sub3(b.max, b.min)) * 0.5,
            parent,
            first_object: tree.objects.len() as u32,
            object_count: 0,
        };
        if t.children.is_empty() {
            tree.objects.extend_from_slice(&t.objects);
            node.object_count = t.objects.len() as u32;
        }
        tree.nodes.push(node);
        for c in &t.children {
            self.build(c, index, tree);
        }
    }
}

/// `cRenderableContainer_BoxTree::Compile`: the node tree for `objs` in insertion order.
pub fn compile(objs: &[Object], params: &Params) -> Tree {
    let c = Compiler { objs, p: params };
    let mut root = Temp {
        objects: (0..objs.len() as u32).collect(),
        ..Temp::default()
    };
    c.compile(&mut root, true, -1);
    let mut tree = Tree::default();
    c.build(&root, NO_PARENT, &mut tree);
    tree
}
