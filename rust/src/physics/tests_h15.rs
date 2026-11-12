//! H15 replay: surface-data parse of the real materials.cfg files against the hashes recorded
//! from the legacy C++ loader (rust/tests/golden/H15/surface_parse.txt). Skips without game data.

use super::host::Str;
use super::surface_xml::{self, ImpactDef, SurfaceDef};
use std::collections::BTreeMap;
use std::path::PathBuf;

#[derive(Default)]
struct Enc(Vec<u8>);

impl Enc {
    fn u32(&mut self, v: u32) {
        self.0.extend_from_slice(&v.to_le_bytes());
    }
    fn i32(&mut self, v: i32) {
        self.u32(v as u32);
    }
    fn f(&mut self, v: f32) {
        self.u32(v.to_bits());
    }
    fn s(&mut self, v: Str) {
        // SAFETY: the parser keeps the strings alive during the sink call.
        let b = unsafe { v.bytes() };
        self.u32(b.len() as u32);
        self.0.extend_from_slice(b);
    }
    fn impacts(&mut self, p: *const ImpactDef, n: usize) {
        self.u32(n as u32);
        for i in 0..n {
            // SAFETY: `p` points at `n` defs owned by the parser for the sink call.
            let d = unsafe { *p.add(i) };
            self.f(d.min_speed);
            self.s(d.sound);
            self.s(d.ps);
            self.i32(d.ps_prio);
        }
    }
    fn def(&mut self, d: &SurfaceDef) {
        for s in [
            d.name,
            d.user_var,
            d.step_type,
            d.scrape_sound,
            d.roll_sound,
        ] {
            self.s(s);
        }
        for v in [d.elasticity, d.kinetic_friction, d.static_friction] {
            self.f(v);
        }
        for v in [d.priority, d.elasticity_mode, d.friction_mode] {
            self.i32(v);
        }
        for v in [
            d.min_scrape_speed,
            d.min_scrape_freq,
            d.min_scrape_freq_speed,
            d.middle_scrape_speed,
            d.max_scrape_freq_speed,
            d.max_scrape_freq,
        ] {
            self.f(v);
        }
        self.i32(d.min_scrape_contacts);
        for v in [
            d.min_roll_speed,
            d.min_roll_freq,
            d.min_roll_volume,
            d.min_roll_freq_speed,
            d.middle_roll_speed,
            d.max_roll_freq_speed,
            d.max_roll_freq,
            d.max_roll_volume,
        ] {
            self.f(v);
        }
        self.u32(d.roll_axis_flags);
        self.impacts(d.impacts, d.n_impacts);
        self.impacts(d.hits, d.n_hits);
    }
}

fn fnv(b: &[u8]) -> u64 {
    b.iter().fold(0xcbf2_9ce4_8422_2325, |h, &x| {
        (h ^ u64::from(x)).wrapping_mul(0x0100_0000_01b3)
    })
}

/// C++ walks a `std::map` keyed by name; a duplicate name keeps the first definition.
fn parse_hash(data: &[u8]) -> (usize, u64) {
    let mut map: BTreeMap<Vec<u8>, Vec<u8>> = BTreeMap::new();
    surface_xml::parse(data, true, |d| {
        let mut e = Enc::default();
        e.def(d);
        // SAFETY: name valid during the sink call.
        let name = unsafe { d.name.bytes() }.to_vec();
        map.entry(name).or_insert(e.0);
    });
    let all: Vec<u8> = map.values().flatten().copied().collect();
    (map.len(), fnv(&all))
}

fn steam_common() -> PathBuf {
    std::env::var_os("STEAM_COMMON").map_or_else(
        || {
            PathBuf::from(std::env::var_os("HOME").unwrap_or_default())
                .join(".local/share/Steam/steamapps/common")
        },
        PathBuf::from,
    )
}

#[test]
fn real_materials_cfg_match_legacy_loader() {
    let golden = include_str!("../../tests/golden/H15/surface_parse.txt");
    let root = steam_common();
    let mut checked = 0;
    for line in golden.lines() {
        let mut it = line.rsplitn(3, ' ');
        let (Some(hash), Some(n), Some(file)) = (it.next(), it.next(), it.next()) else {
            panic!("bad golden line {line}");
        };
        let Ok(data) = std::fs::read(root.join(file)) else {
            continue;
        };
        let want = (
            n.parse::<usize>().unwrap_or(usize::MAX),
            u64::from_str_radix(hash, 16).unwrap_or(0),
        );
        assert_eq!(parse_hash(&data), want, "{file}");
        checked += 1;
    }
    eprintln!(
        "surface golden: {checked}/{} files match",
        golden.lines().count()
    );
}

#[test]
fn surface_hash_is_order_independent_of_duplicates() {
    let (n, _) = parse_hash(b"<Materials><Material Name=\"a\"/><Material Name=\"a\" Priority=\"3\"/><Material Name=\"\"/></Materials>");
    assert_eq!(n, 1);
}
