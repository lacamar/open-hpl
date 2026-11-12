//! Sample decoding as OALWrapper does it (`OAL_WAVSample`, `OAL_OggSample`): a RIFF/WAVE parse
//! in Rust and Ogg Vorbis through `cshim/vorbis_shim.c`. Used to hash decoded audio against the
//! original loaders; playback itself stays in OALWrapper.

use core::ffi::{c_char, c_long};

#[derive(Debug, PartialEq)]
pub struct Pcm {
    pub channels: i32,
    pub frequency: i32,
    pub bits: i32,
    pub samples: i64,
    pub data: Vec<u8>,
}

fn i32_at(b: &[u8], p: usize) -> Option<i32> {
    Some(i32::from_le_bytes(
        b.get(p..p.checked_add(4)?)?.try_into().ok()?,
    ))
}

fn i16_at(b: &[u8], p: usize) -> Option<i16> {
    Some(i16::from_le_bytes(
        b.get(p..p.checked_add(2)?)?.try_into().ok()?,
    ))
}

/// `find_chunk`: the offset of the first chunk named `id` at or after `start`. A chunk size that
/// would move the cursor backwards ends the search (the original looped or read out of bounds).
fn find_chunk(b: &[u8], start: usize, id: &[u8; 4]) -> Option<usize> {
    let mut p = start;
    while p < b.len().checked_sub(8)? {
        if b.get(p..p + 4)? == id {
            return Some(p);
        }
        let size = i32_at(b, p + 4)?;
        p = p.checked_add(8)?.checked_add(usize::try_from(size).ok()?)?;
    }
    None
}

/// `cOAL_WAVSample::CreateFromBuffer`.
pub fn decode_wav(b: &[u8]) -> Option<Pcm> {
    if b.get(..4)? != b"RIFF" || b.get(8..12)? != b"WAVE" {
        return None;
    }
    let fmt = find_chunk(b, 12, b"fmt ")?;
    // WAVE_Format is 24 bytes starting at the chunk header
    let format_tag = i16_at(b, fmt + 8)?;
    let channels = i16_at(b, fmt + 10)?;
    let rate = i32_at(b, fmt + 12)?;
    let bits = i16_at(b, fmt + 22)?;
    if format_tag != 1 {
        return None;
    }
    let after_fmt = fmt
        .checked_add(8)?
        .checked_add(usize::try_from(i32_at(b, fmt + 4)?).ok()?)?;
    let data = find_chunk(b, after_fmt, b"data")?;
    let base = data + 8;
    let size = usize::try_from(i32_at(b, data + 4)?).ok()?;
    let bytes = b.get(base..base.checked_add(size)?)?;
    let size = i64::try_from(size).ok()?;
    let (bits, samples) = match (channels == 2, bits == 8) {
        (true, true) => (8, size >> 1),
        (true, false) => (16, size >> 2),
        (false, true) => (8, size),
        (false, false) => (16, size >> 1),
    };
    Some(Pcm {
        channels: i32::from(channels),
        frequency: rate,
        bits,
        samples,
        data: bytes.to_vec(),
    })
}

#[repr(C)]
struct Vorbis {
    _private: [u8; 0],
}

unsafe extern "C" {
    fn ohpl_vorbis_open(path: *const c_char) -> *mut Vorbis;
    fn ohpl_vorbis_info(v: *mut Vorbis, channels: *mut i32, rate: *mut c_long, total: *mut c_long);
    fn ohpl_vorbis_next(v: *mut Vorbis, buf: *mut c_char, len: i32) -> c_long;
    fn ohpl_vorbis_close(v: *mut Vorbis);
}

/// `STREAMING_BLOCK_SIZE` of OALWrapper.
const BLOCK: usize = 4096 * 4;

/// `cOAL_OggSample::LoadOgg` on an Ogg file: 16-bit little-endian interleaved PCM.
pub fn decode_ogg(path: &std::ffi::CStr) -> Option<Pcm> {
    // SAFETY: `path` is NUL-terminated.
    let v = unsafe { ohpl_vorbis_open(path.as_ptr()) };
    if v.is_null() {
        return None;
    }
    let (mut channels, mut rate, mut total) = (0, 0, 0);
    // SAFETY: `v` is open.
    unsafe { ohpl_vorbis_info(v, &mut channels, &mut rate, &mut total) };
    let mut data = Vec::new();
    let mut chunk = vec![0u8; BLOCK];
    let ok = loop {
        // SAFETY: `chunk` is `BLOCK` bytes.
        let n = unsafe { ohpl_vorbis_next(v, chunk.as_mut_ptr().cast(), BLOCK as i32) };
        if n == 0 {
            break true;
        }
        let Some(read) = usize::try_from(n).ok().and_then(|n| chunk.get(..n)) else {
            break false;
        };
        data.extend_from_slice(read);
    };
    // SAFETY: `v` is open and closed once.
    unsafe { ohpl_vorbis_close(v) };
    ok.then_some(Pcm {
        channels,
        frequency: i32::try_from(rate).ok()?,
        bits: 16,
        samples: total,
        data,
    })
}

/// FNV-1a over the header fields (little-endian) and the sample bytes.
pub fn hash(p: &Pcm) -> u64 {
    let mut h = 0xcbf2_9ce4_8422_2325u64;
    let mut eat = |bytes: &[u8]| {
        for &b in bytes {
            h ^= u64::from(b);
            h = h.wrapping_mul(0x0000_0100_0000_01b3);
        }
    };
    eat(&p.channels.to_le_bytes());
    eat(&p.frequency.to_le_bytes());
    eat(&p.bits.to_le_bytes());
    eat(&p.samples.to_le_bytes());
    eat(&(p.data.len() as u64).to_le_bytes());
    eat(&p.data);
    h
}
