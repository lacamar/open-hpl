//! C ABI for `fsb5.rs`. Inputs are (pointer, length) pairs owned by the caller; outputs go through
//! a sink callback. Setup-packet lookup, file I/O and libvorbis stay in C++.

use crate::fsb5::{self, Sample};
use crate::string_ffi::{Sink, emit, units};
use std::ffi::c_void;
use std::ptr;

#[repr(C)]
pub struct FsbSample {
    name: *const u8,
    name_len: usize,
    frequency: u32,
    channels: i32,
    data_offset: usize,
    data_size: usize,
    num_pcm_samples: u32,
    has_vorbis_crc: bool,
    vorbis_crc: u32,
}

impl FsbSample {
    fn from_core(file: &[u8], s: &Sample) -> Self {
        let name = &file[s.name.clone()];
        Self {
            name: name.as_ptr(),
            name_len: name.len(),
            frequency: s.frequency,
            channels: s.channels,
            data_offset: s.data_offset,
            data_size: s.data_size,
            num_pcm_samples: s.num_pcm_samples,
            has_vorbis_crc: s.vorbis_crc.is_some(),
            vorbis_crc: s.vorbis_crc.unwrap_or(0),
        }
    }

    fn to_core(&self) -> Sample {
        Sample {
            name: 0..0,
            frequency: self.frequency,
            channels: self.channels,
            data_offset: self.data_offset,
            data_size: self.data_size,
            num_pcm_samples: self.num_pcm_samples,
            vorbis_crc: self.has_vorbis_crc.then_some(self.vorbis_crc),
        }
    }
}

type SampleSink = unsafe extern "C" fn(*mut c_void, *const FsbSample);

/// # Safety
/// `file` readable for `n` bytes; `mode` writable; `sink` accepts `ctx` and a sample whose name
/// points into `file`. Returns false (nothing emitted, `mode` untouched) on a malformed bank.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_fsb_parse(
    file: *const u8,
    n: usize,
    mode: *mut u32,
    sink: SampleSink,
    ctx: *mut c_void,
) -> bool {
    // SAFETY: caller contract.
    let file = unsafe { units(file, n) };
    let Some(parsed) = fsb5::parse(file) else {
        return false;
    };
    // SAFETY: caller contract.
    unsafe { *mode = parsed.mode };
    for s in &parsed.samples {
        // SAFETY: caller contract.
        unsafe { sink(ctx, &FsbSample::from_core(file, s)) };
    }
    true
}

/// # Safety
/// `sample` is valid; `file` readable for `n` bytes; `sink` accepts `ctx`. Returns false when the
/// mode is not PCM16/IMA or the sample data lies outside the file.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_fsb_encode_pcm(
    mode: u32,
    sample: *const FsbSample,
    file: *const u8,
    n: usize,
    sink: Sink<u8>,
    ctx: *mut c_void,
) -> bool {
    // SAFETY: caller contract.
    let (s, file) = unsafe { ((*sample).to_core(), units(file, n)) };
    match fsb5::encode_pcm(mode, &s, file) {
        // SAFETY: caller contract.
        Some(out) => unsafe { emit(sink, ctx, &out) },
        None => return false,
    }
    true
}

/// # Safety
/// As `ohpl_fsb_encode_pcm`; `setup` readable for `setup_len` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_fsb_mux_vorbis(
    sample: *const FsbSample,
    file: *const u8,
    n: usize,
    setup: *const u8,
    setup_len: usize,
    sink: Sink<u8>,
    ctx: *mut c_void,
) -> bool {
    // SAFETY: caller contract.
    let (s, file, setup) =
        unsafe { ((*sample).to_core(), units(file, n), units(setup, setup_len)) };
    match fsb5::mux_vorbis(&s, file, setup) {
        // SAFETY: caller contract.
        Some(out) => unsafe { emit(sink, ctx, &out) },
        None => return false,
    }
    true
}

/// # Safety
/// `sample` is valid; `pcm` readable for `n` bytes; `sink` accepts `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_fsb_wav_pcm16(
    sample: *const FsbSample,
    pcm: *const u8,
    n: usize,
    sink: Sink<u8>,
    ctx: *mut c_void,
) {
    // SAFETY: caller contract.
    let (s, pcm) = unsafe { ((*sample).to_core(), units(pcm, n)) };
    let out = fsb5::write_pcm16_wav(s.frequency, s.channels, s.num_pcm_samples, pcm);
    // SAFETY: caller contract.
    unsafe { emit(sink, ctx, &out) }
}

/// # Safety
/// `left` and `right` are writable for `channels` floats.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_fsb_downmix_matrix(channels: usize, left: *mut f32, right: *mut f32) {
    let (l, r) = fsb5::downmix_matrix(channels);
    // SAFETY: caller contract; `l` and `r` hold `channels` floats.
    unsafe {
        ptr::copy_nonoverlapping(l.as_ptr(), left, channels);
        ptr::copy_nonoverlapping(r.as_ptr(), right, channels);
    }
}
