//! Headless control protocol: one flat JSON object per request line, one JSON line per response.

use std::collections::BTreeMap;

/// `isspace` in the C locale.
fn is_space(c: u8) -> bool {
    matches!(c, b' ' | 9..=13)
}

fn skip_ws(s: &[u8], pos: &mut usize) {
    while s.get(*pos).is_some_and(|&c| is_space(c)) {
        *pos += 1;
    }
}

fn parse_string(s: &[u8], pos: &mut usize) -> Vec<u8> {
    let mut out = Vec::new();
    if s.get(*pos) != Some(&b'"') {
        return out;
    }
    *pos += 1;
    while let Some(&c) = s.get(*pos) {
        if c == b'"' {
            break;
        }
        match (c, s.get(*pos + 1)) {
            (b'\\', Some(&next)) => {
                out.push(match next {
                    b'n' => b'\n',
                    b'r' => b'\r',
                    b't' => b'\t',
                    other => other,
                });
                *pos += 2;
            }
            _ => {
                out.push(c);
                *pos += 1;
            }
        }
    }
    if *pos < s.len() {
        *pos += 1;
    }
    out
}

fn parse_bare(s: &[u8], pos: &mut usize) -> Vec<u8> {
    let start = *pos;
    while s
        .get(*pos)
        .is_some_and(|&c| c != b',' && c != b'}' && !is_space(c))
    {
        *pos += 1;
    }
    s.get(start..*pos).unwrap_or_default().to_vec()
}

/// `ParseFlatJsonObject`: stops silently at the first malformed token; a repeated key keeps the
/// last value; the map iterates in `std::map<std::string, ...>` (unsigned byte) order.
pub fn parse_flat_json(line: &[u8]) -> BTreeMap<Vec<u8>, Vec<u8>> {
    let mut fields = BTreeMap::new();
    let mut pos = 0;
    skip_ws(line, &mut pos);
    if line.get(pos) != Some(&b'{') {
        return fields;
    }
    pos += 1;
    loop {
        skip_ws(line, &mut pos);
        if line.get(pos) != Some(&b'"') {
            break;
        }
        let key = parse_string(line, &mut pos);
        skip_ws(line, &mut pos);
        if line.get(pos) != Some(&b':') {
            break;
        }
        pos += 1;
        skip_ws(line, &mut pos);
        let val = if line.get(pos) == Some(&b'"') {
            parse_string(line, &mut pos)
        } else {
            parse_bare(line, &mut pos)
        };
        fields.insert(key, val);
        skip_ws(line, &mut pos);
        if line.get(pos) == Some(&b',') {
            pos += 1;
            continue;
        }
        break;
    }
    fields
}

/// Response-side `JsonEscape`: other control characters are dropped.
pub fn json_escape(s: &[u8]) -> Vec<u8> {
    let mut out = Vec::with_capacity(s.len());
    for &c in s {
        match c {
            b'"' => out.extend_from_slice(b"\\\""),
            b'\\' => out.extend_from_slice(b"\\\\"),
            b'\n' => out.extend_from_slice(b"\\n"),
            b'\r' => out.extend_from_slice(b"\\r"),
            b'\t' => out.extend_from_slice(b"\\t"),
            0..0x20 => {}
            _ => out.push(c),
        }
    }
    out
}

/// `cHeadlessResponse::ToJson`; field values are already JSON, keys are written as given.
pub fn response_json(ok: bool, error: &[u8], fields: &[(&[u8], &[u8])]) -> Vec<u8> {
    let mut out = b"{\"ok\":".to_vec();
    out.extend_from_slice(if ok { b"true" } else { b"false" });
    if !ok && !error.is_empty() {
        out.extend_from_slice(b",\"error\":\"");
        out.extend(json_escape(error));
        out.push(b'"');
    }
    for (key, value) in fields {
        out.extend_from_slice(b",\"");
        out.extend_from_slice(key);
        out.extend_from_slice(b"\":");
        out.extend_from_slice(value);
    }
    out.push(b'}');
    out
}

/// Receive buffer of the control socket: yields each non-empty `\n`-terminated line once.
#[derive(Debug, Default)]
pub struct LineSplitter {
    buf: Vec<u8>,
}

impl LineSplitter {
    pub fn push(&mut self, chunk: &[u8]) -> Vec<Vec<u8>> {
        self.buf.extend_from_slice(chunk);
        let mut lines = Vec::new();
        while let Some(nl) = self.buf.iter().position(|&c| c == b'\n') {
            let rest = self.buf.split_off(nl + 1);
            self.buf.pop();
            let line = std::mem::replace(&mut self.buf, rest);
            if !line.is_empty() {
                lines.push(line);
            }
        }
        lines
    }
}
