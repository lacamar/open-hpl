//! Replays `rust/tests/golden/H3/mgr.txt`: the stub-manager op sequence of `harness/mgr_probe.cpp`
//! (recorded from the C++ `iResourceManager`) over the Steam game data; skipped where the data is absent.

use super::manager::{self, Manager};
use super::searcher::Searcher;
use crate::resources::ResMeta;
use crate::string;
use std::collections::BTreeMap;
use std::ffi::c_void;
use std::path::{Path, PathBuf};

const GAMES: [(&str, &str); 4] = [
    ("soma", "SOMA"),
    ("amnesia", "Amnesia The Dark Descent"),
    ("rebirth", "Amnesia Rebirth"),
    ("bunker", "Amnesia The Bunker"),
];
const BASIS: u64 = 1_469_598_103_934_665_603;

pub(super) fn fnv(h: u64, bytes: &[u8]) -> u64 {
    bytes.iter().fold(h, |h, &b| {
        (h ^ u64::from(b)).wrapping_mul(1_099_511_628_211)
    })
}

pub(super) fn golden(name: &str) -> String {
    std::fs::read_to_string(
        Path::new(env!("CARGO_MANIFEST_DIR"))
            .join("tests/golden/H3")
            .join(name),
    )
    .unwrap_or_default()
}

pub(super) fn game_root(key: &str) -> Option<PathBuf> {
    let base = std::env::var_os("OHPL_H3_CORPUS").map_or_else(
        || {
            Path::new(&std::env::var_os("HOME").unwrap_or_default())
                .join(".local/share/Steam/steamapps/common")
        },
        PathBuf::from,
    );
    let root = base.join(GAMES.iter().find(|g| g.0 == key)?.1);
    root.is_dir().then_some(root)
}

/// Regular files below `dir` as `/`-separated relative paths; directory symlinks are not followed.
pub(super) fn relative_files(root: &Path, dir: &Path, out: &mut Vec<Vec<u8>>) {
    let Ok(rd) = std::fs::read_dir(dir) else {
        return;
    };
    for e in rd.flatten() {
        let p = e.path();
        if e.file_type().is_ok_and(|t| t.is_dir()) {
            relative_files(root, &p, out);
        } else if std::fs::metadata(&p).is_ok_and(|m| m.is_file())
            && let Ok(rel) = p.strip_prefix(root)
        {
            out.push(rel.as_os_str().as_encoded_bytes().to_vec());
        }
    }
}

struct Res {
    id: usize,
    path: Vec<u32>,
    meta: ResMeta,
}

struct Sim {
    fs: Searcher,
    mgr: Manager,
    res: BTreeMap<usize, Box<Res>>,
    serial: usize,
    op: u64,
    seed: u64,
}

fn ptr(id: usize) -> *mut c_void {
    std::ptr::without_provenance_mut(id)
}

impl Sim {
    fn rnd(&mut self, n: u64) -> u64 {
        self.seed ^= self.seed << 13;
        self.seed ^= self.seed >> 7;
        self.seed ^= self.seed << 17;
        self.seed.checked_rem(n).unwrap_or(0)
    }

    fn live(&mut self) -> Vec<usize> {
        (0..self.mgr.len())
            .filter_map(|i| {
                let e = self.mgr.at(i);
                // SAFETY: `at` returns a live entry for `i < len`.
                (!e.is_null()).then(|| unsafe { (*e).second } as usize)
            })
            .collect()
    }

    fn id(&self, id: usize) -> String {
        if id == 0 {
            "null".into()
        } else {
            id.to_string()
        }
    }

    fn users(&self, id: usize) -> u32 {
        self.res.get(&id).map_or(0, |r| r.meta.user_count)
    }

    fn destroy(&mut self, id: usize) {
        if self.users(id) == 0 && self.mgr.remove(ptr(id)) {
            self.res.remove(&id);
        }
    }

    fn create(&mut self, name: &[u8]) -> String {
        manager::begin_load(name, 0);
        let (path, eq) = match self.fs.get(name, true) {
            Some((i, c)) => (self.fs.path(i).to_vec(), c.unwrap_or(-1)),
            None => (Vec::new(), 0),
        };
        let mut found = self.mgr.get(&path) as usize;
        let mut shown = path.clone();
        if found != 0 {
            shown.clear();
        } else if !path.is_empty() {
            self.serial += 1;
            let mut r = Box::new(Res {
                id: self.serial,
                path,
                meta: ResMeta::new(0, self.op),
            });
            r.meta.handle = self.serial as u64;
            // SAFETY: the box outlives its manager entry (removed before drop).
            unsafe { self.mgr.add(ptr(r.id), &r.path, &r.meta) };
            found = r.id;
            self.res.insert(found, r);
        }
        if let Some(r) = self.res.get_mut(&found) {
            r.meta.user_count += 1;
            r.meta.time = self.op;
        }
        manager::end_load(0);
        format!(
            "create {} eq={eq} -> {} {} tabs={}",
            String::from_utf8_lossy(name),
            self.id(found),
            String::from_utf8_lossy(&string::wide_to_utf8(&shown)),
            2 * manager::tab_count()
        )
    }

    fn nest(&mut self, depth: i32) -> String {
        let mut out = String::new();
        for i in 0..depth {
            manager::begin_load(format!("n{i}").as_bytes(), 0);
            out += &format!("{},", 2 * manager::tab_count());
        }
        for _ in 0..depth {
            manager::end_load(0);
        }
        out + &(2 * manager::tab_count()).to_string()
    }

    fn dump(&mut self) -> String {
        let live = self.live();
        let mut s = Vec::new();
        for &id in &live {
            if let Some(r) = self.res.get(&id) {
                s.extend(string::wide_to_utf8(&r.path));
                s.extend(format!(":{}:{};", r.id, r.meta.user_count).bytes());
            }
        }
        format!("dump {} {:016x}", live.len(), fnv(BASIS, &s))
    }
}

pub(super) fn run(root: &Path, ops: i64) -> u64 {
    let mut rels = Vec::new();
    relative_files(root, root, &mut rels);
    rels.sort();
    let mut names: Vec<Vec<u8>> = Vec::new();
    let stride = (rels.len() / 3000).max(1);
    for rel in rels.iter().step_by(stride) {
        let slash = rel.iter().rposition(|&b| b == b'/');
        let base = slash.map_or(&rel[..], |i| &rel[i + 1..]);
        let dir = slash.map_or(&[][..], |i| &rel[..i]);
        let parent = dir.rsplit(|&b| b == b'/').next().unwrap_or(&[]);
        names.push(base.to_vec());
        if !parent.is_empty() {
            names.push([parent, b"/", base].concat());
        }
        names.push(base.to_ascii_uppercase());
    }
    names.push(b"does_not_exist.png".to_vec());

    let mut fs = Searcher::default();
    let root8 = root.as_os_str().as_encoded_bytes();
    fs.add_directory(&string::utf8_to_wide(root8), b"*.*", true);
    manager::reset_tabs();
    let mut sim = Sim {
        fs,
        mgr: Manager::default(),
        res: BTreeMap::new(),
        serial: 0,
        op: 0,
        seed: 88_172_645_463_325_252,
    };
    let mut total = BASIS;
    let dump = std::env::var_os("OHPL_H3_DUMP");
    let emit = |line: String, total: &mut u64| {
        if let Some(d) = &dump {
            use std::io::Write;
            if let Ok(mut f) = std::fs::OpenOptions::new()
                .create(true)
                .append(true)
                .open(d)
            {
                writeln!(f, "{line}").ok();
            }
        }
        *total = fnv(*total, (line + "\n").as_bytes());
    };
    let pick = |sim: &mut Sim, live: &[usize]| {
        live.get(sim.rnd(live.len() as u64) as usize)
            .copied()
            .unwrap_or(0)
    };
    for op in 0..ops {
        sim.op = op as u64;
        let k = sim.rnd(100);
        let live = if k >= 40 { sim.live() } else { Vec::new() };
        let has = !live.is_empty();
        let line = if k < 40 {
            let n = names
                .get(sim.rnd(names.len() as u64) as usize)
                .cloned()
                .unwrap_or_default();
            sim.create(&n)
        } else if k < 55 && has {
            let id = pick(&mut sim, &live);
            if let Some(r) = sim.res.get_mut(&id) {
                r.meta.dec_user();
            }
            let mut l = format!("release {id} users={}", sim.users(id));
            if sim.rnd(2) != 0 {
                sim.destroy(id);
                l += &format!(" destroy {id} size={}", sim.live().len());
            }
            l
        } else if k < 65 && has {
            let id = pick(&mut sim, &live);
            if let Some(r) = sim.res.get_mut(&id) {
                r.meta.user_count += 1;
                r.meta.time = op as u64;
            }
            format!("addref {id} users={}", sim.users(id))
        } else if k < 70 {
            let keep = sim.rnd(20) as i64;
            for p in sim.mgr.eviction_tail(keep) {
                let id = p as usize;
                if sim.users(id) == 0 && sim.mgr.remove(p) {
                    sim.res.remove(&id);
                }
            }
            format!("unused {keep} {}", sim.dump())
        } else if k < 80 && has {
            let id = pick(&mut sim, &live);
            let mut path = sim.res.get(&id).map(|r| r.path.clone()).unwrap_or_default();
            if sim.rnd(5) == 0 {
                path.push(u32::from(b'x'));
            }
            let got = sim.mgr.get(&path) as usize;
            format!(
                "get {} -> {}",
                String::from_utf8_lossy(&string::wide_to_utf8(&path)),
                sim.id(got)
            )
        } else if k < 85 {
            let depth = 1 + sim.rnd(3) as i32;
            let nest = sim.nest(depth);
            let n = names
                .get(sim.rnd(names.len() as u64) as usize)
                .cloned()
                .unwrap_or_default();
            format!("nested {nest} {}", sim.create(&n))
        } else if k < 95 {
            sim.dump()
        } else if has {
            let id = pick(&mut sim, &live);
            if let Some(r) = sim.res.get_mut(&id) {
                r.meta.user_count = 0;
            }
            sim.destroy(id);
            format!("force {id} size={}", sim.live().len())
        } else {
            "noop".into()
        };
        emit(line, &mut total);
    }
    loop {
        let p = sim.mgr.first();
        if p.is_null() {
            break;
        }
        let id = p as usize;
        if let Some(r) = sim.res.get_mut(&id) {
            r.meta.user_count = 0;
        }
        sim.destroy(id);
    }
    emit(format!("final {}", sim.live().len()), &mut total);
    total
}

#[test]
fn manager_sequences_match_cpp() {
    let _g = manager::TEST_LOCK
        .lock()
        .unwrap_or_else(std::sync::PoisonError::into_inner);
    let mut checked = 0;
    for line in golden("mgr.txt").lines() {
        let f: Vec<&str> = line.split('\t').collect();
        let [game, ops, digest] = f[..] else {
            panic!("bad line {line}")
        };
        let Some(root) = game_root(game) else {
            continue;
        };
        assert_eq!(
            format!("{:016x}", run(&root, ops.parse().unwrap_or(0))),
            digest,
            "{game}"
        );
        checked += 1;
    }
    eprintln!("manager golden: {checked} games");
}
