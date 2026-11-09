//! FSB5 container parsing and the pure sample encoders (WAV, Ogg muxing, IMA ADPCM).
#![allow(
    clippy::indexing_slicing,
    clippy::unwrap_used,
    clippy::expect_used,
    clippy::panic,
    clippy::unreachable,
    clippy::integer_division_remainder_used,
    clippy::string_slice,
    reason = "panic audit pending (open-hpl-smj.23)"
)]

use std::ops::Range;

pub const MODE_PCM16: u32 = 2;
pub const MODE_IMA_ADPCM: u32 = 7;
pub const MODE_VORBIS: u32 = 15;

const CHUNK_CHANNELS: u32 = 1;
const CHUNK_FREQUENCY: u32 = 2;
const CHUNK_VORBIS_DATA: u32 = 11;
const HEADER_LEN: usize = 60;

#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct Sample {
    pub name: Range<usize>,
    pub frequency: u32,
    pub channels: i32,
    pub data_offset: usize,
    pub data_size: usize,
    pub num_pcm_samples: u32,
    pub vorbis_crc: Option<u32>,
}

#[derive(Debug, PartialEq, Eq)]
pub struct Parsed {
    pub mode: u32,
    pub samples: Vec<Sample>,
}

fn u32le(d: &[u8], p: usize) -> Option<u32> {
    Some(u32::from_le_bytes(
        d.get(p..p.checked_add(4)?)?.try_into().ok()?,
    ))
}

fn frequency_enum(i: u64) -> u32 {
    const TABLE: [u32; 10] = [
        0, 8000, 11000, 11025, 16000, 22050, 24000, 32000, 44100, 48000,
    ];
    TABLE.get(i as usize).copied().unwrap_or(0)
}

pub fn parse(file: &[u8]) -> Option<Parsed> {
    if file.len() < HEADER_LEN || &file[..4] != b"FSB5" {
        return None;
    }
    let field = |o| u32le(file, o);
    let version = field(4)?;
    let num_samples = field(8)?;
    let sample_headers_size = field(12)? as usize;
    let name_table_size = field(16)? as usize;
    let data_size = field(20)? as usize;
    let mode = field(24)?;
    if version == 0 || HEADER_LEN + sample_headers_size + name_table_size > file.len() {
        return None;
    }

    const CHANNELS: [i32; 4] = [1, 2, 6, 8];
    let mut pos = HEADER_LEN;
    let mut samples = Vec::new();
    for _ in 0..num_samples {
        let raw = u64::from_le_bytes(file.get(pos..pos + 8)?.try_into().ok()?);
        pos += 8;
        let mut s = Sample {
            frequency: frequency_enum((raw >> 1) & 0xF),
            channels: CHANNELS[((raw >> 5) & 0x3) as usize],
            data_offset: ((raw >> 7) & 0x7FF_FFFF) as usize * 32,
            num_pcm_samples: ((raw >> 34) & 0x3FFF_FFFF) as u32,
            ..Sample::default()
        };
        let mut next = raw & 1 != 0;
        while next {
            let craw = u32le(file, pos)?;
            pos += 4;
            next = craw & 1 != 0;
            let size = ((craw >> 1) & 0xFF_FFFF) as usize;
            let kind = (craw >> 25) & 0x7F;
            if pos + size > file.len() {
                return None;
            }
            if kind == CHUNK_VORBIS_DATA && size >= 4 {
                s.vorbis_crc = u32le(file, pos);
            } else if kind == CHUNK_CHANNELS && size >= 1 {
                s.channels = i32::from(file[pos]);
            } else if kind == CHUNK_FREQUENCY && size >= 4 {
                s.frequency = u32le(file, pos)?;
            }
            pos += size;
        }
        samples.push(s);
    }

    let table = pos;
    if name_table_size > 0 {
        // u32 product wraps in the C++ original
        if table + num_samples.wrapping_mul(4) as usize > file.len() {
            return None;
        }
        let limit = file.len().min(table + name_table_size);
        for (i, s) in samples.iter_mut().enumerate() {
            let start = table + u32le(file, table + i * 4)? as usize;
            if start >= file.len() {
                continue;
            }
            let len = file[start.min(limit)..limit]
                .iter()
                .take_while(|&&b| b != 0)
                .count();
            s.name = start..start + len;
        }
    }
    let data_base = table + name_table_size;

    let offsets: Vec<usize> = samples.iter().map(|s| s.data_offset).collect();
    for (i, s) in samples.iter_mut().enumerate() {
        let end = offsets.get(i + 1).copied().unwrap_or(data_size);
        s.data_size = end.saturating_sub(s.data_offset);
        s.data_offset += data_base;
    }
    Some(Parsed { mode, samples })
}

pub fn sample_data<'a>(file: &'a [u8], s: &Sample) -> Option<&'a [u8]> {
    file.get(s.data_offset..s.data_offset.checked_add(s.data_size)?)
}

fn push_u32(out: &mut Vec<u8>, v: u32) {
    out.extend_from_slice(&v.to_le_bytes());
}

pub fn write_pcm16_wav(frequency: u32, channels: i32, num_pcm_samples: u32, pcm: &[u8]) -> Vec<u8> {
    let block_align = (channels as u32).wrapping_mul(2);
    let data_bytes = (num_pcm_samples.wrapping_mul(block_align) as usize).min(pcm.len()) as u32;
    let mut out = Vec::with_capacity(44 + data_bytes as usize);
    out.extend_from_slice(b"RIFF");
    push_u32(&mut out, 36u32.wrapping_add(data_bytes));
    out.extend_from_slice(b"WAVEfmt ");
    push_u32(&mut out, 16);
    out.extend_from_slice(&[1, 0, channels as u8, 0]);
    push_u32(&mut out, frequency);
    push_u32(&mut out, frequency.wrapping_mul(block_align));
    out.extend_from_slice(&[block_align as u8, 0, 16, 0]);
    out.extend_from_slice(b"data");
    push_u32(&mut out, data_bytes);
    out.extend_from_slice(&pcm[..data_bytes as usize]);
    out
}

/// Vorbis channel order FL C FR SL SR [BL BR] LFE; LFE dropped. Meant for `channels >= 3`.
pub fn downmix_matrix(channels: usize) -> (Vec<f32>, Vec<f32>) {
    let mut l = vec![0.5f32; channels];
    let mut r = vec![0.5f32; channels];
    if channels >= 3 {
        l[0] = 1.0;
        r[2] = 1.0;
        r[0] = 0.0;
        l[2] = 0.0;
        let mut c = 3;
        while c + 1 < channels - 1 {
            r[c] = 0.0;
            l[c + 1] = 0.0;
            c += 2;
        }
    }
    (l, r)
}

fn wav_stereo(s: &Sample, pcm: &[i16]) -> Vec<u8> {
    let ch = s.channels as usize;
    if ch <= 2 {
        let bytes: Vec<u8> = pcm.iter().flat_map(|v| v.to_le_bytes()).collect();
        return write_pcm16_wav(s.frequency, s.channels, s.num_pcm_samples, &bytes);
    }
    let (ml, mr) = downmix_matrix(ch);
    let mix = |frame: &[i16], m: &[f32]| {
        let v = frame
            .iter()
            .zip(m)
            .fold(0f32, |a, (&p, &g)| a + f32::from(p) * g);
        v.clamp(-32768.0, 32767.0) as i16
    };
    let bytes: Vec<u8> = pcm
        .chunks_exact(ch)
        .flat_map(|f| [mix(f, &ml), mix(f, &mr)])
        .flat_map(i16::to_le_bytes)
        .collect();
    write_pcm16_wav(s.frequency, 2, s.num_pcm_samples, &bytes)
}

/// FMOD's Xbox IMA layout: per block 4-byte channel headers, then 4-byte words interleaved per channel.
fn decode_ima(s: &Sample, data: &[u8]) -> Vec<i16> {
    const STEP: [i32; 89] = [
        7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 50, 55, 60,
        66, 73, 80, 88, 97, 107, 118, 130, 143, 157, 173, 190, 209, 230, 253, 279, 307, 337, 371,
        408, 449, 494, 544, 598, 658, 724, 796, 876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878,
        2066, 2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358, 5894, 6484, 7132, 7845,
        8630, 9493, 10442, 11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086,
        29794, 32767,
    ];
    const INDEX: [i32; 16] = [-1, -1, -1, -1, 2, 4, 6, 8, -1, -1, -1, -1, 2, 4, 6, 8];
    let ch = s.channels as usize;
    let block_len = 36 * ch;
    let want = s.num_pcm_samples as usize * ch;
    let mut out: Vec<i16> = Vec::new();
    let mut off = 0;
    while off + block_len <= s.data_size && out.len() < want {
        let block = &data[off..off + block_len];
        let base = out.len();
        out.resize(base + 64 * ch, 0);
        for c in 0..ch {
            let mut pred = i32::from(i16::from_le_bytes([block[c * 4], block[c * 4 + 1]]));
            let mut idx = i32::from(block[c * 4 + 2]).min(88);
            for w in 0..8 {
                for b in 0..8 {
                    let byte = block[4 * ch + (w * ch + c) * 4 + b / 2];
                    let n = usize::from(if b & 1 != 0 { byte >> 4 } else { byte & 0xF });
                    let step = STEP[idx as usize];
                    let mut diff = step >> 3;
                    if n & 1 != 0 {
                        diff += step >> 2;
                    }
                    if n & 2 != 0 {
                        diff += step >> 1;
                    }
                    if n & 4 != 0 {
                        diff += step;
                    }
                    pred = if n & 8 != 0 { pred - diff } else { pred + diff }.clamp(-32768, 32767);
                    idx = (idx + INDEX[n]).clamp(0, 88);
                    out[base + (w * 8 + b) * ch + c] = pred as i16;
                }
            }
        }
        off += block_len;
    }
    out
}

/// WAV for PCM16 and IMA ADPCM modes; `None` when the sample data lies outside the file.
pub fn encode_pcm(mode: u32, s: &Sample, file: &[u8]) -> Option<Vec<u8>> {
    let data = sample_data(file, s)?;
    if mode == MODE_PCM16 {
        let pcm: Vec<i16> = data
            .chunks_exact(2)
            .map(|b| i16::from_le_bytes([b[0], b[1]]))
            .collect();
        Some(wav_stereo(s, &pcm))
    } else if mode == MODE_IMA_ADPCM {
        Some(wav_stereo(s, &decode_ima(s, data)))
    } else {
        None
    }
}

const fn ogg_crc_table() -> [u32; 256] {
    let mut t = [0u32; 256];
    let mut i = 0;
    while i < 256 {
        let mut r = (i as u32) << 24;
        let mut j = 0;
        while j < 8 {
            r = if r & 0x8000_0000 != 0 {
                (r << 1) ^ 0x04c1_1db7
            } else {
                r << 1
            };
            j += 1;
        }
        t[i] = r;
        i += 1;
    }
    t
}

// libogg's CRC: MSB-first, no reflection - not zlib's
fn ogg_crc(data: &[u8]) -> u32 {
    const TABLE: [u32; 256] = ogg_crc_table();
    data.iter().fold(0u32, |crc, &b| {
        (crc << 8) ^ TABLE[((crc >> 24) as u8 ^ b) as usize]
    })
}

struct OggMuxer {
    serial: u32,
    page_seq: u32,
    wrote_any_page: bool,
    continued: bool,
    segments: Vec<u8>,
    body: Vec<u8>,
    out: Vec<u8>,
}

impl OggMuxer {
    fn new(serial: u32) -> Self {
        Self {
            serial,
            page_seq: 0,
            wrote_any_page: false,
            continued: false,
            segments: Vec::new(),
            body: Vec::new(),
            out: Vec::new(),
        }
    }

    fn add_packet(&mut self, data: &[u8]) {
        let mut rest = data;
        loop {
            let lacing = rest.len().min(255);
            if self.segments.len() == 255 {
                self.flush(false, -1);
                self.continued = true;
            }
            self.segments.push(lacing as u8);
            self.body.extend_from_slice(&rest[..lacing]);
            rest = &rest[lacing..];
            if rest.is_empty() {
                break;
            }
        }
    }

    fn flush(&mut self, eos: bool, granule: i64) {
        if self.segments.is_empty() {
            return;
        }
        let mut page = b"OggS\0".to_vec();
        page.push(
            u8::from(self.continued)
                | if self.wrote_any_page { 0 } else { 2 }
                | if eos { 4 } else { 0 },
        );
        page.extend_from_slice(&(granule as u64).to_le_bytes());
        push_u32(&mut page, self.serial);
        push_u32(&mut page, self.page_seq);
        self.page_seq = self.page_seq.wrapping_add(1);
        let crc_pos = page.len();
        push_u32(&mut page, 0);
        page.push(self.segments.len() as u8);
        page.extend_from_slice(&self.segments);
        page.extend_from_slice(&self.body);
        let crc = ogg_crc(&page);
        page[crc_pos..crc_pos + 4].copy_from_slice(&crc.to_le_bytes());
        self.out.extend_from_slice(&page);
        self.wrote_any_page = true;
        self.continued = false;
        self.segments.clear();
        self.body.clear();
    }
}

fn vorbis_id_header(channels: i32, frequency: u32) -> Vec<u8> {
    let mut h = vec![0x01];
    h.extend_from_slice(b"vorbis");
    push_u32(&mut h, 0);
    h.push(channels as u8);
    push_u32(&mut h, frequency);
    h.extend_from_slice(&[0; 12]);
    h.extend_from_slice(&[0xB8, 0x01]);
    h
}

fn vorbis_comment_header() -> Vec<u8> {
    let mut h = vec![0x03];
    h.extend_from_slice(b"vorbis");
    h.extend_from_slice(&[0, 0, 0, 0, 0, 0, 0, 0, 1]);
    h
}

/// Ogg Vorbis stream for one sample; `None` when the sample data lies outside the file.
pub fn mux_vorbis(s: &Sample, file: &[u8], setup: &[u8]) -> Option<Vec<u8>> {
    let data = sample_data(file, s)?;
    let mut m = OggMuxer::new(1);
    m.add_packet(&vorbis_id_header(s.channels, s.frequency));
    m.flush(false, -1);
    m.add_packet(&vorbis_comment_header());
    m.add_packet(setup);
    m.flush(false, -1);
    let mut off = 0;
    while off + 2 <= data.len() {
        let len = usize::from(u16::from_le_bytes([data[off], data[off + 1]]));
        off += 2;
        if len == 0 || off + len > data.len() {
            break;
        }
        m.add_packet(&data[off..off + len]);
        off += len;
    }
    m.flush(true, i64::from(s.num_pcm_samples));
    Some(m.out)
}

#[cfg(test)]
mod tests {
    use super::*;

    /// One chunk: (type, payload).
    type Chunk<'a> = (u32, &'a [u8]);

    fn bank(mode: u32, samples: &[(u64, &[Chunk], &str)], data: &[u8]) -> Vec<u8> {
        let (mut heads, mut strs, mut table) = (Vec::new(), Vec::new(), Vec::new());
        for (raw, chunks, name) in samples {
            let raw = raw | u64::from(!chunks.is_empty());
            heads.extend_from_slice(&raw.to_le_bytes());
            for (i, (kind, payload)) in chunks.iter().enumerate() {
                let craw =
                    u32::from(i + 1 < chunks.len()) | (payload.len() as u32) << 1 | kind << 25;
                heads.extend_from_slice(&craw.to_le_bytes());
                heads.extend_from_slice(payload);
            }
            table.extend_from_slice(&((samples.len() * 4 + strs.len()) as u32).to_le_bytes());
            strs.extend_from_slice(name.as_bytes());
            strs.push(0);
        }
        table.extend_from_slice(&strs);
        let mut f = b"FSB5".to_vec();
        for v in [
            1,
            samples.len() as u32,
            heads.len() as u32,
            table.len() as u32,
            data.len() as u32,
            mode,
        ] {
            f.extend_from_slice(&v.to_le_bytes());
        }
        f.resize(60, 0);
        f.extend_from_slice(&heads);
        f.extend_from_slice(&table);
        f.extend_from_slice(data);
        f
    }

    // freq enum, channel enum, data offset in 32-byte units, pcm samples
    fn raw(freq: u64, ch: u64, off: u64, pcm: u64) -> u64 {
        freq << 1 | ch << 5 | off << 7 | pcm << 34
    }

    #[test]
    fn parses_fields_names_and_sizes() {
        let f = bank(
            2,
            &[(raw(5, 1, 0, 100), &[], "a"), (raw(9, 3, 2, 7), &[], "bc")],
            &[0; 100],
        );
        let p = parse(&f).unwrap();
        assert_eq!(p.mode, 2);
        let base = 60 + 16 + (8 + 2 + 3);
        let names: Vec<_> = p.samples.iter().map(|s| &f[s.name.clone()]).collect();
        assert_eq!(names, [&b"a"[..], b"bc"]);
        let s = &p.samples;
        assert_eq!(
            (s[0].frequency, s[0].channels, s[0].num_pcm_samples),
            (22050, 2, 100)
        );
        assert_eq!(
            (s[1].frequency, s[1].channels, s[1].num_pcm_samples),
            (48000, 8, 7)
        );
        assert_eq!((s[0].data_offset, s[0].data_size), (base, 64));
        assert_eq!((s[1].data_offset, s[1].data_size), (base + 64, 36));
    }

    #[test]
    fn chunks_override_channels_frequency_and_set_crc() {
        let chunks: &[Chunk] = &[(1, &[6]), (2, &[0x39, 0x30, 0, 0]), (11, &[1, 2, 3, 4])];
        let f = bank(15, &[(raw(1, 0, 0, 1), chunks, "x")], &[]);
        let s = &parse(&f).unwrap().samples[0];
        assert_eq!(
            (s.channels, s.frequency, s.vorbis_crc),
            (6, 12345, Some(0x0403_0201))
        );
    }

    #[test]
    fn short_chunks_are_ignored() {
        let chunks: &[Chunk] = &[(1, &[]), (2, &[1, 2, 3]), (11, &[1, 2, 3])];
        let f = bank(15, &[(raw(1, 1, 0, 1), chunks, "x")], &[]);
        let s = &parse(&f).unwrap().samples[0];
        assert_eq!((s.channels, s.frequency, s.vorbis_crc), (2, 8000, None));
    }

    #[test]
    fn rejects_malformed_banks() {
        let good = bank(2, &[(raw(5, 1, 0, 1), &[], "a")], &[0; 4]);
        assert!(parse(&good).is_some());
        assert!(parse(&good[..59]).is_none());
        let mut bad = good.clone();
        bad[0] = b'X';
        assert!(parse(&bad).is_none());
        let mut bad = good.clone();
        bad[4] = 0;
        assert!(parse(&bad).is_none());
        let mut bad = good.clone();
        bad[8] = 200;
        assert!(parse(&bad).is_none());
        let mut bad = good.clone();
        bad[12..16].copy_from_slice(&u32::MAX.to_le_bytes());
        assert!(parse(&bad).is_none());
        let chunks: &[Chunk] = &[(1, &[6])];
        let mut bad = bank(2, &[(raw(5, 1, 0, 1), chunks, "a")], &[]);
        bad[68..72].copy_from_slice(&(1u32 | 0xFF_FFFF << 1).to_le_bytes());
        assert!(parse(&bad).is_none());
    }

    #[test]
    fn out_of_range_name_offset_gives_empty_name() {
        let mut f = bank(2, &[(raw(5, 1, 0, 1), &[], "a")], &[0; 4]);
        f[68..72].copy_from_slice(&0x1000u32.to_le_bytes());
        assert_eq!(parse(&f).unwrap().samples[0].name, 0..0);
    }

    #[test]
    fn data_outside_file_fails_encoding() {
        let f = bank(2, &[(raw(5, 1, 0, 1), &[], "a")], &[0; 4]);
        let mut s = parse(&f).unwrap().samples.remove(0);
        assert!(encode_pcm(MODE_PCM16, &s, &f).is_some());
        s.data_size += 1;
        assert_eq!(encode_pcm(MODE_PCM16, &s, &f), None);
        assert_eq!(mux_vorbis(&s, &f, &[]), None);
    }

    #[test]
    fn pcm16_wav_header_and_clamped_length() {
        let data: Vec<u8> = (1..=8).collect();
        let f = bank(2, &[(raw(5, 1, 0, 1), &[], "a")], &data);
        let s = &parse(&f).unwrap().samples[0];
        let wav = encode_pcm(MODE_PCM16, s, &f).unwrap();
        let mut want = b"RIFF".to_vec();
        want.extend_from_slice(&40u32.to_le_bytes());
        want.extend_from_slice(b"WAVEfmt ");
        want.extend_from_slice(&[16, 0, 0, 0, 1, 0, 2, 0]);
        want.extend_from_slice(&22050u32.to_le_bytes());
        want.extend_from_slice(&88200u32.to_le_bytes());
        want.extend_from_slice(&[4, 0, 16, 0]);
        want.extend_from_slice(b"data");
        want.extend_from_slice(&4u32.to_le_bytes());
        want.extend_from_slice(&[1, 2, 3, 4]);
        assert_eq!(wav, want);
    }

    #[test]
    fn downmix_matrix_for_six_channels() {
        let (l, r) = downmix_matrix(6);
        assert_eq!(l, [1.0, 0.5, 0.0, 0.5, 0.0, 0.5]);
        assert_eq!(r, [0.0, 0.5, 1.0, 0.0, 0.5, 0.5]);
    }

    #[test]
    fn surround_pcm_folds_to_stereo() {
        let mut data = Vec::new();
        for v in [1000i16, 2000, 3000, 4000, 5000, 6000] {
            data.extend_from_slice(&v.to_le_bytes());
        }
        let f = bank(2, &[(raw(5, 0, 0, 1), &[(1, &[6])], "a")], &data);
        let s = &parse(&f).unwrap().samples[0];
        let wav = encode_pcm(MODE_PCM16, s, &f).unwrap();
        assert_eq!(wav[22], 2);
        // l = 1000 + 1000 + 0 + 2000 + 0 + 3000, r = 0 + 1000 + 3000 + 0 + 2500 + 3000
        assert_eq!(
            &wav[44..],
            [7000i16.to_le_bytes(), 9500i16.to_le_bytes()].concat()
        );
    }

    #[test]
    fn ima_block_decodes_by_hand() {
        let mut block = vec![0u8; 36];
        block[..2].copy_from_slice(&1000i16.to_le_bytes());
        block[4] = 0x04;
        let s = Sample {
            channels: 1,
            num_pcm_samples: 64,
            data_size: 36,
            ..Sample::default()
        };
        let pcm = decode_ima(&s, &block);
        assert_eq!(pcm.len(), 64);
        // nibble 4 at index 0 adds 7 (index 2); nibble 0 adds step >> 3: 9 -> 1 (index 1), 8 -> 1 (index 0), 7 -> 0
        assert_eq!(pcm[..3], [1007, 1008, 1009]);
        assert!(pcm[2..].iter().all(|&v| v == 1009));
    }

    type Page = (u8, u64, Vec<u8>, Vec<u8>);

    fn pages(ogg: &[u8]) -> Vec<Page> {
        let (mut out, mut p) = (Vec::new(), 0);
        while p < ogg.len() {
            assert_eq!(&ogg[p..p + 4], b"OggS");
            let n = usize::from(ogg[p + 26]);
            let segs = ogg[p + 27..p + 27 + n].to_vec();
            let body_len: usize = segs.iter().map(|&s| usize::from(s)).sum();
            let end = p + 27 + n + body_len;
            let mut page = ogg[p..end].to_vec();
            let crc = u32::from_le_bytes(page[22..26].try_into().unwrap());
            page[22..26].fill(0);
            let mut r = 0u32;
            for &b in &page {
                r ^= u32::from(b) << 24;
                for _ in 0..8 {
                    r = if r & 0x8000_0000 != 0 {
                        r << 1 ^ 0x04c1_1db7
                    } else {
                        r << 1
                    };
                }
            }
            assert_eq!(r, crc);
            let granule = u64::from_le_bytes(ogg[p + 6..p + 14].try_into().unwrap());
            out.push((ogg[p + 5], granule, segs, ogg[p + 27 + n..end].to_vec()));
            p = end;
        }
        out
    }

    #[test]
    fn vorbis_mux_page_layout() {
        let setup = [5u8; 300];
        let mut data = Vec::new();
        for p in [&[9u8; 3][..], &[8u8; 2]] {
            data.extend_from_slice(&(p.len() as u16).to_le_bytes());
            data.extend_from_slice(p);
        }
        data.extend_from_slice(&[0, 0, 7, 7]);
        let f = bank(
            15,
            &[(raw(5, 1, 0, 777), &[(11, &[1, 2, 3, 4])], "a")],
            &data,
        );
        let s = &parse(&f).unwrap().samples[0];
        let pg = pages(&mux_vorbis(s, &f, &setup).unwrap());
        assert_eq!(pg.len(), 3);
        assert_eq!((pg[0].0, pg[0].1, &pg[0].2[..]), (2, u64::MAX, &[30u8][..]));
        assert_eq!(&pg[0].3[..8], b"\x01vorbis\0");
        assert_eq!(pg[0].3[11], 2);
        assert_eq!(&pg[0].3[12..16], &22050u32.to_le_bytes());
        assert_eq!(pg[0].3[28..], [0xB8, 0x01]);
        assert_eq!(pg[1].2, [16, 255, 45]);
        assert_eq!((pg[2].0, pg[2].1, &pg[2].2[..]), (4, 777, &[3u8, 2][..]));
    }

    #[test]
    fn long_packets_span_pages() {
        let setup = vec![1u8; 255 * 256];
        let f = bank(15, &[(raw(5, 0, 0, 1), &[(11, &[0; 4])], "a")], &[]);
        let s = &parse(&f).unwrap().samples[0];
        let pg = pages(&mux_vorbis(s, &f, &setup).unwrap());
        assert!(pg.iter().any(|p| p.0 & 1 == 1));
        assert!(pg.iter().all(|p| p.2.len() <= 255));
    }
}
