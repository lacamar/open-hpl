//! Decisions of `LowLevelSystemSDL`, `PlatformSDL` and `SDLEngineSetup`: Steam read-only roots,
//! command line classification, environment setup, script output text. The syscalls (mount,
//! unshare), SDL and object creation stay in C++.

mod ffi;

#[cfg(test)]
mod tests;

const COMMON: &[u8] = b"/steamapps/common/";
const WIN_COMMON: &[u8] = b"\\steamapps\\common\\";

fn find(hay: &[u8], needle: &[u8], from: usize) -> Option<usize> {
    let tail = hay.get(from..)?;
    if needle.is_empty() {
        return Some(from);
    }
    tail.windows(needle.len())
        .position(|w| w == needle)
        .map(|p| p + from)
}

fn find_byte(hay: &[u8], b: u8, from: usize) -> Option<usize> {
    hay.get(from..)?
        .iter()
        .position(|&x| x == b)
        .map(|p| p + from)
}

/// Prefix of `real + "/"` through `/steamapps/common` when it contains `/steamapps/common/`.
pub fn steam_root(real: &[u8]) -> Option<Vec<u8>> {
    let mut full = real.to_vec();
    full.push(b'/');
    let pos = find(&full, COMMON, 0)?;
    full.truncate(pos + COMMON.len() - 1);
    Some(full)
}

/// Value of a `"path"  "<value>"` line of libraryfolders.vdf.
pub fn vdf_path(line: &[u8]) -> Option<&[u8]> {
    let key = find(line, b"\"path\"", 0)?;
    let start = find_byte(line, b'"', key + 6)?;
    let end = find_byte(line, b'"', start + 1)?;
    line.get(start + 1..end)
}

/// `cwd` is the working directory used to resolve a relative `path`.
pub fn looks_like_steam_install(path: &[u8], cwd: Option<&[u8]>) -> bool {
    let joined;
    let full = match (path.first(), cwd) {
        (Some(&c), Some(cwd)) if c != b'/' => {
            joined = [cwd, b"/", path].concat();
            joined.as_slice()
        }
        _ => path,
    };
    find(full, COMMON, 0).is_some() || find(full, WIN_COMMON, 0).is_some()
}

const ST_NOSUID: u64 = 2;
const ST_NODEV: u64 = 4;
const ST_NOEXEC: u64 = 8;
const ST_NOATIME: u64 = 1024;
const ST_NODIRATIME: u64 = 2048;
const ST_RELATIME: u64 = 4096;
const MS_RDONLY: u64 = 1;
const MS_NOSUID: u64 = 2;
const MS_NODEV: u64 = 4;
const MS_NOEXEC: u64 = 8;
const MS_REMOUNT: u64 = 32;
const MS_NOATIME: u64 = 1024;
const MS_NODIRATIME: u64 = 2048;
const MS_BIND: u64 = 4096;
const MS_RELATIME: u64 = 1 << 21;

/// Flags of the read-only bind remount that keep the mount's existing locked flags.
pub fn remount_flags(st_flag: u64) -> u64 {
    const MAP: [(u64, u64); 6] = [
        (ST_NOSUID, MS_NOSUID),
        (ST_NODEV, MS_NODEV),
        (ST_NOEXEC, MS_NOEXEC),
        (ST_NOATIME, MS_NOATIME),
        (ST_NODIRATIME, MS_NODIRATIME),
        (ST_RELATIME, MS_RELATIME),
    ];
    MAP.iter()
        .filter(|&&(st, _)| st_flag & st != 0)
        .fold(MS_BIND | MS_REMOUNT | MS_RDONLY, |acc, &(_, ms)| acc | ms)
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Arg {
    Cwd,
    Skip,
    GameDirCandidate,
    Command,
}

pub fn classify_arg(arg: &[u8], got_game_dir: bool) -> Arg {
    if arg == b"-cwd" {
        Arg::Cwd
    } else if arg.starts_with(b"-psn") {
        Arg::Skip
    } else if !got_game_dir && arg.first() != Some(&b'-') {
        Arg::GameDirCandidate
    } else {
        Arg::Command
    }
}

pub const ENV_ALSOFT_NULL: u32 = 1;
pub const ENV_VIDEODRIVER_X11: u32 = 2;
pub const ENV_DISABLE_SCREENSAVER: u32 = 4;

pub struct EnvInputs {
    pub headless: bool,
    pub alsoft_set: bool,
    pub video: bool,
    pub videodriver_set: bool,
    pub display_set: bool,
}

pub fn engine_env(e: &EnvInputs) -> u32 {
    let mut out = 0;
    if e.headless && !e.alsoft_set {
        out |= ENV_ALSOFT_NULL;
    }
    if e.video {
        if e.headless && !e.videodriver_set && e.display_set {
            out |= ENV_VIDEODRIVER_X11;
        }
        if !e.headless {
            out |= ENV_DISABLE_SCREENSAVER;
        }
    }
    out
}

pub fn renderer_is_vulkan(name: &[u8]) -> bool {
    name.eq_ignore_ascii_case(b"vulkan")
}

pub const SCRIPT_MSG_ERROR: u32 = 0;
pub const SCRIPT_MSG_WARNING: u32 = 1;
pub const SCRIPT_MSG_INFO: u32 = 2;

/// `"%s (%d, %d) : %s : %s\n"`. The original wrote into a 1024-byte stack buffer; this has no limit.
pub fn script_message(section: &[u8], row: i32, col: i32, kind: u32, message: &[u8]) -> Vec<u8> {
    let tag: &[u8] = match kind {
        SCRIPT_MSG_WARNING => b"WARN",
        SCRIPT_MSG_INFO => b"INFO",
        _ => b"ERR ",
    };
    let mut out = Vec::with_capacity(section.len() + message.len() + 32);
    out.extend_from_slice(section);
    out.extend_from_slice(format!(" ({row}, {col}) : ").as_bytes());
    out.extend_from_slice(tag);
    out.extend_from_slice(b" : ");
    out.extend_from_slice(message);
    out.push(b'\n');
    out
}

const CHUNK: usize = 500;

/// Pieces `cScriptOutput::Display` logs: 500-byte chunks while more than 500 bytes remain, then the rest.
pub fn script_chunks(msg: &[u8]) -> Vec<&[u8]> {
    let mut out = Vec::new();
    let mut rest = msg;
    while rest.len() > CHUNK {
        let (head, tail) = rest.split_at(CHUNK);
        out.push(head);
        rest = tail;
    }
    out.push(rest);
    out
}
