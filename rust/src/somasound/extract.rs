//! `SomaFsb.cpp`: the cache directory and the extraction of bank samples into it.

use super::fsb::{MODE_VORBIS, cached_state, downmix_block, mode_supported, output_name};
use super::vorbis_setups::SETUPS;
use crate::fsb5::{self, Sample};
use crate::platform::{Kind, SystemEnv, special_path};
use std::collections::{BTreeMap, BTreeSet};
use std::ffi::{OsStr, c_long, c_void};
use std::os::unix::ffi::OsStrExt;
use std::path::Path;

unsafe extern "C" {
    fn ohpl_vorbis_open_mem(data: *const u8, len: usize) -> *mut c_void;
    fn ohpl_vorbis_read_float(v: *mut c_void, planes: *mut *mut *mut f32, frames: i32) -> c_long;
    fn ohpl_vorbis_clear_mem(v: *mut c_void);
}

fn path(b: &[u8]) -> &Path {
    Path::new(OsStr::from_bytes(b))
}

/// `$XDG_CACHE_HOME/open-hpl/<game>/<sub>/`, created if missing.
pub fn cache_dir(game: &[u8], sub: &[u8]) -> Vec<u8> {
    let mut dir = special_path(&SystemEnv, Kind::CacheHome).unwrap_or_default();
    for part in [
        b"open-hpl/".to_vec(),
        [game, b"/"].concat(),
        [sub, b"/"].concat(),
    ] {
        dir.extend_from_slice(&part);
        if !path(&dir).is_dir() {
            let _ = std::fs::create_dir(path(&dir));
        }
    }
    dir
}

pub(super) fn read_whole(p: &[u8]) -> Option<Vec<u8>> {
    std::fs::read(path(p)).ok().filter(|d| !d.is_empty())
}

fn downmix_ogg(s: &Sample, ogg: &[u8]) -> Option<Vec<u8>> {
    // SAFETY: `ogg` outlives the handle, which is cleared before returning.
    let v = unsafe { ohpl_vorbis_open_mem(ogg.as_ptr(), ogg.len()) };
    if v.is_null() {
        return None;
    }
    let ch = usize::try_from(s.channels).unwrap_or(0);
    let (left, right) = fsb5::downmix_matrix(ch);
    let mut pcm: Vec<i16> = Vec::with_capacity(s.num_pcm_samples as usize * 2);
    loop {
        let mut planes: *mut *mut f32 = std::ptr::null_mut();
        // SAFETY: `v` is live; `planes` receives the decoder's channel table for this read.
        let n = unsafe { ohpl_vorbis_read_float(v, &mut planes, 4096) };
        if n <= 0 || planes.is_null() {
            break;
        }
        let n = n as usize;
        let chans: Vec<&[f32]> = (0..ch)
            // SAFETY: the decoder returned `ch` planes of `n` floats each.
            .map(|c| unsafe { std::slice::from_raw_parts(*planes.add(c), n) })
            .collect();
        downmix_block(&chans, &left, &right, n, &mut pcm);
    }
    // SAFETY: `v` came from `ohpl_vorbis_open_mem`.
    unsafe { ohpl_vorbis_clear_mem(v) };
    let bytes: Vec<u8> = pcm.iter().flat_map(|x| x.to_le_bytes()).collect();
    Some(fsb5::write_pcm16_wav(
        s.frequency,
        2,
        s.num_pcm_samples,
        &bytes,
    ))
}

fn encode(mode: u32, s: &Sample, file: &[u8]) -> Option<Vec<u8>> {
    if mode != MODE_VORBIS {
        return fsb5::encode_pcm(mode, s, file);
    }
    let setup = SETUPS.iter().find(|(c, _)| Some(*c) == s.vorbis_crc)?.1;
    let ogg = fsb5::mux_vorbis(s, file, setup)?;
    if s.channels > 2 {
        downmix_ogg(s, &ogg)
    } else {
        Some(ogg)
    }
}

/// `cSomaFsb::ExtractSamples`: fills `out` with sample name to cache file name.
pub fn extract_samples(
    bank: &[u8],
    cache: &[u8],
    prefix: &[u8],
    samples: &[Vec<u8>],
    out: &mut BTreeMap<Vec<u8>, Vec<u8>>,
) {
    let in_cache = |name: &[u8]| path(&[cache, name].concat()).exists();
    let mut missing: BTreeSet<&[u8]> = BTreeSet::new();
    for s in samples {
        let ogg = [prefix, s, b".ogg"].concat();
        let has_ogg = in_cache(&ogg);
        let wav = [prefix, s, b".wav"].concat();
        match cached_state(has_ogg, !has_ogg && in_cache(&wav)) {
            1 => {
                out.insert(s.clone(), ogg);
            }
            2 => {
                out.insert(s.clone(), wav);
            }
            _ => {
                missing.insert(s);
            }
        }
    }
    if !missing.is_empty() && !bank.is_empty() {
        add_missing(bank, cache, prefix, &missing, out);
    }
    super::dump::fsb_digests(out, cache);
}

fn add_missing(
    bank: &[u8],
    cache: &[u8],
    prefix: &[u8],
    missing: &BTreeSet<&[u8]>,
    out: &mut BTreeMap<Vec<u8>, Vec<u8>>,
) {
    let Some(file) = read_whole(bank) else { return };
    let Some(parsed) = fsb5::parse(&file) else {
        return;
    };
    if !mode_supported(parsed.mode) {
        return;
    }
    for s in &parsed.samples {
        let name = file.get(s.name.clone()).unwrap_or_default();
        if !missing.contains(name) {
            continue;
        }
        let out_name = output_name(prefix, name, parsed.mode, s.channels);
        let Some(data) = encode(parsed.mode, s, &file) else {
            continue;
        };
        let _ = std::fs::write(path(&[cache, &out_name].concat()), &data);
        out.insert(name.to_vec(), out_name);
    }
}
