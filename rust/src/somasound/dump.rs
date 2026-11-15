//! A text form of the event store, floats as bit patterns: the differential's comparison format.

use super::events::Events;
use std::io::Write;

fn bits(f: f32) -> u32 {
    f.to_bits()
}

impl Events {
    pub fn dump(&self) -> Vec<u8> {
        let mut o: Vec<u8> = Vec::new();
        for (key, &i) in &self.map {
            let Some(e) = self.list.get(i) else { continue };
            let _ = o.write_all(b"EV ");
            let _ = o.write_all(key);
            let _ = o.write_all(b"\x1fname=");
            let _ = o.write_all(&e.name);
            let _ = writeln!(
                o,
                " {:08x} {:08x} {:08x} {:08x} {} {:08x} {:08x} {} {:08x} {:08x} {} {} {} {:08x} {:08x} {} {} {}",
                bits(e.volume),
                bits(e.volume_rand_db),
                bits(e.pitch),
                bits(e.pitch_rand),
                u8::from(e.is_3d),
                bits(e.pan_level),
                bits(e.reverb_gain),
                u8::from(e.one_shot),
                bits(e.min_dist),
                bits(e.max_dist),
                e.rolloff,
                e.max_playbacks,
                e.max_behavior,
                bits(e.fade_in),
                bits(e.fade_out),
                u8::from(e.stream),
                e.category_type,
                u8::from(e.loaded)
            );
            for p in &e.params {
                let _ = o.write_all(b" P ");
                let _ = o.write_all(&p.name);
                let _ = writeln!(
                    o,
                    " {:08x} {:08x} {:08x} {:08x} {:08x} {} {}",
                    bits(p.min),
                    bits(p.max),
                    bits(p.velocity),
                    bits(p.seek),
                    bits(p.sustain),
                    p.loop_mode,
                    p.builtin
                );
            }
            for (n, d) in e.defs.iter().enumerate() {
                let _ = writeln!(
                    o,
                    " D {n} {} {:08x} {:08x} {:08x} {:08x} {:08x} {:08x} {} {:08x} {:08x} {} {} {}",
                    u8::from(d.sequential),
                    bits(d.volume),
                    bits(d.volume_rand_db),
                    bits(d.pitch),
                    bits(d.pitch_rand),
                    bits(d.spawn_min),
                    bits(d.spawn_max),
                    d.spawn_count,
                    bits(d.delay_min),
                    bits(d.delay_max),
                    d.last,
                    d.waves.len(),
                    d.files.len()
                );
                for w in &d.waves {
                    let _ = o.write_all(b"  W ");
                    let _ = o.write_all(&w.bank);
                    let _ = o.write_all(b" ");
                    let _ = o.write_all(&w.sample);
                    let _ = o.write_all(b"\n");
                }
                for f in &d.files {
                    let _ = o.write_all(b"  F ");
                    let _ = o.write_all(f);
                    let _ = o.write_all(b"\n");
                }
            }
            for (n, l) in e.layers.iter().enumerate() {
                let _ = writeln!(o, " L {n} {}", l.param);
                for s in &l.sounds {
                    let _ = writeln!(
                        o,
                        "  S {} {:08x} {} {:08x} {:08x}",
                        s.def,
                        bits(s.volume),
                        s.loop_mode,
                        bits(s.x0),
                        bits(s.x1)
                    );
                }
                for en in &l.envelopes {
                    let _ = write!(o, "  E {} {} {}", en.dsp, en.param, en.points.len());
                    for p in &en.points {
                        let _ = write!(o, " {:08x},{:08x}", bits(p[0]), bits(p[1]));
                    }
                    let _ = o.write_all(b"\n");
                }
            }
        }
        o
    }
}

mod hook {
    use super::super::instance::Bank;
    use std::fs::File;
    use std::io::Write;
    use std::sync::Mutex;
    use std::sync::OnceLock;
    use std::sync::atomic::{AtomicU64, Ordering};

    static OUT: OnceLock<Option<Mutex<File>>> = OnceLock::new();
    static FRAME: AtomicU64 = AtomicU64::new(0);

    fn out() -> Option<&'static Mutex<File>> {
        OUT.get_or_init(|| {
            let p = std::env::var_os("OHPL_S7_DUMP").filter(|p| !p.is_empty())?;
            File::create(p).ok().map(Mutex::new)
        })
        .as_ref()
    }

    pub(super) fn write(b: &[u8]) {
        if let Some(Ok(mut f)) = out().map(Mutex::lock) {
            let _ = f.write_all(b);
        }
    }

    pub(super) fn fnv(b: &[u8]) -> u64 {
        b.iter().fold(0xcbf2_9ce4_8422_2325, |h, &c| {
            (h ^ u64::from(c)).wrapping_mul(0x0100_0000_01b3)
        })
    }

    impl Bank {
        pub fn dump_frame(&self, paused: bool) {
            if out().is_none() {
                return;
            }
            let n = FRAME.fetch_add(1, Ordering::Relaxed);
            let mut o: Vec<u8> = Vec::new();
            for i in self.live() {
                let s = &i.st;
                let ev = self.events.list.get(s.event);
                let _ = write!(o, "F{n} p={} id={} ", u8::from(paused), s.id);
                if let Some(e) = ev {
                    let _ = o.write_all(&e.name);
                }
                let _ = write!(
                    o,
                    " type={} cat={} vol={:08x} vm={:08x} sm={:08x} fade={:08x} paused={} stopped={} started={} t={:08x} aud={:08x} voices={}",
                    s.entry_type,
                    ev.map_or(-1, |e| e.category_type),
                    s.volume.to_bits(),
                    s.volume_mul.to_bits(),
                    s.speed_mul.to_bits(),
                    s.fade.to_bits(),
                    u8::from(s.paused),
                    u8::from(s.stopped),
                    u8::from(s.started),
                    s.time.to_bits(),
                    s.audibility.to_bits(),
                    i.voices.len()
                );
                for p in &i.param_value {
                    let _ = write!(o, " {:08x}", p.to_bits());
                }
                for v in &i.voices {
                    let _ = write!(
                        o,
                        " [{} {} {:08x} {:08x} {:08x}]",
                        v.layer,
                        v.sound,
                        v.gain.to_bits(),
                        v.speed.to_bits(),
                        v.gain_hf.to_bits()
                    );
                }
                let _ = o.write_all(b"\n");
            }
            write(&o);
        }
    }
}

pub(super) fn text(tag: &[u8], x: &[u8]) {
    let mut o = tag.to_vec();
    o.push(b' ');
    o.extend_from_slice(x);
    o.push(b'\n');
    hook::write(&o);
}

/// `tag name length fnv1a64` of a produced sample or file.
pub(super) fn blob(tag: &[u8], name: &[u8], d: &[u8]) {
    let mut o = tag.to_vec();
    o.push(b' ');
    o.extend_from_slice(name);
    o.extend_from_slice(format!(" {} {:016x}\n", d.len(), hook::fnv(d)).as_bytes());
    hook::write(&o);
}

pub(super) fn fsb_digests(out: &std::collections::BTreeMap<Vec<u8>, Vec<u8>>, cache: &[u8]) {
    for name in out.values() {
        if let Some(d) = super::extract::read_whole(&[cache, name].concat()) {
            blob(b"FSB", name, &d);
        }
    }
}

/// # Safety
/// Both are valid for their lengths.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_s7_dump_text(tag: *const u8, tn: usize, t: *const u8, n: usize) {
    // SAFETY: caller contract.
    unsafe {
        text(
            std::slice::from_raw_parts(tag, tn),
            std::slice::from_raw_parts(t, n),
        )
    }
}
