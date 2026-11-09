//! Growable little-endian byte buffer with zlib and CRC sections (`hpl::cBinaryBuffer`).

use crate::crc::Crc;
use std::ffi::{CStr, c_char, c_int, c_uint, c_ulong, c_void};

const INITIAL_RESERVE: usize = 100;
const CHUNK: usize = 65536;

#[repr(C)]
struct ZStream {
    next_in: *const u8,
    avail_in: c_uint,
    total_in: c_ulong,
    next_out: *mut u8,
    avail_out: c_uint,
    total_out: c_ulong,
    msg: *const c_char,
    state: *mut c_void,
    zalloc: *const c_void,
    zfree: *const c_void,
    opaque: *mut c_void,
    data_type: c_int,
    adler: c_ulong,
    reserved: c_ulong,
}

#[link(name = "z")]
unsafe extern "C" {
    fn deflateInit_(s: *mut ZStream, level: c_int, version: *const c_char, size: c_int) -> c_int;
    fn deflate(s: *mut ZStream, flush: c_int) -> c_int;
    fn deflateEnd(s: *mut ZStream) -> c_int;
    fn inflateInit_(s: *mut ZStream, version: *const c_char, size: c_int) -> c_int;
    fn inflate(s: *mut ZStream, flush: c_int) -> c_int;
    fn inflateEnd(s: *mut ZStream) -> c_int;
}

const Z_OK: c_int = 0;
const Z_STREAM_END: c_int = 1;
const Z_STREAM_ERROR: c_int = -2;
const Z_FINISH: c_int = 4;
const Z_NO_FLUSH: c_int = 0;
// zlib only checks the major version character and the struct size.
const ZLIB_VERSION: &CStr = c"1.3.1";

fn new_zstream() -> ZStream {
    ZStream {
        next_in: std::ptr::null(),
        avail_in: 0,
        total_in: 0,
        next_out: std::ptr::null_mut(),
        avail_out: 0,
        total_out: 0,
        msg: std::ptr::null(),
        state: std::ptr::null_mut(),
        zalloc: std::ptr::null(),
        zfree: std::ptr::null(),
        opaque: std::ptr::null_mut(),
        data_type: 0,
        adler: 0,
        reserved: 0,
    }
}

/// `reserved` is the logical capacity the C++ reported; `buf.len()` is at least `max(reserved, size)`.
/// `size` can exceed `reserved` after overwriting mid-buffer, as in the C++.
pub struct BinaryBuffer {
    buf: Vec<u8>,
    reserved: usize,
    pos: usize,
    size: usize,
    crc_start: usize,
    scratch: Vec<u8>,
}

impl Default for BinaryBuffer {
    fn default() -> Self {
        Self::new()
    }
}

impl BinaryBuffer {
    pub fn new() -> Self {
        Self {
            buf: vec![0; INITIAL_RESERVE],
            reserved: INITIAL_RESERVE,
            pos: 0,
            size: 0,
            crc_start: 0,
            scratch: Vec::new(),
        }
    }

    pub fn clear(&mut self) {
        let scratch = std::mem::take(&mut self.scratch);
        *self = Self::new();
        self.scratch = scratch;
    }

    /// Replaces the contents; reserved size equals `data.len()`, position 0.
    pub fn assign(&mut self, data: &[u8]) {
        self.buf = data.to_vec();
        self.reserved = data.len();
        self.size = data.len();
        self.pos = 0;
    }

    pub fn data_ptr(&mut self) -> *mut u8 {
        self.buf.as_mut_ptr()
    }

    pub fn size(&self) -> usize {
        self.size
    }

    pub fn reserved(&self) -> usize {
        self.reserved
    }

    pub fn pos(&self) -> usize {
        self.pos
    }

    pub fn reserve(&mut self, size: usize) -> bool {
        if size <= self.reserved {
            return false;
        }
        if size > self.buf.len() {
            if self.buf.try_reserve_exact(size - self.buf.len()).is_err() {
                return false;
            }
            self.buf.resize(size, 0);
        }
        self.reserved = size;
        true
    }

    pub fn set_pos(&mut self, pos: usize) -> bool {
        if pos >= self.size {
            return false;
        }
        self.pos = pos;
        true
    }

    pub fn add_pos(&mut self, n: usize) -> bool {
        match self.pos.checked_add(n) {
            Some(p) if p < self.size => {
                self.pos = p;
                true
            }
            _ => false,
        }
    }

    /// Writes at the position, then grows `size` by `data.len()` even when overwriting.
    pub fn add_data(&mut self, data: &[u8]) {
        // C++ wrote past its allocation when the position ran far beyond the end (see decompress_from)
        if self.pos.saturating_sub(self.size) > i32::MAX as usize {
            self.pos = self.size;
        }
        let end = self.pos + data.len();
        if end > self.reserved {
            self.reserved = self.size * 2 + data.len();
        }
        let need = self.reserved.max(end).max(self.size + data.len());
        if need > self.buf.len() {
            self.buf.resize(need, 0);
        }
        if let Some(dst) = self.buf.get_mut(self.pos..end) {
            dst.copy_from_slice(data);
        }
        self.pos = end;
        self.size += data.len();
    }

    /// Fails without copying when fewer than `out.len()` bytes remain, moving to EOF.
    pub fn get_data(&mut self, out: &mut [u8]) -> bool {
        if let Some(end) = self.pos.checked_add(out.len()).filter(|&e| e <= self.size)
            && let Some(src) = self.buf.get(self.pos..end)
        {
            out.copy_from_slice(src);
            self.pos = end;
            return true;
        }
        self.pos = self.size;
        false
    }

    /// `per_elem` adds element by element, which sets the growth points; otherwise one add.
    pub fn add_array(&mut self, native: &[u8], elem: usize, per_elem: bool) {
        let mut le = native.to_vec();
        if cfg!(target_endian = "big") {
            le.chunks_exact_mut(elem).for_each(<[u8]>::reverse);
        }
        if per_elem {
            le.chunks(elem).for_each(|c| self.add_data(c));
        } else {
            self.add_data(&le);
        }
    }

    /// Per-element reads: elements still available are filled, the rest are zeroed.
    pub fn get_array(&mut self, out: &mut [u8], elem: usize) {
        for chunk in out.chunks_exact_mut(elem) {
            if !self.get_data(chunk) {
                chunk.fill(0);
            } else if cfg!(target_endian = "big") {
                chunk.reverse();
            }
        }
    }

    pub fn get_u8(&mut self) -> u8 {
        let mut b = [0];
        self.get_data(&mut b);
        b[0]
    }

    pub fn get_i32(&mut self) -> i32 {
        let mut b = [0; 4];
        self.get_data(&mut b);
        i32::from_le_bytes(b)
    }

    pub fn add_i32(&mut self, v: i32) {
        self.add_data(&v.to_le_bytes());
    }

    /// Stops at NUL or EOF; a last byte with no terminator is dropped.
    pub fn get_string(&mut self) -> &[u8] {
        let mut s = std::mem::take(&mut self.scratch);
        s.clear();
        let mut c = self.get_u8();
        while c != 0 && self.pos < self.size {
            s.push(c);
            c = self.get_u8();
        }
        self.scratch = s;
        &self.scratch
    }

    pub fn set_i32(&mut self, v: i32, pos: usize) {
        if let Some(end) = pos.checked_add(4)
            && end < self.size
            && let Some(dst) = self.buf.get_mut(pos..end)
        {
            dst.copy_from_slice(&v.to_le_bytes());
        }
    }

    pub fn xor_transform(&mut self, key: &[u8]) {
        if key.is_empty() {
            return;
        }
        let size = self.size;
        self.buf
            .get_mut(..size)
            .unwrap_or_default()
            .iter_mut()
            .zip(key.iter().cycle())
            .for_each(|(b, k)| *b ^= k);
    }

    pub fn crc_begin(&mut self) {
        self.crc_start = self.pos;
        self.add_i32(0);
    }

    pub fn crc_end(&mut self, key: u32) -> u32 {
        let start = self.crc_start;
        let crc = self.get_crc(
            key,
            start.wrapping_add(4) as i32,
            self.size.wrapping_sub(start).wrapping_sub(4) as i32,
        );
        if let Some(end) = start.checked_add(4).filter(|&e| e <= self.reserved)
            && let Some(dst) = self.buf.get_mut(start..end)
        {
            dst.copy_from_slice(&crc.to_le_bytes());
        }
        self.crc_start = 0;
        crc
    }

    /// Negative `data_pos` means the current position, negative `count` means to the end.
    /// Ranges outside the data are clamped (the C++ read out of bounds).
    pub fn get_crc(&self, key: u32, data_pos: i32, count: i32) -> u32 {
        let pos = if data_pos < 0 {
            self.pos
        } else {
            data_pos as usize
        };
        let count = if count < 0 {
            self.size.saturating_sub(pos)
        } else {
            count as usize
        };
        let start = pos.min(self.size);
        let end = pos.saturating_add(count).min(self.size);
        let mut crc = Crc::new(key);
        crc.put_data(self.buf.get(start..end).unwrap_or_default());
        crc.done()
    }

    pub fn check_internal_crc(&mut self, key: u32, count: i32) -> bool {
        let count = if count > 0 { count - 4 } else { count };
        let saved = self.get_i32() as u32;
        saved == self.get_crc(key, -1, count)
    }

    pub fn check_crc(&self, crc: u32, key: u32, count: i32) -> bool {
        crc == self.get_crc(key, -1, count)
    }

    /// `level` below zero selects zlib's default. With `write_size` the compressed length
    /// is stored in the leading int32 (the C++ crashed there).
    pub fn compress_and_add(&mut self, src: &[u8], level: i32, write_size: bool) -> bool {
        if level > 9 {
            return false;
        }
        let start = self.pos;
        if write_size {
            self.add_i32(0);
        }
        let mut z = new_zstream();
        // SAFETY: `z` is a zeroed stream; version and size describe this struct.
        let ret = unsafe {
            deflateInit_(
                &mut z,
                level.max(-1),
                ZLIB_VERSION.as_ptr(),
                size_of::<ZStream>() as c_int,
            )
        };
        if ret != Z_OK {
            return false;
        }
        z.avail_in = src.len() as c_uint;
        z.next_in = src.as_ptr();
        let mut out = vec![0u8; CHUNK];
        loop {
            z.avail_out = CHUNK as c_uint;
            z.next_out = out.as_mut_ptr();
            // SAFETY: stream initialised above; next_in/next_out point to live buffers of avail_* bytes.
            let ret = unsafe { deflate(&mut z, Z_FINISH) };
            if ret == Z_STREAM_ERROR {
                // SAFETY: stream was initialised.
                unsafe { deflateEnd(&mut z) };
                return false;
            }
            self.add_data(out.get(..CHUNK - z.avail_out as usize).unwrap_or_default());
            if z.avail_out != 0 {
                break;
            }
        }
        // SAFETY: stream was initialised.
        unsafe { deflateEnd(&mut z) };
        if write_size {
            let total = (self.pos - start - 4) as i32;
            if let Some(dst) = self.buf.get_mut(start..start + 4) {
                dst.copy_from_slice(&total.to_le_bytes());
            }
        }
        true
    }

    /// Partial output stays added when inflate fails midway or the input is truncated.
    pub fn decompress_and_add(&mut self, src: &[u8]) -> bool {
        let mut z = new_zstream();
        z.avail_in = src.len() as c_uint;
        z.next_in = src.as_ptr();
        // SAFETY: `z` is a zeroed stream; version and size describe this struct.
        let ret =
            unsafe { inflateInit_(&mut z, ZLIB_VERSION.as_ptr(), size_of::<ZStream>() as c_int) };
        if ret != Z_OK {
            return false;
        }
        let mut out = vec![0u8; CHUNK];
        loop {
            z.avail_out = CHUNK as c_uint;
            z.next_out = out.as_mut_ptr();
            // SAFETY: stream initialised above; next_in/next_out point to live buffers of avail_* bytes.
            let ret = unsafe { inflate(&mut z, Z_NO_FLUSH) };
            if ret != Z_OK && ret != Z_STREAM_END {
                // SAFETY: stream was initialised.
                unsafe { inflateEnd(&mut z) };
                return false;
            }
            self.add_data(out.get(..CHUNK - z.avail_out as usize).unwrap_or_default());
            if z.avail_out != 0 {
                break;
            }
        }
        // SAFETY: stream was initialised.
        unsafe { inflateEnd(&mut z) };
        true
    }

    /// Reads the compressed span at `src`'s position and advances it by the span length,
    /// even past the end. A span longer than the data is clamped (the C++ read out of bounds).
    pub fn decompress_from(&mut self, src: &mut BinaryBuffer, size_written: bool) -> bool {
        let n = if size_written {
            src.get_i32() as isize as usize
        } else {
            src.size.wrapping_sub(src.pos)
        };
        let from = src.pos.min(src.size);
        let to = from.saturating_add(n).min(src.size);
        let ok = self.decompress_and_add(src.buf.get(from..to).unwrap_or_default());
        src.pos = src.pos.wrapping_add(n);
        ok
    }
}

mod ffi {
    use super::BinaryBuffer;
    use std::slice;

    /// Copies `ptr[..len]` when it overlaps the buffer's own storage, which a growth would free.
    ///
    /// # Safety
    /// `ptr` addresses `len` readable bytes (or `len` is 0).
    unsafe fn input<'a>(
        bb: &BinaryBuffer,
        ptr: *const u8,
        len: usize,
        tmp: &'a mut Vec<u8>,
    ) -> &'a [u8] {
        if len == 0 {
            return &[];
        }
        let own = bb.buf.as_ptr_range();
        let end = ptr.wrapping_add(len);
        if ptr < own.end && end > own.start {
            // SAFETY: caller contract.
            tmp.extend_from_slice(unsafe { slice::from_raw_parts(ptr, len) });
            tmp
        } else {
            // SAFETY: caller contract; the range is disjoint from the buffer's storage.
            unsafe { slice::from_raw_parts(ptr, len) }
        }
    }

    #[unsafe(no_mangle)]
    pub extern "C" fn ohpl_bb_new() -> *mut BinaryBuffer {
        Box::into_raw(Box::new(BinaryBuffer::new()))
    }

    /// # Safety
    /// `bb` comes from `ohpl_bb_new` and is not used afterwards.
    #[unsafe(no_mangle)]
    pub unsafe extern "C" fn ohpl_bb_free(bb: *mut BinaryBuffer) {
        // SAFETY: caller contract.
        drop(unsafe { Box::from_raw(bb) });
    }

    /// # Safety
    /// `bb` is live.
    #[unsafe(no_mangle)]
    pub unsafe extern "C" fn ohpl_bb_clear(bb: *mut BinaryBuffer) {
        // SAFETY: caller contract.
        unsafe { &mut *bb }.clear();
    }

    /// # Safety
    /// `bb` is live; `data` points to `len` readable bytes (or `len` is 0).
    #[unsafe(no_mangle)]
    pub unsafe extern "C" fn ohpl_bb_assign(bb: *mut BinaryBuffer, data: *const u8, len: usize) {
        // SAFETY: caller contract.
        let bb = unsafe { &mut *bb };
        let mut tmp = Vec::new();
        // SAFETY: caller contract.
        let src = unsafe { input(bb, data, len, &mut tmp) }.to_vec();
        bb.assign(&src);
    }

    /// # Safety
    /// `bb` is live. The pointer is valid until the next growing call.
    #[unsafe(no_mangle)]
    pub unsafe extern "C" fn ohpl_bb_data(bb: *mut BinaryBuffer) -> *mut u8 {
        // SAFETY: caller contract.
        unsafe { &mut *bb }.data_ptr()
    }

    /// # Safety
    /// `bb` is live.
    #[unsafe(no_mangle)]
    pub unsafe extern "C" fn ohpl_bb_size(bb: *const BinaryBuffer) -> usize {
        // SAFETY: caller contract.
        unsafe { &*bb }.size()
    }

    /// # Safety
    /// `bb` is live.
    #[unsafe(no_mangle)]
    pub unsafe extern "C" fn ohpl_bb_reserved(bb: *const BinaryBuffer) -> usize {
        // SAFETY: caller contract.
        unsafe { &*bb }.reserved()
    }

    /// # Safety
    /// `bb` is live.
    #[unsafe(no_mangle)]
    pub unsafe extern "C" fn ohpl_bb_pos(bb: *const BinaryBuffer) -> usize {
        // SAFETY: caller contract.
        unsafe { &*bb }.pos()
    }

    /// # Safety
    /// `bb` is live.
    #[unsafe(no_mangle)]
    pub unsafe extern "C" fn ohpl_bb_reserve(bb: *mut BinaryBuffer, size: usize) -> bool {
        // SAFETY: caller contract.
        unsafe { &mut *bb }.reserve(size)
    }

    /// # Safety
    /// `bb` is live.
    #[unsafe(no_mangle)]
    pub unsafe extern "C" fn ohpl_bb_set_pos(bb: *mut BinaryBuffer, pos: usize) -> bool {
        // SAFETY: caller contract.
        unsafe { &mut *bb }.set_pos(pos)
    }

    /// # Safety
    /// `bb` is live.
    #[unsafe(no_mangle)]
    pub unsafe extern "C" fn ohpl_bb_add_pos(bb: *mut BinaryBuffer, n: usize) -> bool {
        // SAFETY: caller contract.
        unsafe { &mut *bb }.add_pos(n)
    }

    /// # Safety
    /// `bb` is live; `data` points to `elem * count` readable bytes in host byte order.
    #[unsafe(no_mangle)]
    pub unsafe extern "C" fn ohpl_bb_add_array(
        bb: *mut BinaryBuffer,
        data: *const u8,
        elem: usize,
        count: usize,
        per_elem: bool,
    ) {
        // SAFETY: caller contract.
        let bb = unsafe { &mut *bb };
        let Some(len) = elem.checked_mul(count) else {
            return;
        };
        let mut tmp = Vec::new();
        // SAFETY: caller contract.
        let src = unsafe { input(bb, data, len, &mut tmp) };
        bb.add_array(src, elem.max(1), per_elem);
    }

    /// # Safety
    /// `bb` is live; `out` points to `elem * count` writable bytes.
    #[unsafe(no_mangle)]
    pub unsafe extern "C" fn ohpl_bb_get_array(
        bb: *mut BinaryBuffer,
        out: *mut u8,
        elem: usize,
        count: usize,
    ) {
        // SAFETY: caller contract.
        let bb = unsafe { &mut *bb };
        let Some(len) = elem.checked_mul(count) else {
            return;
        };
        if len == 0 {
            return;
        }
        let mut tmp = vec![0; len];
        bb.get_array(&mut tmp, elem);
        // SAFETY: caller contract; `tmp` is a separate allocation.
        unsafe { std::ptr::copy_nonoverlapping(tmp.as_ptr(), out, len) };
    }

    /// # Safety
    /// `bb` is live; `out` points to `len` writable bytes, left untouched on failure.
    #[unsafe(no_mangle)]
    pub unsafe extern "C" fn ohpl_bb_get_bytes(
        bb: *mut BinaryBuffer,
        out: *mut u8,
        len: usize,
    ) -> bool {
        // SAFETY: caller contract.
        let bb = unsafe { &mut *bb };
        let mut tmp = vec![0; len];
        let ok = bb.get_data(&mut tmp);
        if ok && len > 0 {
            // SAFETY: caller contract; `tmp` is a separate allocation.
            unsafe { std::ptr::copy_nonoverlapping(tmp.as_ptr(), out, len) };
        }
        ok
    }

    /// Returns the bytes without NUL; valid until the next call on `bb`.
    ///
    /// # Safety
    /// `bb` is live; `len` is writable.
    #[unsafe(no_mangle)]
    pub unsafe extern "C" fn ohpl_bb_get_string(
        bb: *mut BinaryBuffer,
        len: *mut usize,
    ) -> *const u8 {
        // SAFETY: caller contract.
        let s = unsafe { &mut *bb }.get_string();
        // SAFETY: caller contract.
        unsafe { *len = s.len() };
        s.as_ptr()
    }

    /// # Safety
    /// `bb` is live; `src` points to `len` readable bytes.
    #[unsafe(no_mangle)]
    pub unsafe extern "C" fn ohpl_bb_compress_and_add(
        bb: *mut BinaryBuffer,
        src: *const u8,
        len: usize,
        level: i32,
        write_size: bool,
    ) -> bool {
        if src.is_null() {
            return false;
        }
        // SAFETY: caller contract.
        let bb = unsafe { &mut *bb };
        let mut tmp = Vec::new();
        // SAFETY: caller contract.
        let src = unsafe { input(bb, src, len, &mut tmp) };
        bb.compress_and_add(src, level, write_size)
    }

    /// # Safety
    /// `bb` is live; `src` points to `len` readable bytes.
    #[unsafe(no_mangle)]
    pub unsafe extern "C" fn ohpl_bb_decompress_and_add(
        bb: *mut BinaryBuffer,
        src: *const u8,
        len: usize,
    ) -> bool {
        if src.is_null() {
            return false;
        }
        // SAFETY: caller contract.
        let bb = unsafe { &mut *bb };
        let mut tmp = Vec::new();
        // SAFETY: caller contract.
        let src = unsafe { input(bb, src, len, &mut tmp) };
        bb.decompress_and_add(src)
    }

    /// Returns false when `dst` and `src` are the same buffer.
    ///
    /// # Safety
    /// Both are live.
    #[unsafe(no_mangle)]
    pub unsafe extern "C" fn ohpl_bb_decompress_from(
        dst: *mut BinaryBuffer,
        src: *mut BinaryBuffer,
        size_written: bool,
    ) -> bool {
        if std::ptr::eq(dst, src) {
            return false;
        }
        // SAFETY: caller contract; distinct buffers checked above.
        unsafe { &mut *dst }.decompress_from(unsafe { &mut *src }, size_written)
    }

    /// # Safety
    /// `bb` is live; `key` points to `len` readable bytes.
    #[unsafe(no_mangle)]
    pub unsafe extern "C" fn ohpl_bb_xor(bb: *mut BinaryBuffer, key: *const u8, len: usize) {
        if len == 0 {
            return;
        }
        // SAFETY: caller contract.
        unsafe { &mut *bb }.xor_transform(unsafe { slice::from_raw_parts(key, len) });
    }

    /// # Safety
    /// `bb` is live.
    #[unsafe(no_mangle)]
    pub unsafe extern "C" fn ohpl_bb_crc_begin(bb: *mut BinaryBuffer) {
        // SAFETY: caller contract.
        unsafe { &mut *bb }.crc_begin();
    }

    /// # Safety
    /// `bb` is live.
    #[unsafe(no_mangle)]
    pub unsafe extern "C" fn ohpl_bb_crc_end(bb: *mut BinaryBuffer, key: u32) -> u32 {
        // SAFETY: caller contract.
        unsafe { &mut *bb }.crc_end(key)
    }

    /// # Safety
    /// `bb` is live.
    #[unsafe(no_mangle)]
    pub unsafe extern "C" fn ohpl_bb_get_crc(
        bb: *const BinaryBuffer,
        key: u32,
        pos: i32,
        count: i32,
    ) -> u32 {
        // SAFETY: caller contract.
        unsafe { &*bb }.get_crc(key, pos, count)
    }

    /// # Safety
    /// `bb` is live.
    #[unsafe(no_mangle)]
    pub unsafe extern "C" fn ohpl_bb_check_internal_crc(
        bb: *mut BinaryBuffer,
        key: u32,
        count: i32,
    ) -> bool {
        // SAFETY: caller contract.
        unsafe { &mut *bb }.check_internal_crc(key, count)
    }

    /// # Safety
    /// `bb` is live.
    #[unsafe(no_mangle)]
    pub unsafe extern "C" fn ohpl_bb_check_crc(
        bb: *const BinaryBuffer,
        crc: u32,
        key: u32,
        count: i32,
    ) -> bool {
        // SAFETY: caller contract.
        unsafe { &*bb }.check_crc(crc, key, count)
    }

    /// # Safety
    /// `bb` is live.
    #[unsafe(no_mangle)]
    pub unsafe extern "C" fn ohpl_bb_set_i32(bb: *mut BinaryBuffer, v: i32, pos: usize) {
        // SAFETY: caller contract.
        unsafe { &mut *bb }.set_i32(v, pos);
    }
}

#[cfg(test)]
mod tests {
    use super::BinaryBuffer;

    fn hex(b: &BinaryBuffer) -> String {
        b.buf[..b.size].iter().map(|x| format!("{x:02x}")).collect()
    }

    fn unhex(s: &str) -> Vec<u8> {
        (0..s.len())
            .step_by(2)
            .map(|i| u8::from_str_radix(&s[i..i + 2], 16).unwrap())
            .collect()
    }

    // Expected values recorded from the C++ cBinaryBuffer before cut-over.
    #[test]
    fn layout_is_little_endian() {
        let mut b = BinaryBuffer::new();
        b.add_array(&0x0102_0304i32.to_ne_bytes(), 4, true);
        b.add_array(&(-2i16).to_ne_bytes(), 2, true);
        b.add_array(&1.5f32.to_ne_bytes(), 4, true);
        b.add_array(b"hi\0", 1, false);
        b.add_array(&[1], 1, false);
        assert_eq!(hex(&b), "04030201feff0000c03f68690001");
        assert_eq!((b.size(), b.reserved()), (14, 100));
    }

    #[test]
    fn overwrite_grows_size_and_reserved_follows_growth_rule() {
        let mut b = BinaryBuffer::new();
        (0..25).for_each(|i| b.add_i32(i));
        assert_eq!((b.reserved(), b.size()), (100, 100));
        b.set_pos(0);
        (0..8).for_each(|_| b.add_array(&0f32.to_ne_bytes(), 4, true));
        assert_eq!((b.size(), b.pos(), b.reserved()), (132, 32, 100));
    }

    #[test]
    fn growth_happens_per_element() {
        let mut b = BinaryBuffer::new();
        b.add_data(&[0; 87]);
        b.add_array(&[0; 32], 4, true);
        assert_eq!((b.size(), b.reserved()), (119, 202));
    }

    #[test]
    fn get_string_drops_unterminated_last_byte() {
        let mut b = BinaryBuffer::new();
        b.add_data(b"ab\0cd");
        b.set_pos(0);
        assert_eq!(b.get_string(), b"ab");
        assert_eq!(b.pos(), 3);
        assert_eq!(b.get_string(), b"c");
        assert_eq!(b.pos(), 5);
    }

    #[test]
    fn short_reads_move_to_eof() {
        let mut b = BinaryBuffer::new();
        b.add_data(&[1, 2, 3]);
        b.set_pos(0);
        assert_eq!(b.get_i32(), 0);
        assert_eq!(b.pos(), 3);

        let mut b = BinaryBuffer::new();
        b.add_i32(0x1122_3344);
        b.add_i32(0x5566_7788);
        b.add_data(&[9]);
        b.set_pos(0);
        let mut out = [7u8; 16];
        b.get_array(&mut out, 4);
        assert_eq!(&out[..8], &[0x44, 0x33, 0x22, 0x11, 0x88, 0x77, 0x66, 0x55]);
        assert_eq!(&out[8..], &[0; 8]);
        assert_eq!(b.pos(), 9);
    }

    #[test]
    fn get_bytes_is_all_or_nothing() {
        let mut b = BinaryBuffer::new();
        b.add_data(&[1, 2, 3]);
        b.set_pos(0);
        let mut out = [9u8; 4];
        assert!(!b.get_data(&mut out));
        assert_eq!((out, b.pos()), ([9; 4], 3));
    }

    #[test]
    fn position_bounds_are_strict() {
        let mut b = BinaryBuffer::new();
        b.add_data(&[0, 1, 2, 3]);
        assert!(b.set_pos(3));
        assert!(!b.set_pos(4));
        b.set_pos(0);
        assert!(b.add_pos(2) && b.add_pos(1) && b.add_pos(0));
        assert_eq!(b.pos(), 3);
        assert!(!b.add_pos(1));
        assert!(!b.add_pos(usize::MAX));
    }

    #[test]
    fn set_i32_needs_room_past_the_word() {
        let mut b = BinaryBuffer::new();
        b.add_data(&[0, 1, 2, 3]);
        b.set_i32(-1, 0);
        assert_eq!(hex(&b), "00010203");
        b.set_pos(3);
        b.add_i32(0);
        b.set_i32(0x7f, 4);
        b.set_i32(0x7e, 5);
        assert_eq!(hex(&b), "0001020000000000");
        b.set_i32(0x7f, 3);
        assert_eq!(hex(&b), "0001027f00000000");
        b.set_i32(1, usize::MAX);
    }

    #[test]
    fn compress_matches_zlib_streams() {
        let src = b"hello hello hello hello";
        let cases = [
            (-1, "789ccb48cdc9c957c8402701680308b1"),
            (
                0,
                "7801011700e8ff68656c6c6f2068656c6c6f2068656c6c6f2068656c6c6f680308b1",
            ),
            (1, "7801cb48cdc9c957c8402701680308b1"),
            (9, "78dacb48cdc9c957c8402701680308b1"),
        ];
        for (level, want) in cases {
            let mut b = BinaryBuffer::new();
            assert!(b.compress_and_add(src, level, false), "level {level}");
            assert_eq!(hex(&b), want, "level {level}");
        }
        let mut b = BinaryBuffer::new();
        assert!(!b.compress_and_add(src, 10, false));
        assert_eq!(b.size(), 0);
    }

    #[test]
    fn compress_with_size_prefix_stores_length() {
        let mut b = BinaryBuffer::new();
        assert!(b.compress_and_add(b"hello hello hello hello", -1, true));
        assert_eq!(hex(&b), "10000000789ccb48cdc9c957c8402701680308b1");
    }

    #[test]
    fn large_streams_cross_chunk_boundaries() {
        for len in [65535, 65536, 65537, 200_000] {
            let src: Vec<u8> = (0..len).map(|i| (i / 7) as u8).collect();
            let mut comp = BinaryBuffer::new();
            assert!(comp.compress_and_add(&src, 6, false));
            let mut out = BinaryBuffer::new();
            assert!(out.decompress_and_add(&comp.buf[..comp.size]));
            assert_eq!(out.buf[..out.size], src[..], "len {len}");
        }
    }

    #[test]
    fn decompress_roundtrip_and_failures() {
        let mut comp = BinaryBuffer::new();
        comp.compress_and_add(b"hello hello hello hello", -1, false);
        comp.set_pos(0);
        let mut out = BinaryBuffer::new();
        assert!(out.decompress_from(&mut comp, false));
        assert_eq!(out.buf[..out.size], *b"hello hello hello hello");
        assert_eq!(comp.pos(), 16);

        let mut t = BinaryBuffer::new();
        assert!(t.decompress_and_add(&unhex("789ccb48cdc9")));
        assert_eq!(t.buf[..t.size], *b"hel");

        let mut e = BinaryBuffer::new();
        assert!(!e.decompress_and_add(b"garbage!"));
        assert_eq!(e.size(), 0);
    }

    #[test]
    fn decompress_from_clamps_oversized_span() {
        let mut src = BinaryBuffer::new();
        src.add_i32(1_000_000);
        src.add_data(&unhex("789ccb48cdc9c957c8402701680308b1"));
        src.set_pos(0);
        let mut out = BinaryBuffer::new();
        assert!(out.decompress_from(&mut src, true));
        assert_eq!(out.buf[..out.size], *b"hello hello hello hello");
        assert!(src.pos() > src.size());
    }

    #[test]
    fn add_data_with_position_far_past_end_appends() {
        let mut b = BinaryBuffer::new();
        b.add_data(b"abcd");
        b.pos = usize::MAX - 1;
        b.add_data(b"xy");
        assert_eq!((b.pos(), b.size()), (6, 6));
        assert_eq!(b.buf[..6], *b"abcdxy");
    }

    #[test]
    fn add_data_with_position_past_end_zero_fills() {
        let mut b = BinaryBuffer::new();
        b.add_data(b"ab");
        b.pos = 5;
        b.add_data(b"c");
        assert_eq!((b.pos(), b.size()), (6, 3));
        assert_eq!(b.buf[..6], *b"ab\0\0\0c");
    }

    #[test]
    fn crc_section_roundtrip() {
        let key = 0x16AF_2C1D;
        let mut b = BinaryBuffer::new();
        b.crc_begin();
        b.add_i32(5);
        b.add_data(b"data\0");
        assert_eq!(b.crc_end(key), 0x7795_328b);
        assert_eq!(hex(&b), "8b329577050000006461746100");
        b.set_pos(0);
        assert!(b.check_internal_crc(key, -1));
        assert_eq!(b.pos(), 4);
        assert_eq!(b.get_crc(key, 4, -1), 0x7795_328b);
        assert_eq!(b.get_crc(key, 4, 2), 0x1b81_22b5);
        assert!(b.check_crc(0x7795_328b, key, -1));
        assert!(!b.check_crc(0, key, -1));
    }

    #[test]
    fn crc_ranges_outside_the_data_are_clamped() {
        let mut b = BinaryBuffer::new();
        b.add_data(&[1, 2, 3]);
        let empty = b.get_crc(7, 3, -1);
        assert_eq!(b.get_crc(7, 99, -1), empty);
        assert_eq!(b.get_crc(7, 0, 99), b.get_crc(7, 0, -1));
    }

    #[test]
    fn xor_cycles_key() {
        let mut b = BinaryBuffer::new();
        b.add_data(&[0, 1, 2, 3, 4, 5]);
        b.xor_transform(&0x4E5F_16F0u32.to_le_bytes());
        assert_eq!(hex(&b), "f0175d4df413");
        b.xor_transform(&[]);
        assert_eq!(hex(&b), "f0175d4df413");
    }

    #[test]
    fn assign_and_reserve() {
        let mut b = BinaryBuffer::new();
        b.assign(&[1, 2, 3]);
        assert_eq!((b.size(), b.reserved(), b.pos()), (3, 3, 0));
        assert!(!b.reserve(3));
        assert!(b.reserve(10));
        assert_eq!(b.reserved(), 10);
        b.add_data(&[4]);
        assert_eq!((b.size(), b.reserved()), (4, 10));
    }

    #[test]
    fn zstream_matches_c_layout() {
        assert_eq!(size_of::<super::ZStream>(), 112);
    }
}
