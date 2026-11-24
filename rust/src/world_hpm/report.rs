//! Load report of one `.hpm` map: the JSON the SOMA tools read and the per-track log lines.

use std::cell::RefCell;
use std::collections::BTreeMap;
use std::fmt::Write;

#[derive(Default)]
struct Track {
    in_xml: i32,
    created: i32,
    time_ms: i32,
    file_missing: bool,
    skipped: BTreeMap<Vec<u8>, i32>,
}

#[derive(Default)]
pub struct Report {
    tracks: BTreeMap<Vec<u8>, Track>,
    terrain_active: bool,
}

thread_local! {
    static LAST: RefCell<Report> = RefCell::new(Report::default());
}

impl Report {
    pub fn track(
        &mut self,
        name: &[u8],
        in_xml: i32,
        created: i32,
        time_ms: i32,
        file_missing: bool,
        skipped: impl Iterator<Item = (Vec<u8>, i32)>,
    ) {
        let t = self.tracks.entry(name.to_vec()).or_default();
        (t.in_xml, t.created, t.time_ms, t.file_missing) = (in_xml, created, time_ms, file_missing);
        t.skipped.extend(skipped);
    }

    pub fn terrain(&mut self) {
        self.terrain_active = true;
    }

    pub fn publish(self) {
        LAST.with(|l| *l.borrow_mut() = self);
    }

    fn render(&self, map: &[u8], total_ms: i32) -> (Vec<u8>, Vec<u8>) {
        let mut json = b"{\"map\":\"".to_vec();
        escape(map, &mut json);
        let _ = write!(
            Out(&mut json),
            "\",\"total_ms\":{total_ms},\"terrain_active\":{},\"tracks\":{{",
            self.terrain_active
        );
        let mut log = Vec::new();
        for (i, (name, t)) in self.tracks.iter().enumerate() {
            if i > 0 {
                json.push(b',');
            }
            json.push(b'"');
            json.extend_from_slice(name);
            let _ = write!(
                Out(&mut json),
                "\":{{\"xml\":{},\"created\":{},\"ms\":{},\"file_missing\":{},\"skipped\":{{",
                t.in_xml,
                t.created,
                t.time_ms,
                t.file_missing
            );
            for (j, (reason, n)) in t.skipped.iter().enumerate() {
                if j > 0 {
                    json.push(b',');
                }
                json.push(b'"');
                escape(reason, &mut json);
                let _ = write!(Out(&mut json), "\":{n}");
            }
            json.extend_from_slice(b"}}");
            log.extend_from_slice(b"  SOMA hpm track ");
            log.extend_from_slice(name);
            log.resize(log.len() + 20usize.saturating_sub(name.len()), b' ');
            let _ = writeln!(
                Out(&mut log),
                " xml={} created={} skipped={} ({} ms)",
                t.in_xml,
                t.created,
                t.in_xml.wrapping_sub(t.created),
                t.time_ms
            );
        }
        json.extend_from_slice(b"}}");
        (json, log)
    }
}

struct Out<'a>(&'a mut Vec<u8>);

impl Write for Out<'_> {
    fn write_str(&mut self, s: &str) -> std::fmt::Result {
        self.0.extend_from_slice(s.as_bytes());
        Ok(())
    }
}

fn escape(s: &[u8], out: &mut Vec<u8>) {
    for &c in s {
        match c {
            b'"' | b'\\' => out.extend_from_slice(&[b'\\', c]),
            0..0x20 => out.push(b' '),
            _ => out.push(c),
        }
    }
}

type Sink = unsafe extern "C" fn(*mut std::ffi::c_void, *const u8, usize, *const u8, usize);

/// Renders the report of the last `ohpl_hpm_parse` on this thread: JSON, then the log lines.
///
/// # Safety
/// `map` points to `n` bytes; `sink` accepts `ctx` and reads its two buffers during the call.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_hpm_report(
    map: *const u8,
    n: usize,
    total_ms: i32,
    sink: Sink,
    ctx: *mut std::ffi::c_void,
) {
    // SAFETY: caller contract.
    let map = unsafe { std::slice::from_raw_parts(map, n) };
    let (json, log) = LAST.with(|l| l.borrow().render(map, total_ms));
    // SAFETY: caller contract.
    unsafe { sink(ctx, json.as_ptr(), json.len(), log.as_ptr(), log.len()) }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn renders_like_the_cpp_report() {
        let mut r = Report::default();
        r.track(
            b"lights",
            3,
            2,
            5,
            false,
            [(b"a\"b".to_vec(), 1)].into_iter(),
        );
        r.terrain();
        let (json, log) = r.render(b"m\\x", 12);
        assert_eq!(
            String::from_utf8_lossy(&json),
            r#"{"map":"m\\x","total_ms":12,"terrain_active":true,"tracks":{"lights":{"xml":3,"created":2,"ms":5,"file_missing":false,"skipped":{"a\"b":1}}}}"#
        );
        assert_eq!(
            String::from_utf8_lossy(&log),
            "  SOMA hpm track lights               xml=3 created=2 skipped=1 (5 ms)\n"
        );
    }
}
