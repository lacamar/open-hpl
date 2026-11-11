//! `cOut` / `cIn` of `SomaSave.cpp`: native-endian PODs and u32-length strings.

#[derive(Default)]
pub struct Out {
    pub s: Vec<u8>,
}

impl Out {
    pub fn bytes(&mut self, b: &[u8]) {
        self.s.extend_from_slice(b);
    }

    pub fn u8(&mut self, x: u8) {
        self.s.push(x);
    }

    pub fn bool(&mut self, x: bool) {
        self.s.push(u8::from(x));
    }

    pub fn u32(&mut self, x: u32) {
        self.bytes(&x.to_ne_bytes());
    }

    pub fn i32(&mut self, x: i32) {
        self.bytes(&x.to_ne_bytes());
    }

    pub fn u64(&mut self, x: u64) {
        self.bytes(&x.to_ne_bytes());
    }

    pub fn f32(&mut self, x: f32) {
        self.bytes(&x.to_bits().to_ne_bytes());
    }

    pub fn f32s(&mut self, x: &[f32]) {
        for &f in x {
            self.f32(f);
        }
    }

    /// `Pod((uint32_t)x.size())`: the C++ truncates to 32 bits.
    pub fn len(&mut self, n: usize) {
        self.u32(n as u32);
    }

    pub fn str(&mut self, x: &[u8]) {
        self.len(x.len());
        self.bytes(x);
    }
}

#[derive(Clone)]
pub struct In<'a> {
    pub s: &'a [u8],
    pub p: usize,
    pub ok: bool,
}

impl<'a> In<'a> {
    pub fn new(s: &'a [u8]) -> Self {
        In { s, p: 0, ok: true }
    }

    pub fn left(&self) -> bool {
        self.p < self.s.len()
    }

    /// `cIn::Bytes`: a failed read keeps the cursor and clears `ok`.
    pub fn take(&mut self, n: usize) -> Option<&'a [u8]> {
        if !self.ok {
            return None;
        }
        let end = self.p.checked_add(n).filter(|&e| e <= self.s.len());
        let Some(end) = end else {
            self.ok = false;
            return None;
        };
        let r = self.s.get(self.p..end);
        self.p = end;
        r
    }

    fn arr<const N: usize>(&mut self) -> [u8; N] {
        let mut a = [0u8; N];
        if let Some(b) = self.take(N) {
            a.copy_from_slice(b);
        }
        a
    }

    pub fn u8(&mut self) -> u8 {
        u8::from_ne_bytes(self.arr())
    }

    /// `Pod<bool>`: any non-zero byte is true (the C++ reads other values as UB).
    pub fn bool(&mut self) -> bool {
        self.u8() != 0
    }

    pub fn u32(&mut self) -> u32 {
        u32::from_ne_bytes(self.arr())
    }

    pub fn i32(&mut self) -> i32 {
        i32::from_ne_bytes(self.arr())
    }

    pub fn u64(&mut self) -> u64 {
        u64::from_ne_bytes(self.arr())
    }

    pub fn f32(&mut self) -> f32 {
        f32::from_bits(u32::from_ne_bytes(self.arr()))
    }

    /// A POD of `N` floats read as one `Pod<T>`: all or nothing.
    pub fn f32s<const N: usize>(&mut self) -> [f32; N] {
        let mut a = [0f32; N];
        if let Some(b) = self.take(N * 4) {
            for (d, c) in a.iter_mut().zip(b.chunks_exact(4)) {
                *d = c.try_into().map(f32::from_ne_bytes).unwrap_or(0.0);
            }
        }
        a
    }

    /// `N` consecutive `Pod<float>` reads.
    pub fn f32_each<const N: usize>(&mut self) -> [f32; N] {
        let mut a = [0f32; N];
        for x in &mut a {
            *x = self.f32();
        }
        a
    }

    /// A POD of `N` ints read as one `Pod<T>`: all or nothing.
    pub fn i32s<const N: usize>(&mut self) -> [i32; N] {
        let mut a = [0i32; N];
        if let Some(b) = self.take(N * 4) {
            for (d, c) in a.iter_mut().zip(b.chunks_exact(4)) {
                *d = c.try_into().map(i32::from_ne_bytes).unwrap_or(0);
            }
        }
        a
    }

    pub fn blob<const N: usize>(&mut self) -> [u8; N] {
        self.arr()
    }

    pub fn str(&mut self) -> &'a [u8] {
        let n = self.u32() as usize;
        if !self.ok || self.p.saturating_add(n) > self.s.len() {
            self.ok = false;
            return &[];
        }
        self.take(n).unwrap_or(&[])
    }

    /// `SkipObject`: the property length is not bounds-checked, like the C++.
    pub fn skip_object(&mut self) {
        let n = self.u32();
        let mut i = 0;
        while i < n && self.ok {
            self.str();
            self.str();
            let l = self.u32() as usize;
            self.p = self.p.saturating_add(l);
            i += 1;
        }
    }
}
