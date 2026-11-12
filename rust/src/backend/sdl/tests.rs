//! Replays the L1 golden vectors (rust/tests/golden/L1, generated from the original C++ by gen/gen.sh).

use super::fs::{find_files, find_folders, pati_match};
use super::input::{flatten, route};
use super::plat::unique_modes;
use super::start::parse_args;
use super::sync::translate_prio;
use super::timer::micros;
use crate::sys::sdl2::{
    SDL_CONTROLLERDEVICEADDED, SDL_CONTROLLERDEVICEREMOVED, SDL_DisplayMode, SDL_Event, SDL_QUIT,
};
use std::path::{Path, PathBuf};

fn golden(name: &str) -> String {
    let p = Path::new(env!("CARGO_MANIFEST_DIR"))
        .join("tests/golden/L1")
        .join(name);
    std::fs::read_to_string(&p).unwrap_or_else(|e| panic!("{}: {e}", p.display()))
}

fn unhex(s: &str) -> Vec<u8> {
    (0..s.len() / 2)
        .map(|i| u8::from_str_radix(&s[i * 2..i * 2 + 2], 16).unwrap())
        .collect()
}

fn wide(s: &str) -> Vec<u32> {
    s.chars().map(u32::from).collect()
}

#[test]
fn events_match_the_cpp_conversion() {
    let mut n = 0;
    for line in golden("events.txt").lines() {
        let mut it = line.split(' ');
        assert_eq!(it.next(), Some("E"));
        let raw = unhex(it.next().unwrap());
        let want = unhex(it.next().unwrap());
        assert_eq!(raw.len(), size_of::<SDL_Event>());
        let mut ev: SDL_Event = unsafe { std::mem::zeroed() };
        unsafe {
            std::ptr::copy_nonoverlapping(raw.as_ptr(), (&raw mut ev).cast::<u8>(), raw.len())
        };
        let got = flatten(&ev);
        let got_bytes =
            unsafe { std::slice::from_raw_parts((&raw const got).cast::<u8>(), size_of_val(&got)) };
        assert_eq!(got_bytes, want.as_slice(), "event {n}");
        n += 1;
    }
    assert_eq!(n, 600);
}

#[test]
fn pattern_matching_matches_the_cpp() {
    let mut n = 0;
    for line in golden("pati.txt").lines() {
        let f: Vec<&str> = line.split('\t').collect();
        let want = f[2] == "1";
        assert_eq!(pati_match(&wide(f[0]), &wide(f[1])), want, "{line}");
        n += 1;
    }
    assert_eq!(n, 224);
}

#[test]
fn timer_micros_match_the_cpp() {
    let mut n = 0;
    for line in golden("timer.txt").lines() {
        let f: Vec<&str> = line.split(' ').collect();
        let v: Vec<i64> = f[..4].iter().map(|x| x.parse().unwrap()).collect();
        let want = u64::from_str_radix(f[4], 16).unwrap();
        let got = micros(v[2], v[3]) - micros(v[0], v[1]);
        assert_eq!(got.to_bits(), want, "{line}");
        n += 1;
    }
    assert_eq!(n, 200);
}

fn mode(bpp: i32, w: i32, h: i32) -> SDL_DisplayMode {
    SDL_DisplayMode {
        format: (bpp as u32) << 8,
        w,
        h,
        refresh_rate: 0,
        driverdata: std::ptr::null_mut(),
    }
}

#[test]
fn video_modes_match_the_cpp_set() {
    let text = golden("modes.txt");
    let mut lines = text.lines().peekable();
    let mut cases = 0;
    while let Some(head) = lines.next() {
        let nd: usize = head.strip_prefix("CASE ").unwrap().parse().unwrap();
        let mut displays = Vec::new();
        for _ in 0..nd {
            let d: Vec<i32> = lines
                .next()
                .unwrap()
                .strip_prefix("DISPLAY ")
                .unwrap()
                .split(' ')
                .map(|x| x.parse().unwrap())
                .collect();
            let mut modes = Vec::new();
            for _ in 0..d[1] {
                let m: Vec<i32> = lines
                    .next()
                    .unwrap()
                    .strip_prefix("MODE ")
                    .unwrap()
                    .split(' ')
                    .map(|x| x.parse().unwrap())
                    .collect();
                modes.push(mode(m[2], m[0], m[1]));
            }
            displays.push((mode(d[0], 0, 0), modes));
        }
        let mut want = Vec::new();
        while let Some(l) = lines.next_if(|l| l.starts_with("OUT ")) {
            let o: Vec<i32> = l[4..].split(' ').map(|x| x.parse().unwrap()).collect();
            want.push((o[0], o[1], o[2], o[3]));
        }
        assert_eq!(unique_modes(&displays), want, "case {cases}");
        cases += 1;
    }
    assert_eq!(cases, 60);
}

#[test]
fn argument_pass_matches_the_cpp() {
    let text = golden("args.txt");
    let mut lines = text.lines();
    let mut n = 0;
    while let Some(a) = lines.next() {
        let args: Vec<Vec<u8>> = a
            .strip_prefix("ARGS [")
            .and_then(|a| a.strip_suffix(']'))
            .unwrap()
            .split('|')
            .filter(|s| !s.is_empty())
            .map(|s| s.as_bytes().to_vec())
            .collect();
        let cmd = lines
            .next()
            .unwrap()
            .strip_prefix("CMD [")
            .and_then(|l| l.strip_suffix(']'))
            .unwrap();
        let cwd = lines.next().unwrap() == "CWD 1";
        let (got_cmd, got_cwd) = parse_args(&args, |p| {
            if p.starts_with(b"dir") {
                Some(true)
            } else if p.starts_with(b"baddir") {
                Some(false)
            } else {
                None
            }
        });
        assert_eq!(String::from_utf8(got_cmd).unwrap(), cmd, "{a}");
        assert_eq!(got_cwd, cwd, "{a}");
        n += 1;
    }
    assert_eq!(n, 14);
}

fn scratch(name: &str) -> PathBuf {
    let d = std::env::temp_dir().join(format!("ohpl-l1-{name}-{}", std::process::id()));
    let _ = std::fs::remove_dir_all(&d);
    std::fs::create_dir_all(&d).unwrap();
    d
}

fn fixture() -> PathBuf {
    let base = scratch("find");
    for n in [
        "a.TXT",
        "b.txt",
        "c.dat",
        ".hidden.txt",
        "Zed.Txt",
        "x y.txt",
        "ab",
        "10.txt",
        "9.txt",
    ] {
        std::fs::write(base.join(n), b"").unwrap();
    }
    for n in ["Sub", "subb", ".dotdir", "dir.txt"] {
        std::fs::create_dir(base.join(n)).unwrap();
    }
    std::os::unix::fs::symlink("Sub", base.join("linkdir")).unwrap();
    std::os::unix::fs::symlink("missing", base.join("dangling")).unwrap();
    base
}

fn names(v: Vec<Vec<u32>>) -> Vec<String> {
    v.into_iter()
        .map(|w| w.into_iter().filter_map(char::from_u32).collect())
        .collect()
}

#[test]
fn directory_listing_matches_the_cpp() {
    let base = fixture();
    let dir = base.to_str().unwrap().as_bytes().to_vec();
    let mut slash = dir.clone();
    slash.push(b'/');
    let mut n = 0;
    for line in golden("find.txt").lines() {
        let f: Vec<&str> = line.split('\t').collect();
        let head: Vec<&str> = f[0].split(' ').collect();
        let want: Vec<String> = f[1..].iter().map(|s| s.to_string()).collect();
        let got = match head[0] {
            "FILES" => names(find_files(&dir, &wide(head[1]))),
            "FILESSLASH" => names(find_files(&slash, &wide(head[1]))),
            "FOLDERS" => names(find_folders(&dir, head[1] == "1", head[2] == "1")),
            other => panic!("{other}"),
        };
        assert_eq!(got, want, "{line}");
        n += 1;
    }
    assert_eq!(n, 20);
    let _ = std::fs::remove_dir_all(&base);
}

#[test]
fn routing_keeps_hotplug_and_drops_quit() {
    let mut e: SDL_Event = unsafe { std::mem::zeroed() };
    e.type_ = SDL_CONTROLLERDEVICEADDED;
    assert_eq!(route(&e, |_| false), (true, 2));
    e.type_ = SDL_CONTROLLERDEVICEREMOVED;
    assert_eq!(route(&e, |_| false), (true, 4));
    assert_eq!(route(&e, |_| true), (false, 4));
    e.type_ = SDL_QUIT;
    assert_eq!(route(&e, |_| false), (false, 1));
    e.type_ = 0x300;
    assert_eq!(route(&e, |_| true), (false, 0));
}

#[test]
fn thread_priorities_map_to_sdl() {
    assert_eq!(translate_prio(0), 0);
    assert_eq!(translate_prio(1), 1);
    assert_eq!(translate_prio(2), 2);
    assert_eq!(translate_prio(3), 1);
}

#[test]
fn file_primitives_behave_like_posix() {
    use super::fs::{create_folder, file_exists, file_size, folder_exists, full_path, remove_file};
    let base = scratch("prim");
    let sub = base.join("a/b/c");
    let sub8 = sub.to_str().unwrap().as_bytes();
    assert!(create_folder(sub8));
    assert!(!create_folder(sub8));
    assert!(folder_exists(sub8));
    let file = base.join("f.bin");
    std::fs::write(&file, [1u8; 17]).unwrap();
    let f8 = file.to_str().unwrap().as_bytes();
    assert_eq!(file_size(f8), 17);
    assert!(file_exists(f8) && !folder_exists(f8));
    assert_eq!(file_size(b"/nonexistent/x"), 0);
    assert_eq!(
        full_path(f8).unwrap(),
        std::fs::canonicalize(&file)
            .unwrap()
            .as_os_str()
            .as_encoded_bytes()
    );
    remove_file(f8);
    assert!(!file_exists(f8));
    let _ = std::fs::remove_dir_all(&base);
}

#[test]
fn library_folders_give_steam_roots() {
    use std::os::unix::ffi::OsStrExt;
    let base = scratch("vdf");
    std::fs::create_dir_all(base.join("lib/steamapps/common")).unwrap();
    let vdf = base.join("libraryfolders.vdf");
    let lib = std::fs::canonicalize(base.join("lib")).unwrap();
    let text = format!(
        "\"libraryfolders\"\n{{\n\t\"0\"\n\t{{\n\t\t\"path\"\t\t\"{}\"\n\t}}\n\t\"1\" {{ \"path\" \"/nonexistent\" }}\n}}",
        lib.display()
    );
    std::fs::write(&vdf, text).unwrap();
    let mut roots = std::collections::BTreeSet::new();
    super::start::add_library_folders(&mut roots, vdf.as_os_str().as_bytes());
    let want = [lib.as_os_str().as_bytes(), b"/steamapps/common"].concat();
    assert_eq!(roots.into_iter().collect::<Vec<_>>(), vec![want]);
    let _ = std::fs::remove_dir_all(&base);
}
