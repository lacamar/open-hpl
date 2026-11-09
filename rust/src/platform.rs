//! XDG Base Directory / xdg-user-dirs resolution (byte paths, as `getenv` returns them).
#![allow(
    clippy::indexing_slicing,
    clippy::unwrap_used,
    clippy::expect_used,
    clippy::panic,
    clippy::unreachable,
    clippy::integer_division_remainder_used,
    clippy::string_slice,
    reason = "panic audit pending (open-hpl-smj.23)"
)]

use std::ffi::OsStr;
use std::os::unix::ffi::{OsStrExt, OsStringExt};

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Kind {
    Personal,
    DataHome,
    ConfigHome,
    CacheHome,
    StateHome,
    Pictures,
}

impl Kind {
    pub fn from_u32(k: u32) -> Option<Self> {
        Some(match k {
            0 => Self::Personal,
            1 => Self::DataHome,
            2 => Self::ConfigHome,
            3 => Self::CacheHome,
            4 => Self::StateHome,
            5 => Self::Pictures,
            _ => return None,
        })
    }
}

pub trait Env {
    fn var(&self, name: &str) -> Option<Vec<u8>>;
    fn read(&self, path: &[u8]) -> Option<Vec<u8>>;
}

pub struct SystemEnv;

impl Env for SystemEnv {
    fn var(&self, name: &str) -> Option<Vec<u8>> {
        std::env::var_os(name).map(OsStringExt::into_vec)
    }

    fn read(&self, path: &[u8]) -> Option<Vec<u8>> {
        std::fs::read(OsStr::from_bytes(path)).ok()
    }
}

fn with_slash(mut v: Vec<u8>) -> Vec<u8> {
    if v.last() != Some(&b'/') {
        v.push(b'/');
    }
    v
}

fn find(hay: &[u8], needle: &[u8]) -> Option<usize> {
    hay.windows(needle.len()).position(|w| w == needle)
}

/// `None` where HOME is needed but unset (the C++ original threw `logic_error`).
fn base_dir(env: &dyn Env, var: &str, default: &str) -> Option<Vec<u8>> {
    if let Some(v) = env.var(var).filter(|v| v.first() == Some(&b'/')) {
        return Some(with_slash(v));
    }
    let mut p = with_slash(env.var("HOME")?);
    p.extend_from_slice(default.as_bytes());
    Some(p)
}

fn user_dir(env: &dyn Env, key: &str, default: &str) -> Option<Vec<u8>> {
    let mut home = env.var("HOME").unwrap_or_default();
    if !home.is_empty() {
        home = with_slash(home);
    }
    let mut file = base_dir(env, "XDG_CONFIG_HOME", ".config/")?;
    file.extend_from_slice(b"user-dirs.dirs");
    let wanted = [key.as_bytes(), b"=\""].concat();
    for line in env.read(&file).unwrap_or_default().split(|&b| b == b'\n') {
        let Some(k) = find(line, &wanted) else {
            continue;
        };
        let start = k + wanted.len();
        let Some(end) = line[start..].iter().position(|&b| b == b'"') else {
            continue;
        };
        let mut val = line[start..start + end].to_vec();
        if let Some(h) = find(&val, b"$HOME") {
            val.splice(h..h + 5, home.iter().copied());
        }
        if !val.is_empty() {
            // the C++ round-trips through mbstowcs, which stops at NUL
            val.truncate(val.iter().position(|&b| b == 0).unwrap_or(val.len()));
            return Some(with_slash(val));
        }
    }
    home.extend_from_slice(default.as_bytes());
    Some(home)
}

pub fn special_path(env: &dyn Env, kind: Kind) -> Option<Vec<u8>> {
    match kind {
        Kind::Personal => env.var("HOME").map(with_slash),
        Kind::DataHome => base_dir(env, "XDG_DATA_HOME", ".local/share/"),
        Kind::ConfigHome => base_dir(env, "XDG_CONFIG_HOME", ".config/"),
        Kind::CacheHome => base_dir(env, "XDG_CACHE_HOME", ".cache/"),
        Kind::StateHome => base_dir(env, "XDG_STATE_HOME", ".local/state/"),
        Kind::Pictures => user_dir(env, "XDG_PICTURES_DIR", "Pictures/"),
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::collections::HashMap;

    #[derive(Default)]
    struct Fake {
        vars: HashMap<&'static str, &'static [u8]>,
        file: Option<&'static [u8]>,
    }

    impl Env for Fake {
        fn var(&self, name: &str) -> Option<Vec<u8>> {
            self.vars.get(name).map(|v| v.to_vec())
        }
        fn read(&self, path: &[u8]) -> Option<Vec<u8>> {
            assert!(path.ends_with(b"/user-dirs.dirs"));
            self.file.map(<[u8]>::to_vec)
        }
    }

    type Vars<'a> = &'a [(&'static str, &'static [u8])];
    type Case = (
        Vec<(&'static str, &'static [u8])>,
        Kind,
        Option<&'static str>,
    );

    fn fake(vars: Vars, file: Option<&'static [u8]>) -> Fake {
        Fake {
            vars: vars.iter().copied().collect(),
            file,
        }
    }

    fn path(f: &Fake, kind: Kind) -> Option<String> {
        special_path(f, kind).map(|v| String::from_utf8(v).unwrap())
    }

    #[test]
    fn base_dirs() {
        let h: Vars = &[("HOME", b"/h")];
        let hd = |v: &'static [u8]| -> Vec<(&'static str, &'static [u8])> {
            vec![("HOME", b"/h"), ("XDG_DATA_HOME", v)]
        };
        let cases: Vec<Case> = vec![
            (h.to_vec(), Kind::DataHome, Some("/h/.local/share/")),
            (h.to_vec(), Kind::StateHome, Some("/h/.local/state/")),
            (vec![("HOME", b"/h/")], Kind::CacheHome, Some("/h/.cache/")),
            (vec![("HOME", b"")], Kind::ConfigHome, Some("/.config/")),
            (hd(b"/d"), Kind::DataHome, Some("/d/")),
            (hd(b"/d//"), Kind::DataHome, Some("/d//")),
            (hd(b"rel"), Kind::DataHome, Some("/h/.local/share/")),
            (hd(b""), Kind::DataHome, Some("/h/.local/share/")),
            (vec![("XDG_DATA_HOME", b"rel")], Kind::DataHome, None),
        ];
        for (vars, kind, want) in cases {
            let got = path(&fake(&vars, None), kind);
            assert_eq!(got.as_deref(), want, "{vars:?} {kind:?}");
        }
    }

    #[test]
    fn personal_and_non_utf8() {
        let f = fake(&[("HOME", b"/h\xff")], None);
        assert_eq!(special_path(&f, Kind::Personal).unwrap(), b"/h\xff/");
        assert_eq!(special_path(&fake(&[], None), Kind::Personal), None);
    }

    #[test]
    fn user_dirs() {
        let h: Vars = &[("HOME", b"/h")];
        let cases: [(&[u8], &str); 9] = [
            (b"XDG_PICTURES_DIR=\"$HOME/Photos\"\n", "/h//Photos/"),
            (b"XDG_PICTURES_DIR=\"/abs/p\"\r\n", "/abs/p/"),
            (
                b"# XDG_PICTURES_DIR=\"/c\"\nXDG_PICTURES_DIR=\"/x\"\n",
                "/c/",
            ),
            (b"XDG_PICTURES_DIR=\"/nocr\"", "/nocr/"),
            (
                b"XDG_PICTURES_DIR=\"\"\nXDG_PICTURES_DIR=\"/second\"\n",
                "/second/",
            ),
            (
                b"XDG_PICTURES_DIR=\"/unterminated\nXDG_PICTURES_DIR=\"/ok\"\n",
                "/ok/",
            ),
            (b"XDG_PICTURES_DIR=/bare\n", "/h/Pictures/"),
            (b"XDG_DESKTOP_DIR=\"/d\"\n", "/h/Pictures/"),
            (b"XDG_PICTURES_DIR=\"$HOME/a$HOME\"\n", "/h//a$HOME/"),
        ];
        for (file, want) in cases {
            let got = path(&fake(h, Some(file)), Kind::Pictures);
            assert_eq!(
                got.as_deref(),
                Some(want),
                "{:?}",
                String::from_utf8_lossy(file)
            );
        }
        assert_eq!(
            special_path(
                &fake(h, Some(b"XDG_PICTURES_DIR=\"/nul\0x\"\n")),
                Kind::Pictures
            )
            .unwrap(),
            b"/nul/"
        );
        assert_eq!(
            path(&fake(h, None), Kind::Pictures).as_deref(),
            Some("/h/Pictures/")
        );
        assert_eq!(
            path(&fake(&[("HOME", b"")], None), Kind::Pictures).as_deref(),
            Some("Pictures/")
        );
        assert_eq!(path(&fake(&[], None), Kind::Pictures), None);
    }
}
