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
