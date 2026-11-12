//! Replays `rust/tests/golden/H2/*.txt` (recorded from the pre-port C++ by the harness in
//! `rust/tests/golden/H2/harness`) over the Steam game data; skipped where the data is absent.

use super::config::{Config, Status};
use super::searcher::Searcher;
use crate::string;
use crate::xml::Doc;
use std::path::{Path, PathBuf};

const GAMES: [(&str, &str); 4] = [
    ("soma", "SOMA"),
    ("amnesia", "Amnesia The Dark Descent"),
    ("rebirth", "Amnesia Rebirth"),
    ("bunker", "Amnesia The Bunker"),
];

fn fnv(h: u64, bytes: &[u8]) -> u64 {
    bytes.iter().fold(h, |h, &b| {
        (h ^ u64::from(b)).wrapping_mul(1_099_511_628_211)
    })
}

const FNV_BASIS: u64 = 1_469_598_103_934_665_603;

fn golden(name: &str) -> String {
    let p = Path::new(env!("CARGO_MANIFEST_DIR"))
        .join("tests/golden/H2")
        .join(name);
    std::fs::read_to_string(p).unwrap_or_default()
}

fn game_root(key: &str) -> Option<PathBuf> {
    let base = std::env::var_os("OHPL_H2_CORPUS").map_or_else(
        || {
            let home = std::env::var_os("HOME").unwrap_or_default();
            Path::new(&home).join(".local/share/Steam/steamapps/common")
        },
        PathBuf::from,
    );
    let dir = GAMES.iter().find(|g| g.0 == key)?.1;
    let root = base.join(dir);
    root.is_dir().then_some(root)
}

fn scratch(name: &str) -> PathBuf {
    let base = std::env::var_os("XDG_CACHE_HOME").map_or_else(
        || Path::new(&std::env::var_os("HOME").unwrap_or_default()).join(".cache"),
        PathBuf::from,
    );
    let dir = base.join("open-hpl/h2-tmp");
    std::fs::create_dir_all(&dir).ok();
    dir.join(name)
}

/// `ok|row|col|desc` header, then `SaveFile` and `operator<<` output, as `xml_probe tiny` hashes.
fn xml_digest(data: &[u8]) -> u64 {
    let mut doc = Doc::new();
    let ok = doc.load(data, true);
    let mut s = if ok {
        b"1|0|0|\n".to_vec()
    } else {
        let e = doc.error_info();
        format!("0|{}|{}|{}\n", e.row, e.col, e.desc).into_bytes()
    };
    s.extend(doc.print_file());
    s.extend(doc.print_stream());
    fnv(FNV_BASIS, &s)
}

#[test]
fn xml_corpus_matches_tinyxml() {
    let mut checked = 0usize;
    let mut missing = 0usize;
    for line in golden("xml_raw.txt").lines() {
        let Some((key, digest)) = line.rsplit_once('\t') else {
            continue;
        };
        let Some((game, rel)) = key.split_once(':') else {
            continue;
        };
        let Some(root) = game_root(game) else {
            missing += 1;
            continue;
        };
        let Ok(data) = std::fs::read(root.join(rel)) else {
            missing += 1;
            continue;
        };
        assert_eq!(format!("{:016x}", xml_digest(&data)), digest, "{key}");
        checked += 1;
    }
    eprintln!("xml golden: {checked} checked, {missing} skipped");
}

#[test]
fn config_roundtrip_plain_and_crc() {
    let path = scratch("cfg_unit.cfg");
    let path8 = path.as_os_str().as_encoded_bytes().to_vec();
    let mut c = Config::default();
    c.set(b"Main", b"Name", b"a<b>&\"c");
    c.set(b"Main", b"Name", b"replaced");
    c.set(b"Other", b"N", b"1");
    for crc in [false, true] {
        assert_eq!(c.save(&path8, crc), Status::Ok);
        let mut back = Config::default();
        assert_eq!(back.load(&path8, crc), Status::Ok);
        assert_eq!(back.get(b"Main", b"Name"), Some(&b"replaced"[..]));
        assert_eq!(back.get(b"Other", b"N"), Some(&b"1"[..]));
        assert_eq!(back.get(b"Main", b"Missing"), None);
    }
    let mut plain = Config::default();
    plain.set(b"A", b"b", b"c");
    assert_eq!(plain.save(&path8, false), Status::Ok);
    assert_eq!(plain.load(&path8, true), Status::Crc);
    assert_eq!(plain.load(b"/nonexistent/h2.cfg", false), Status::Open);
}

fn collect(dir: &Path, out: &mut Vec<Vec<u8>>) {
    let Ok(rd) = std::fs::read_dir(dir) else {
        return;
    };
    for e in rd.flatten() {
        let p = e.path();
        if p.is_dir() {
            collect(&p, out);
        } else if p.is_file() {
            out.push(p.as_os_str().as_encoded_bytes().to_vec());
        }
    }
}

fn base_name(p: &[u8]) -> &[u8] {
    p.rsplit(|&b| b == b'/').next().unwrap_or(p)
}

fn parent_of(p: &[u8]) -> &[u8] {
    p.iter()
        .rposition(|&b| b == b'/')
        .map_or(&[][..], |i| &p[..i])
}

fn fs_digest(root: &Path) -> (usize, usize, u64) {
    let root8 = root.as_os_str().as_encoded_bytes().to_vec();
    let mut s = Searcher::default();
    s.add_directory(&string::utf8_to_wide(&root8), b"*.*", true);
    let mut files = Vec::new();
    collect(root, &mut files);
    files.sort();
    let (mut count, mut total) = (0usize, FNV_BASIS);
    let mut query = |s: &Searcher, q: &[u8], with_count: bool| {
        let (path, eq) = match s.get(q, with_count) {
            Some((id, eq)) => (string::wide_to_utf8(s.path(id)), eq),
            None => (Vec::new(), with_count.then_some(0)),
        };
        let mut line = q.to_vec();
        line.push(b'\t');
        line.extend(path);
        line.extend(format!("\t{}\n", eq.unwrap_or(-99)).bytes());
        total = fnv(total, &line);
        count += 1;
    };
    for f in &files {
        let name = base_name(f);
        let parent = parent_of(f);
        let pname = base_name(parent);
        let gname = base_name(parent_of(parent));
        let join = |a: &[u8], sep: u8, b: &[u8]| [a, &[sep], b].concat();
        query(&s, name, false);
        query(&s, name, true);
        query(&s, &join(pname, b'/', name), true);
        query(&s, &join(&join(gname, b'\\', pname), b'/', name), true);
        query(&s, &join(b"nodir_zz", b'/', name), true);
        query(&s, &name.to_ascii_uppercase(), false);
    }
    query(&s, b"definitely_missing_file.xyz", true);
    s.clear();
    query(&s, files.first().map_or(&b"x"[..], |f| base_name(f)), true);
    (files.len(), count, total)
}

#[test]
fn searcher_corpus_matches_cpp() {
    for line in golden("fs.txt").lines() {
        let f: Vec<&str> = line.split('\t').collect();
        let [game, files, queries, digest] = f[..] else {
            continue;
        };
        let Some(root) = game_root(game) else {
            continue;
        };
        let (nf, nq, d) = fs_digest(&root);
        assert_eq!(
            (nf.to_string(), nq.to_string(), format!("{d:016x}")),
            (files.to_string(), queries.to_string(), digest.to_string()),
            "{game}"
        );
    }
}
