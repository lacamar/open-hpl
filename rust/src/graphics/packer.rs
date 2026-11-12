//! `cFrameBitmap` atlas placement: the guillotine rect tree and the reorganisation order.

use crate::math::{Rect2l, rect_fit_l};

const MIN_HOLE: i32 = 6;

#[derive(Clone, Copy, Debug, PartialEq)]
pub struct Node {
    pub rect: Rect2l,
    pub handle: i32,
    left: Option<usize>,
    right: Option<usize>,
}

#[derive(Clone, Copy, Debug, PartialEq)]
pub struct Insert {
    /// Placed rect including the 1-pixel border on every side; `None` when nothing fits.
    pub rect: Option<Rect2l>,
    /// A free leaf was visited at or before the placement.
    pub found_empty: bool,
}

#[derive(Debug, Default)]
pub struct RectPacker {
    nodes: Vec<Node>,
}

impl RectPacker {
    pub fn new(w: i32, h: i32) -> Self {
        let mut p = RectPacker::default();
        p.clear(w, h);
        p
    }

    pub fn clear(&mut self, w: i32, h: i32) {
        self.nodes.clear();
        self.nodes.push(Node {
            rect: Rect2l { x: 0, y: 0, w, h },
            handle: -1,
            left: None,
            right: None,
        });
    }

    fn leaves(&self) -> Vec<usize> {
        let mut out = Vec::new();
        let mut stack = vec![0usize];
        while let Some(i) = stack.pop() {
            let Some(n) = self.nodes.get(i) else { continue };
            if n.left.is_none() && n.right.is_none() {
                out.push(i);
            }
            stack.extend(n.right);
            stack.extend(n.left);
        }
        out
    }

    /// Node data in the order of `BinTree::GetNodeList` (left, node, right).
    pub fn nodes_in_order(&self) -> Vec<Node> {
        let mut out = Vec::new();
        let mut stack = Vec::new();
        let mut cur = Some(0usize);
        while cur.is_some() || !stack.is_empty() {
            while let Some(i) = cur {
                stack.push(i);
                cur = self.nodes.get(i).and_then(|n| n.left);
            }
            let Some(i) = stack.pop() else { break };
            if let Some(n) = self.nodes.get(i) {
                out.push(*n);
                cur = n.right;
            }
        }
        out
    }

    pub fn leaf_nodes(&self) -> Vec<Node> {
        self.leaves()
            .into_iter()
            .filter_map(|i| self.nodes.get(i).copied())
            .collect()
    }

    fn push(&mut self, rect: Rect2l, handle: i32) -> usize {
        self.nodes.push(Node {
            rect,
            handle,
            left: None,
            right: None,
        });
        self.nodes.len() - 1
    }

    fn set_children(&mut self, parent: usize, left: usize, right: Option<usize>) {
        if let Some(n) = self.nodes.get_mut(parent) {
            n.left = Some(left);
            n.right = right;
        }
    }

    /// `cFrameBitmap::AddBitmap` placement for a bitmap of `w` x `h` (border added here).
    pub fn insert(&mut self, w: i32, h: i32) -> Insert {
        let sw = w.wrapping_add(2);
        let sh = h.wrapping_add(2);
        let mut found_empty = false;
        for i in self.leaves() {
            let Some(&node) = self.nodes.get(i) else {
                continue;
            };
            if node.handle >= 0 {
                continue;
            }
            found_empty = true;
            let new = Rect2l {
                x: node.rect.x,
                y: node.rect.y,
                w: sw,
                h: sh,
            };
            if !rect_fit_l(&new, &node.rect) {
                continue;
            }
            if minimum_fit(&new, &node.rect) {
                if let Some(n) = self.nodes.get_mut(i) {
                    n.rect = new;
                    n.handle = 1;
                }
            } else {
                let upper = self.push(
                    Rect2l {
                        x: new.x,
                        y: new.y,
                        w: node.rect.w,
                        h: new.h,
                    },
                    -2,
                );
                let lower = self.push(
                    Rect2l {
                        x: new.x,
                        y: new.y.wrapping_add(new.h),
                        w: node.rect.w,
                        h: node.rect.h.wrapping_sub(new.h),
                    },
                    -3,
                );
                self.set_children(i, upper, Some(lower));
                let image = self.push(new, 2);
                let rest = self.push(
                    Rect2l {
                        x: new.x.wrapping_add(new.w),
                        y: new.y,
                        w: node.rect.w.wrapping_sub(new.w),
                        h: new.h,
                    },
                    -4,
                );
                self.set_children(upper, image, Some(rest));
            }
            return Insert {
                rect: Some(new),
                found_empty,
            };
        }
        Insert {
            rect: None,
            found_empty,
        }
    }
}

pub fn minimum_fit(src: &Rect2l, dest: &Rect2l) -> bool {
    dest.w.wrapping_sub(src.w) < MIN_HOLE && dest.h.wrapping_sub(src.h) < MIN_HOLE
}

/// Insertion order used by `cFrameBitmap::Reorganize`: largest side first, then largest area;
/// ties keep their original order (`std::multiset` inserts after equal keys).
pub fn reorganize_order(sizes: &[(i32, i32)]) -> Vec<usize> {
    let mut idx: Vec<usize> = (0..sizes.len()).collect();
    let key = |i: &usize| {
        let (x, y) = sizes.get(*i).copied().unwrap_or((0, 0));
        (x.max(y), x.wrapping_mul(y))
    };
    idx.sort_by_key(|i| std::cmp::Reverse(key(i)));
    idx
}
