//! Mock-host tests of the H3/H2 pass-2 flows (no C++ needed).

use super::h3_ops::{Create, Flow, Info, Ops, Str, WStr, c, k};
use super::h4_handler::{self, HandlerOps, h};
use super::h4_res::{self, Res, ResOps};
use super::lang::{Lang, LangOps};
use super::manager::{self, Manager, TEST_LOCK};
use crate::resources::{ExtList, LoaderTable, ResMeta};
use crate::string;
use crate::string_ffi::{Sink, units};
use std::collections::BTreeMap;
use std::ffi::c_void;
use std::sync::PoisonError;

struct Obj {
    name: Vec<u8>,
    path: Vec<u32>,
    meta: ResMeta,
}

#[derive(Default)]
struct Mock {
    files: BTreeMap<Vec<u8>, Vec<u32>>,
    log: Vec<String>,
    // Boxed: the host hands out stable pointers.
    #[allow(clippy::vec_box)]
    objs: Vec<Box<Obj>>,
    fail_create: bool,
    now: u64,
    destroyed: Vec<Vec<u8>>,
    calls: Vec<(i32, i64)>,
}

fn w(s: &str) -> Vec<u32> {
    s.chars().map(u32::from).collect()
}

unsafe fn mock<'a>(ctx: *mut c_void) -> &'a mut Mock {
    // SAFETY: tests pass a live `Mock` as ctx.
    unsafe { &mut *ctx.cast::<Mock>() }
}

unsafe extern "C" fn m_log(ctx: *mut c_void, level: i32, p: *const u8, n: usize) {
    // SAFETY: test host.
    unsafe {
        let m = mock(ctx);
        m.log
            .push(format!("{level}:{}", String::from_utf8_lossy(units(p, n))));
    }
}

unsafe extern "C" fn m_now(ctx: *mut c_void) -> u64 {
    // SAFETY: test host.
    unsafe {
        let m = mock(ctx);
        m.now += 3;
        m.now
    }
}

unsafe extern "C" fn m_file_path(
    ctx: *mut c_void,
    p: *const u8,
    n: usize,
    count: *mut i32,
    len: *mut usize,
) -> *const u32 {
    // SAFETY: test host.
    unsafe {
        let m = mock(ctx);
        let name = string::to_lower(units(p, n));
        if !count.is_null() {
            *count = 1;
        }
        match m.files.get(&name) {
            Some(path) => {
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

unsafe extern "C" fn m_info(ctx: *mut c_void, obj: *mut c_void, out: *mut Info) {
    // SAFETY: test host; `obj` is a boxed `Obj` owned by the mock.
    unsafe {
        let _ = mock(ctx);
        let o = &mut *obj.cast::<Obj>();
        (*out).name = Str::of(&o.name);
        (*out).path = WStr::of(&o.path);
        (*out).meta = &raw mut o.meta;
    }
}

unsafe extern "C" fn m_destroy(ctx: *mut c_void, obj: *mut c_void) {
    // SAFETY: test host.
    unsafe {
        let m = mock(ctx);
        m.destroyed.push((*obj.cast::<Obj>()).name.clone());
    }
}

unsafe extern "C" fn m_done(ctx: *mut c_void, p: *const u8, n: usize, t: u64) {
    // SAFETY: test host.
    unsafe {
        let m = mock(ctx);
        m.log
            .push(format!("done:{}:{t}", String::from_utf8_lossy(units(p, n))));
    }
}

unsafe extern "C" fn m_create(ctx: *mut c_void, cr: *mut Create) -> *mut c_void {
    // SAFETY: test host.
    unsafe {
        let m = mock(ctx);
        let cr = &mut *cr;
        if m.fail_create {
            cr.status = 2;
            return std::ptr::null_mut();
        }
        let name = if cr.name.n == 0 {
            string::file_name(units(cr.path.p, cr.path.n))
                .iter()
                .map(|&u| u as u8)
                .collect()
        } else {
            units(cr.name.p, cr.name.n).to_vec()
        };
        let mut o = Box::new(Obj {
            name,
            path: units(cr.path.p, cr.path.n).to_vec(),
            meta: ResMeta::new(0, 1),
        });
        let p: *mut Obj = &raw mut *o;
        m.objs.push(o);
        p.cast()
    }
}

unsafe extern "C" fn m_call(ctx: *mut c_void, what: i32, _obj: *mut c_void, i: i64) -> i64 {
    // SAFETY: test host.
    unsafe {
        mock(ctx).calls.push((what, i));
    }
    0
}

unsafe extern "C" fn m_text(
    _: *mut c_void,
    _: i32,
    _: *mut c_void,
    _: WStr,
    _: Sink<u8>,
    _: *mut c_void,
) {
}

fn ops(m: &mut Mock) -> Ops {
    Ops {
        ctx: std::ptr::from_mut(m).cast(),
        log: m_log,
        now: m_now,
        file_path: m_file_path,
        info: m_info,
        destroy: m_destroy,
        load_done: m_done,
        create: m_create,
        call: m_call,
        text: m_text,
    }
}

fn host() -> Mock {
    let mut m = Mock::default();
    m.files.insert(b"a.dae".to_vec(), w("/g/a.dae"));
    m.files.insert(b"hero.ent".to_vec(), w("/g/hero.ent"));
    m.files.insert(b"hello.hps".to_vec(), w("/g/hello.hps"));
    m.files.insert(b"tex01.png".to_vec(), w("/g/tex01.png"));
    m.files.insert(b"tex02.png".to_vec(), w("/g/tex02.png"));
    m
}

#[test]
fn model_flow_loads_once_and_counts_users() {
    let _g = TEST_LOCK.lock().unwrap_or_else(PoisonError::into_inner);
    manager::reset_tabs();
    let mut host = host();
    let o = ops(&mut host);
    let mut mgr = Manager::default();
    let exts: [&[u8]; 1] = [b"dae"];
    let first = Flow::new(&mut mgr, &o).model(true, b"a", &exts, 0);
    assert!(!first.is_null());
    let again = Flow::new(&mut mgr, &o).model(true, b"a", &exts, 0);
    assert_eq!(first, again);
    assert_eq!(manager::tab_count(), 0);
    // SAFETY: `first` is a boxed `Obj` owned by the mock.
    assert_eq!(unsafe { &*first.cast::<Obj>() }.meta.user_count, 2);
    assert_eq!(host.objs.len(), 1);
    let missing = Flow::new(&mut mgr, &o).model(true, b"nope", &exts, 0);
    assert!(missing.is_null());
    assert!(
        host.log
            .iter()
            .any(|l| l == "2:Couldn't find mesh file 'nope' in any supported format!\n")
    );
}

#[test]
fn create_failure_logs_and_closes_the_tab() {
    let _g = TEST_LOCK.lock().unwrap_or_else(PoisonError::into_inner);
    manager::reset_tabs();
    let mut host = host();
    host.fail_create = true;
    let o = ops(&mut host);
    let mut mgr = Manager::default();
    assert!(Flow::new(&mut mgr, &o).ent_file(b"hero").is_null());
    assert_eq!(manager::tab_count(), 0);
    assert!(
        host.log
            .iter()
            .any(|l| l.starts_with("2:Couldn't load entity file '/g/hero.ent'!"))
    );
}

#[test]
fn texture_anim_errors_leave_the_load_tab_open() {
    let _g = TEST_LOCK.lock().unwrap_or_else(PoisonError::into_inner);
    manager::reset_tabs();
    let mut host = host();
    let o = ops(&mut host);
    let mut mgr = Manager::default();
    let exts: [&[u8]; 1] = [b"png"];
    assert!(
        Flow::new(&mut mgr, &o)
            .texture_anim(b"plain.png", false, 1, 0, 0, &exts)
            .is_null()
    );
    assert_eq!(manager::tab_count(), 1);
    assert!(host.log.iter().any(|l| l.contains("must contain '01'!")));
    manager::reset_tabs();
}

#[test]
fn texture_anim_registers_and_updates_the_set() {
    let _g = TEST_LOCK.lock().unwrap_or_else(PoisonError::into_inner);
    manager::reset_tabs();
    let mut host = host();
    let o = ops(&mut host);
    let mut mgr = Manager::default();
    let exts: [&[u8]; 1] = [b"png"];
    let t = Flow::new(&mut mgr, &o).texture_anim(b"tex01.png", false, 1, 0, 0, &exts);
    assert!(!t.is_null());
    assert_eq!(mgr.anim.len(), 1);
    Flow::new(&mut mgr, &o).texture_update(7);
    assert!(host.calls.contains(&(c::UPDATE, 7)));
    Flow::new(&mut mgr, &o).texture_destroy(t);
    assert!(mgr.anim.is_empty());
    assert_eq!(host.destroyed, vec![b"tex.png".to_vec()]);
}

#[test]
fn destroy_all_empties_in_map_order_and_unused_keeps_the_best() {
    let _g = TEST_LOCK.lock().unwrap_or_else(PoisonError::into_inner);
    manager::reset_tabs();
    let mut host = host();
    let o = ops(&mut host);
    let mut mgr = Manager::default();
    let a = Flow::new(&mut mgr, &o).ent_file(b"hero");
    let s = Flow::new(&mut mgr, &o).script(b"hello", std::ptr::null_mut());
    assert!(!a.is_null() && !s.is_null());
    Flow::new(&mut mgr, &o).destroy_unused(5);
    assert_eq!(mgr.len(), 2);
    // SAFETY: boxed objects owned by the mock.
    unsafe { &mut *a.cast::<Obj>() }.meta.user_count = 0;
    Flow::new(&mut mgr, &o).destroy_unused(1);
    assert_eq!(mgr.len(), 1);
    assert_eq!(host.destroyed.len(), 1);
}

#[test]
fn image_frames_grow_and_unlock() {
    let _g = TEST_LOCK.lock().unwrap_or_else(PoisonError::into_inner);
    manager::reset_tabs();
    let mut host = host();
    let o = ops(&mut host);
    let mut mgr = Manager::default();
    let h = Flow::new(&mut mgr, &o).image_create_frame(64, 64);
    assert_eq!(h, 0);
    assert_eq!(mgr.frames.bitmap.len(), 1);
    assert_eq!(mgr.frames.handle, 1);
    Flow::new(&mut mgr, &o).image_set_frame_locked(0, true);
    assert!(host.calls.contains(&(c::BMPFRAME_SET_LOCKED, 1)));
    assert!(Flow::new(&mut mgr, &o).image_frame_texture(0).is_null());
}

fn lang_ops(m: &mut Mock) -> LangOps {
    unsafe extern "C" fn add_dir(ctx: *mut c_void, p: WStr, sub: bool) {
        // SAFETY: test host.
        unsafe {
            mock(ctx).log.push(format!(
                "dir:{}:{sub}",
                String::from_utf8_lossy(&string::wide_to_utf8(units(p.p, p.n)))
            ));
        }
    }
    unsafe extern "C" fn exists(_: *mut c_void, _: WStr) -> bool {
        false
    }
    unsafe extern "C" fn read(
        ctx: *mut c_void,
        _: WStr,
        sink: Sink<u8>,
        sc: *mut c_void,
        ok: *mut bool,
    ) -> bool {
        // SAFETY: test host.
        unsafe {
            let m = mock(ctx);
            let Some(data) = m.files.get(b"lang".as_slice()) else {
                return false;
            };
            let bytes: Vec<u8> = data.iter().map(|&u| u as u8).collect();
            *ok = true;
            sink(sc, bytes.as_ptr(), bytes.len());
            true
        }
    }
    unsafe extern "C" fn text_new(_: *mut c_void, p: *const i32, n: usize) -> *mut c_void {
        // SAFETY: test host.
        Box::into_raw(Box::new(unsafe { units(p, n) }.to_vec())).cast()
    }
    unsafe extern "C" fn text_free(ctx: *mut c_void, t: *mut c_void) {
        // SAFETY: test host.
        unsafe {
            mock(ctx).log.push("free".into());
            drop(Box::from_raw(t.cast::<Vec<i32>>()));
        }
    }
    LangOps {
        ctx: std::ptr::from_mut(m).cast(),
        log: m_log,
        add_dir,
        file_exists: exists,
        read_file: read,
        text_new,
        text_free,
    }
}

#[test]
fn language_merge_keeps_first_entry_and_set_replaces() {
    let mut host = host();
    host.files.insert(
        b"lang".to_vec(),
        w(r#"<LANGUAGE><RESOURCES><Directory Path="/lang/a" AddSubDirs="true"/></RESOURCES><CATEGORY Name="Menu"><Entry Name="Start">Go[br]now</Entry><Entry Name="start">dup</Entry></CATEGORY></LANGUAGE>"#),
    );
    let ops = lang_ops(&mut host);
    let mut lang = Lang::new(ops);
    assert!(lang.add_from_file(&w("x.lang"), true, &w("ALT/")));
    assert!(lang.has_entry(b"menu", b"START"));
    let t = lang.translate(b"Menu", b"Start");
    // SAFETY: the mock text is a boxed `Vec<i32>`.
    assert_eq!(
        unsafe { &*t.cast::<Vec<i32>>() },
        &[71, 111, 10, 110, 111, 119]
    );
    assert!(lang.translate(b"Nope", b"x").is_null());
    lang.set_entry(b"Menu", b"Start", &[65]);
    // SAFETY: as above.
    assert_eq!(
        unsafe { &*lang.translate(b"Menu", b"Start").cast::<Vec<i32>>() },
        &[65]
    );
    drop(lang);
    let log = host.log.join("|");
    assert!(log.contains("dir:ALT/lang/a:true|dir:lang/a:true"));
    assert!(log.contains("W:") || log.contains("1:Could not find language file category 'Nope'"));
    assert_eq!(host.log.iter().filter(|l| *l == "free").count(), 2);
}

fn res_ops(m: &mut Mock) -> ResOps {
    unsafe extern "C" fn create(ctx: *mut c_void, what: i32, _: WStr) -> *mut c_void {
        // SAFETY: test host.
        unsafe {
            mock(ctx).log.push(format!("create:{what}"));
        }
        std::ptr::without_provenance_mut(100 + usize::try_from(what).unwrap_or(0))
    }
    unsafe extern "C" fn destroy(ctx: *mut c_void, what: i32, _: *mut c_void) {
        // SAFETY: test host.
        unsafe {
            mock(ctx).log.push(format!("destroy:{what}"));
        }
    }
    unsafe extern "C" fn call(ctx: *mut c_void, what: i32, obj: *mut c_void, _: i64) -> i64 {
        // SAFETY: test host.
        unsafe {
            mock(ctx).log.push(format!("call:{what}:{}", obj.addr()));
        }
        i64::from(what == h4_res::w::HAS_LANGUAGE)
    }
    unsafe extern "C" fn add_dir(_: *mut c_void, _: WStr, _: Str, _: bool) {}
    unsafe extern "C" fn clear(_: *mut c_void) {}
    unsafe extern "C" fn read_dirs(
        _: *mut c_void,
        _: Str,
        _: h4_res::EachDir,
        _: *mut c_void,
    ) -> bool {
        false
    }
    ResOps {
        ctx: std::ptr::from_mut(m).cast(),
        log: m_log,
        create,
        destroy,
        call,
        file_path: m_file_path,
        add_dir,
        clear_dirs: clear,
        read_dirs,
    }
}

#[test]
fn resources_init_keeps_the_sound_slot_as_particle_manager() {
    let mut host = host();
    let ops = res_ops(&mut host);
    let mut r = Res::new(ops);
    r.init();
    r.update(0);
    let updates: Vec<usize> = host
        .log
        .iter()
        .filter_map(|l| l.strip_prefix(&format!("call:{}:", h4_res::w::UPDATE_MANAGER)))
        .filter_map(|a| a.parse().ok())
        .collect();
    assert_eq!(updates.len(), 13);
    let particle = 100 + usize::try_from(h4_res::w::PARTICLE).unwrap_or(0);
    assert_eq!(updates.iter().filter(|&&a| a == particle).count(), 2);
    assert!(
        host.log
            .first()
            .is_some_and(|l| l.starts_with("0:Initializing Resources Module"))
    );
    r.shutdown();
    let destroys: Vec<&String> = host
        .log
        .iter()
        .filter(|l| l.starts_with("destroy:"))
        .collect();
    assert_eq!(destroys.first().map(|s| s.as_str()), Some("destroy:8"));
    assert!(host.log.iter().any(|l| l == "destroy:18"));
}

#[test]
fn loader_tables_warn_and_fall_back() {
    let mut host = host();
    let ops = res_ops(&mut host);
    let mut r = Res::new(ops);
    let one = std::ptr::without_provenance_mut(1);
    let two = std::ptr::without_provenance_mut(2);
    r.add_entity_loader(b"A", one, false);
    r.add_entity_loader(b"B", two, true);
    r.add_entity_loader(b"A", two, false);
    assert_eq!(r.entity_loader(b"A"), one);
    assert_eq!(r.entity_loader(b"Z"), two);
    assert!(r.area_loader(b"Z").is_null());
    assert_eq!(
        host.log,
        vec![
            "1:No loader for type 'Z' found!\n".to_string(),
            "0:Using default loader!\n".to_string(),
            "1:No loader for area type 'Z' found!\n".to_string(),
        ]
    );
}

#[test]
fn handler_dispatch_logs_unknown_extensions_and_prefers_cached_msh() {
    unsafe extern "C" fn log(ctx: *mut c_void, _: i32, p: *const u8, n: usize) {
        // SAFETY: test host.
        unsafe {
            (*ctx.cast::<Vec<String>>()).push(String::from_utf8_lossy(units(p, n)).into_owned())
        }
    }
    unsafe extern "C" fn call(
        ctx: *mut c_void,
        what: i32,
        obj: *mut c_void,
        f: WStr,
        _: i64,
        _: *mut c_void,
    ) -> *mut c_void {
        // SAFETY: test host.
        unsafe {
            let file = String::from_utf8_lossy(&string::wide_to_utf8(units(f.p, f.n))).into_owned();
            (*ctx.cast::<Vec<String>>()).push(format!("call:{what}:{file}"));
        }
        match what {
            h::MESH_CACHE_USABLE => std::ptr::without_provenance_mut(usize::from(f.n == 5)),
            _ => obj.wrapping_add(7),
        }
    }
    let mut lines: Vec<String> = Vec::new();
    let ops = HandlerOps {
        ctx: (&raw mut lines).cast(),
        log,
        call,
    };
    let exts = ExtList(vec![b"dae".to_vec(), b"msh".to_vec()]);
    let mut table = LoaderTable::default();
    let loader = std::ptr::without_provenance_mut(0x100);
    // SAFETY: `exts` outlives the table.
    unsafe { table.add(loader, &raw const exts) };
    // SAFETY: the extension list is live.
    unsafe {
        assert!(
            h4_handler::run(
                &table,
                &ops,
                h::BITMAP_LOAD,
                &w("a.PNG"),
                0,
                std::ptr::null_mut()
            )
            .is_null()
        );
        let m = h4_handler::run(
            &table,
            &ops,
            h::MESH_LOAD,
            &w("a.dae"),
            3,
            std::ptr::null_mut(),
        );
        assert!(!m.is_null());
    }
    assert_eq!(
        lines.first().map(String::as_str),
        Some("No loader for file extension 'png' found!\n")
    );
    assert!(lines.contains(&"call:7:a.dae".to_string()));
    assert!(lines.iter().any(|l| l == "call:0:a.msh"));
}

#[test]
fn physics_name_comes_from_the_host() {
    let _g = TEST_LOCK.lock().unwrap_or_else(PoisonError::into_inner);
    let mut host = host();
    let o = ops(&mut host);
    let mut mgr = Manager::default();
    assert!(
        Flow::new(&mut mgr, &o)
            .material_physics_name(b"none")
            .is_empty()
    );
    let _ = k::MATERIAL;
}
