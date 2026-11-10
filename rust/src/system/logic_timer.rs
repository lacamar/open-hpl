//! `cLogicTimer` arithmetic; the caller supplies the application time in milliseconds.

#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq)]
pub struct LogicTimer {
    pub local_time: f64,
    pub local_time_add: f64,
    pub speed_mul: f64,
    pub max_updates: i32,
    pub update_count: i32,
}

impl LogicTimer {
    pub fn new(updates_per_sec: i32, now: u64) -> Self {
        let mut t = Self {
            local_time: 0.0,
            local_time_add: 0.0,
            speed_mul: 1.0,
            max_updates: updates_per_sec.checked_div(10).unwrap_or(0),
            update_count: 0,
        };
        t.set_updates_per_sec(updates_per_sec, now);
        t
    }

    pub fn reset(&mut self, now: u64) {
        self.local_time = now as f64;
    }

    pub fn want_update(&mut self, now: u64) -> bool {
        self.update_count = self.update_count.wrapping_add(1);
        if self.update_count > self.max_updates {
            return false;
        }
        if self.local_time < now as f64 {
            self.local_time += self.local_time_add / self.speed_mul;
            return true;
        }
        false
    }

    pub fn end_update_loop(&mut self, now: u64) {
        if self.update_count > self.max_updates {
            self.reset(now);
        }
        self.update_count = 0;
    }

    pub fn set_updates_per_sec(&mut self, updates_per_sec: i32, now: u64) {
        self.local_time_add = 1000.0 / f64::from(updates_per_sec);
        self.reset(now);
    }

    pub fn updates_per_sec(&self) -> i32 {
        (1000.0 / self.local_time_add).round() as i64 as i32
    }

    pub fn step_size(&self) -> f32 {
        (self.local_time_add / 1000.0) as f32
    }
}
