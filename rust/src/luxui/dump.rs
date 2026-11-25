//! Real-engine dump hooks of the inventory, hint, message and journal UI: OHPL_A11A_INVDUMP (item,
//! type, combine and save sequence), OHPL_A11B_DUMP (hint and message driver, draw trace),
//! OHPL_A11B_ANSWERS (recorded engine answers) and OHPL_A11C_DUMP (journal driver, draw trace).
//! The output is compared byte for byte with the recorded C++ runs.

use super::event::Str;
use super::host;
use super::invffi as inv;
use super::jrn as jm;
use super::jrnffi as jf;
use crate::gui::direct;
use crate::somascript::cfmt::{fmt_g, fmt_g9};
use crate::types::{Color, Vector2f, Vector3f};
use std::ffi::c_void;
use std::fs::File;
use std::io::Write;
use std::sync::atomic::{AtomicBool, AtomicI32, AtomicU8, AtomicU64, Ordering};
use std::sync::{Mutex, OnceLock};

pub const DT: f32 = 1.0 / 60.0;

/// The message callback the driver passes; it logs and never reaches the engine.
pub const CALLBACK: usize = 1;

struct Dump {
    env: &'static str,
    file: OnceLock<Option<Mutex<File>>>,
}

impl Dump {
    const fn new(env: &'static str) -> Self {
        Dump {
            env,
            file: OnceLock::new(),
        }
    }

    fn out(&self) -> Option<&Mutex<File>> {
        self.file
            .get_or_init(|| {
                let p = std::env::var_os(self.env).filter(|v| !v.is_empty())?;
                File::create(p).ok().map(Mutex::new)
            })
            .as_ref()
    }

    fn on(&self) -> bool {
        self.out().is_some()
    }

    fn line(&self, s: &str) {
        if let Some(Ok(mut f)) = self.out().map(Mutex::lock) {
            let _ = f.write_all(format!("{s}\n").as_bytes());
        }
    }
}

static B: Dump = Dump::new("OHPL_A11B_DUMP");
static ANS: Dump = Dump::new("OHPL_A11B_ANSWERS");
static C: Dump = Dump::new("OHPL_A11C_DUMP");

fn g9(v: f32) -> String {
    String::from_utf8_lossy(&fmt_g9(v)).into_owned()
}

fn g6(v: f32) -> String {
    String::from_utf8_lossy(&fmt_g(v, 48)).into_owned()
}

fn lossy(s: &[u8]) -> std::borrow::Cow<'_, str> {
    String::from_utf8_lossy(s)
}

fn units(s: &str) -> Vec<u32> {
    s.chars().map(u32::from).collect()
}

fn vals(v: &[f32]) -> String {
    v.iter().map(|&x| g9(x)).collect::<Vec<_>>().join(" ")
}

fn ascii(c: u32) -> Option<char> {
    u8::try_from(c)
        .ok()
        .filter(|b| (32..127).contains(b))
        .map(char::from)
}

/// Printable ASCII as is, anything else as \u%04x; `bar` is the char kept as is besides.
fn text32(t: &[u32], keep_bar: bool) -> String {
    t.iter()
        .map(|&c| match ascii(c) {
            Some(ch) if keep_bar || ch != '|' => ch.to_string(),
            _ => format!("\\u{c:04x}"),
        })
        .collect()
}

pub fn answers() -> bool {
    ANS.on()
}

pub fn answer(op: &str, input: &str, out: &str) {
    ANS.line(&format!("{op}\t{input}\t{out}"));
}

pub fn esc(s: &[u8]) -> String {
    s.iter()
        .map(|&c| match ascii(u32::from(c)) {
            Some(ch) if ch != '\\' && ch != '|' => ch.to_string(),
            _ => format!("\\u{c:04x}"),
        })
        .collect()
}

pub fn esc32(t: &[u32]) -> String {
    t.iter()
        .map(|&c| match ascii(c) {
            Some(ch) if ch != '\\' && ch != '|' => ch.to_string(),
            _ => format!("\\u{c:04x}"),
        })
        .collect()
}

pub fn num(v: f32) -> String {
    g9(v)
}

pub fn enabled() -> bool {
    B.on()
}

pub fn log(s: &str) {
    B.line(s);
}

pub fn log_font(pos: [f32; 3], size: [f32; 2], col: [f32; 4], align: i32, mat: i32, text: &[u32]) {
    if B.on() {
        B.line(&format!(
            "F {} {} {} {align} {mat} |{}",
            vals(&pos),
            vals(&size),
            vals(&col),
            text32(text, true)
        ));
    }
}

pub fn log_gfx(pos: [f32; 3], size: [f32; 2], col: [f32; 4], mat: i32) {
    if B.on() {
        B.line(&format!(
            "G {} {} {} {mat}",
            vals(&pos),
            vals(&size),
            vals(&col)
        ));
    }
}

fn map_loaded() -> bool {
    // SAFETY: the table's function takes no arguments.
    host::table().is_some_and(|t| unsafe { (t.map_loaded)() } != 0)
}

fn in_journal() -> bool {
    // SAFETY: as `map_loaded`.
    host::table().is_some_and(|t| unsafe { (t.journal_active)() } != 0)
}

fn enter_journal() {
    if let Some(t) = host::table() {
        // SAFETY: as `map_loaded`.
        unsafe { (t.enter_journal)() };
    }
}

static LAST_HINT: AtomicU64 = AtomicU64::new(0);
static LAST_MSG: AtomicU64 = AtomicU64::new(0);

pub fn hint_created(h: u64) {
    LAST_HINT.store(h, Ordering::Relaxed);
}

pub fn msg_created(h: u64) {
    LAST_MSG.store(h, Ordering::Relaxed);
}

fn hint_add(h: u64, name: &str, text: &str, time: f32) -> i32 {
    let t = units(text);
    // SAFETY: both buffers live for the call.
    unsafe {
        super::handlers_ffi::ohpl_a11b_hint_add(
            h,
            name.as_ptr(),
            name.len(),
            t.as_ptr(),
            t.len(),
            time,
        )
    }
}

fn hint_block(h: u64, name: &str, on: i32) {
    // SAFETY: the name lives for the call.
    unsafe { super::handlers_ffi::ohpl_a11b_hint_block(h, name.as_ptr(), name.len(), on) };
}

fn hint_remove(h: u64, name: &str) -> i32 {
    // SAFETY: as `hint_block`.
    unsafe { super::handlers_ffi::ohpl_a11b_hint_remove(h, name.as_ptr(), name.len()) }
}

fn hint_names(h: u64, blocked: i32) -> Vec<Vec<u8>> {
    unsafe extern "C" fn push(ctx: *mut c_void, p: *const u8, n: usize) {
        // SAFETY: `ctx` is the vector below; the handler passes `n` readable bytes.
        let v = unsafe { &mut *ctx.cast::<Vec<Vec<u8>>>() };
        v.push(if p.is_null() {
            Vec::new()
        } else {
            unsafe { std::slice::from_raw_parts(p, n) }.to_vec()
        });
    }
    let mut v: Vec<Vec<u8>> = Vec::new();
    // SAFETY: `push` accepts the vector.
    unsafe {
        super::handlers_ffi::ohpl_a11b_hint_names(
            h,
            blocked,
            push,
            std::ptr::from_mut(&mut v).cast(),
        );
    }
    v
}

fn hint_set_names(h: u64, blocked: i32, names: &[Vec<u8>]) {
    super::handlers_ffi::ohpl_a11b_hint_names_clear(h, blocked);
    for n in names {
        // SAFETY: `n` lives for the call.
        unsafe { super::handlers_ffi::ohpl_a11b_hint_names_add(h, blocked, n.as_ptr(), n.len()) };
    }
}

fn msg_set(m: u64, text: &str, time: f32) {
    let t = units(text);
    // SAFETY: the buffer lives for the call.
    unsafe { super::handlers_ffi::ohpl_a11b_msg_set_message(m, t.as_ptr(), t.len(), time) };
}

fn msg_pause(m: u64, text: &str, yes_no: i32, cb: usize) {
    let t = units(text);
    // SAFETY: the buffer lives for the call.
    unsafe { super::handlers_ffi::ohpl_a11b_msg_start_pause(m, t.as_ptr(), t.len(), yes_no, cb) };
}

static B_FRAME: AtomicI32 = AtomicI32::new(0);

/// One step of the A11b driver; the message handler draws it, after the A11c step.
pub fn tick() {
    use super::handlers_ffi as hf;
    c_tick();
    if !B.on() {
        return;
    }
    if !map_loaded() {
        B_FRAME.store(0, Ordering::Relaxed);
        return;
    }
    let f = B_FRAME.fetch_add(1, Ordering::Relaxed) + 1;
    let (h, m) = (
        LAST_HINT.load(Ordering::Relaxed),
        LAST_MSG.load(Ordering::Relaxed),
    );
    let yn = |b: i32| i32::from(b != 0);
    match f {
        3 => {
            log(&format!(
                "add a {}",
                yn(hint_add(h, "a11b_a", "Hint one", 2.0))
            ));
            log(&format!(
                "add a again {}",
                yn(hint_add(h, "a11b_a", "Hint one", 2.0))
            ));
        }
        6 => msg_set(
            m,
            "Game message one that is somewhat long and has to wrap over several rows of the screen",
            2.0,
        ),
        9 => {
            hint_block(h, "a11b_b", 1);
            log(&format!(
                "add b blocked {}",
                yn(hint_add(h, "a11b_b", "Blocked", 1.0))
            ));
            hint_block(h, "a11b_b", 0);
            log(&format!("remove a {}", yn(hint_remove(h, "a11b_a"))));
            log(&format!("remove zz {}", yn(hint_remove(h, "zz"))));
        }
        12 => hf::ohpl_a11b_msg_start_quest(m),
        15 => msg_pause(
            m,
            "Pause message text that is long enough to wrap around the pause box more than once, really",
            1,
            CALLBACK,
        ),
        40 => hf::ohpl_a11b_msg_do_action(m, 0, 1),
        41 => hf::ohpl_a11b_msg_do_action(m, 0, 0),
        45 => msg_pause(m, "Second pause", 0, 0),
        50 | 60 => hf::ohpl_a11b_msg_do_action(m, 1, 1),
        55 => msg_pause(m, "Third pause", 1, CALLBACK),
        65 => hf::ohpl_a11b_msg_set_pause(m, 1),
        70 => hf::ohpl_a11b_msg_set_pause(m, 0),
        80 => hf::ohpl_a11b_hint_set_active(h, 0),
        82 => hf::ohpl_a11b_hint_set_active(h, 1),
        85 => log(&format!(
            "add c {}",
            yn(hint_add(
                h,
                "a11b_c",
                "Press $ButInteract and $ButAttack and $ButJump to go on with a long line of text that wraps over rows",
                0.5
            ))
        )),
        250 => log(&format!(
            "add d {}",
            yn(hint_add(h, "a11b_d", "Short $ButInteract hint", 0.5))
        )),
        330 => log(&format!(
            "add e {}",
            yn(hint_add(
                h,
                "a11b_e",
                "Duration derived from the string $ButLean",
                0.0
            ))
        )),
        100 => {
            let (given, blocked) = (hint_names(h, 0), hint_names(h, 1));
            for n in &given {
                log(&format!("given {}", lossy(n)));
            }
            for n in &blocked {
                log(&format!("blocked {}", lossy(n)));
            }
            hint_set_names(h, 0, &given);
            hint_set_names(h, 1, &blocked);
            hf::ohpl_a11b_msg_reset(m);
            hf::ohpl_a11b_hint_reset(h);
        }
        _ => {}
    }
    log(&format!(
        "tick {f} hint={} msg={} pause={} sub={} esub={}",
        yn(hf::ohpl_a11b_hint_is_active(h)),
        yn(hf::ohpl_a11b_msg_flag(m, 0)),
        yn(hf::ohpl_a11b_msg_flag(m, 1)),
        yn(hf::ohpl_a11b_msg_flag(m, 2)),
        yn(hf::ohpl_a11b_msg_flag(m, 3))
    ));
}

pub fn c_on() -> bool {
    C.on()
}

/// Read by the C++ draw sites of `cGuiSet`: set once the journal driver runs.
#[allow(non_upper_case_globals)]
#[unsafe(no_mangle)]
pub static ohpl_a11c_trace_on: AtomicU8 = AtomicU8::new(0);

pub fn trace_gfx(set: usize, g: usize, pos: &Vector3f, size: &Vector2f, col: &Color, mat: i32) {
    if ohpl_a11c_trace_on.load(Ordering::Acquire) == 0 || set != jf::ohpl_jrn_get_set() {
        return;
    }
    let s = if g == 0 {
        [0.0; 2]
    } else {
        direct::gfx_active_size(g)
    };
    C.line(&format!(
        "G {} {} {} {mat} {}",
        vals(&[pos.x, pos.y, pos.z]),
        vals(&[size.x, size.y]),
        vals(&[col.r, col.g, col.b, col.a]),
        vals(&s)
    ));
}

/// # Safety
/// `pos`, `size` and `col` are live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a11c_trace_gfx(
    set: usize,
    g: usize,
    pos: *const Vector3f,
    size: *const Vector2f,
    col: *const Color,
    mat: i32,
) {
    // SAFETY: caller contract.
    unsafe { trace_gfx(set, g, &*pos, &*size, &*col, mat) };
}

/// # Safety
/// `text` is `n` readable units; the vectors and the colour are live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a11c_trace_text(
    set: usize,
    text: *const u32,
    n: usize,
    pos: *const Vector3f,
    size: *const Vector2f,
    col: *const Color,
    align: i32,
) {
    if ohpl_a11c_trace_on.load(Ordering::Acquire) == 0 || set != jf::ohpl_jrn_get_set() {
        return;
    }
    // SAFETY: caller contract.
    let (t, p, s, c) = unsafe {
        let t = if text.is_null() {
            &[][..]
        } else {
            std::slice::from_raw_parts(text, n)
        };
        (t, *pos, *size, *col)
    };
    C.line(&format!(
        "F {} {} {} {align} |{}",
        vals(&[p.x, p.y, p.z]),
        vals(&[s.x, s.y]),
        vals(&[c.r, c.g, c.b, c.a]),
        text32(t, false)
    ));
}

static C_FRAME: AtomicI32 = AtomicI32::new(0);

type Entry4 = [Vec<u8>; 4];

#[derive(Default)]
struct Snapshot {
    last: (i32, i32, i32),
    notes: Vec<Entry4>,
    conts: Vec<(Vec<u8>, Vec<Entry4>)>,
    quests: Vec<(bool, [Vec<u8>; 3])>,
}

fn snapshot() -> Snapshot {
    let field4 = |f: &dyn Fn(i32) -> Vec<u8>| [f(0), f(1), f(2), f(3)];
    Snapshot {
        last: jm::last_read(),
        notes: (0..jm::note_count())
            .map(|i| field4(&|f| jm::note_field(i, f)))
            .collect(),
        conts: (0..jm::cont_count())
            .map(|c| {
                (
                    jm::cont_type(c),
                    (0..jm::diary_count(c))
                        .map(|i| field4(&|f| jm::diary_field(c, i, f)))
                        .collect(),
                )
            })
            .collect(),
        quests: (0..jm::quest_count())
            .map(|i| {
                (
                    jm::quest_active(i),
                    [0, 1, 2].map(|f| jm::quest_field(i, f)),
                )
            })
            .collect(),
    }
}

fn restore(sn: &Snapshot) {
    jm::load_begin(sn.last.0, sn.last.1, sn.last.2);
    let entry = |e: &Entry4| jm::Entry {
        name: e[0].clone(),
        text: e[1].clone(),
        icon: e[2].clone(),
        image: e[3].clone(),
    };
    for n in &sn.notes {
        jm::load_note(entry(n));
    }
    for (ty, diaries) in &sn.conts {
        jm::load_container(ty.clone());
        for d in diaries {
            jm::load_diary(entry(d));
        }
    }
    for (active, q) in &sn.quests {
        jm::load_quest(jm::Quest {
            active: *active,
            name: q[0].clone(),
            name_entry: q[1].clone(),
            text_entry: q[2].clone(),
        });
    }
}

fn show(tag: &str, sn: &Snapshot) {
    C.line(&format!(
        "{tag} last={},{},{} notes={} conts={} quests={}",
        sn.last.0,
        sn.last.1,
        sn.last.2,
        sn.notes.len(),
        sn.conts.len(),
        sn.quests.len()
    ));
    for (i, n) in sn.notes.iter().enumerate() {
        C.line(&format!(
            "  n[{i}] {} {} {} {}",
            lossy(&n[0]),
            lossy(&n[1]),
            lossy(&n[2]),
            lossy(&n[3])
        ));
    }
    for (c, (ty, diaries)) in sn.conts.iter().enumerate() {
        C.line(&format!("  c[{c}] {} {}", lossy(ty), diaries.len()));
        for (i, d) in diaries.iter().enumerate() {
            C.line(&format!(
                "    d[{i}] {} {} {} {}",
                lossy(&d[0]),
                lossy(&d[1]),
                lossy(&d[2]),
                lossy(&d[3])
            ));
        }
    }
    for (i, (active, q)) in sn.quests.iter().enumerate() {
        C.line(&format!(
            "  q[{i}] {} {} {} {}",
            i32::from(*active),
            lossy(&q[0]),
            lossy(&q[1]),
            lossy(&q[2])
        ));
    }
}

fn snap(tag: &str) {
    show(tag, &snapshot());
}

fn s(x: &str) -> Str {
    Str::of(x.as_bytes())
}

const BUTTON_PRESS: i32 = 19;
const ARROW_PRESS: i32 = 17;
const PRIMARY: i32 = 1;
const SECONDARY: i32 = 2;
const UP_RIGHT_DOWN_LEFT: [i32; 4] = [1, 2, 4, 8];

fn button(b: i32) {
    direct::send_value(BUTTON_PRESS, b);
}

fn arrow(dir: usize) {
    direct::send_value(
        ARROW_PRESS,
        UP_RIGHT_DOWN_LEFT.get(dir).copied().unwrap_or(0),
    );
}

const RIGHT: usize = 1;
const DOWN: usize = 2;
const LEFT: usize = 3;

/// One step of the A11c driver; the journal draws it and so does the message handler.
pub fn c_tick() {
    if !C.on() {
        return;
    }
    if !map_loaded() {
        C_FRAME.store(0, Ordering::Relaxed);
        return;
    }
    ohpl_a11c_trace_on.store(1, Ordering::Release);
    let fr = C_FRAME.fetch_add(1, Ordering::Relaxed) + 1;
    C.line(&format!("tick {fr}"));
    let diary = |name: &str| {
        let i = jf::ohpl_jrn_add_diary(s(name), s("diary_page.tga"));
        C.line(&format!("diary idx {i}"));
    };
    match fr {
        3 => {
            jf::ohpl_jrn_reset();
            for (n, i) in [
                ("CH01L03_VillageTales", "papernote.tga"),
                ("CH01L11_AccountOfHowling", "letter.tga"),
                ("CH01L03_WineCellar", "old_note.tga"),
            ] {
                jf::ohpl_jrn_add_note(s(n), s(i));
            }
            jf::ohpl_jrn_add_diary(s("CH01L03_Daniel"), s("diary_page.tga"));
            diary("CH01L03_Daniel");
            diary("CH01L06_Daniel02");
            let q = |label: &str, v: i32| C.line(&format!("{label} {v}"));
            q(
                "quest add",
                jf::ohpl_jrn_add_quest(s("00Trail"), s("00Trail")),
            );
            q(
                "quest again",
                jf::ohpl_jrn_add_quest(s("00Trail"), s("00Trail")),
            );
            q("quest add", jf::ohpl_jrn_add_quest(s("02Web"), s("02Web")));
            q(
                "quest add",
                jf::ohpl_jrn_add_quest(s("03Books"), s("03Books")),
            );
            q("quest off", jf::ohpl_jrn_disable_quest(s("02Web")));
            q("quest off again", jf::ohpl_jrn_disable_quest(s("02Web")));
            snap("added");
        }
        5 => {
            let saved = snapshot();
            jf::ohpl_jrn_reset();
            snap("reset");
            restore(&saved);
            snap("restored");
        }
        8 => enter_journal(),
        20 | 70 | 114 | 138 | 206 => {
            if !in_journal() {
                enter_journal();
            }
        }
        250 => {
            if !in_journal() {
                jf::ohpl_jrn_open_last_read();
            }
        }
        270 => {
            snap("final");
            if !in_journal() {
                jf::ohpl_jrn_set_force_instant_exit(1);
                enter_journal();
                jf::ohpl_jrn_open_note_idx(1, 0);
            }
        }
        300 => {
            diary("CH01L03_Daniel");
            jf::ohpl_jrn_add_quest(s("04FindChemicals"), s("04FindChemicals"));
            snap("end");
        }
        _ if !in_journal() => {}
        24 | 32 | 40 | 44 | 48 | 78 | 88 | 96 | 124 | 180 | 210 | 214 => button(PRIMARY),
        28 | 74 | 84 | 116 | 120 | 164 => arrow(DOWN),
        52 | 56 | 100 | 222 => button(SECONDARY),
        60 | 64 | 104 | 108 | 132 | 188 | 192 | 196 | 200 | 226 | 230 | 234 => {
            jf::ohpl_jrn_exit_pressed(0);
        }
        140 => jf::ohpl_jrn_change_state(1),
        148 => jf::ohpl_jrn_open_note_idx(0, 0),
        156 => arrow(RIGHT),
        160 => arrow(LEFT),
        172 => jf::ohpl_jrn_open_diary_idx(0, 1, 0),
        262 | 282 => jf::ohpl_jrn_exit_pressed(1),
        _ => {}
    }
}

fn item_str(h: usize, which: i32) -> String {
    unsafe extern "C" fn assign(ctx: *mut c_void, p: *const u8, n: usize) {
        // SAFETY: `ctx` is the vector below; `n` readable bytes at `p`.
        let v = unsafe { &mut *ctx.cast::<Vec<u8>>() };
        *v = if p.is_null() {
            Vec::new()
        } else {
            unsafe { std::slice::from_raw_parts(p, n) }.to_vec()
        };
    }
    let mut v: Vec<u8> = Vec::new();
    unsafe { inv::ohpl_inv_item_str(h, which, assign, std::ptr::from_mut(&mut v).cast()) };
    String::from_utf8_lossy(&v).into_owned()
}

fn combine_str(i: i32, which: i32) -> String {
    unsafe extern "C" fn assign(ctx: *mut c_void, p: *const u8, n: usize) {
        // SAFETY: as `item_str`.
        let v = unsafe { &mut *ctx.cast::<Vec<u8>>() };
        *v = if p.is_null() {
            Vec::new()
        } else {
            unsafe { std::slice::from_raw_parts(p, n) }.to_vec()
        };
    }
    let mut v: Vec<u8> = Vec::new();
    unsafe { inv::ohpl_inv_combine_str(i, which, assign, std::ptr::from_mut(&mut v).cast()) };
    String::from_utf8_lossy(&v).into_owned()
}

fn type_name(t: i32) -> String {
    unsafe extern "C" fn assign(ctx: *mut c_void, p: *const u8, n: usize) {
        // SAFETY: as `item_str`.
        let v = unsafe { &mut *ctx.cast::<Vec<u8>>() };
        *v = if p.is_null() {
            Vec::new()
        } else {
            unsafe { std::slice::from_raw_parts(p, n) }.to_vec()
        };
    }
    let mut v: Vec<u8> = Vec::new();
    unsafe { inv::ohpl_inv_type_name(t, assign, std::ptr::from_mut(&mut v).cast()) };
    String::from_utf8_lossy(&v).into_owned()
}

fn items(out: &mut String, tag: &str, equip: bool) {
    let n = inv::ohpl_inv_item_num();
    let eq = if equip {
        inv::ohpl_inv_item_index(inv::ohpl_inv_equipped())
    } else {
        -2
    };
    out.push_str(&format!(
        "{tag} n={n} eq={eq} dis={} msg={} picked={}\n",
        inv::ohpl_inv_disabled(),
        inv::ohpl_inv_message_active(),
        i32::from(inv::ohpl_inv_picked() != 0)
    ));
    for i in 0..n {
        let p = inv::ohpl_inv_item_at(i);
        out.push_str(&format!(
            "  [{i}] name={} type={} sub={} val={} extra={} img={} cnt={} amt={} ne={} de={} gfx={}\n",
            item_str(p, 0),
            inv::ohpl_inv_item_type(p),
            item_str(p, 1),
            item_str(p, 2),
            item_str(p, 3),
            item_str(p, 4),
            inv::ohpl_inv_item_count(p),
            g6(inv::ohpl_inv_item_amount(p)),
            item_str(p, 5),
            item_str(p, 6),
            i32::from(inv::ohpl_inv_item_image(p) != 0)
        ));
    }
}

struct Add(
    &'static str,
    i32,
    &'static str,
    f32,
    &'static str,
    &'static str,
);

#[allow(clippy::too_many_arguments)]
fn add_item(
    name: &str,
    ty: i32,
    sub: &str,
    image: &str,
    amount: f32,
    val: &str,
    extra: &str,
    remove: &mut i32,
) -> usize {
    // SAFETY: `remove` is writable.
    unsafe {
        inv::ohpl_inv_add_item(
            s(name),
            ty,
            s(sub),
            s(image),
            amount,
            s(val),
            s(extra),
            remove,
        )
    }
}

/// `OhplA11aInvDump`: item, type, combine and save-data sequence through the public inventory calls.
pub fn inv_dump() {
    static DONE: AtomicBool = AtomicBool::new(false);
    let Some(path) = std::env::var_os("OHPL_A11A_INVDUMP") else {
        return;
    };
    if DONE.swap(true, Ordering::Relaxed) {
        return;
    }
    let Ok(mut file) = File::create(path) else {
        return;
    };
    let mut o = String::new();
    let types = [
        "Puzzle",
        "coins",
        "NOTE",
        "Diary",
        "Lantern",
        "Health",
        "Sanity",
        "LampOil",
        "Tinderbox",
        "HandObject",
    ];
    for name in types {
        let t = inv::ohpl_inv_type_from_string(s(name));
        o.push_str(&format!(
            "type {name} -> {t} name={} count={} max={} pickup={} hasmax={}\n",
            type_name(t),
            inv::ohpl_inv_type_has_count(t),
            inv::ohpl_inv_type_max_count(t),
            inv::ohpl_inv_type_show_pickup(t),
            inv::ohpl_inv_type_has_max(t)
        ));
    }
    inv::ohpl_inv_reset();
    let mut remove = 0;
    let adds = [
        Add("puz_a", 0, "agrippa_head", 0.0, "", ""),
        Add("puz_b", 0, "bone_saw", 1.0, "L02_Shipment", "x"),
        Add("hp_a", 5, "health", 25.0, "", ""),
        Add("hp_b", 5, "health", 25.0, "", ""),
        Add("san_a", 6, "sanity", 30.0, "", ""),
        Add("oil_a", 7, "oil", 20.0, "", ""),
        Add("oil_b", 7, "oil", 20.0, "", ""),
        Add("lantern", 4, "lantern", 0.0, "", ""),
        Add("hand_a", 9, "bucket_of_tar", 0.0, "", ""),
        Add("hand_b", 9, "bone_saw", 0.0, "", ""),
        Add("tb_a", 8, "tinderbox", 0.0, "", ""),
        Add("coins", 1, "coins_large", 10.0, "", ""),
    ];
    for Add(name, ty, sub, amount, val, extra) in adds {
        remove = 0;
        let p = add_item(
            name,
            ty,
            sub,
            "agrippa_head.tga",
            amount,
            val,
            extra,
            &mut remove,
        );
        o.push_str(&format!(
            "add {name} -> {} remove={remove} idx={}\n",
            i32::from(p != 0),
            inv::ohpl_inv_item_index(p)
        ));
        items(&mut o, " state", true);
    }
    for _ in 0..100 {
        add_item(
            "hp_x",
            5,
            "health",
            "agrippa_head.tga",
            25.0,
            "",
            "",
            &mut remove,
        );
    }
    items(&mut o, "capped", true);
    let has = |t: i32| inv::ohpl_inv_has_of_type(t);
    o.push_str(&format!(
        "has lantern={} oil={} tinder={} note={}\n",
        has(4),
        has(7),
        has(8),
        has(2)
    ));
    let by_name = |n: &str| inv::ohpl_inv_get_by_name(s(n));
    o.push_str(&format!(
        "get puz_b={} sub bone_saw={} nothing={}\n",
        inv::ohpl_inv_item_index(by_name("puz_b")),
        inv::ohpl_inv_item_index(inv::ohpl_inv_get_by_subtype(s("bone_saw"))),
        i32::from(by_name("nothing") != 0)
    ));
    inv::ohpl_inv_set_equipped(by_name("hand_b"));
    inv::ohpl_inv_add_combine(s("cb1"), s("puz_a"), s("puz_b"), s("CombineFn"), 1);
    inv::ohpl_inv_add_combine(s("cb2"), s("hp_a"), s("oil_a"), s("OtherFn"), 0);
    inv::ohpl_inv_remove_combine(s("cb2"));
    inv::ohpl_inv_set_disabled(1);
    for (a, b) in [
        ("puz_a", "puz_b"),
        ("puz_b", "puz_a"),
        ("hp_a", "oil_a"),
        ("puz_a", "puz_a"),
    ] {
        let c = inv::ohpl_inv_find_combine(s(a), s(b));
        let r = if c >= 0 {
            format!("{},{}", combine_str(c, 0), combine_str(c, 3))
        } else {
            "none".to_owned()
        };
        o.push_str(&format!("comb {a},{b} -> {r}\n"));
    }

    let n = inv::ohpl_inv_item_num();
    let saved: Vec<usize> = (0..n).map(|i| inv::ohpl_inv_item_at(i)).collect();
    let (dis, eq) = (
        inv::ohpl_inv_disabled(),
        inv::ohpl_inv_item_index(inv::ohpl_inv_equipped()),
    );
    let cbs: Vec<[String; 4]> = (0..inv::ohpl_inv_combine_num())
        .map(|i| [0, 1, 2, 3].map(|w| combine_str(i, w)))
        .collect();
    let auto: Vec<i32> = (0..cbs.len() as i32)
        .map(|i| inv::ohpl_inv_combine_auto(i))
        .collect();
    struct Rec {
        str7: [String; 7],
        ty: i32,
        amount: f32,
        count: i32,
    }
    let recs: Vec<Rec> = saved
        .iter()
        .map(|&p| Rec {
            str7: [0, 1, 2, 3, 4, 5, 6].map(|w| item_str(p, w)),
            ty: inv::ohpl_inv_item_type(p),
            amount: inv::ohpl_inv_item_amount(p),
            count: inv::ohpl_inv_item_count(p),
        })
        .collect();
    o.push_str(&format!(
        "save dis={dis} eq={eq} items={} cbs={}\n",
        recs.len(),
        cbs.len()
    ));
    for (i, r) in recs.iter().enumerate() {
        o.push_str(&format!(
            "  s[{i}] {} {} {} {} {} {} {} {} {} {}\n",
            r.str7[0],
            r.ty,
            r.str7[1],
            r.str7[2],
            r.str7[3],
            r.str7[4],
            g6(r.amount),
            r.count,
            r.str7[5],
            r.str7[6]
        ));
    }
    for (i, (c, a)) in cbs.iter().zip(&auto).enumerate() {
        o.push_str(&format!(
            "  c[{i}] {} {} {} {} {a}\n",
            c[0], c[1], c[2], c[3]
        ));
    }

    inv::ohpl_inv_reset();
    items(&mut o, "reset", true);
    inv::ohpl_inv_set_disabled(dis);
    inv::ohpl_inv_set_equipped(if eq > 0 { inv::ohpl_inv_item_at(eq) } else { 0 });
    for r in &recs {
        let p = add_item(
            &r.str7[0],
            r.ty,
            &r.str7[1],
            &r.str7[4],
            r.amount,
            &r.str7[2],
            &r.str7[3],
            &mut remove,
        );
        inv::ohpl_inv_item_set_entries(p, s(&r.str7[5]), s(&r.str7[6]));
        inv::ohpl_inv_item_set_count(p, r.count);
    }
    for (c, a) in cbs.iter().zip(&auto) {
        inv::ohpl_inv_add_combine(s(&c[0]), s(&c[1]), s(&c[2]), s(&c[3]), *a);
    }
    items(&mut o, "restored", false);
    o.push_str(&format!(
        "restored comb={}\n",
        i32::from(inv::ohpl_inv_find_combine(s("puz_b"), s("puz_a")) >= 0)
    ));
    inv::ohpl_inv_remove(by_name("hp_a"));
    inv::ohpl_inv_remove_by_name(s("puz_a"));
    inv::ohpl_inv_remove_by_name(s("oil_a"));
    inv::ohpl_inv_remove_by_name(s("missing"));
    inv::ohpl_inv_remove_of_type(6);
    items(&mut o, "removed", false);
    inv::ohpl_inv_reset();
    inv::ohpl_inv_load_script();
    let _ = file.write_all(o.as_bytes());
}
