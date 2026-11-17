use super::boot::*;
use super::ffi::{Arg, Host, MainNums, Ret, ohpl_ga9_ret_bytes};
use std::cell::RefCell;
use std::ffi::c_void;

const GOLDEN: &str = include_str!("../../tests/golden/A13b/boot-amnesia.txt");

#[derive(Default)]
struct Fake {
    ops: RefCell<Vec<u32>>,
    cfg: RefCell<Vec<String>>,
    loaders: RefCell<Vec<(i64, String)>>,
    modules: RefCell<Vec<(i64, String)>>,
    flags: i64,
    fail_step: u32,
    module_count: i64,
}

fn text(a: &Arg) -> String {
    if a.p.is_null() {
        return String::new();
    }
    // SAFETY: string arguments point to `n` live bytes for the duration of the call.
    String::from_utf8_lossy(unsafe { std::slice::from_raw_parts(a.p, a.n) }).into_owned()
}

unsafe extern "C" fn fake_call(ctx: *mut c_void, op: u32, a: *const Arg, n: usize, r: *mut Ret) {
    // SAFETY: `ctx` is the test's `Fake`; `a` holds `n` arguments; `r` is the live `Ret`.
    let (fake, a, r) = unsafe {
        (
            &*ctx.cast::<Fake>(),
            std::slice::from_raw_parts(a, n),
            &mut *r,
        )
    };
    fake.ops.borrow_mut().push(op);
    match op {
        0..=8 => {
            fake.cfg
                .borrow_mut()
                .push(format!("{}.{}", text(&a[0]), text(&a[1])));
            match op {
                0 | 1 => r.i = a[2].i,
                2 => r.f = a[2].f,
                3 => {
                    // SAFETY: `r` is live; the default is `a[2].n` bytes.
                    unsafe { ohpl_ga9_ret_bytes(r, a[2].p, a[2].n) };
                }
                _ => {}
            }
        }
        bop::FLAGS | bop::EXIT_BEGIN => r.i = fake.flags,
        bop::MODULE_COUNT => r.i = fake.module_count,
        bop::G_ACHIEVEMENTS => r.i = 1,
        bop::G_MODULE => fake.modules.borrow_mut().push((a[0].i, text(&a[1]))),
        bop::G_LOADER => fake.loaders.borrow_mut().push((a[0].i, text(&a[2]))),
        bop::Q_DEFAULT_LANG => {
            // SAFETY: `r` is live.
            unsafe { ohpl_ga9_ret_bytes(r, b"english.lang".as_ptr(), 12) };
        }
        _ => r.i = i64::from(op != fake.fail_step),
    }
}

fn host(f: &Fake) -> Host {
    Host {
        ctx: std::ptr::from_ref(f).cast_mut().cast(),
        call: fake_call,
    }
}

fn run_init(flags: i64) -> Vec<u32> {
    let f = Fake {
        flags,
        ..Fake::default()
    };
    assert!(init(host(&f)));
    f.ops.take()
}

fn tail(ops: &[u32], n: usize) -> Vec<u32> {
    ops[ops.len() - n..].to_vec()
}

#[test]
fn start_modes() {
    let pre = run_init(FLAG_PRE_MENU | FLAG_MENU);
    assert_eq!(tail(&pre, 2), [bop::START_PRE, bop::INIT_OVER]);
    let menu = run_init(FLAG_MENU);
    assert_eq!(tail(&menu, 2), [bop::START_MENU, bop::INIT_OVER]);
    let direct = run_init(0);
    assert_eq!(
        tail(&direct, 6),
        [
            bop::START_DIRECT,
            bop::INIT_USER_CFG,
            bop::LOCK_INPUT,
            bop::RELATIVE_MOUSE,
            bop::START_GAME,
            bop::INIT_OVER
        ]
    );
    let window = run_init(FLAG_FULLSCREEN);
    assert_eq!(
        tail(&window, 5),
        [
            bop::START_DIRECT,
            bop::INIT_USER_CFG,
            bop::RELATIVE_MOUSE,
            bop::START_GAME,
            bop::INIT_OVER
        ]
    );
    let debug = run_init(FLAG_DEBUG_WINDOW);
    assert_eq!(
        tail(&debug, 4),
        [
            bop::START_DIRECT,
            bop::INIT_USER_CFG,
            bop::START_GAME,
            bop::INIT_OVER
        ]
    );
}

#[test]
fn init_stops_at_the_failed_step() {
    for step in [
        bop::STEP_PARSE,
        bop::STEP_APP,
        bop::STEP_MAIN_CONFIG,
        bop::SERIAL,
        bop::STEP_ENGINE,
        bop::STEP_FEATURE,
        bop::STEP_GAME,
        bop::INIT_USER_CFG,
    ] {
        let f = Fake {
            fail_step: step,
            ..Fake::default()
        };
        assert!(!init(host(&f)));
        assert_eq!(f.ops.borrow().last(), Some(&step));
    }
}

#[test]
fn init_order() {
    let ops = run_init(FLAG_MENU);
    assert_eq!(
        &ops[..12],
        [
            bop::INIT_BEGIN,
            bop::STEP_PARSE,
            bop::STEP_APP,
            bop::STEP_MAIN_CONFIG,
            bop::LOG_VERSION,
            bop::SERIAL,
            bop::LOG_NEWLINE,
            bop::STEP_ENGINE,
            bop::STEP_FEATURE,
            bop::STEP_GAME,
            bop::HEADLESS,
            bop::FLAGS
        ]
    );
}

#[test]
fn exit_order() {
    let f = Fake {
        flags: FLAG_SAVE_AT_EXIT,
        module_count: 3,
        ..Fake::default()
    };
    exit(host(&f));
    assert_eq!(
        *f.ops.borrow(),
        [
            bop::EXIT_BEGIN,
            bop::SAVE_CONFIG,
            bop::EXIT_GAME,
            bop::MODULE_COUNT,
            bop::DELETE_MODULE,
            bop::DELETE_MODULE,
            bop::DELETE_MODULE,
            bop::EXIT_CONFIG,
            bop::EXIT_END
        ]
    );
    let g = Fake::default();
    exit(host(&g));
    assert!(!g.ops.borrow().contains(&bop::SAVE_CONFIG));
}

fn golden_between(from: &str, to: &str) -> Vec<&'static str> {
    GOLDEN
        .lines()
        .skip_while(|l| *l != from)
        .skip(1)
        .take_while(|l| *l != to)
        .collect()
}

fn run_engine() -> Fake {
    let f = Fake::default();
    // SAFETY: `MainNums` is plain old data; all-zero is a valid value.
    let n: MainNums = unsafe { std::mem::zeroed() };
    assert!(init_engine(host(&f), &n));
    f
}

#[test]
fn engine_config_reads_match_the_recorded_boot() {
    let expected: Vec<String> = golden_between("step InitEngine", "step CheckFeatureSupport")
        .into_iter()
        .filter_map(|l| l.strip_prefix("cfg main_settings.cfg "))
        .map(|l| l.split(" = ").next().unwrap_or_default().to_string())
        .collect();
    assert_eq!(expected.len(), 7);
    assert_eq!(*run_engine().cfg.borrow(), expected);
}

#[test]
fn engine_ops_follow_the_recorded_boot() {
    let table = [
        ("engine vars", bop::E_VARS),
        ("engine sound ", bop::E_SOUND),
        ("static shadowQuality", bop::E_STATIC_A),
        ("static ssaoDiv", bop::E_STATIC_SSAO),
        ("static edgeSmooth", bop::E_STATIC_MISC),
        ("engine create", bop::E_CREATE),
        ("engine vsync", bop::E_VSYNC),
        ("engine material", bop::E_MATERIAL),
        ("engine resourceDirs", bop::E_RESOURCES),
        ("engine surfaceData", bop::E_SURFACE),
        ("engine language", bop::E_LANGUAGE_BEGIN),
        ("engine fastload", bop::E_FASTLOAD),
        ("engine sounddev", bop::E_SOUND_DEVICE),
    ];
    let expected: Vec<u32> = golden_between("step InitEngine", "step CheckFeatureSupport")
        .into_iter()
        .filter(|l| !l.starts_with("cfg "))
        .filter_map(|l| {
            table
                .iter()
                .find(|(p, _)| l.starts_with(p))
                .map(|&(_, o)| o)
        })
        .collect();
    assert_eq!(expected.len(), table.len());
    let logged: Vec<u32> = run_engine()
        .ops
        .take()
        .into_iter()
        .filter(|o| table.iter().any(|&(_, t)| t == *o))
        .collect();
    assert_eq!(logged, expected);
}

#[test]
fn game_tables_match_the_recorded_boot() {
    let f = Fake::default();
    assert!(init_game(host(&f)));
    let golden_modules = GOLDEN.lines().filter(|l| l.starts_with("module ")).count();
    let ours: Vec<String> = f
        .modules
        .borrow()
        .iter()
        .filter(|(id, _)| *id != 24)
        .map(|(_, c)| c.clone())
        .collect();
    assert_eq!(ours.len(), golden_modules);
    let golden_containers: Vec<String> = GOLDEN
        .lines()
        .filter_map(|l| l.strip_prefix("module "))
        .filter_map(|l| l.split("container=").nth(1))
        .map(|c| {
            if c == "<global>" {
                String::new()
            } else {
                c.to_string()
            }
        })
        .collect();
    assert_eq!(ours, golden_containers);
    let keys = |kind: &str| -> Vec<String> {
        GOLDEN
            .lines()
            .filter_map(|l| l.strip_prefix(&format!("loader {kind} ")))
            .filter_map(|l| l.split(' ').nth(1))
            .map(str::to_string)
            .collect()
    };
    let got = |kind: i64| -> Vec<String> {
        f.loaders
            .borrow()
            .iter()
            .filter(|(k, _)| *k == kind)
            .map(|(_, key)| key.clone())
            .collect()
    };
    assert_eq!(got(0), keys("entity"));
    assert_eq!(got(1), keys("area"));
    let containers: Vec<&str> = GOLDEN
        .lines()
        .find_map(|l| l.strip_prefix("containers "))
        .map(|l| l.split(' ').collect())
        .unwrap_or_default();
    let ours: Vec<&str> = CONTAINERS
        .iter()
        .map(|c| std::str::from_utf8(c).unwrap_or_default())
        .collect();
    assert_eq!(ours, containers);
}

#[test]
fn achievements_follow_the_default_modules() {
    let f = Fake::default();
    assert!(init_game(host(&f)));
    let ops = f.ops.borrow();
    let at = |op: u32| ops.iter().position(|o| *o == op);
    assert!(at(bop::G_ACHIEVEMENTS) < at(bop::G_ACHIEVEMENTS_END));
    assert_eq!(ops.iter().filter(|o| **o == bop::G_ACHIEVEMENT).count(), 18);
    assert_eq!(tail(&ops, 2), [bop::G_MESSAGE, bop::G_MESSAGE]);
}
