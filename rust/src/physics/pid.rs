//! `cPidController<float>`.

use crate::math::{fma, v3};
use crate::types::Vector3f;

#[derive(Debug, Default)]
pub struct Pid {
    pub p: f32,
    pub i: f32,
    pub d: f32,
    errors: Vec<f32>,
    steps: Vec<f32>,
    integral: f32,
    derivative: f32,
    error_num: i32,
    last_num: i32,
}

impl Pid {
    pub fn new() -> Self {
        let mut pid = Pid::default();
        pid.reset();
        pid
    }

    pub fn set_error_num(&mut self, n: i32) {
        let n = usize::try_from(n).unwrap_or(0);
        self.errors.resize(n, 0.0);
        self.steps.resize(n, 0.0);
    }

    pub fn reset(&mut self) {
        self.error_num = 0;
        self.last_num = -1;
        self.steps.fill(0.0);
        self.integral = 0.0;
        self.derivative = 0.0;
    }

    /// C++ writes `mvErrors[mlErrorNum]` unchecked, so an empty buffer or an index left behind by
    /// a shrinking `SetErrorNum` is UB there; here the output is 0 and the index wraps to 0.
    pub fn output(&mut self, error: f32, time_step: f32) -> f32 {
        if self.errors.is_empty() {
            return 0.0;
        }
        let mut idx = usize::try_from(self.error_num).unwrap_or(0);
        if idx >= self.errors.len() {
            idx = 0;
        }
        if let Some(e) = self.errors.get_mut(idx) {
            *e = error;
        }
        if let Some(t) = self.steps.get_mut(idx) {
            *t = time_step;
        }

        self.integral = 0.0;
        for (e, t) in self.errors.iter().zip(&self.steps) {
            self.integral = fma(*e, *t, self.integral);
        }

        self.derivative = 0.0;
        let last = usize::try_from(self.last_num).ok();
        if let Some(last) = last {
            let prev = self.errors.get(last).copied().unwrap_or(0.0);
            self.derivative = (error - prev) / time_step;
        }

        self.last_num = i32::try_from(idx).unwrap_or(0);
        let mut next = idx + 1;
        if next >= self.errors.len() {
            next = 0;
        }
        self.error_num = i32::try_from(next).unwrap_or(0);

        fma(
            self.derivative,
            self.d,
            fma(error, self.p, self.integral * self.i),
        )
    }
}

/// `cPidController<cVector3f>`.
#[derive(Debug, Default)]
pub struct Pid3 {
    pub p: f32,
    pub i: f32,
    pub d: f32,
    errors: Vec<Vector3f>,
    steps: Vec<f32>,
    error_num: usize,
    last_num: Option<usize>,
}

impl Pid3 {
    pub fn new(error_num: usize) -> Self {
        Pid3 {
            errors: vec![Vector3f::default(); error_num],
            steps: vec![0.0; error_num],
            ..Pid3::default()
        }
    }

    pub fn reset(&mut self) {
        self.error_num = 0;
        self.last_num = None;
        self.steps.fill(0.0);
    }

    /// Empty buffers are UB in C++ (unchecked index); here the output is zero.
    pub fn output(&mut self, error: Vector3f, time_step: f32) -> Vector3f {
        let n = self.errors.len();
        if n == 0 {
            return Vector3f::default();
        }
        let idx = if self.error_num >= n {
            0
        } else {
            self.error_num
        };
        if let (Some(e), Some(t)) = (self.errors.get_mut(idx), self.steps.get_mut(idx)) {
            *e = error;
            *t = time_step;
        }

        let mut integral = Vector3f::default();
        for (e, t) in self.errors.iter().zip(&self.steps) {
            integral = v3(
                fma(e.x, *t, integral.x),
                fma(e.y, *t, integral.y),
                fma(e.z, *t, integral.z),
            );
        }

        let mut derivative = Vector3f::default();
        if let Some(last) = self.last_num {
            let prev = self.errors.get(last).copied().unwrap_or_default();
            derivative = v3(
                (error.x - prev.x) / time_step,
                (error.y - prev.y) / time_step,
                (error.z - prev.z) / time_step,
            );
        }

        self.last_num = Some(idx);
        self.error_num = if idx + 1 >= n { 0 } else { idx + 1 };

        let term = |e: f32, int: f32, der: f32| fma(der, self.d, fma(e, self.p, int * self.i));
        v3(
            term(error.x, integral.x, derivative.x),
            term(error.y, integral.y, derivative.y),
            term(error.z, integral.z, derivative.z),
        )
    }
}
