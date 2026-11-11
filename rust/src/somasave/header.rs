//! Save file header, trailer and the `Load` version check (`SomaSave.cpp`).

use super::codec::{In, Out};

/// Magic whose last byte is `'0' + version`; this build writes 31.
pub const MAGIC: [u8; 8] = *b"OHPLSAVO";
pub const MIN_VERSION: i32 = 2;
pub const MAX_VERSION: i32 = 31;

/// Everything before the world body.
pub struct HeaderOut<'a> {
    pub map: &'a [u8],
    pub start_pos: &'a [u8],
    pub visited: &'a [&'a [u8]],
    pub vars: &'a [u8],
    pub exploration: bool,
    pub preload: &'a [u8],
    pub played: &'a [&'a [u8]],
}

pub fn write_header(o: &mut Out, h: &HeaderOut) {
    o.bytes(&MAGIC);
    o.str(h.map);
    o.str(h.start_pos);
    o.len(h.visited.len());
    for s in h.visited {
        o.str(s);
    }
    o.str(h.vars);
    o.bool(h.exploration);
    o.str(h.preload);
    o.len(h.played.len());
    for s in h.played {
        o.str(s);
    }
}

#[derive(Default, Debug, PartialEq)]
pub struct HeaderIn {
    pub version: i32,
    pub map: Vec<u8>,
    pub start_pos: Vec<u8>,
    pub visited: Vec<Vec<u8>>,
    pub vars: Vec<u8>,
    pub exploration: bool,
    /// Empty before version 19.
    pub preload: Vec<u8>,
    pub played: Vec<Vec<u8>>,
    /// Offset of the world body.
    pub body: usize,
}

#[derive(Debug, PartialEq, Eq)]
pub enum LoadStatus {
    Ok,
    /// Missing file, bad magic or unsupported version: logged by the caller.
    Unreadable,
    /// Truncated header or empty map: `Load` returns false silently (after storing the preload).
    Rejected,
}

/// `cSomaSaveHandler::Load` up to the point where the state is stashed; the fields parsed so far
/// are returned whatever the status.
pub fn read_header(data: &[u8], file_open: bool) -> (LoadStatus, HeaderIn) {
    let mut i = In::new(data);
    let mut h = HeaderIn::default();
    let magic: [u8; 8] = i.blob();
    h.version = i32::from(magic.last().copied().unwrap_or(0)) - i32::from(b'0');
    if !file_open
        || magic.get(..7) != MAGIC.get(..7)
        || !(MIN_VERSION..=MAX_VERSION).contains(&h.version)
    {
        return (LoadStatus::Unreadable, HeaderIn::default());
    }
    h.map = i.str().to_vec();
    h.start_pos = i.str().to_vec();
    let n = i.u32();
    let mut k = 0;
    while k < n && i.ok {
        h.visited.push(i.str().to_vec());
        k += 1;
    }
    h.vars = i.str().to_vec();
    h.exploration = if h.version == 2 { false } else { i.bool() };
    h.preload = if h.version >= 19 {
        i.str().to_vec()
    } else {
        Vec::new()
    };
    let n = if h.version >= 21 { i.u32() } else { 0 };
    let mut k = 0;
    while k < n && i.ok {
        h.played.push(i.str().to_vec());
        k += 1;
    }
    h.body = i.p;
    if !i.ok || h.map.is_empty() {
        return (LoadStatus::Rejected, h);
    }
    (LoadStatus::Ok, h)
}
