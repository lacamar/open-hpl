//! Replays `tests/golden/H5` (recorded from the C++ `cFrameBitmap` / `cProgramComboManager`).

use std::collections::BTreeMap;
use std::ffi::c_void;
use std::fmt::Write;
use std::fs;
use std::path::PathBuf;

use super::atlas::{Atlas, Pix};
use super::bitmap::{self, Surface};
use super::combo::{FRAGMENT_BIT, VERTEX_BIT};
use super::ffi_h5::*;
use super::{formats, uv};
use crate::math::Rect2l;
use crate::types::{Vector2l, Vector3l};

fn golden(name: &str) -> String {
    let p = PathBuf::from(env!("CARGO_MANIFEST_DIR"))
        .join("tests/golden/H5")
        .join(name);
    fs::read_to_string(&p).unwrap_or_else(|e| panic!("{}: {e}", p.display()))
}

fn names(prefix: &str) -> Vec<String> {
    let dir = PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("tests/golden/H5");
    let mut v: Vec<String> = fs::read_dir(dir)
        .unwrap()
        .filter_map(|e| e.ok()?.file_name().into_string().ok())
        .filter(|n| n.starts_with(prefix) && n.ends_with(".txt"))
        .collect();
    v.sort();
    v
}

fn fnv(b: &[u8]) -> u64 {
    b.iter().fold(1469598103934665603u64, |h, &x| {
        (h ^ u64::from(x)).wrapping_mul(1099511628211)
    })
}

fn surf(w: i32, h: i32) -> Surface {
    Surface {
        size: Vector3l { x: w, y: h, z: 1 },
        format: formats::RGBA,
        bytes_per_pixel: formats::channels(formats::RGBA),
    }
}

struct Sub {
    has_img: bool,
    need_uv: bool,
    updates: i32,
    tex: [(f32, f32); 4],
}

struct Replay {
    atlas: Atlas,
    pix: Vec<u8>,
    s: Surface,
    src: Vector2l,
    images: Vec<(Rect2l, Option<usize>)>,
    subs: Vec<Sub>,
    tex_pic: i32,
    out: String,
    uploads: Vec<String>,
}

fn quad(r: &Rect2l, src: &Vector2l) -> [(f32, f32); 4] {
    uv::subimage_uvs(r, src).map(|v| (v.x, v.y))
}

impl Replay {
    fn new(w: i32, h: i32) -> Self {
        let s = surf(w, h);
        let mut pix = vec![0u8; (w * h * 4) as usize];
        bitmap::clear(
            &mut pix,
            i64::from(w * h),
            4,
            formats::RGBA,
            [255, 0, 255, 0],
        );
        Replay {
            atlas: Atlas::new(w, h, 3),
            pix,
            s,
            src: Vector2l { x: w, y: h },
            images: Vec::new(),
            subs: Vec::new(),
            tex_pic: 0,
            out: format!("frame {w} {h}\n"),
            uploads: Vec::new(),
        }
    }

    fn dst(&mut self) -> Pix<'_> {
        Pix {
            surface: Surface {
                size: self.s.size,
                format: self.s.format,
                bytes_per_pixel: self.s.bytes_per_pixel,
            },
            data: Some(&mut self.pix),
        }
    }

    fn add(&mut self, n: i32, w: i32, h: i32) {
        let src: Vec<u8> = (0..w * h * 4)
            .map(|k| {
                n.wrapping_mul(131)
                    .wrapping_add(k.wrapping_mul(7))
                    .wrapping_add((k >> 8).wrapping_mul(13)) as u8
            })
            .collect();
        let mut a = std::mem::replace(&mut self.atlas, Atlas::new(1, 1, 0));
        let r = a.add(&mut self.dst(), &surf(w, h), Some(&src), false);
        self.atlas = a;
        let _ = write!(
            self.out,
            "-o add {n} {w} {h}\nfound={}",
            u8::from(r.is_some())
        );
        if let Some(a) = r {
            let id = self.subs.len();
            let _ = write!(
                self.out,
                " sub={id} rect={},{},{},{}",
                a.rect.x, a.rect.y, a.rect.w, a.rect.h
            );
            self.images.push((a.rect, Some(id)));
            self.subs.push(Sub {
                has_img: true,
                need_uv: false,
                updates: 0,
                tex: quad(&a.rect, &self.src),
            });
            self.tex_pic = self.atlas.pic_count;
        }
        let a = &self.atlas;
        let _ = writeln!(
            self.out,
            " full={} upd={} adds={} pic={} texpic={}",
            u8::from(a.full),
            u8::from(a.updated),
            a.additions,
            a.pic_count,
            self.tex_pic
        );
    }

    fn del(&mut self, i: usize) {
        let _ = writeln!(self.out, "-o del {i}");
        for im in &mut self.images {
            if im.1 == Some(i) {
                im.1 = None;
            }
        }
        self.atlas.need_reorg = true;
    }

    fn reorganize(&mut self) {
        let live: Vec<usize> = (0..self.images.len())
            .filter(|&i| self.images[i].1.is_some())
            .collect();
        let rects: Vec<Rect2l> = live.iter().map(|&i| self.images[i].0).collect();
        let owners: Vec<usize> = live.iter().filter_map(|&i| self.images[i].1).collect();
        let mut a = std::mem::replace(&mut self.atlas, Atlas::new(1, 1, 0));
        let r = a.reorganize(&mut self.dst(), &rects);
        self.atlas = a;
        for s in &mut self.subs {
            s.has_img = false;
        }
        self.images = r
            .order
            .iter()
            .map(|&i| (r.rects[i], Some(owners[i])))
            .collect();
        for &(_, o) in &self.images {
            let o = o.unwrap();
            self.subs[o].has_img = true;
            self.subs[o].need_uv = true;
        }
        if !r.order.is_empty() {
            self.tex_pic = self.atlas.pic_count;
        }
    }

    fn before_draw(&mut self) {
        if std::mem::take(&mut self.atlas.need_reorg) {
            self.reorganize();
        }
        if std::mem::take(&mut self.atlas.updated) {
            self.uploads.push(format!("{:016x}", fnv(&self.pix)));
        }
    }

    fn flush(&mut self, last: bool) {
        self.out.push_str("-o flush\n");
        self.before_draw();
        let h = fnv(&self.pix);
        let a = &self.atlas;
        if last {
            let _ = writeln!(self.out, "final upd={} hash={h:016x}", u8::from(a.updated));
            return;
        }
        let _ = writeln!(
            self.out,
            "flushed upd={} adds={} pic={} texpic={} hash={h:016x}",
            u8::from(a.updated),
            a.additions,
            a.pic_count,
            self.tex_pic
        );
        for (r, s) in &self.images {
            let id = s.map_or(-1, |s| s as i64);
            let _ = writeln!(
                self.out,
                "  img sub={id} rect={},{},{},{}",
                r.x, r.y, r.w, r.h
            );
        }
    }

    fn subflush(&mut self, i: usize) {
        let _ = writeln!(self.out, "-o subflush {i}");
        self.before_draw();
        if self.subs[i].need_uv {
            if self.subs[i].has_img {
                let r = self.images.iter().find(|im| im.1 == Some(i)).unwrap().0;
                self.subs[i].updates += 1;
                self.subs[i].tex = quad(&r, &self.src);
            }
            self.subs[i].need_uv = false;
        }
        let s = &self.subs[i];
        let _ = write!(self.out, "sub {i} upd={}", s.updates);
        for (a, b) in s.tex {
            let _ = write!(self.out, " {:08x},{:08x}", a.to_bits(), b.to_bits());
        }
        self.out.push('\n');
    }
}

#[test]
fn atlas_replays_cpp() {
    let _g = super::tests::global();
    let files = names("atlas_");
    assert!(files.len() >= 25);
    for f in files {
        let want = golden(&f);
        let mut lines = want.lines().peekable();
        let first = lines.next().unwrap();
        let mut it = first.split(' ').skip(1).map(|v| v.parse::<i32>().unwrap());
        let mut r = Replay::new(it.next().unwrap(), it.next().unwrap());
        while let Some(l) = lines.next() {
            let p: Vec<&str> = l.split(' ').collect();
            let n = |i: usize| p[i].parse::<i32>().unwrap();
            match p[1] {
                "add" => r.add(n(2), n(3), n(4)),
                "del" => r.del(n(2) as usize),
                "lock" => {
                    r.atlas.locked = n(2) != 0;
                    let _ = write!(r.out, "-o lock {}\nlocked={}\n", n(2), n(2));
                }
                "flush" => {
                    let last = lines.peek().is_some_and(|l| l.starts_with("final"));
                    r.flush(last);
                }
                "subflush" => r.subflush(n(2) as usize),
                _ => continue,
            }
            // skip the recorded result lines of this op; compared as a whole below
            while lines.peek().is_some_and(|l| !l.starts_with("-o ")) {
                lines.next();
            }
        }
        assert_eq!(r.out, want, "{f}");
        let trace: Vec<String> = golden(&f.replace("atlas_", "trace_atlas_"))
            .lines()
            .map(|l| {
                l.split("h=")
                    .nth(1)
                    .unwrap()
                    .trim_end_matches("\")")
                    .to_string()
            })
            .map(|h| h.trim_end_matches('}').to_string())
            .collect();
        assert_eq!(r.uploads, trace, "{f} uploads");
    }
}

fn s(p: &[u8]) -> String {
    String::from_utf8_lossy(p).into_owned()
}

unsafe extern "C" fn collect_bytes(ctx: *mut c_void, p: *const u8, n: usize) {
    // SAFETY: test sink; `ctx` is a `Vec<u8>`.
    unsafe { (*ctx.cast::<Vec<u8>>()).extend_from_slice(std::slice::from_raw_parts(p, n)) }
}

unsafe extern "C" fn collect_var(
    ctx: *mut c_void,
    k: *const u8,
    kn: usize,
    v: *const u8,
    vn: usize,
) {
    // SAFETY: test sink; `ctx` is a map.
    unsafe {
        let m = &mut *ctx.cast::<BTreeMap<Vec<u8>, Vec<u8>>>();
        m.insert(
            std::slice::from_raw_parts(k, kn).to_vec(),
            std::slice::from_raw_parts(v, vn).to_vec(),
        );
    }
}

#[test]
fn combo_replays_cpp() {
    let files = names("combo_");
    assert!(files.len() >= 20);
    for f in files {
        let want = golden(&f);
        let mut lines = want.lines().peekable();
        let head: Vec<&str> = lines.next().unwrap().split(' ').collect();
        let modes: usize = head[2].parse().unwrap();
        let c = unsafe { ohpl_h5_combo_new(head[1].as_ptr(), head[1].len(), modes) };
        let mut creates = 0;
        while let Some(l) = lines.next() {
            let p: Vec<&str> = l.split(' ').collect();
            if p[0] != "-o" {
                continue;
            }
            let n = |i: usize| p[i].parse::<i32>().unwrap();
            match p[1] {
                "rename" => unsafe { ohpl_h5_combo_set_name(c, p[2].as_ptr(), p[2].len()) },
                "setup" => unsafe {
                    ohpl_h5_combo_setup_mode(
                        c,
                        n(2),
                        p[3].as_ptr(),
                        p[3].len(),
                        p[4].as_ptr(),
                        p[4].len(),
                        p[5].as_ptr(),
                        p[5].len(),
                    );
                    for t in &p[6..] {
                        let f: Vec<&str> = t.split(':').collect();
                        if f[0] == "f" {
                            ohpl_h5_combo_add_feature(
                                c,
                                n(2),
                                f[1].parse().unwrap(),
                                f[2].as_ptr(),
                                f[2].len(),
                            );
                        } else {
                            ohpl_h5_combo_add_default(
                                c,
                                n(2),
                                f[1].as_ptr(),
                                f[1].len(),
                                f[2].as_ptr(),
                                f[2].len(),
                            );
                        }
                    }
                },
                "name" => {
                    let mut out = Vec::new();
                    unsafe {
                        ohpl_h5_combo_program_name(
                            c,
                            n(2),
                            n(3),
                            collect_bytes,
                            (&raw mut out).cast(),
                        )
                    };
                    assert_eq!(s(&out), lines.next().unwrap(), "{f}");
                }
                "shader" => {
                    let ty: u32 = p[4].parse().unwrap();
                    let Some(create) = lines.peek().filter(|l| l.starts_with("create")).copied()
                    else {
                        continue;
                    };
                    lines.next();
                    creates += 1;
                    let mut file = Vec::new();
                    unsafe {
                        ohpl_h5_combo_shader_file(
                            c,
                            n(2),
                            ty,
                            collect_bytes,
                            (&raw mut file).cast(),
                        )
                    };
                    let mut vars: BTreeMap<Vec<u8>, Vec<u8>> = BTreeMap::new();
                    unsafe {
                        ohpl_h5_combo_shader_vars(
                            c,
                            n(2),
                            n(3),
                            ty,
                            collect_var,
                            (&raw mut vars).cast(),
                        )
                    };
                    let mut got = format!(
                        "create file={} type={} vars=",
                        s(&file),
                        u32::from(ty == FRAGMENT_BIT)
                    );
                    assert!(ty == VERTEX_BIT || ty == FRAGMENT_BIT);
                    for (k, v) in vars {
                        let _ = write!(got, "{}={};", s(&k), s(&v));
                    }
                    assert_eq!(got, create, "{f}");
                }
                _ => {}
            }
        }
        assert!(creates > 0, "{f}");
        unsafe { ohpl_h5_combo_free(c) };
    }
}
