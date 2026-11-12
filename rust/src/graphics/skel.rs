//! `cBone`/`cSkeleton` state: transform cache with lazy world/inverse update, bone order, name map.

use crate::math::{matrix_inverse, matrix_mul};
use crate::types::Matrixf;
use std::collections::BTreeMap;

const IDENT: Matrixf = Matrixf {
    m: [
        [1.0, 0.0, 0.0, 0.0],
        [0.0, 1.0, 0.0, 0.0],
        [0.0, 0.0, 1.0, 0.0],
        [0.0, 0.0, 0.0, 1.0],
    ],
};

pub struct Bone {
    pub unit_scale: f32,
    pub value: i32,
    local: Matrixf,
    world: Matrixf,
    inv_world: Matrixf,
    inv_bind: Matrixf,
    has_inv_bind: bool,
    local_unscaled: Matrixf,
    world_unscaled: Matrixf,
    needs_update: bool,
    needs_unscaled: bool,
    name: Vec<u8>,
    sid: Vec<u8>,
    parent: Option<usize>,
    children: Vec<usize>,
}

/// Bones are boxed so matrix addresses handed to C++ stay valid while the skeleton grows.
pub struct Skeleton {
    #[allow(clippy::vec_box)] // matrix addresses must survive the Vec growing
    bones: Vec<Box<Bone>>,
    order: Vec<usize>,
    names: BTreeMap<Vec<u8>, i32>,
}

impl Default for Skeleton {
    fn default() -> Self {
        Self::new()
    }
}

impl Skeleton {
    /// Bone 0 is the root (`__root_bone`), not part of the bone order.
    pub fn new() -> Self {
        let mut s = Self {
            bones: Vec::new(),
            order: Vec::new(),
            names: BTreeMap::new(),
        };
        s.push(b"__root_bone", b"", None);
        s.set_transform(0, &IDENT);
        s.set_unscaled(0, &IDENT);
        s
    }

    fn push(&mut self, name: &[u8], sid: &[u8], parent: Option<usize>) -> usize {
        self.bones.push(Box::new(Bone {
            unit_scale: 1.0,
            value: 0,
            local: Matrixf::default(),
            world: Matrixf::default(),
            inv_world: Matrixf::default(),
            inv_bind: Matrixf::default(),
            has_inv_bind: false,
            local_unscaled: Matrixf::default(),
            world_unscaled: Matrixf::default(),
            needs_update: true,
            // uninitialised in C++
            needs_unscaled: true,
            name: name.to_vec(),
            sid: sid.to_vec(),
            parent,
            children: Vec::new(),
        }));
        self.bones.len() - 1
    }

    /// `cBone::CreateChildBone` + `cSkeleton::AddBone`.
    pub fn create_child(&mut self, parent: usize, name: &[u8], sid: &[u8]) -> Option<usize> {
        self.bones.get(parent)?;
        let id = self.push(name, sid, Some(parent));
        self.bones.get_mut(parent)?.children.push(id);
        self.order.push(id);
        self.names
            .entry(name.to_vec())
            .or_insert((self.order.len() - 1) as i32);
        Some(id)
    }

    pub fn bone(&self, id: usize) -> Option<&Bone> {
        self.bones.get(id).map(|b| &**b)
    }

    pub fn bone_mut(&mut self, id: usize) -> Option<&mut Bone> {
        self.bones.get_mut(id).map(|b| &mut **b)
    }

    pub fn parent(&self, id: usize) -> Option<usize> {
        self.bones.get(id)?.parent
    }

    pub fn num(&self) -> i32 {
        self.order.len() as i32
    }

    pub fn by_index(&self, idx: i32) -> Option<usize> {
        self.order.get(usize::try_from(idx).ok()?).copied()
    }

    pub fn index_by_name(&self, name: &[u8]) -> i32 {
        self.names.get(name).copied().unwrap_or(-1)
    }

    pub fn index_by_sid(&self, sid: &[u8]) -> i32 {
        self.order
            .iter()
            .position(|&b| self.bones.get(b).is_some_and(|b| b.sid == sid))
            .map_or(-1, |i| i as i32)
    }

    pub fn set_transform(&mut self, id: usize, m: &Matrixf) {
        if let Some(b) = self.bones.get_mut(id) {
            b.local = *m;
            self.mark(id, false);
        }
    }

    fn mark(&mut self, id: usize, unscaled: bool) {
        let mut stack = vec![id];
        while let Some(i) = stack.pop() {
            if let Some(b) = self.bones.get_mut(i) {
                if unscaled {
                    b.needs_unscaled = true;
                } else {
                    b.needs_update = true;
                }
                stack.extend(b.children.iter().copied());
            }
        }
    }

    pub fn set_unscaled(&mut self, id: usize, m: &Matrixf) {
        if let Some(b) = self.bones.get_mut(id) {
            b.local_unscaled = *m;
            self.mark(id, true);
        }
    }

    pub fn needs_update_unscaled(&mut self, id: usize) {
        self.mark(id, true);
    }

    pub fn set_inv_bind(&mut self, id: usize, m: &Matrixf) {
        if let Some(b) = self.bones.get_mut(id) {
            b.inv_bind = *m;
            b.has_inv_bind = true;
        }
    }

    /// Detached bones keep their parent link, as in C++.
    pub fn detach(&mut self, id: usize) {
        let Some(parent) = self.bones.get(id).and_then(|b| b.parent) else {
            return;
        };
        if let Some(p) = self.bones.get_mut(parent)
            && let Some(pos) = p.children.iter().position(|&c| c == id)
        {
            p.children.remove(pos);
        }
        if let Some(pos) = self.order.iter().position(|&b| b == id) {
            self.order.remove(pos);
        }
        self.names.clear();
        for (i, &b) in self.order.iter().enumerate() {
            if let Some(b) = self.bones.get(b) {
                self.names.entry(b.name.clone()).or_insert(i as i32);
            }
        }
    }

    fn update(&mut self, id: usize) {
        let Some(b) = self.bones.get(id) else { return };
        if !b.needs_update {
            return;
        }
        let parent = b.parent;
        if let Some(p) = parent {
            self.update(p);
        }
        let pw = parent.and_then(|p| self.bones.get(p)).map(|p| p.world);
        if let Some(b) = self.bones.get_mut(id) {
            b.world = match pw {
                None => b.local,
                Some(pw) => matrix_mul(&pw, &b.local),
            };
            b.inv_world = matrix_inverse(&b.world);
            b.needs_update = false;
        }
    }

    fn update_unscaled(&mut self, id: usize) {
        let Some(b) = self.bones.get(id) else { return };
        if !b.needs_unscaled {
            return;
        }
        let parent = b.parent;
        if let Some(p) = parent {
            self.update_unscaled(p);
        }
        let pw = parent
            .and_then(|p| self.bones.get(p))
            .map(|p| p.world_unscaled);
        if let Some(b) = self.bones.get_mut(id) {
            b.world_unscaled = match pw {
                None => b.local_unscaled,
                Some(pw) => matrix_mul(&pw, &b.local_unscaled),
            };
            b.needs_unscaled = false;
        }
    }

    pub fn local(&self, id: usize) -> Option<&Matrixf> {
        self.bones.get(id).map(|b| &b.local)
    }

    pub fn local_unscaled(&self, id: usize) -> Option<&Matrixf> {
        self.bones.get(id).map(|b| &b.local_unscaled)
    }

    pub fn world(&mut self, id: usize) -> Option<&Matrixf> {
        self.update(id);
        self.bones.get(id).map(|b| &b.world)
    }

    pub fn inv_world(&mut self, id: usize) -> Option<&Matrixf> {
        self.update(id);
        self.bones.get(id).map(|b| &b.inv_world)
    }

    pub fn world_unscaled(&mut self, id: usize) -> Option<&Matrixf> {
        self.update_unscaled(id);
        self.bones.get(id).map(|b| &b.world_unscaled)
    }

    pub fn inv_bind(&mut self, id: usize) -> Option<&Matrixf> {
        if self.bones.get(id)?.has_inv_bind {
            self.bones.get(id).map(|b| &b.inv_bind)
        } else {
            self.inv_world(id)
        }
    }
}
