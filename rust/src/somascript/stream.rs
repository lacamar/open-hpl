//! The parts of libstdc++'s `std::istringstream` the global-variable loader depends on:
//! sticky fail/eof bits, whitespace skipping and `num_get` digit scanning.

use super::cfmt;

pub(crate) struct Stream<'a> {
    data: &'a [u8],
    pos: usize,
    eof: bool,
    fail: bool,
}

fn is_space(c: u8) -> bool {
    matches!(c, b' ' | b'\t' | b'\n' | 0x0b | 0x0c | b'\r')
}

impl<'a> Stream<'a> {
    pub(crate) fn new(data: &'a [u8]) -> Self {
        Self {
            data,
            pos: 0,
            eof: false,
            fail: false,
        }
    }

    pub(crate) fn good(&self) -> bool {
        !self.eof && !self.fail
    }

    #[cfg(test)]
    pub(crate) fn failed(&self) -> bool {
        self.fail
    }

    fn peek(&self) -> Option<u8> {
        self.data.get(self.pos).copied()
    }

    /// Formatted-input sentry: a stream that is not good fails; leading whitespace is skipped.
    fn sentry(&mut self) -> bool {
        if !self.good() {
            self.fail = true;
            return false;
        }
        while let Some(c) = self.peek() {
            if !is_space(c) {
                return true;
            }
            self.pos += 1;
        }
        self.eof = true;
        self.fail = true;
        false
    }

    /// `in >> std::string`.
    pub(crate) fn token(&mut self) -> Option<&'a [u8]> {
        if !self.sentry() {
            return None;
        }
        let start = self.pos;
        while let Some(c) = self.peek() {
            if is_space(c) {
                break;
            }
            self.pos += 1;
        }
        if self.pos == self.data.len() {
            self.eof = true;
        }
        self.data.get(start..self.pos)
    }

    /// `in.ignore(max, '\n')`.
    pub(crate) fn ignore_line(&mut self) {
        if !self.good() {
            self.fail = true;
            return;
        }
        while let Some(c) = self.peek() {
            self.pos += 1;
            if c == b'\n' {
                return;
            }
        }
        self.eof = true;
    }

    /// `in.get()`.
    pub(crate) fn get(&mut self) -> Option<u8> {
        if !self.good() {
            self.fail = true;
            return None;
        }
        match self.peek() {
            Some(c) => {
                self.pos += 1;
                Some(c)
            }
            None => {
                self.eof = true;
                self.fail = true;
                None
            }
        }
    }

    /// `in.read(buf, n)`: short reads set eof and fail.
    pub(crate) fn read(&mut self, n: usize) -> &'a [u8] {
        if !self.good() {
            self.fail = true;
            return &[];
        }
        let end = self.pos.saturating_add(n).min(self.data.len());
        let r = self.data.get(self.pos..end).unwrap_or_default();
        self.pos = end;
        if r.len() < n {
            self.eof = true;
            self.fail = true;
        }
        r
    }

    /// Sign and digits of `num_get::_M_extract_int` in base 10.
    /// Returns (negative, magnitude, overflowed, any digit seen), or `None` if the sentry failed.
    fn extract_int(&mut self) -> Option<(bool, u64, bool, bool)> {
        if !self.sentry() {
            return None;
        }
        let mut negative = false;
        if let Some(c @ (b'+' | b'-')) = self.peek() {
            negative = c == b'-';
            self.pos += 1;
        }
        let mut found = false;
        while self.peek() == Some(b'0') {
            found = true;
            self.pos += 1;
        }
        let mut mag: u64 = 0;
        let mut over = false;
        while let Some(c) = self.peek().filter(u8::is_ascii_digit) {
            found = true;
            let d = u64::from(c - b'0');
            match mag.checked_mul(10).and_then(|m| m.checked_add(d)) {
                Some(m) => mag = m,
                None => over = true,
            }
            self.pos += 1;
        }
        if self.pos == self.data.len() {
            self.eof = true;
        }
        Some((negative, mag, over, found))
    }

    /// `in >> long`.
    fn long(&mut self) -> Option<i64> {
        let (neg, mag, over, found) = self.extract_int()?;
        if !found {
            self.fail = true;
            return Some(0);
        }
        let lim = if neg { 1u64 << 63 } else { (1u64 << 63) - 1 };
        if over || mag > lim {
            self.fail = true;
            return Some(if neg { i64::MIN } else { i64::MAX });
        }
        Some(if neg {
            (mag as i64).wrapping_neg()
        } else {
            mag as i64
        })
    }

    /// `in >> int`: parsed as long, then range checked.
    pub(crate) fn int(&mut self, v: &mut i32) {
        let Some(l) = self.long() else { return };
        match i32::try_from(l) {
            Ok(x) => *v = x,
            Err(_) => {
                self.fail = true;
                *v = if l < 0 { i32::MIN } else { i32::MAX };
            }
        }
    }

    /// `in >> bool` without boolalpha.
    pub(crate) fn boolean(&mut self, v: &mut bool) {
        let Some(l) = self.long() else { return };
        if l == 0 || l == 1 {
            *v = l == 1;
        } else {
            *v = true;
            self.fail = true;
        }
    }

    /// `in >> size_t`: a minus sign negates modulo 2^64.
    pub(crate) fn size(&mut self, v: &mut usize) {
        let Some((neg, mag, over, found)) = self.extract_int() else {
            return;
        };
        if !found {
            self.fail = true;
            *v = 0;
        } else if over {
            self.fail = true;
            *v = usize::MAX;
        } else {
            *v = if neg {
                (mag as usize).wrapping_neg()
            } else {
                mag as usize
            };
        }
    }

    /// `in >> float`: only digits, a point and an exponent are collected; `strtof` converts them.
    pub(crate) fn float(&mut self, v: &mut f32) {
        if !self.sentry() {
            return;
        }
        let mut text = Vec::new();
        if let Some(c @ (b'+' | b'-')) = self.peek() {
            text.push(c);
            self.pos += 1;
        }
        let (mut point, mut exp, mut mantissa) = (false, false, false);
        while let Some(c) = self.peek() {
            let take = if c.is_ascii_digit() {
                if !exp {
                    mantissa = true;
                }
                true
            } else if c == b'.' && !point && !exp {
                point = true;
                true
            } else if (c == b'e' || c == b'E') && !exp && mantissa {
                exp = true;
                true
            } else {
                (c == b'+' || c == b'-') && matches!(text.last(), Some(b'e' | b'E'))
            };
            if !take {
                break;
            }
            text.push(c);
            self.pos += 1;
        }
        if self.pos == self.data.len() {
            self.eof = true;
        }
        match cfmt::c_strtof_full(&text) {
            Some(f) if f.is_infinite() => {
                *v = f.signum() * f32::MAX;
                self.fail = true;
            }
            Some(f) => *v = f,
            None => {
                *v = 0.0;
                self.fail = true;
            }
        }
    }
}
