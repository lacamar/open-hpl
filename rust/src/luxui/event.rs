//! Ordered side effects returned to C++, mirrored in `amnesia/src/game/RustLuxUi.h`.

pub const EV_MAX: usize = 64;

#[repr(C)]
#[derive(Clone, Copy)]
pub struct Str {
    pub p: *const u8,
    pub n: usize,
}

impl Str {
    pub const NONE: Str = Str {
        p: std::ptr::null(),
        n: 0,
    };

    pub fn of(s: &[u8]) -> Str {
        Str {
            p: s.as_ptr(),
            n: s.len(),
        }
    }
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct Ev {
    pub kind: u32,
    pub a: i32,
    pub b: i32,
    pub v: [f32; 10],
    pub s0: Str,
    pub s1: Str,
}

impl Ev {
    pub fn new(kind: u32) -> Ev {
        Ev {
            kind,
            a: 0,
            b: 0,
            v: [0.0; 10],
            s0: Str::NONE,
            s1: Str::NONE,
        }
    }

    pub fn ints(kind: u32, a: i32, b: i32) -> Ev {
        Ev {
            a,
            b,
            ..Ev::new(kind)
        }
    }

    pub fn strs(kind: u32, s0: &[u8], s1: &[u8]) -> Ev {
        Ev {
            s0: Str::of(s0),
            s1: Str::of(s1),
            ..Ev::new(kind)
        }
    }

    pub fn floats(kind: u32, v: &[f32]) -> Ev {
        let mut e = Ev::new(kind);
        for (d, s) in e.v.iter_mut().zip(v) {
            *d = *s;
        }
        e
    }
}

#[derive(Default)]
pub struct Out {
    pub evs: Vec<Ev>,
    arena: Vec<Vec<u8>>,
}

impl Out {
    pub fn push(&mut self, e: Ev) {
        if self.evs.len() < EV_MAX {
            self.evs.push(e);
        }
    }

    pub fn clear(&mut self) {
        self.evs.clear();
        self.arena.clear();
    }

    /// Keeps `bytes` alive until the next [`Out::clear`] and points a [`Str`] at them.
    pub fn own(&mut self, bytes: Vec<u8>) -> Str {
        let s = Str::of(&bytes);
        self.arena.push(bytes);
        s
    }

    pub fn text(&mut self, kind: u32, bytes: Vec<u8>) {
        let s0 = self.own(bytes);
        self.push(Ev {
            s0,
            ..Ev::new(kind)
        });
    }
}

/// # Safety
/// `out` must point at `cap` writable events.
pub unsafe fn flush(o: &Out, out: *mut Ev, cap: usize) -> usize {
    let n = o.evs.len().min(cap);
    for (i, e) in o.evs.iter().take(n).enumerate() {
        // SAFETY: i < cap.
        unsafe { out.add(i).write(*e) };
    }
    n
}
