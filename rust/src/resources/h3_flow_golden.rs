//! Replays `rust/tests/golden/H3/flow.txt`: the manager-flow walk of `harness/flow_probe.cpp` (digest recorded from
//! the pre-port C++ flows) against the Rust flows driven through a Rust re-implementation of the probe's stub host.
//! Skipped per game where the Steam data is absent. `OHPL_H3_FLOW_DUMP=file` writes the transcript for diffing.

use super::h3_ops::{Create, Flow, Info, LOG_CREATE_AND_DELETE, Ops, Str, TEST_TIME, WStr, c, k};
use super::manager::{self, Manager, TEST_LOCK};
use super::manager_golden::{fnv, game_root, golden, relative_files};
use super::searcher::Searcher;
use crate::resources::ResMeta;
use crate::string;
use crate::string_ffi::{Sink, units};
use std::collections::BTreeMap;
use std::ffi::c_void;
use std::path::Path;
use std::sync::atomic::Ordering;

const BASIS: u64 = 1_469_598_103_934_665_603;
const NAMES: [&str; 13] = [
    "anim", "mesh", "ent", "script", "font", "sound", "sndent", "ps", "tex", "image", "mat",
    "shader", "video",
];
const SHUTDOWN: [i32; 13] = [0, 2, 1, 3, 6, 5, 4, 11, 7, 8, 9, 10, -1];
const BITMAP_EXTS: [&[u8]; 5] = [b"png", b"jpg", b"dds", b"tga", b"bmp"];
const MESH_EXTS: [&[u8]; 3] = [b"dae", b"msh", b"fbx"];
const SOUND_FORMATS: [&[u8]; 2] = [b"ogg", b"wav"];

fn fails(key: &[u8], salt: u64) -> bool {
    fnv(BASIS, key).wrapping_add(salt).checked_rem(7) == Some(0)
}

fn narrow(p: &[u32]) -> Vec<u8> {
    string::wide_to_utf8(p)
}

fn s(b: &[u8]) -> String {
    String::from_utf8_lossy(b).into_owned()
}

fn float6(x: f32) -> String {
    s(&string::float_to_string(x, 6, false))
}

struct Obj {
    kind: &'static str,
    name: Vec<u8>,
    path: Vec<u32>,
    meta: ResMeta,
    mem: i32,
    stream: bool,
    frame: usize,
    bmp_frame: usize,
}

struct Bf {
    handle: i32,
    cap: i32,
    imgs: i32,
    additions: i32,
    locked: bool,
    tf: usize,
}

struct Tf {
    handle: i32,
    pics: i32,
    custom: bool,
}

struct MatType {
    trans: bool,
    used: Vec<i64>,
}

struct Host {
    fs: *const Searcher,
    log: String,
    serial: u64,
    img: u64,
    time: u64,
    objs: BTreeMap<usize, Box<Obj>>,
    bfs: BTreeMap<usize, Box<Bf>>,
    tfs: BTreeMap<usize, Box<Tf>>,
    types: BTreeMap<Vec<u8>, Box<MatType>>,
    ps: Box<i32>,
    err: String,
    scratch: Vec<u8>,
    mgr: *mut Manager,
    flavor: usize,
}

fn host<'a>(ctx: *mut c_void) -> &'a mut Host {
    // SAFETY: the replay passes a live `Host` as ctx and uses it single-threaded.
    unsafe { &mut *ctx.cast::<Host>() }
}

fn addr<T>(b: &T) -> usize {
    std::ptr::from_ref(b).addr()
}

fn raw(a: usize) -> *mut c_void {
    std::ptr::with_exposed_provenance_mut(a)
}

impl Host {
    fn obj(&mut self, p: *mut c_void) -> &mut Obj {
        // SAFETY: only pointers handed out by `new_obj` and still alive reach here.
        unsafe { &mut *p.cast::<Obj>() }
    }

    fn new_obj(&mut self, kind: &'static str, name: &[u8], path: &[u32]) -> *mut c_void {
        let mut o = Box::new(Obj {
            kind,
            name: name.to_vec(),
            path: path.to_vec(),
            meta: ResMeta::new(0, self.time),
            mem: i32::try_from(fnv(BASIS, name).checked_rem(1000).unwrap_or(0)).unwrap_or(0),
            stream: false,
            frame: 0,
            bmp_frame: 0,
        });
        self.serial += 1;
        o.meta.handle = self.serial;
        let a = addr(&*o);
        self.objs.insert(a, o);
        raw(a)
    }

    fn make(&mut self, kind: &'static str, name: &[u8], path: &[u32], salt: u64) -> *mut c_void {
        let p = narrow(path);
        let fail = fails(&[name, &p].concat(), salt);
        self.log += &format!(
            "M:{kind} {} {}{}\n",
            s(name),
            s(&p),
            if fail { " fail" } else { "" }
        );
        if fail {
            std::ptr::null_mut()
        } else {
            self.new_obj(kind, name, path)
        }
    }

    fn bitmap(&mut self, path: &[u32]) -> Option<*mut i32> {
        let p = narrow(path);
        let fail = fails(&p, 1);
        self.log += &format!("B:{}{}\n", s(&p), if fail { " fail" } else { "" });
        (!fail).then(|| {
            Box::into_raw(Box::new(
                i32::try_from(fnv(BASIS, &p).checked_rem(1000).unwrap_or(0)).unwrap_or(0),
            ))
        })
    }

    fn destroy(&mut self, p: *mut c_void) {
        if let Some(o) = self.objs.remove(&p.addr()) {
            self.log += &format!("D:{} {}\n", o.kind, s(&o.name));
            if o.meta.log_destruction && LOG_CREATE_AND_DELETE.load(Ordering::Relaxed) {
                self.log += &format!("L:  Destroyed resource '{}'\n", s(&o.name));
            }
        }
    }

    fn mat_type(&mut self, name: &[u8]) -> *mut c_void {
        let h = fnv(BASIS, name);
        self.log += &format!("mat-type {}\n", s(name));
        if h.checked_rem(9) == Some(0) {
            return std::ptr::null_mut();
        }
        let t = self.types.entry(name.to_vec()).or_insert_with(|| {
            const ALL: [i64; 11] = [0, 1, 2, 3, 5, 6, 12, 13, 14, 21, 22];
            let mut used: Vec<i64> = ALL
                .iter()
                .enumerate()
                .filter(|(i, _)| (h >> i) & 1 == 1)
                .map(|(_, v)| *v)
                .collect();
            if used.is_empty() {
                used.push(0);
            }
            Box::new(MatType {
                trans: h.checked_rem(3) == Some(0),
                used,
            })
        });
        raw(addr(&**t))
    }

    fn add_bitmap(&mut self, bf: *mut c_void, bmp: *mut c_void, path: &[u32]) -> *mut c_void {
        // SAFETY: `bf`/`bmp` are live host objects.
        let (bmp_v, f) = unsafe { (*bmp.cast::<i32>(), &mut *bf.cast::<Bf>()) };
        let fit = (bmp_v + f.additions).checked_rem(3) != Some(0) && f.imgs < f.cap;
        f.additions += 1;
        self.log += &format!(
            "add-bitmap f={} {}{}\n",
            f.handle,
            bmp_v,
            if fit { " ok" } else { " nofit" }
        );
        if !fit {
            return std::ptr::null_mut();
        }
        let tf = f.tf;
        f.imgs += 1;
        self.img += 1;
        let name = format!("img{}", self.img).into_bytes();
        let p = self.new_obj("image", &name, path);
        let (tfp, bfp) = (tf, bf.addr());
        self.obj(p).frame = tfp;
        self.obj(p).bmp_frame = bfp;
        if let Some(t) = self.tfs.get_mut(&tfp) {
            t.pics += 1;
        }
        p
    }

    fn tex(&mut self, cr: &mut Create, n: usize) -> *mut c_void {
        let mut bmps = Vec::new();
        for i in 0..n {
            // SAFETY: `paths` holds `npaths` entries, valid for the call.
            let path: Vec<u32> = unsafe {
                if cr.kind == k::TEX_SIMPLE {
                    units(cr.path.p, cr.path.n).to_vec()
                } else {
                    let w = &units(cr.paths, cr.npaths)[i];
                    units(w.p, w.n).to_vec()
                }
            };
            match self.bitmap(&path) {
                Some(b) => bmps.push(b),
                None => {
                    for b in bmps {
                        // SAFETY: from `Box::into_raw`.
                        drop(unsafe { Box::from_raw(b) });
                    }
                    cr.status = 1;
                    cr.idx = i as i32;
                    return std::ptr::null_mut();
                }
            }
        }
        for b in bmps {
            // SAFETY: from `Box::into_raw`.
            drop(unsafe { Box::from_raw(b) });
        }
        let kind = match cr.kind {
            k::TEX_SIMPLE => "tex",
            k::TEX_ANIM => "texanim",
            _ => "texcube",
        };
        // SAFETY: the create struct strings are valid for the call.
        let (name, path) = unsafe {
            (
                units(cr.name.p, cr.name.n).to_vec(),
                units(cr.path.p, cr.path.n).to_vec(),
            )
        };
        let p = self.make(kind, &name, &path, 2);
        if p.is_null() {
            cr.status = 2;
        }
        p
    }
}

unsafe extern "C" fn h_log(ctx: *mut c_void, level: i32, p: *const u8, n: usize) {
    // SAFETY: the host contract of `Ops`.
    let msg = unsafe { units(p, n) };
    let prefix = match level {
        2 => "E:",
        1 => "W:",
        _ => "L:",
    };
    let msg = s(msg);
    // `vsnprintf` into a 2048 byte buffer.
    let cut: String = msg
        .chars()
        .scan(0usize, |n, ch| {
            *n += ch.len_utf8();
            (*n <= 2047).then_some(ch)
        })
        .collect();
    let h = host(ctx);
    h.log += prefix;
    h.log += &cut;
}

unsafe extern "C" fn h_now(_: *mut c_void) -> u64 {
    0
}

unsafe extern "C" fn h_file_path(
    ctx: *mut c_void,
    p: *const u8,
    n: usize,
    count: *mut i32,
    len: *mut usize,
) -> *const u32 {
    let h = host(ctx);
    // SAFETY: the searcher outlives the replay; `p` valid for `n`.
    unsafe {
        let fs = &*h.fs;
        match fs.get(units(p, n), !count.is_null()) {
            Some((id, cnt)) => {
                if let Some(c) = cnt.filter(|_| !count.is_null()) {
                    *count = c;
                }
                let path = fs.path(id);
                *len = path.len();
                path.as_ptr()
            }
            None => {
                *len = 0;
                std::ptr::null()
            }
        }
    }
}

unsafe extern "C" fn h_info(ctx: *mut c_void, p: *mut c_void, out: *mut Info) {
    let h = host(ctx);
    let o = h.obj(p);
    // SAFETY: `out` is a valid out pointer; the object outlives the call.
    unsafe {
        (*out).name = Str::of(&o.name);
        (*out).path = WStr::of(&o.path);
        (*out).meta = &raw mut o.meta;
    }
}

unsafe extern "C" fn h_destroy(ctx: *mut c_void, p: *mut c_void) {
    host(ctx).destroy(p);
}

unsafe extern "C" fn h_done(_: *mut c_void, _: *const u8, _: usize, _: u64) {}

fn cs(x: Str) -> Vec<u8> {
    // SAFETY: strings in a create struct are valid for the call.
    unsafe { units(x.p, x.n).to_vec() }
}

fn cw(x: WStr) -> Vec<u32> {
    // SAFETY: as above.
    unsafe { units(x.p, x.n).to_vec() }
}

unsafe extern "C" fn h_create(ctx: *mut c_void, cr: *mut Create) -> *mut c_void {
    let h = host(ctx);
    // SAFETY: `cr` is valid for the call.
    let cr = unsafe { &mut *cr };
    let name = cs(cr.name);
    let path = cw(cr.path);
    let file_of = |p: &[u32]| string::file_name(&narrow(p)).to_vec();
    let null = std::ptr::null_mut();
    match cr.kind {
        k::ANIMATION => h.make("anim", &file_of(&path), &path, 0),
        k::MESH => h.make("mesh", &file_of(&path), &path, 0),
        k::ENT_FILE => h.make("ent", &name, &path, 0),
        k::SCRIPT => h.make("script", &name, &path, 0),
        k::SOUND_DATA => {
            let p = h.make(
                if cr.i[0] != 0 { "stream" } else { "sample" },
                &name,
                &path,
                0,
            );
            if !p.is_null() {
                h.obj(p).stream = cr.i[0] != 0;
            }
            p
        }
        k::SOUND_ENTITY => h.make("sndent", &name, &path, 0),
        k::CUSTOM_RESOLVER => {
            h.log += &format!("resolve {}\n", s(&name));
            if name.len() % 2 == 1 {
                null
            } else {
                h.new_obj("custom", &name, &[])
            }
        }
        k::VIDEO => {
            let p = h.make("video", &file_of(&path), &path, 0);
            if !p.is_null() {
                h.obj(p).path = path;
            }
            p
        }
        k::FONT_TTF | k::FONT_BITMAP => {
            h.log += &format!("M:fontobj {}\n", s(&name));
            h.make(
                if cr.kind == k::FONT_TTF {
                    "ttf"
                } else {
                    "bmpfont"
                },
                &name,
                &path,
                1,
            )
        }
        k::FONT_UNKNOWN => {
            h.log += &format!("M:fontobj {}\n", s(&name));
            h.new_obj("fontobj", &name, &path)
        }
        k::PS_DATA_FILE | k::PS_DATA_ELEMENT => h.make("psdata", &name, &path, 0),
        k::PS_CREATE => {
            let dn = h.obj(cr.ptr).name.clone();
            h.log += &format!(
                "PS:{} {} {} {}\n",
                s(&dn),
                s(&name),
                s(&cs(cr.name2)),
                float6(cr.f[0])
            );
            std::ptr::from_mut(&mut *h.ps).cast()
        }
        k::TEX_SIMPLE => h.tex(cr, 1),
        k::TEX_ANIM | k::TEX_CUBE => h.tex(cr, cr.npaths),
        k::IMAGE => h.bitmap(&path).map_or(null, |b| b.cast()),
        k::MAT_NEW => {
            let blank = cr.i[0] != 0;
            h.log += &format!(
                "mat-new {} {}{}\n",
                s(&name),
                s(&narrow(&path)),
                if blank { " blank" } else { " typed" }
            );
            h.new_obj("mat", &name, &path)
        }
        k::MAT_TYPE => h.mat_type(&name),
        k::MAT_SET => {
            let mn = h.obj(cr.ptr).name.clone();
            h.log += &format!(
                "mat-set {} {} {} {} {}\n",
                s(&mn),
                cr.i[0],
                cr.i[1],
                cr.i[2],
                s(&name)
            );
            null
        }
        k::MAT_TEXTURE => {
            h.log += &format!(
                "mat-tex {} {} {} {} {}\n",
                s(&name),
                cr.i[0],
                cr.i[1],
                cr.i[2],
                cr.i[3]
            );
            if cr.i[2] < 0 || fails(&[name.as_slice()].concat(), 3) {
                return null;
            }
            h.new_obj("mattex", &name, &[])
        }
        k::MAT_TEX_SETUP => {
            let (tn, mn) = (h.obj(cr.ptr).name.clone(), h.obj(cr.ptr2).name.clone());
            h.log += &format!(
                "mat-texsetup {} {} {} {} {} {}\n",
                s(&tn),
                s(&mn),
                cr.i[0],
                cr.i[1],
                cr.i[2],
                float6(cr.f[0])
            );
            null
        }
        k::MAT_VAR => {
            if cr.i[0] != 0 {
                h.log += "mat-vars-clear\n";
            } else {
                h.log += &format!("mat-var {}={}\n", s(&name), s(&cs(cr.name2)));
            }
            null
        }
        k::MAT_VARS_APPLY => {
            let mn = h.obj(cr.ptr2).name.clone();
            h.log += &format!("mat-vars-apply {}\n", s(&mn));
            null
        }
        k::MAT_ADD_UV => {
            let mn = h.obj(cr.ptr).name.clone();
            h.log += &format!(
                "mat-uv {} {} {} {} {}\n",
                s(&mn),
                cr.i[0],
                cr.i[1],
                float6(cr.f[0]),
                float6(cr.f[1])
            );
            null
        }
        k::IMG_BITMAP_FRAME => {
            let (w, hh, handle) = (cr.i[0], cr.i[1], cr.i[2]);
            let tf = Box::new(Tf {
                handle,
                pics: 0,
                custom: false,
            });
            let tfa = addr(&*tf);
            h.tfs.insert(tfa, tf);
            let bf = Box::new(Bf {
                handle,
                cap: 2 + (w + hh).checked_rem(3).unwrap_or(0),
                imgs: 0,
                additions: 0,
                locked: false,
                tf: tfa,
            });
            h.log += &format!("new-frame {w}x{hh} h={handle} cap={}\n", bf.cap);
            let a = addr(&*bf);
            h.bfs.insert(a, bf);
            raw(a)
        }
        k::IMG_CUSTOM_FRAME => {
            let tf = Box::new(Tf {
                handle: cr.i[0],
                pics: 0,
                custom: true,
            });
            let a = addr(&*tf);
            h.tfs.insert(a, tf);
            raw(a)
        }
        k::IMG_ADD_BITMAP => h.add_bitmap(cr.ptr, cr.ptr2, &path),
        k::SHADER_PARSE => {
            h.log += &format!(
                "S:parse {} {} {}{} {}\n",
                s(&name),
                s(&cs(cr.name2)),
                cr.i[0],
                cr.i[1],
                s(&narrow(&path))
            );
            null
        }
        k::SHADER_TRANSPILE => {
            h.log += &format!("S:transpile {}\n", s(&name));
            cr.status = i32::from(fails(&name, 4));
            if cr.status != 0 {
                h.err = format!("transpile failed {}", s(&name));
            }
            null
        }
        k::SHADER_FILE => h.make("shfile", &name, &path, 5),
        k::SHADER_STRING => h.make("shstr", &name, &path, 6),
        _ => null,
    }
}

unsafe extern "C" fn h_call(ctx: *mut c_void, what: i32, p: *mut c_void, arg: i64) -> i64 {
    let h = host(ctx);
    // SAFETY: the pointer kinds below are the ones the flows pass for each code.
    unsafe {
        match what {
            c::DESTROY_VIRTUAL => {
                let o = ops(h);
                let mut f = Flow::from_raw(h.mgr, &o);
                match h.flavor {
                    5 => f.sound_destroy(p),
                    7 => f.particle_destroy(p),
                    8 => f.texture_destroy(p),
                    9 => f.image_destroy(p),
                    11 => f.gpu_destroy(p),
                    12 => f.video_destroy(p),
                    _ => f.destroy_standard(p),
                }
                0
            }
            c::IS_STREAM => i64::from(h.obj(p).stream),
            c::SOUND_SET_MGR => {
                let nm = s(&h.obj(p).name);
                h.log += &format!("set-sound-mgr {}\n", nm);
                0
            }
            c::MEM_SIZE => i64::from(h.obj(p).mem),
            c::UPDATE => {
                let f = f32::from_bits(arg as u32);
                let nm = s(&h.obj(p).name);
                h.log += &format!("update {} {}\n", nm, float6(f));
                0
            }
            c::PRELOAD_SOUNDS => {
                let nm = s(&h.obj(p).name);
                h.log += &format!("preload {}\n", nm);
                0
            }
            c::IMG_FRAME_TEX => h.obj(p).frame as i64,
            c::IMG_FRAME_BMP => h.obj(p).bmp_frame as i64,
            c::FRAME_DEC_PIC => {
                (*p.cast::<Tf>()).pics -= 1;
                0
            }
            c::BMPFRAME_DEC_PIC => {
                (*p.cast::<Bf>()).imgs -= 1;
                0
            }
            c::FRAME_IS_EMPTY => i64::from((*p.cast::<Tf>()).pics <= 0),
            c::FRAME_IS_CUSTOM => i64::from((*p.cast::<Tf>()).custom),
            c::BMPFRAME_HANDLE => i64::from((*p.cast::<Bf>()).handle),
            c::BMPFRAME_TEX => (*p.cast::<Bf>()).tf as i64,
            c::BMPFRAME_IS_FULL => i64::from((*p.cast::<Bf>()).imgs >= (*p.cast::<Bf>()).cap),
            c::BMPFRAME_IS_LOCKED => i64::from((*p.cast::<Bf>()).locked),
            c::BMPFRAME_ADDITIONS => i64::from((*p.cast::<Bf>()).additions),
            c::BMPFRAME_REORGANIZE => {
                let f = &mut *p.cast::<Bf>();
                h.log += &format!("reorganize {}\n", f.handle);
                f.additions = 0;
                0
            }
            c::BMPFRAME_FLUSH => i64::from((*p.cast::<Bf>()).handle.checked_rem(2).unwrap_or(0)),
            c::BMPFRAME_SET_LOCKED => {
                (*p.cast::<Bf>()).locked = arg != 0;
                0
            }
            c::BMPFRAME_DELETE => {
                h.log += &format!("del-bf {}\n", (*p.cast::<Bf>()).handle);
                h.bfs.remove(&p.addr());
                0
            }
            c::FRAME_DELETE => {
                h.log += &format!("del-tf {}\n", (*p.cast::<Tf>()).handle);
                h.tfs.remove(&p.addr());
                0
            }
            c::FRAME_HANDLE => i64::from((*p.cast::<Tf>()).handle),
            c::BMP_WIDTH => i64::from(*p.cast::<i32>() % 97 * 7 + 1),
            c::BMP_HEIGHT => i64::from(*p.cast::<i32>() % 89 * 11 + 1),
            c::BMP_DELETE => {
                drop(Box::from_raw(p.cast::<i32>()));
                0
            }
            c::SHADER_POST => {
                let nm = s(&h.obj(p).name);
                h.log += &format!("S:post {} {arg}\n", nm);
                0
            }
            c::MAT_TYPE_Q => {
                let t = &*p.cast::<MatType>();
                match arg {
                    0 => i64::from(t.trans),
                    1 => t.used.len() as i64,
                    n => t.used.get((n - 2) as usize).copied().unwrap_or(0),
                }
            }
            c::MAT_COMPILE => {
                let nm = s(&h.obj(p).name);
                h.log += &format!("mat-compile {}\n", nm);
                0
            }
            _ => 0,
        }
    }
}

unsafe extern "C" fn h_text(
    ctx: *mut c_void,
    what: i32,
    p: *mut c_void,
    path: WStr,
    sink: Sink<u8>,
    sctx: *mut c_void,
) {
    let h = host(ctx);
    let mut out: Vec<u8> = Vec::new();
    match what {
        c::SHADER_ERROR => out = h.err.clone().into_bytes(),
        c::PHYSICS_NAME_OF => {
            out = if p.is_null() {
                [b"phys-file-".as_slice(), &narrow(&cw(path))].concat()
            } else {
                [b"phys-of-".as_slice(), &h.obj(p).name].concat()
            };
        }
        c::FILE_BYTES => {
            let p8 = s(&narrow(&cw(path)));
            out = match std::fs::read(&p8) {
                Ok(data) => {
                    let mut v = vec![2];
                    v.extend(data);
                    v
                }
                Err(_) => vec![0],
            };
        }
        _ => {}
    }
    h.scratch = out;
    // SAFETY: the sink accepts the buffer for the call.
    unsafe { sink(sctx, h.scratch.as_ptr(), h.scratch.len()) };
}

fn ops(h: &mut Host) -> Ops {
    Ops {
        ctx: std::ptr::from_mut(h).cast(),
        log: h_log,
        now: h_now,
        file_path: h_file_path,
        info: h_info,
        destroy: h_destroy,
        load_done: h_done,
        create: h_create,
        call: h_call,
        text: h_text,
    }
}

fn normalize_ms(t: &str) -> String {
    let mut out = String::new();
    let mut rest = t;
    while let Some(i) = rest.find(" in ") {
        let (head, tail) = rest.split_at(i);
        out += head;
        let body = tail.get(4..).unwrap_or("");
        let digits = body.strip_prefix('-').unwrap_or(body);
        let n = digits.bytes().take_while(u8::is_ascii_digit).count();
        if n > 0 && digits.get(n..).is_some_and(|r| r.starts_with(" ms")) {
            out += " in N ms";
            let used = 4 + (body.len() - digits.len()) + n + 3;
            rest = tail.get(used..).unwrap_or("");
        } else {
            out += " in ";
            rest = body;
        }
    }
    out + rest
}

fn normalize_hex(t: &str) -> String {
    let b = t.as_bytes();
    let mut out = String::new();
    let mut i = 0;
    while i < b.len() {
        if b.get(i) == Some(&b'0')
            && b.get(i + 1) == Some(&b'x')
            && b.get(i + 2)
                .is_some_and(|c| c.is_ascii_hexdigit() && !c.is_ascii_uppercase())
        {
            let mut j = i + 2;
            while b
                .get(j)
                .is_some_and(|c| c.is_ascii_hexdigit() && !c.is_ascii_uppercase())
            {
                j += 1;
            }
            out += "0xP";
            i = j;
        } else {
            out.push(char::from(b.get(i).copied().unwrap_or(b' ')));
            i += 1;
        }
    }
    out
}

fn sort_updates(log: &str) -> String {
    let mut upd = Vec::new();
    let mut rest = String::new();
    let mut first = None;
    for l in log.split_inclusive('\n') {
        if l.starts_with("update ") {
            upd.push(l.to_string());
            first.get_or_insert(rest.len());
        } else {
            rest += l;
        }
    }
    upd.sort();
    match first {
        None => rest,
        Some(at) => format!(
            "{}{}{}",
            rest.get(..at).unwrap_or(""),
            upd.concat(),
            rest.get(at..).unwrap_or("")
        ),
    }
}

struct Mgr {
    m: Manager,
    h: Box<Host>,
    flavor: usize,
}

impl Mgr {
    fn flow<'a>(m: &'a mut Manager, o: &'a Ops) -> Flow<'a> {
        Flow::new(m, o)
    }

    fn id(&mut self, p: *mut c_void) -> String {
        if p.is_null() {
            "null".into()
        } else {
            let o = self.h.obj(p);
            format!("{}:{}", o.meta.handle, s(&o.name))
        }
    }

    fn live(&mut self) -> Vec<*mut c_void> {
        self.m.resources()
    }

    fn dump(&mut self) -> String {
        let live = self.live();
        let mut text = Vec::new();
        for &p in &live {
            let o = self.h.obj(p);
            text.extend(
                unroot(String::from_utf8_lossy(&narrow(&o.path)).into_owned()).into_bytes(),
            );
            text.extend(format!(":{}:{};", o.meta.handle, o.meta.user_count).bytes());
        }
        format!("{} {:016x}", live.len(), fnv(BASIS, &text))
    }

    fn destroy(&mut self, p: *mut c_void) {
        let o = ops(&mut self.h);
        let mut f = Self::flow(&mut self.m, &o);
        match self.flavor {
            5 => f.sound_destroy(p),
            7 => f.particle_destroy(p),
            8 => f.texture_destroy(p),
            9 => f.image_destroy(p),
            11 => f.gpu_destroy(p),
            12 => f.video_destroy(p),
            _ => f.destroy_standard(p),
        }
    }
}

struct Rng(u64);

impl Rng {
    fn next(&mut self, n: u64) -> u64 {
        self.0 ^= self.0 << 13;
        self.0 ^= self.0 >> 7;
        self.0 ^= self.0 << 17;
        self.0.checked_rem(n).unwrap_or(0)
    }
}

fn extension(rel: &[u8]) -> (Vec<u8>, Vec<u8>, Vec<u8>, Vec<u8>) {
    let slash = rel.iter().rposition(|&b| b == b'/');
    let base = slash.map_or(rel, |i| rel.get(i + 1..).unwrap_or_default());
    let dir = slash.map_or(&[][..], |i| rel.get(..i).unwrap_or_default());
    let parent = dir.rsplit(|&b| b == b'/').next().unwrap_or_default();
    let dot = base.iter().rposition(|&b| b == b'.');
    let (stem, ext) = match dot {
        Some(i) if i > 0 => (
            base.get(..i).unwrap_or_default(),
            base.get(i + 1..).unwrap_or_default(),
        ),
        _ => (base, &[][..]),
    };
    (base.to_vec(), stem.to_vec(), ext.to_vec(), parent.to_vec())
}

type Names = Vec<Vec<u8>>;

fn corpus(root: &Path, extra: &Path) -> (Names, Names, Names) {
    const KINDS: [&str; 21] = [
        "png", "jpg", "dds", "tga", "bmp", "dae", "msh", "fbx", "ent", "hps", "chps", "ttf", "fnt",
        "snt", "mat", "ogg", "wav", "ps", "ogv", "glsl", "hpsl",
    ];
    let mut rels = Vec::new();
    relative_files(root, root, &mut rels);
    rels.sort();
    let stride = (rels.len() / 4000).max(1);
    let (mut names, mut snames, mut mnames): (Names, Names, Names) =
        (Vec::new(), Vec::new(), Vec::new());
    for rel in rels.iter().step_by(stride) {
        let (base, stem, ext, parent) = extension(rel);
        if !KINDS.iter().any(|k| k.as_bytes() == ext.as_slice()) {
            continue;
        }
        names.push(base.clone());
        if ext == b"hpsl" {
            names.push([stem.as_slice(), b".glsl"].concat());
            names.push([parent.as_slice(), b"/", stem.as_slice(), b".glsl"].concat());
            snames.push([stem.as_slice(), b".glsl"].concat());
            snames.push(base.clone());
        }
        if ext == b"glsl" {
            snames.push(base.clone());
        }
        names.push(stem.clone());
        if !parent.is_empty() {
            names.push([parent.as_slice(), b"/", base.as_slice()].concat());
        }
        names.push(base.to_ascii_uppercase());
    }
    if let Ok(rd) = std::fs::read_dir(extra) {
        mnames = rd
            .flatten()
            .map(|e| e.file_name().as_encoded_bytes().to_vec())
            .collect();
        mnames.sort();
        for m in &mnames {
            for _ in 0..6 {
                names.push(m.clone());
            }
        }
    }
    names.push(b"does_not_exist.png".to_vec());
    names.push(b"anim01.png".to_vec());
    (names, snames, mnames)
}

thread_local!(static EXTRA: std::cell::RefCell<String> = const { std::cell::RefCell::new(String::new()) });

// The matcases dir lives in the checkout; hash it as a fixed name so goldens don't depend on where the repo is.
fn unroot(s: String) -> String {
    EXTRA.with(|e| {
        let e = e.borrow();
        if e.is_empty() {
            s
        } else {
            s.replace(e.as_str(), "<matcases>")
        }
    })
}

pub(super) fn run(root: &Path, extra: &Path, ops_n: i64, dump: Option<&Path>) -> u64 {
    manager::reset_tabs();
    EXTRA.with(|e| *e.borrow_mut() = extra.to_string_lossy().into_owned());
    let (names, snames, mnames) = corpus(root, extra);
    let mut fs = Searcher::default();
    fs.add_directory(
        &string::utf8_to_wide(root.as_os_str().as_encoded_bytes()),
        b"*.*",
        true,
    );
    if extra.is_dir() {
        fs.add_directory(
            &string::utf8_to_wide(extra.as_os_str().as_encoded_bytes()),
            b"*.*",
            true,
        );
    }
    let mut mgrs: Vec<Mgr> = (0..13)
        .map(|f| {
            let mut m = Manager::default();
            m.frames.size = (512, 512);
            Mgr {
                m,
                h: Box::new(Host {
                    fs: &raw const fs,
                    log: String::new(),
                    serial: 0,
                    img: 0,
                    time: 0,
                    objs: BTreeMap::new(),
                    bfs: BTreeMap::new(),
                    tfs: BTreeMap::new(),
                    types: BTreeMap::new(),
                    ps: Box::new(0),
                    err: String::new(),
                    scratch: Vec::new(),
                    mgr: std::ptr::null_mut(),
                    flavor: f,
                }),
                flavor: f,
            }
        })
        .collect();
    for mg in &mut mgrs {
        mg.h.mgr = &raw mut mg.m;
    }
    let mut rng = Rng(88_172_645_463_325_252);
    let mut transcript = String::new();
    let mut total = BASIS;
    let dummy_vars: *mut c_void = Box::into_raw(Box::new(0i32)).cast();
    let mut img_seed = 0i32;
    let tick = |op: i64| 1_700_000_000 + u64::try_from(op).unwrap_or(0) / 5;
    for op in 0..ops_n {
        TEST_TIME.with(|t| t.set(Some(tick(op))));
        let f = rng.next(13) as usize;
        let mg = &mut mgrs[f];
        mg.h.time = tick(op);
        mg.h.log.clear();
        let mut line = format!("{} ", NAMES[f]);
        let k = rng.next(100);
        let live = mg.live();
        let pick = |rng: &mut Rng| {
            names
                .get(rng.next(names.len() as u64) as usize)
                .cloned()
                .unwrap_or_default()
        };
        let o = ops(&mut mg.h);
        if k < 50 {
            let mut n = pick(&mut rng);
            let ns = s(&n);
            let mut fl = Flow::new(&mut mg.m, &o);
            let mut want_id = true;
            let r: *mut c_void;
            match f {
                0 => {
                    r = fl.model(false, &n, &MESH_EXTS, 0);
                    line += &format!("create {ns} -> ");
                }
                1 => {
                    let flag = rng.next(4) as i32;
                    r = fl.model(true, &n, &MESH_EXTS, flag);
                    line += &format!("create {ns} -> ");
                }
                2 => {
                    r = fl.ent_file(&n);
                    line += &format!("create {ns} -> ");
                }
                3 => {
                    r = fl.script(&n, std::ptr::null_mut());
                    line += &format!("create {ns} -> ");
                }
                4 => {
                    r = fl.font(&n, 12, 32, 255);
                    line += &format!("create {ns} -> ");
                }
                6 => {
                    if rng.next(4) == 0 {
                        fl.sound_entity_preload(&n, true);
                        line += &format!("preload {ns}");
                        r = std::ptr::null_mut();
                        want_id = false;
                    } else {
                        r = fl.sound_entity(&n, true);
                        line += &format!("create {ns} -> ");
                    }
                }
                10 => {
                    if rng.next(12) == 0 {
                        let b = rng.next(2) != 0;
                        fl.m().mat_blank = b;
                        line += &format!("blank {} ", i32::from(b));
                    }
                    if !mnames.is_empty() && rng.next(2) != 0 {
                        n = mnames
                            .get(rng.next(mnames.len() as u64) as usize)
                            .cloned()
                            .unwrap_or_default();
                    }
                    r = fl.material(&n);
                    line += &format!("create {} -> ", s(&n));
                }
                5 => {
                    let stream = rng.next(2) != 0;
                    let looped = rng.next(2) != 0;
                    r = fl.sound(&n, stream, looped, &SOUND_FORMATS);
                    line += &format!("create {ns} -> ");
                }
                12 => {
                    r = fl.video(&n);
                    line += &format!("create {ns} -> ");
                }
                7 => {
                    let v = rng.next(3);
                    if v == 0 {
                        fl.particle_preload(&n);
                        line += &format!("preload {ns}");
                        r = std::ptr::null_mut();
                        want_id = false;
                    } else {
                        let name = format!("ps{}", rng.next(5)).into_bytes();
                        let got = if v == 2 {
                            fl.particle_by_element(&name, &n, std::ptr::null_mut(), [1.0, 2.0, 3.0])
                        } else {
                            fl.particle_by_type(&name, &n, [1.0, 2.0, 3.0])
                        };
                        line += &format!(
                            "{}{ns} -> {}",
                            if v == 1 { "type " } else { "data " },
                            i32::from(!got.is_null())
                        );
                        r = std::ptr::null_mut();
                        want_id = false;
                    }
                }
                8 => {
                    let v = rng.next(4);
                    if v == 0 {
                        let mip = rng.next(2) != 0;
                        r = fl.texture_anim(&n, mip, 1, 0, 0, &BITMAP_EXTS);
                        line += &format!("anim {ns} -> ");
                    } else if v == 1 {
                        let mip = rng.next(2) != 0;
                        r = fl.texture_cube(&n, mip, 0, 0, 3, &BITMAP_EXTS);
                        line += &format!("cube {ns} -> ");
                    } else {
                        let mip = rng.next(2) != 0;
                        r = fl.texture_simple(&n, mip, 0, 1, 0, &BITMAP_EXTS);
                        line += &format!("simple {ns} -> ");
                    }
                }
                9 => {
                    let v = rng.next(8);
                    if v == 0 {
                        img_seed += 37;
                        let bmp = Box::into_raw(Box::new(img_seed));
                        let handle = if rng.next(3) != 0 {
                            -1
                        } else {
                            rng.next(4) as i32
                        };
                        r = fl.image_from_bitmap(bmp.cast(), handle);
                        // SAFETY: from `Box::into_raw`.
                        drop(unsafe { Box::from_raw(bmp) });
                        line += "frombmp -> ";
                    } else if v == 1 {
                        let w = 64 + rng.next(300) as i32;
                        let hh = 64 + rng.next(300) as i32;
                        line += &format!("createframe {}", fl.image_create_frame(w, hh));
                        r = std::ptr::null_mut();
                        want_id = false;
                    } else if v == 2 {
                        let hd = rng.next(5) as i32;
                        let lock = rng.next(2) != 0;
                        fl.image_set_frame_locked(hd, lock);
                        line += &format!("lock {hd}");
                        r = std::ptr::null_mut();
                        want_id = false;
                    } else if v == 3 {
                        line += &format!("flush {}", fl.image_flush_all());
                        r = std::ptr::null_mut();
                        want_id = false;
                    } else {
                        let handle = if rng.next(4) != 0 {
                            -1
                        } else {
                            rng.next(4) as i32
                        };
                        r = fl.image_create_in_frame(&n, handle, &BITMAP_EXTS);
                        line += &format!("create {ns} -> ");
                    }
                }
                _ => {
                    let vars = rng.next(2) != 0;
                    if !snames.is_empty() && rng.next(4) != 0 {
                        n = snames
                            .get(rng.next(snames.len() as u64) as usize)
                            .cloned()
                            .unwrap_or_default();
                    }
                    let ty = rng.next(2) as i32;
                    let transpile = rng.next(3) != 0;
                    let vp = if vars {
                        dummy_vars
                    } else {
                        std::ptr::null_mut()
                    };
                    r = fl.gpu_shader(&n, ty, vp, transpile, i32::from(ty == 1));
                    line += &format!("{}{} -> ", if vars { "vars " } else { "plain " }, s(&n));
                }
            }
            if want_id {
                let id = mg.id(r);
                line += &id;
            }
            if f == 8 {
                line += &format!(" mem={}", mg.m.mem_usage);
            }
        } else if k < 70 && !live.is_empty() {
            let p = live[rng.next(live.len() as u64) as usize];
            let users = {
                let m = mg.h.obj(p);
                if m.meta.user_count > 0 {
                    m.meta.dec_user();
                }
                m.meta.user_count
            };
            line += &format!("release {} users={users}", mg.id(p));
            if rng.next(2) != 0 {
                let id = mg.id(p);
                mg.destroy(p);
                line += &format!(" destroy {id} live={}", mg.live().len());
            }
        } else if k < 76 {
            let keep = rng.next(12) as i64;
            if f != 8 {
                Flow::new(&mut mg.m, &o).destroy_unused(keep);
            }
            line += &format!("unused {keep} {}", mg.dump());
        } else if k < 84 {
            let bits = i64::from(((0.016f32 * (op % 5) as f32).to_bits()) as i32);
            let mut fl = Flow::new(&mut mg.m, &o);
            if f == 12 {
                fl.video_update(bits);
            } else if f == 8 {
                fl.texture_update(bits);
            }
            line += "update";
        } else if k < 90 && !live.is_empty() {
            let p = live[rng.next(live.len() as u64) as usize];
            mg.h.obj(p).meta.user_count = 0;
            let id = mg.id(p);
            mg.destroy(p);
            line += &format!("force {id} live={}", mg.live().len());
        } else {
            line += &mg.dump();
        }
        let log = sort_updates(&mg.h.log);
        let out = normalize_hex(&normalize_ms(&log));
        line += "\n";
        line += &out;
        let line = unroot(line);
        total = fnv(total, line.as_bytes());
        if dump.is_some() {
            transcript += &line;
        }
    }
    for mg in &mut mgrs {
        mg.h.log.clear();
        let o = ops(&mut mg.h);
        let mut fl = Flow::new(&mut mg.m, &o);
        if mg.flavor == 5 {
            fl.sound_destroy_all();
        } else if mg.flavor != 7 {
            fl.destroy_all();
        }
        if let Some(code) = SHUTDOWN.get(mg.flavor).copied().filter(|c| *c >= 0) {
            fl.shutdown(code);
        }
        let line = format!(
            "final {} {}\n{}",
            NAMES[mg.flavor],
            mg.m.len(),
            normalize_hex(&mg.h.log)
        );
        let line = unroot(line);
        total = fnv(total, line.as_bytes());
        if dump.is_some() {
            transcript += &line;
        }
    }
    if let Some(p) = dump {
        let _ = std::fs::write(p, transcript);
    }
    TEST_TIME.with(|t| t.set(None));
    total
}

#[test]
fn flows_match_cpp() {
    let _g = TEST_LOCK
        .lock()
        .unwrap_or_else(std::sync::PoisonError::into_inner);
    manager::reset_tabs();
    LOG_CREATE_AND_DELETE.store(true, Ordering::Relaxed);
    let extra = Path::new(env!("CARGO_MANIFEST_DIR")).join("tests/golden/H3/harness/matcases");
    let dump = std::env::var_os("OHPL_H3_FLOW_DUMP").map(std::path::PathBuf::from);
    let mut checked = 0;
    for line in golden("flow.txt").lines() {
        let mut f = line.split('\t');
        let (Some(game), Some(ops_n), Some(digest)) = (f.next(), f.next(), f.next()) else {
            continue;
        };
        let Some(root) = game_root(game) else {
            continue;
        };
        let got = run(
            &root,
            &extra,
            ops_n.parse().unwrap_or(0),
            dump.as_ref().map(|d| d.with_extension(game)).as_deref(),
        );
        if std::env::var_os("OHPL_H3_FLOW_NOASSERT").is_none() {
            assert_eq!(format!("{got:016x}"), digest, "{game}");
        }
        checked += 1;
    }
    LOG_CREATE_AND_DELETE.store(false, Ordering::Relaxed);
    let _ = checked;
}
