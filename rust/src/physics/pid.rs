//! `cPidController<float>`.

use crate::math::fma;

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
