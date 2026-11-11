//! The decisions of `SomaFsb.cpp` around the FSB5 parser and muxers in `crate::fsb5`: which
//! modes extract, which cached files count, output names, the multichannel downmix.

use crate::math::{clamp, fma};

pub const MODE_PCM16: u32 = 2;
pub const MODE_IMA_ADPCM: u32 = 7;
pub const MODE_VORBIS: u32 = 15;

pub fn mode_supported(mode: u32) -> bool {
    matches!(mode, MODE_PCM16 | MODE_IMA_ADPCM | MODE_VORBIS)
}

/// 0: the sample must be extracted, 1: cached as `.ogg`, 2: cached as `.wav`.
pub fn cached_state(has_ogg: bool, has_wav: bool) -> u8 {
    if has_ogg {
        1
    } else if has_wav {
        2
    } else {
        0
    }
}

/// The cache file name `prefix + sample + ext` for a freshly extracted sample.
pub fn output_name(prefix: &[u8], sample: &[u8], mode: u32, channels: i32) -> Vec<u8> {
    let ext: &[u8] = if mode == MODE_VORBIS && channels <= 2 {
        b".ogg"
    } else {
        b".wav"
    };
    [prefix, sample, ext].concat()
}

/// Index of the first wanted name equal to `sample` (`ExtractBank` stops at the first).
pub fn wanted_index(sample: &[u8], wanted: &[&[u8]]) -> Option<usize> {
    wanted.iter().position(|w| *w == sample)
}

/// Mixes `n` frames of planar float channels into interleaved stereo 16 bit; `left`/`right` are the
/// per-channel mix weights.
pub fn downmix_block(planes: &[&[f32]], left: &[f32], right: &[f32], n: usize, out: &mut Vec<i16>) {
    for i in 0..n {
        let (mut l, mut r) = (0.0f32, 0.0f32);
        for (c, plane) in planes.iter().enumerate() {
            let x = plane.get(i).copied().unwrap_or(0.0);
            l = fma(x, left.get(c).copied().unwrap_or(0.0), l);
            r = fma(x, right.get(c).copied().unwrap_or(0.0), r);
        }
        out.push(clamp(l * 32767.0, -32768.0, 32767.0) as i16);
        out.push(clamp(r * 32767.0, -32768.0, 32767.0) as i16);
    }
}
