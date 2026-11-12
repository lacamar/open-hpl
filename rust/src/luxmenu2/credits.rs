//! `cLuxCredits` scroll and state timing, and `cLuxDemoEnd` fade and button maths.

#[repr(C)]
#[derive(Clone, Copy)]
pub struct Row {
    pub len: u32,
    pub first: u32,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct Credits {
    pub y: f32,
    pub time: f32,
    pub fade_alpha: f32,
    pub state: i32,
    pub end_num: i32,
}

pub const UP_STOP_MUSIC: u32 = 1;
pub const UP_FINISH: u32 = 2;

const STAR: u32 = b'*' as u32;

impl Credits {
    /// `mlEndNum` was uninitialised until `Setup`; -1 draws no code.
    pub fn new() -> Credits {
        let mut c = Credits {
            y: 0.0,
            time: 0.0,
            fade_alpha: 0.0,
            state: 0,
            end_num: -1,
        };
        c.reset();
        c
    }

    pub fn reset(&mut self) {
        self.y = 600.0;
        self.state = 0;
        self.time = 0.0;
        self.fade_alpha = 0.0;
    }

    pub fn update(&mut self, dt: f32, rows: &[Row], scroll_speed: f32, fade_speed: f32) -> u32 {
        let mut out = 0;
        self.time += dt;
        if self.state == 0 || self.state == 1 {
            self.y = (-dt).mul_add(scroll_speed, self.y);
            const SIZE: [f32; 2] = [17.0, 19.0];
            let mut y = self.y;
            for r in rows {
                let size = usize::from(r.first == STAR);
                let add = SIZE.get(size).copied().unwrap_or(0.0);
                if r.len <= 1 {
                    y += add;
                }
                y += add;
            }
            if y < -120.0 {
                out |= UP_STOP_MUSIC;
            }
            if y < -280.0 && self.state == 0 {
                self.time = 0.0;
                self.state += 1;
            }
            if self.state == 0 {
                return out;
            }
        }
        if self.state == 1 || self.state == 4 {
            self.fade_alpha = dt.mul_add(fade_speed, self.fade_alpha);
            if self.fade_alpha > 1.0 {
                self.fade_alpha = 1.0;
                if self.state == 4 {
                    out |= UP_FINISH;
                } else {
                    self.state += 1;
                }
            }
            return out;
        }
        if self.state == 2 {
            self.fade_alpha = (-dt).mul_add(fade_speed, self.fade_alpha);
            if self.fade_alpha < 0.0 {
                self.fade_alpha = 0.0;
                self.state += 1;
            }
        }
        out
    }

    /// True when the music is to be stopped.
    pub fn exit_pressed(&mut self) -> bool {
        if self.state != 0 && self.state != 3 {
            return false;
        }
        if self.time < 3.0 {
            return false;
        }
        self.state += 1;
        self.time = 0.0;
        self.state == 1
    }
}

impl Default for Credits {
    fn default() -> Credits {
        Credits::new()
    }
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct DrawRow {
    pub row: u32,
    pub header: bool,
    pub y: f32,
    pub alpha: f32,
}

/// Visible rows while scrolling (states 0 and 1); `row_add` is the normal and header advance.
pub fn draw_rows(y0: f32, rows: &[Row], row_add: [f32; 2]) -> Vec<DrawRow> {
    let mut out = Vec::new();
    let mut y = y0;
    for (i, r) in rows.iter().enumerate() {
        let mut size = 0usize;
        if r.len <= 1 {
            y += row_add.first().copied().unwrap_or(0.0);
        } else {
            if r.first == STAR {
                size = 1;
            }
            let add = row_add.get(size).copied().unwrap_or(0.0);
            if y >= -add {
                if y > 600.0 {
                    continue;
                }
                let mut alpha = y / 300.0;
                if alpha > 1.0 {
                    alpha = 2.0 - alpha;
                }
                out.push(DrawRow {
                    row: i as u32,
                    header: size == 1,
                    y,
                    alpha,
                });
            }
        }
        y += row_add.get(size).copied().unwrap_or(0.0);
    }
    out
}

/// Code drawn under "The End"; -1 for none.
pub fn end_code(end_num: i32, ptest: bool) -> i32 {
    if (0..=2).contains(&end_num) && !ptest {
        end_num
    } else {
        -1
    }
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct DemoEnd {
    pub fade_alpha: f32,
    pub time: f32,
    pub exiting: bool,
    pub website: bool,
}

pub const DE_EXIT: u32 = 1;
pub const DE_BROWSER: u32 = 2;

impl DemoEnd {
    pub fn new() -> DemoEnd {
        DemoEnd {
            fade_alpha: 1.0,
            time: 0.0,
            exiting: false,
            website: false,
        }
    }

    pub fn reset(&mut self) {
        self.exiting = false;
        self.website = false;
    }

    pub fn update(&mut self, dt: f32) -> u32 {
        let mut out = 0;
        self.time += dt;
        if self.exiting {
            self.fade_alpha = dt.mul_add(0.7, self.fade_alpha);
            if self.fade_alpha > 1.0 {
                self.fade_alpha = 1.0;
                out |= DE_EXIT;
                if self.website {
                    out |= DE_BROWSER;
                }
            }
        } else if self.fade_alpha > 0.0 {
            self.fade_alpha = (-dt).mul_add(0.7, self.fade_alpha);
            if self.fade_alpha < 0.0 {
                self.fade_alpha = 0.0;
            }
        }
        out
    }

    pub fn exit(&mut self, website: bool) {
        self.exiting = true;
        self.website = website;
    }
}

impl Default for DemoEnd {
    fn default() -> DemoEnd {
        DemoEnd::new()
    }
}

/// `ButtonLabelOnUpdate`: advances the hover fade and returns the font colour.
pub fn button_color(fade: &mut f32, hover: bool, dt: f32, base: [f32; 4]) -> [f32; 4] {
    if hover {
        if *fade < 1.0 {
            *fade = dt.mul_add(3.0, *fade);
            if *fade > 1.0 {
                *fade = 1.0;
            }
        }
    } else if *fade > 0.0 {
        *fade -= dt;
        if *fade < 0.0 {
            *fade = 0.0;
        }
    }
    let t = *fade;
    let red = [1.0f32, 0.0, 0.0, 1.0];
    let mut out = [0.0f32; 4];
    for ((o, b), r) in out.iter_mut().zip(base).zip(red) {
        *o = b * (1.0 - t) + r * t;
    }
    out
}

/// Y of each message row (blank rows included); the caller draws the non-empty ones.
pub fn message_rows(count: usize, text_y: f32, font_h: f32) -> Vec<f32> {
    let mut y = text_y;
    let mut out = Vec::with_capacity(count);
    for _ in 0..count {
        out.push(y);
        y += font_h + 1.0;
    }
    out
}
