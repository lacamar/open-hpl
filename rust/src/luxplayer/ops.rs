//! Ordered side-effect records returned by every step; C++ replays them in order.

pub const MAX_OPS: usize = 16;

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Op {
    pub kind: u32,
    pub i: i32,
    pub a: f32,
    pub b: f32,
    pub c: f32,
    pub d: f32,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Ops {
    pub n: u32,
    pub ops: [Op; MAX_OPS],
}

impl Ops {
    pub fn push(&mut self, kind: u32, i: i32, a: f32, b: f32, c: f32, d: f32) {
        if let Some(slot) = self.ops.get_mut(self.n as usize) {
            *slot = Op {
                kind,
                i,
                a,
                b,
                c,
                d,
            };
            self.n += 1;
        }
    }

    pub fn k(&mut self, kind: u32) {
        self.push(kind, 0, 0.0, 0.0, 0.0, 0.0);
    }

    pub fn ki(&mut self, kind: u32, i: i32) {
        self.push(kind, i, 0.0, 0.0, 0.0, 0.0);
    }

    pub fn kf(&mut self, kind: u32, a: f32) {
        self.push(kind, 0, a, 0.0, 0.0, 0.0);
    }

    pub fn kff(&mut self, kind: u32, a: f32, b: f32) {
        self.push(kind, 0, a, b, 0.0, 0.0);
    }

    pub fn kfff(&mut self, kind: u32, a: f32, b: f32, c: f32) {
        self.push(kind, 0, a, b, c, 0.0);
    }

    #[cfg(test)]
    pub fn as_slice(&self) -> &[Op] {
        self.ops.get(..self.n as usize).unwrap_or(&[])
    }
}
