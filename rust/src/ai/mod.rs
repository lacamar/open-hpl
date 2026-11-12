//! `cAINodeContainer` graph (node grid, edge compile, nearest-node queries, free-path ray setup)
//! and `cAStarHandler`. Ray casts stay in C++ behind `CastFn`.
//!
//! Open-list ties follow `std::set<cAStarNode*>` order, i.e. the address of the `cAINode`,
//! passed in as `Node::key`.

use crate::math::{cross3, dot3, normalize3, sub3, v3};
use crate::resources::std_sort;
use crate::types::{Vector2f, Vector3f};
use std::cell::RefCell;
use std::collections::BTreeMap;
use std::ffi::c_void;

mod ffi;
#[cfg(test)]
mod tests;

pub const SKIP_DYNAMIC: u32 = 2;
pub const SKIP_VOLATILE: u32 = 4;

/// Returns true when the ray hits something.
pub type CastFn = unsafe extern "C" fn(*mut c_void, u32, *const Vector3f, *const Vector3f) -> bool;
pub type CanAddFn = unsafe extern "C" fn(*mut c_void, u32, u32) -> bool;

#[derive(Clone, Copy, Debug, PartialEq)]
pub struct Edge {
    pub dist: f32,
    pub sqr: f32,
    pub to: u32,
}

#[derive(Debug)]
struct Node {
    pos: Vector3f,
    key: u64,
    edges: Vec<Edge>,
}

#[derive(Clone, Copy, Debug)]
pub struct Params {
    pub max_edges: i32,
    pub min_edges: i32,
    pub max_edge_dist: f32,
    pub max_height: f32,
    pub at_center: bool,
}

impl Default for Params {
    fn default() -> Self {
        Self {
            max_edges: 5,
            min_edges: 2,
            max_edge_dist: 3.0,
            max_height: 0.1,
            at_center: true,
        }
    }
}

const NODES_PER_GRID: f32 = 6.0;
const POS_ADDS: [(f32, f32); 5] = [(0.0, 0.0), (1.0, 0.0), (-1.0, 0.0), (0.0, 1.0), (0.0, -1.0)];

fn dist_sqr(a: Vector3f, b: Vector3f) -> f32 {
    let d = sub3(b, a);
    dot3(d, d)
}

fn dist(a: Vector3f, b: Vector3f) -> f32 {
    dist_sqr(a, b).sqrt()
}

pub struct Graph {
    nodes: Vec<Node>,
    size: Vector3f,
    pub params: Params,
    grid_map: (i32, i32),
    grid_size: Vector2f,
    min_pos: Vector2f,
    grids: Vec<Vec<u32>>,
    cast: CastFn,
    ctx: *mut c_void,
    pub result: RefCell<Vec<u32>>,
    by_name: BTreeMap<Vec<u8>, u32>,
    by_id: BTreeMap<i32, u32>,
}

impl Graph {
    pub fn new(cast: CastFn, ctx: *mut c_void, size: Vector3f) -> Self {
        Self {
            nodes: Vec::new(),
            size,
            params: Params::default(),
            grid_map: (0, 0),
            grid_size: Vector2f::default(),
            min_pos: Vector2f::default(),
            grids: Vec::new(),
            cast,
            ctx,
            result: RefCell::new(Vec::new()),
            by_name: BTreeMap::new(),
            by_id: BTreeMap::new(),
        }
    }

    /// Registers the last-added node's lookup keys; like `std::map::insert`, the first node wins.
    pub fn name_node(&mut self, node: u32, name: &[u8], id: i32) {
        self.by_name.entry(name.to_vec()).or_insert(node);
        self.by_id.entry(id).or_insert(node);
    }

    pub fn find_name(&self, name: &[u8]) -> Option<u32> {
        self.by_name.get(name).copied()
    }

    pub fn find_id(&self, id: i32) -> Option<u32> {
        self.by_id.get(&id).copied()
    }

    pub fn add_node(&mut self, pos: Vector3f, key: u64) {
        self.nodes.push(Node {
            pos,
            key,
            edges: Vec::new(),
        });
    }

    pub fn edges(&self, node: u32) -> &[Edge] {
        self.nodes
            .get(node as usize)
            .map_or(&[], |n| n.edges.as_slice())
    }

    pub fn add_edge(&mut self, from: u32, to: u32, dist: f32, sqr: f32) -> bool {
        if self.nodes.get(to as usize).is_none() {
            return false;
        }
        match self.nodes.get_mut(from as usize) {
            Some(n) => {
                n.edges.push(Edge { dist, sqr, to });
                true
            }
            None => false,
        }
    }

    /// `cAINode::AddEdge`.
    pub fn connect(&mut self, from: u32, to: u32) -> bool {
        let (Some(a), Some(b)) = (self.nodes.get(from as usize), self.nodes.get(to as usize))
        else {
            return false;
        };
        let (d, s) = (dist(a.pos, b.pos), dist_sqr(a.pos, b.pos));
        self.add_edge(from, to, d, s)
    }

    /// Empty node list: C++ reads `mvNodes[0]` out of bounds (UB); the grid stays unbuilt here.
    pub fn build_grid(&mut self) {
        let Some(first) = self.nodes.first() else {
            return;
        };
        let mut min = Vector2f {
            x: first.pos.x,
            y: first.pos.z,
        };
        let mut max = min;
        for n in self.nodes.iter().skip(1) {
            if min.x > n.pos.x {
                min.x = n.pos.x;
            }
            if min.y > n.pos.z {
                min.y = n.pos.z;
            }
            if max.x < n.pos.x {
                max.x = n.pos.x;
            }
            if max.y < n.pos.z {
                max.y = n.pos.z;
            }
        }
        self.min_pos = min;
        let count = self.nodes.len() as f32;
        let num = ((count / NODES_PER_GRID).sqrt() + 0.5f32) as i32 + 1;
        self.grid_map = (num, num);
        let cells = (i64::from(num) + 1) * (i64::from(num) + 1);
        self.grids
            .resize_with(usize::try_from(cells).unwrap_or(0), Vec::new);
        self.grid_size = Vector2f {
            x: (max.x - min.x) / num as f32,
            y: (max.y - min.y) / num as f32,
        };
        let stride = i64::from(num) + 1;
        for (i, n) in self.nodes.iter().enumerate() {
            let lx = n.pos.x - min.x;
            let ly = n.pos.z - min.y;
            let mut gx = 0i32;
            let mut gy = 0i32;
            if self.grid_size.x > 0.0 {
                gx = (lx / self.grid_size.x) as i32;
            }
            if self.grid_size.y > 0.0 {
                gy = (ly / self.grid_size.y) as i32;
            }
            // C++ indexes out of bounds here for non-finite positions (UB).
            let idx = i64::from(gy) * stride + i64::from(gx);
            if let Some(cell) = usize::try_from(idx)
                .ok()
                .and_then(|k| self.grids.get_mut(k))
            {
                cell.push(i as u32);
            }
        }
    }

    fn grid_pos(&self, lx: f32, ly: f32) -> (i32, i32) {
        let x = ((lx / self.grid_size.x) as i32).max(0).min(self.grid_map.0);
        let y = ((ly / self.grid_size.y) as i32).max(0).min(self.grid_map.1);
        (x, y)
    }

    fn cell(&self, p: (i32, i32)) -> &[u32] {
        let idx = i64::from(p.1) * (i64::from(self.grid_map.0) + 1) + i64::from(p.0);
        usize::try_from(idx)
            .ok()
            .and_then(|k| self.grids.get(k))
            .map_or(&[], Vec::as_slice)
    }

    /// `cAINodeIterator`, flattened. Radius is not checked against the nodes. Unbuilt grid: C++ UB.
    pub fn query(&self, pos: Vector3f, radius: f32) -> Vec<u32> {
        let mut out = Vec::new();
        if self.grids.is_empty() {
            return out;
        }
        let lx = pos.x - self.min_pos.x;
        let ly = pos.z - self.min_pos.y;
        let start = self.grid_pos(lx - radius, ly - radius);
        let end = self.grid_pos(lx + radius, ly + radius);
        let mut gp = start;
        let inc = |gp: &mut (i32, i32)| {
            gp.0 = gp.0.wrapping_add(1);
            if gp.0 > end.0 {
                gp.0 = start.0;
                gp.1 = gp.1.wrapping_add(1);
                if gp.1 > end.1 {
                    return false;
                }
            }
            true
        };
        loop {
            let mut list = self.cell(gp);
            while list.is_empty() {
                if !inc(&mut gp) {
                    return out;
                }
                list = self.cell(gp);
            }
            out.extend_from_slice(list);
            if !inc(&mut gp) {
                return out;
            }
        }
    }

    pub fn free_path(&self, start: Vector3f, end: Vector3f, ray_num: i32, flags: u32) -> bool {
        let rays = if !(0..=5).contains(&ray_num) {
            5
        } else {
            ray_num as usize
        };
        let mut forward = sub3(end, start);
        normalize3(&mut forward);
        let up = v3(0.0, 1.0, 0.0);
        let right = cross3(forward, up);
        let lift = v3(0.0, self.size.y / 2.0, 0.0);
        let (start_c, end_c) = if self.params.at_center {
            (start, end)
        } else {
            (
                v3(start.x + lift.x, start.y + lift.y, start.z + lift.z),
                v3(end.x + lift.x, end.y + lift.y, end.z + lift.z),
            )
        };
        let half_w = self.size.x * 0.4f32;
        let half_h = self.size.y * 0.4f32;
        for &(ax, ay) in POS_ADDS.iter().take(rays) {
            let (w, h) = (ax * half_w, ay * half_h);
            let add = v3(
                right.x * w + up.x * h,
                right.y * w + up.y * h,
                right.z * w + up.z * h,
            );
            let s = v3(start_c.x + add.x, start_c.y + add.y, start_c.z + add.z);
            let e = v3(end_c.x + add.x, end_c.y + add.y, end_c.z + add.z);
            // SAFETY: `cast`/`ctx` were supplied together by the owner of this graph.
            if unsafe { (self.cast)(self.ctx, flags, &s, &e) } {
                return false;
            }
        }
        true
    }

    pub fn compile(&mut self) {
        self.build_grid();
        let flags = SKIP_DYNAMIC | SKIP_VOLATILE;
        for i in 0..self.nodes.len() {
            let Some(pos) = self.nodes.get(i).map(|n| n.pos) else {
                break;
            };
            let p = self.params;
            for j in self.query(pos, p.max_edge_dist * 1.5f32) {
                if j as usize == i {
                    continue;
                }
                let Some(other) = self.nodes.get(j as usize).map(|n| n.pos) else {
                    continue;
                };
                if dist(pos, other) > p.max_edge_dist * 2.0f32 {
                    continue;
                }
                let height = (pos.y - other.y).abs();
                if height <= p.max_height && self.free_path(pos, other, -1, flags) {
                    self.connect(i as u32, j);
                }
            }
            let Some(node) = self.nodes.get_mut(i) else {
                break;
            };
            std_sort(&mut node.edges, &mut |a: Edge, b: Edge| a.dist < b.dist);
            if p.max_edges > 0 && node.edges.len() as i64 > i64::from(p.max_edges) {
                node.edges.truncate(p.max_edges as usize);
            }
            for k in 0..node.edges.len() {
                let far = node.edges.get(k).is_some_and(|e| e.dist > p.max_edge_dist);
                if far && k as i64 >= i64::from(p.min_edges) {
                    node.edges.truncate(k);
                    break;
                }
            }
        }
    }

    /// `cAStarHandler::GetPath`: `None` when no path, `Some(empty)` when the direct line is free,
    /// otherwise node indices goal first.
    ///
    /// # Safety
    /// `can_add`, when set, accepts `user`.
    pub unsafe fn astar(
        &self,
        start: Vector3f,
        goal: Vector3f,
        max_iter: i32,
        can_add: Option<CanAddFn>,
        user: *mut c_void,
    ) -> Option<Vec<u32>> {
        let max_height = self.params.max_height * 1.5f32;
        if (start.y - goal.y).abs() <= max_height && self.free_path(start, goal, -1, SKIP_DYNAMIC) {
            return Some(Vec::new());
        }
        let max_dist = self.params.max_edge_dist * 2.0f32;
        let mut s = Search {
            g: self,
            goal,
            nodes: Vec::new(),
            open: BTreeMap::new(),
            state: vec![State::Free; self.nodes.len()],
        };
        let mut goals = vec![false; self.nodes.len()];
        for j in self.query(start, max_dist) {
            let Some(n) = self.nodes.get(j as usize) else {
                continue;
            };
            let d = dist(start, n.pos);
            if d < max_dist
                && (start.y - n.pos.y).abs() <= max_height
                && self.free_path(start, n.pos, -1, SKIP_DYNAMIC)
            {
                s.add_open(j, None, d);
            }
        }
        for j in self.query(goal, max_dist) {
            let Some(n) = self.nodes.get(j as usize) else {
                continue;
            };
            let d = dist(goal, n.pos);
            if d < max_dist
                && (goal.y - n.pos.y).abs() <= max_height
                && self.free_path(goal, n.pos, -1, SKIP_DYNAMIC)
                && let Some(flag) = goals.get_mut(j as usize)
            {
                *flag = true;
            }
        }
        let mut found = None;
        let mut count = 0i32;
        while !s.open.is_empty() && (max_iter < 0 || count < max_iter) {
            let (cur, node, cur_dist) = s.pop_best()?;
            if goals.get(node as usize).copied().unwrap_or(false) {
                found = Some(cur);
                break;
            }
            for e in &self.nodes.get(node as usize)?.edges {
                // SAFETY: caller guarantees `can_add`/`user` are a valid pair.
                if can_add.is_none_or(|f| unsafe { f(user, node, e.to) }) {
                    s.add_open(e.to, Some(cur), cur_dist + e.dist);
                }
            }
            count = count.wrapping_add(1);
        }
        let mut cursor = found?;
        let mut path = Vec::new();
        loop {
            let n = s.nodes.get(cursor)?;
            path.push(n.node);
            match n.parent {
                Some(p) => cursor = p,
                None => return Some(path),
            }
        }
    }
}

#[derive(Clone, Copy, PartialEq)]
enum State {
    Free,
    Open,
    Closed,
}

struct ANode {
    node: u32,
    cost: f32,
    dist: f32,
    parent: Option<usize>,
}

struct Search<'a> {
    g: &'a Graph,
    goal: Vector3f,
    nodes: Vec<ANode>,
    open: BTreeMap<u64, usize>,
    state: Vec<State>,
}

impl Search<'_> {
    fn add_open(&mut self, node: u32, parent: Option<usize>, d: f32) {
        let (Some(n), Some(st)) = (
            self.g.nodes.get(node as usize),
            self.state.get_mut(node as usize),
        ) else {
            return;
        };
        if *st != State::Free {
            return;
        }
        *st = State::Open;
        let parent_node = parent
            .and_then(|p| self.nodes.get(p))
            .and_then(|p| self.g.nodes.get(p.node as usize));
        let base = match parent_node {
            Some(pn) => d * (1.0f32 + (n.pos.y - pn.pos.y).abs()),
            None => d,
        };
        let cost = base + dist(n.pos, self.goal);
        self.open.insert(n.key, self.nodes.len());
        self.nodes.push(ANode {
            node,
            cost,
            dist: d,
            parent,
        });
    }

    /// Best open node, first in key order among equal costs; moves it to closed.
    fn pop_best(&mut self) -> Option<(usize, u32, f32)> {
        let mut best: Option<(u64, usize)> = None;
        for (&key, &idx) in &self.open {
            match best {
                None => best = Some((key, idx)),
                Some((_, b)) => {
                    let (bc, c) = (self.nodes.get(b)?.cost, self.nodes.get(idx)?.cost);
                    if bc > c {
                        best = Some((key, idx));
                    }
                }
            }
        }
        let (key, idx) = best?;
        self.open.remove(&key);
        let a = self.nodes.get(idx)?;
        *self.state.get_mut(a.node as usize)? = State::Closed;
        Some((idx, a.node, a.dist))
    }
}
