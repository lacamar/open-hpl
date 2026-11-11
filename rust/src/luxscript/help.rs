//! LuxHelpFuncs: `$command` string parsing and text duration.

use crate::math;

pub(crate) const OP_CHAR: u32 = 0;
pub(crate) const OP_COMMAND: u32 = 1;

pub(crate) const CMD_BUTTON: u32 = 1;
pub(crate) const CMD_LOOK: u32 = 2;
pub(crate) const CMD_BAD: u32 = 3;

fn is_letter(c: u32) -> bool {
    (u32::from(b'a')..=u32::from(b'z')).contains(&c)
        || (u32::from(b'A')..=u32::from(b'Z')).contains(&c)
}

fn wide(s: &[u8]) -> Vec<u32> {
    s.iter().map(|&c| u32::from(c)).collect()
}

fn command_class(cmd: &[u32]) -> u32 {
    if cmd.get(..3) == Some(wide(b"But").as_slice()) {
        CMD_BUTTON
    } else if cmd == wide(b"Look").as_slice() {
        CMD_LOOK
    } else {
        CMD_BAD
    }
}

/// Ordered ops C++ replays: `[OP_CHAR, c, 0, 0]` appends a character; `[OP_COMMAND, start, len, class]`
/// appends the resolved command `input[start..start + len]`. A trailing unterminated command is dropped.
pub(crate) fn parse_plan(input: &[u32]) -> Vec<u32> {
    let mut ops = Vec::new();
    let mut cmd_start = 0usize;
    let mut parsing = false;
    for (i, &c) in input.iter().enumerate() {
        if parsing {
            if !is_letter(c) {
                let cmd = input.get(cmd_start..i).unwrap_or_default();
                ops.extend([
                    OP_COMMAND,
                    cmd_start as u32,
                    cmd.len() as u32,
                    command_class(cmd),
                ]);
                ops.extend([OP_CHAR, c, 0, 0]);
                parsing = false;
            }
        } else if c == u32::from(b'$') {
            parsing = true;
            cmd_start = i + 1;
        } else {
            ops.extend([OP_CHAR, c, 0, 0]);
        }
    }
    ops
}

pub(crate) fn string_duration(start: f32, min: f32, per_char: f32, len: usize) -> f32 {
    let t = math::fma(len as f32, per_char, start);
    if t < min { min } else { t }
}
