//! The journal object (A11c): notes, diaries and quest notes in arenas, the GUI session (widgets, state roots, list
//! pages, hover effects, background), update and draw. The C++ shell (`LuxJournal.h`) forwards its lifecycle; engine
//! objects are reached through the inventory host table in the journal flavour (`invhost::journal_scope`). State is
//! reached through short [`g`] borrows that never span a host call (host ops re-enter through widget callbacks).

use super::event::Str;
use super::invhost::{self as host, A, WStr, game, jg, op, set, wd, wx};
use super::invobj;
use super::journal as j;
use crate::obj::{Arena, Handle};
use std::cell::UnsafeCell;
use std::collections::HashMap;

const MSG_MOUSE_DOWN: i32 = 0;
const MSG_DRAW: i32 = 6;
const MSG_UI_PRESS: i32 = 9;
const MSG_UI_ARROW: i32 = 12;

const ARROW_UP: i32 = 0;
const ARROW_DOWN: i32 = 2;

const MAT_ALPHA: i32 = 0;
const MAT_DIFFUSE: i32 = 2;
const MAT_DEFAULT: i32 = 6;
const ALIGN_LEFT: i32 = 0;
const ALIGN_CENTER: i32 = 2;
const MOUSE_LEFT: i32 = 1;
const INPUT_GAME: i32 = 0;
const INPUT_NULL: i32 = 1;
const INPUT_JOURNAL: i32 = 3;

mod cb {
    pub const MAIN_CLICK: usize = 1;
    pub const MAIN_UI_BUTTON: usize = 2;
    pub const MAIN_DRAW: usize = 3;
    pub const LIST_DRAW: usize = 4;
    pub const NOTE_CLICK: usize = 5;
    pub const NOTE_BACK_CLICK: usize = 6;
    pub const JOURNAL_PRESS: usize = 7;
    pub const DIARY_CLICK: usize = 8;
    pub const IMAGE_DRAW: usize = 9;
    pub const ARROW_CLICK: usize = 10;
    pub const FRAME_CLICK: usize = 11;
    pub const ITEM_PRESS: usize = 12;
    pub const ARROW_PRESS: usize = 13;
    pub const BACK_UI_PRESS: usize = 14;
}

#[derive(Clone, Default)]
pub struct Entry {
    pub name: Vec<u8>,
    pub text: Vec<u8>,
    pub icon: Vec<u8>,
    pub image: Vec<u8>,
}

pub struct Quest {
    pub active: bool,
    pub name: Vec<u8>,
    pub name_entry: Vec<u8>,
    pub text_entry: Vec<u8>,
}

pub type EntryRef = Handle<Entry>;

struct Cont {
    ty: Vec<u8>,
    diaries: Vec<EntryRef>,
}

#[derive(Default)]
struct StateData {
    root: usize,
    alpha: f32,
    session: Vec<usize>,
}

#[derive(Default)]
struct ListPage {
    entries: Vec<Vec<usize>>,
}

#[derive(Clone, Copy, Default)]
struct TextData {
    ty: i32,
    effect: f32,
    extra: u64,
}

#[derive(Clone, Copy, Default)]
struct WData {
    user: i32,
    text: Option<TextData>,
}

pub struct Jrn {
    set: usize,
    screen: [f32; 2],
    set_size: [f32; 2],
    start_pos: [f32; 3],
    text_width: f32,
    max_rows: i32,
    note_font: [f32; 2],
    row_dist: f32,
    header_font: [f32; 2],
    header_y: f32,
    text_start_y: f32,
    notes: Arena<Entry>,
    note_order: Vec<EntryRef>,
    diaries: Arena<Entry>,
    conts: Vec<Cont>,
    quests: Vec<Quest>,
    active: bool,
    alpha: f32,
    force_instant: bool,
    from_inventory: bool,
    last_cat: i32,
    last_entry: i32,
    last_type: i32,
    header: Vec<u32>,
    pages: j::Pages,
    cur_page: i32,
    voice: usize,
    voice_id: i32,
    lists: [Vec<ListPage>; 3],
    cur_list: [i32; 3],
    state: i32,
    st: [StateData; 7],
    wdata: HashMap<usize, WData>,
    wdata_order: Vec<usize>,
    default_nav: [usize; 7],
    img_fwd: [usize; 7],
    img_back: [usize; 7],
    ret: [usize; 7],
    state_bg: usize,
    pulse: f32,
    screen_tex: usize,
    screen_gfx: usize,
    bg_tex: usize,
    bg_gfx: usize,
    white: usize,
    font_default: usize,
    font_menu: usize,
}

struct Global(UnsafeCell<Option<Box<Jrn>>>);

// SAFETY: the game logic runs on one thread.
unsafe impl Sync for Global {}

static JRN: Global = Global(UnsafeCell::new(None));

/// A short borrow of the journal; never keep it across a host call.
fn g() -> Option<&'static mut Jrn> {
    // SAFETY: single game thread; callers keep the borrow within one statement block without host calls.
    unsafe { (*JRN.0.get()).as_deref_mut() }
}

fn install(i: Box<Jrn>) {
    // SAFETY: single game thread, no live borrow.
    unsafe { *JRN.0.get() = Some(i) };
}

fn take() -> Option<Box<Jrn>> {
    // SAFETY: single game thread, no live borrow.
    unsafe { (*JRN.0.get()).take() }
}

fn at<T: Copy>(a: &[T], i: i32, d: T) -> T {
    usize::try_from(i)
        .ok()
        .and_then(|i| a.get(i))
        .copied()
        .unwrap_or(d)
}

fn put<T>(a: &mut [T], i: i32, v: T) {
    if let Some(s) = usize::try_from(i).ok().and_then(|i| a.get_mut(i)) {
        *s = v;
    }
}

fn s(a: &mut A, idx: usize, v: &[u8]) {
    if let Some(slot) = a.s.get_mut(idx) {
        *slot = Str::of(v);
    }
}

fn add_cb(w: usize, msg: i32, kind: usize) {
    host::widget_callback(w, msg, kind);
}

fn nav(w: usize, arrow: i32, to: usize) {
    host::widget_set(w, wd::SET_NAV, &[], arrow, to);
}

fn sound(name: &[u8]) {
    let mut a = A::new();
    a.i[0] = host::ev::SOUND;
    s(&mut a, 0, name);
    host::call(op::ITEM_EVENT, &mut a);
}

fn set_op(kind: i32, v: i32) {
    host::set_flag(kind, v);
}

fn new_state() -> Jrn {
    Jrn {
        set: 0,
        screen: [0.0; 2],
        set_size: [0.0; 2],
        start_pos: [0.0; 3],
        text_width: 0.0,
        max_rows: 0,
        note_font: [0.0; 2],
        row_dist: 0.0,
        header_font: [0.0; 2],
        header_y: 0.0,
        text_start_y: 0.0,
        notes: Arena::default(),
        note_order: Vec::new(),
        diaries: Arena::default(),
        conts: Vec::new(),
        quests: Vec::new(),
        active: false,
        alpha: 0.0,
        force_instant: false,
        from_inventory: false,
        last_cat: 0,
        last_entry: -1,
        last_type: -1,
        header: Vec::new(),
        pages: j::Pages::default(),
        cur_page: 0,
        voice: 0,
        voice_id: 0,
        lists: [Vec::new(), Vec::new(), Vec::new()],
        cur_list: [0; 3],
        state: j::STATE_MAIN,
        st: Default::default(),
        wdata: HashMap::new(),
        wdata_order: Vec::new(),
        default_nav: [0; 7],
        img_fwd: [0; 7],
        img_back: [0; 7],
        ret: [0; 7],
        state_bg: 0,
        pulse: 0.0,
        screen_tex: 0,
        screen_gfx: 0,
        bg_tex: 0,
        bg_gfx: 0,
        white: 0,
        font_default: 0,
        font_menu: 0,
    }
}

// ---- lifecycle ------------------------------------------------------------------------------

/// Constructor of the shell: GUI set, viewport, settings, state roots, white gfx and the screen effect program.
pub fn create() {
    let _f = host::journal_scope();
    let mut a = A::new();
    host::call(op::GUI_INIT, &mut a);
    let mut i = new_state();
    i.set = a.r;
    i.set_size = [a.f[0], a.f[1]];
    i.start_pos = [-a.f[2], -a.f[3], 0.0];
    i.screen = [a.f[4], a.f[5]];
    i.text_width = host::cfg_float(b"NoteTextWidth", 0.0);
    i.max_rows = host::cfg_int(b"NoteMaxPageRows", 0);
    i.note_font = host::cfg_v2(b"NoteFontSize", [0.0; 2]);
    i.row_dist = host::cfg_float(b"NoteRowDist", 0.0);
    i.header_font = host::cfg_v2(b"NoteHeaderFontSize", [0.0; 2]);
    i.header_y = host::cfg_float(b"NoteHeaderStartY", 0.0);
    i.text_start_y = host::cfg_float(b"NoteTextStartY", 0.0);
    install(Box::new(i));
    for n in 0..j::STATE_COUNT {
        let root = host::widget_dummy([0.0; 3], 0);
        if let Some(st) = g().and_then(|i| usize::try_from(n).ok().and_then(|n| i.st.get_mut(n))) {
            st.root = root;
            st.alpha = 0.0;
        }
    }
    let white = host::gfx_rect([1.0, 1.0, 1.0, 1.0], MAT_ALPHA);
    let mut p = A::new();
    host::ll(host::ll::PROGRAM_NEW, &mut p);
    if let Some(i) = g() {
        i.white = white;
    }
}

pub fn destroy() {
    take();
}

pub fn load_fonts() {
    let _f = host::journal_scope();
    let font = |key: &[u8]| {
        let mut a = A::new();
        s(&mut a, 0, key);
        host::call(op::FONT_LOAD, &mut a);
        a.r
    };
    let d = font(b"DefaultFont");
    let m = font(b"MenuFont");
    if let Some(i) = g() {
        i.font_default = d;
        i.font_menu = m;
    }
}

pub fn reset() {
    let _f = host::journal_scope();
    if let Some(i) = g() {
        i.alpha = 0.0;
        i.active = false;
        i.pulse = 0.0;
        i.state_bg = 0;
        i.force_instant = false;
        i.from_inventory = false;
        i.last_entry = -1;
        i.last_type = -1;
        i.voice = 0;
        i.img_fwd = [0; 7];
        i.img_back = [0; 7];
        i.cur_list = [0; 3];
        i.notes = Arena::default();
        i.note_order.clear();
        i.quests.clear();
        i.diaries = Arena::default();
        i.conts.clear();
    }
    reset_session_vars();
}

fn reset_session_vars() {
    set_state_bg(b"");
    if let Some(i) = g() {
        i.state = j::STATE_MAIN;
        i.cur_page = 0;
        for st in &mut i.st {
            st.alpha = 0.0;
        }
    }
}

pub fn set_force_instant_exit(on: bool) {
    if let Some(i) = g() {
        i.force_instant = on;
    }
}

pub fn set_opened_from_inventory(on: bool) {
    if let Some(i) = g() {
        i.from_inventory = on;
    }
}

pub fn get_set() -> usize {
    g().map_or(0, |i| i.set)
}

pub fn update(dt: f32) {
    let _f = host::journal_scope();
    let Some((exit, active)) = g().map(|i| {
        (
            j::update_alpha(&mut i.alpha, i.active, i.from_inventory, dt),
            i.active,
        )
    }) else {
        return;
    };
    match exit {
        j::EXIT_TO_INVENTORY => {
            invobj::set_enter_from_journal(true);
            host::game_s(game::SET_CONTAINER, b"Inventory");
        }
        j::EXIT_TO_DEFAULT => {
            host::game_s(game::SET_CONTAINER, b"Default");
            host::game_i(game::INPUT_STATE, INPUT_GAME);
        }
        _ => {}
    }
    if let Some(i) = g() {
        i.pulse += dt;
    }
    for n in 0..j::STATE_COUNT {
        update_state(n, dt);
    }
    let order = g().map(|i| i.wdata_order.clone()).unwrap_or_default();
    for w in order {
        let over = host::wx(wx::HOVER_OR_FOCUS, w).r != 0;
        if let Some(t) = g()
            .and_then(|i| i.wdata.get_mut(&w))
            .and_then(|d| d.text.as_mut())
        {
            t.effect = j::hover_alpha(t.effect, over, dt);
        }
    }
    let (state, voice, id) = g().map_or((0, 0, 0), |i| (i.state, i.voice, i.voice_id));
    if state == j::STATE_OPEN_NARRATED && active {
        let valid = voice != 0 && voice_valid(voice, id);
        if j::narration_done(state, active, valid) {
            if let Some(i) = g() {
                i.voice = 0;
            }
            exit_pressed(false);
        }
    }
    host::game_f(game::FLASH_UPDATE, dt);
}

fn voice_valid(entry: usize, id: i32) -> bool {
    let mut a = A::new();
    a.i[0] = jg::VOICE_VALID;
    a.i[1] = id;
    a.p[0] = entry;
    host::call(op::JGAME, &mut a);
    a.r != 0
}

fn voice_fade(entry: usize) {
    let mut a = A::new();
    a.i[0] = jg::VOICE_FADE;
    a.p[0] = entry;
    host::call(op::JGAME, &mut a);
}

fn update_state(n: i32, dt: f32) {
    let Some((alpha, session, jalpha)) = g().and_then(|i| {
        let cur = i.state;
        let st = usize::try_from(n).ok().and_then(|n| i.st.get_mut(n))?;
        st.alpha = j::state_alpha(st.alpha, n == cur, dt);
        Some((st.alpha, st.session.clone(), i.alpha))
    }) else {
        return;
    };
    for w in session {
        let mut c = host::widget_get(w, wd::GET_COLOR_MUL).f;
        c[3] = jalpha * alpha;
        host::widget_set(w, wd::SET_COLOR_MUL, &c[..4], 0, 0);
    }
}

pub fn on_enter() {
    let _f = host::journal_scope();
    create_background();
    create_gui();
    if let Some(i) = g() {
        i.active = true;
    }
    host::game_i(game::INPUT_STATE, INPUT_JOURNAL);
    set_op(set::VIEWPORT, 1);
    set_op(set::ACTIVE, 1);
    let present = host::set_query(set::GAMEPAD_PRESENT).r != 0;
    set_op(set::DRAW_MOUSE, i32::from(!present));
    set_op(set::MOUSE_MOVEMENT, i32::from(!present));
    host::call(op::SET_OP, &mut set_arg(set::RESET_MOUSE_OVER, 0));
    set_op(set::GUI_FOCUS, 1);
    sound(b"journal_open");
    host::game(game::FADE_LOOPS);
    host::game(game::STOP_TERROR);
    if !g().is_some_and(|i| i.from_inventory) {
        host::game_i(game::VOICES, 1);
    }
    reset_session_vars();
    let state = g().map_or(0, |i| i.state);
    let bg = j::background(state);
    if !bg.is_empty() {
        set_state_bg(bg);
    }
}

fn set_arg(kind: i32, v: i32) -> A {
    let mut a = A::new();
    a.i[0] = kind;
    a.i[1] = v;
    a
}

pub fn on_leave() {
    let _f = host::journal_scope();
    if !g().is_some_and(|i| i.from_inventory) {
        host::game_i(game::VOICES, 0);
    }
    set_op(set::VIEWPORT, 0);
    set_op(set::ACTIVE, 0);
    destroy_gui();
    destroy_background();
    if let Some(i) = g() {
        i.force_instant = false;
        i.from_inventory = false;
    }
}

pub fn on_draw(frame_time: f32) {
    let _f = host::journal_scope();
    let Some((sg, bgg, white, alpha, start, size, from_inv)) = g().map(|i| {
        (
            i.screen_gfx,
            i.bg_gfx,
            i.white,
            i.alpha,
            i.start_pos,
            i.set_size,
            i.from_inventory,
        )
    }) else {
        return;
    };
    if sg != 0 && alpha < 1.0 {
        host::draw_gfx(sg, start, size, [1.0, 1.0, 1.0, 1.0], MAT_DEFAULT);
    }
    if bgg != 0 {
        host::draw_gfx(
            bgg,
            [start[0] + 0.0, start[1] + 0.0, start[2] + 0.2],
            size,
            [1.0, 1.0, 1.0, alpha],
            MAT_DEFAULT,
        );
    }
    for n in 0..j::STATE_COUNT {
        if g().is_some_and(|i| state_alpha_of(i, n) > 0.0) {
            draw_state(n);
        }
    }
    if from_inv {
        host::draw_gfx(
            white,
            [start[0] + 0.0, start[1] + 0.0, start[2] + 20.0],
            size,
            [0.0, 0.0, 0.0, j::fade_alpha(alpha)],
            MAT_DEFAULT,
        );
    }
    host::game_f(game::FLASH_DRAW, frame_time);
}

fn state_alpha_of(i: &Jrn, n: i32) -> f32 {
    usize::try_from(n)
        .ok()
        .and_then(|n| i.st.get(n))
        .map_or(0.0, |s| s.alpha)
}

fn draw_state(n: i32) {
    let Some((salpha, jalpha, header, pages, cur_state, bg)) = g().map(|i| {
        (
            state_alpha_of(i, n),
            i.alpha,
            i.header.clone(),
            i.pages.pages.len(),
            i.state,
            i.state_bg,
        )
    }) else {
        return;
    };
    let alpha = salpha * jalpha;
    if j::is_open_state(n) {
        if pages == 0 {
            return;
        }
        let Some((hy, hf, fm, fd, nf, tw, tsy, rd, rows)) = g().map(|i| {
            let rows = usize::try_from(i.cur_page)
                .ok()
                .and_then(|p| i.pages.pages.get(p))
                .map(|p| p.rows.clone());
            (
                i.header_y,
                i.header_font,
                i.font_menu,
                i.font_default,
                i.note_font,
                i.text_width,
                i.text_start_y,
                i.row_dist,
                rows,
            )
        }) else {
            return;
        };
        host::draw_font(
            fm,
            &header,
            [400.0, hy, 3.0],
            hf,
            [1.0, 1.0, 1.0, alpha],
            ALIGN_CENTER,
        );
        if let Some(rows) = rows {
            let mut pos = [400.0 - tw / 2.0, tsy, 3.0];
            pos[1] = j::text_page_start_y(rows.len(), rd);
            for row in &rows {
                host::draw_font(fd, row, pos, nf, [1.0, 1.0, 1.0, alpha], ALIGN_LEFT);
                pos[1] += rd;
            }
        }
    }
    if bg != 0 && cur_state == n {
        let sz = host::gfx_sizes(bg).0;
        host::draw_gfx(
            bg,
            [400.0 - sz[0] / 2.0, 300.0 - sz[1] / 2.0, 1.0],
            [-1.0, -1.0],
            [1.0, 1.0, 1.0, alpha],
            MAT_DEFAULT,
        );
    }
}

// ---- exit and state -------------------------------------------------------------------------

pub fn exit_pressed(instant: bool) {
    let _f = host::journal_scope();
    let Some((active, force, state, page, pages)) = g().map(|i| {
        (
            i.active,
            i.force_instant,
            i.state,
            i.cur_page,
            i.pages.pages.len(),
        )
    }) else {
        return;
    };
    let (action, new) = j::exit_pressed(active, instant, force, state, page, pages);
    match action {
        j::PRESS_NEXT_PAGE => set_note_page(new),
        j::PRESS_EXIT_HINT => {
            exit();
            sound(b"journal_close");
            host::jgame(jg::HINT_RECENT);
        }
        j::PRESS_EXIT => {
            exit();
            sound(b"journal_close");
        }
        j::PRESS_CHANGE_STATE => change_state(new),
        _ => {}
    }
}

pub fn exit() {
    let _f = host::journal_scope();
    let Some((active, force, voice, id)) =
        g().map(|i| (i.active, i.force_instant, i.voice, i.voice_id))
    else {
        return;
    };
    if !active {
        return;
    }
    if force {
        let mut a = A::new();
        a.i[0] = jg::PROG_LOW;
        s(
            &mut a,
            0,
            b"Forced Instant Exit in Journal (player finished reading).",
        );
        host::call(op::JGAME, &mut a);
    }
    if voice != 0 && voice_valid(voice, id) {
        voice_fade(voice);
    }
    if let Some(i) = g() {
        i.voice = 0;
    }
    host::game_i(game::INPUT_STATE, INPUT_NULL);
    set_op(set::DRAW_MOUSE, 0);
    set_op(set::GUI_FOCUS, 0);
    if let Some(i) = g() {
        i.active = false;
    }
}

pub fn change_state(state: i32) {
    let _f = host::journal_scope();
    let bg = j::background(state);
    if let Some(i) = g() {
        i.state = state;
    }
    if !bg.is_empty() {
        set_state_bg(bg);
    }
    let roots: Vec<usize> = g()
        .map(|i| i.st.iter().map(|s| s.root).collect())
        .unwrap_or_default();
    for (n, root) in roots.iter().enumerate() {
        host::widget_visible(*root, n as i32 == state);
    }
    on_enter_state(state);
}

fn on_enter_state(state: i32) {
    if state == j::STATE_DIARIES || state == j::STATE_NOTES || state == j::STATE_QUEST_LOG {
        let idx = j::list_index(state);
        let page = g().map_or(0, |i| at(&i.cur_list, idx, 0));
        set_note_list_page(page, state);
    }
    let nav = g().map_or(0, |i| at(&i.default_nav, state, 0));
    set_widget(set::DEFAULT_NAV, nav);
    set_widget(set::FOCUSED_WIDGET, nav);
}

fn set_widget(kind: i32, w: usize) {
    host::set_widget(kind, w);
}

fn set_state_bg(file: &[u8]) {
    let old = g().map_or(0, |i| std::mem::take(&mut i.state_bg));
    if old != 0 {
        host::gfx_destroy(old);
    }
    if !file.is_empty() {
        let n = host::gfx_file(file);
        if let Some(i) = g() {
            i.state_bg = n;
        }
    }
}

// ---- notes, diaries, quests -----------------------------------------------------------------

fn translate(key: &[u8]) -> Vec<u32> {
    host::translate(b"Journal", key)
}

fn game_name() -> Vec<u8> {
    host::jgame(jg::GAME_NAME).ret_bytes()
}

fn main_bool(key: &[u8]) -> bool {
    let mut a = A::new();
    a.i[0] = jg::MAIN_BOOL_GET;
    s(&mut a, 0, key);
    host::call(op::JGAME, &mut a);
    a.r != 0
}

fn main_bool_save(key: &[u8]) {
    let mut a = A::new();
    a.i[0] = jg::MAIN_BOOL_SAVE;
    s(&mut a, 0, key);
    host::call(op::JGAME, &mut a);
}

pub fn add_note(entry: &[u8], image: &[u8]) -> Option<EntryRef> {
    let _f = host::journal_scope();
    let (name, text) = j::note_entries(entry);
    let (large, icon) = j::image_files(image);
    let (h, count) = {
        let i = g()?;
        let h = i.notes.insert(Entry {
            name,
            text,
            icon,
            image: large,
        })?;
        i.note_order.push(h);
        (h, i.note_order.len())
    };
    match j::archivist(&game_name(), count) {
        j::ARCHIVIST_TDD => {
            if main_bool(b"MasterArchivis_Justine") {
                host::jgame(jg::ACH_ARCHIVIST);
            }
            main_bool_save(b"MasterArchivis_TDD");
        }
        j::ARCHIVIST_JUSTINE => {
            if main_bool(b"MasterArchivis_TDD") {
                host::jgame(jg::ACH_ARCHIVIST);
            }
            main_bool_save(b"MasterArchivis_Justine");
        }
        _ => {}
    }
    Some(h)
}

/// `AddDiary`: the diary and its 1-based number inside its container.
pub fn add_diary(entry: &[u8], image: &[u8]) -> Option<(EntryRef, i32)> {
    let i = g()?;
    let ci = match i.conts.iter().position(|c| c.ty == entry) {
        Some(p) => p,
        None => {
            i.conts.push(Cont {
                ty: entry.to_vec(),
                diaries: Vec::new(),
            });
            i.conts.len() - 1
        }
    };
    let num = i.conts.get(ci)?.diaries.len() as i32 + 1;
    let (name, text) = j::diary_entries(entry, num);
    let (large, icon) = j::image_files(image);
    let h = i.diaries.insert(Entry {
        name,
        text,
        icon,
        image: large,
    })?;
    i.conts.get_mut(ci)?.diaries.push(h);
    Some((h, num))
}

pub fn add_quest(name: &[u8], entry: &[u8]) -> bool {
    let Some(i) = g() else { return false };
    if i.quests.iter().any(|q| q.name == name) {
        return false;
    }
    let (name_entry, text_entry) = j::quest_entries(entry);
    i.quests.push(Quest {
        active: true,
        name: name.to_vec(),
        name_entry,
        text_entry,
    });
    true
}

pub fn disable_quest(name: &[u8]) -> bool {
    let Some(i) = g() else { return false };
    let q = i.quests.iter_mut().find(|q| q.name == name);
    match j::disable_quest(q.is_some(), q.as_ref().is_some_and(|q| q.active)) {
        j::QUEST_DISABLED => {
            if let Some(q) = q {
                q.active = false;
            }
            true
        }
        j::QUEST_ALREADY_OFF => false,
        _ => {
            let msg = [b"Quest '", name, b"' does not exist!\n"].concat();
            host::log(0, &msg);
            false
        }
    }
}

/// -1 unknown quest, 0 disabled, 1 active.
pub fn quest_state(name: &[u8]) -> i32 {
    g().and_then(|i| i.quests.iter().find(|q| q.name == name))
        .map_or(-1, |q| i32::from(q.active))
}

fn entry_of(note: bool, h: u64) -> Option<(Entry, EntryRef)> {
    let h = EntryRef::from_bits(h)?;
    let i = g()?;
    let arena = if note { &i.notes } else { &i.diaries };
    arena.get(h).cloned().map(|e| (e, h))
}

pub fn open_note(h: u64, narration: bool) {
    let _f = host::journal_scope();
    let Some((e, h)) = entry_of(true, h) else {
        return;
    };
    if let Some(i) = g() {
        i.last_type = j::LAST_NOTE;
        i.last_cat = -1;
        let items: Vec<usize> = i.note_order.iter().map(|h| h.to_bits() as usize).collect();
        i.last_entry = j::find_index(&items, h.to_bits() as usize);
    }
    open_text(&e, narration, j::STATE_OPEN_NOTE);
}

pub fn open_diary(h: u64, narration: bool) {
    let _f = host::journal_scope();
    let Some((e, _)) = entry_of(false, h) else {
        return;
    };
    set_diary_as_last_read(h);
    open_text(&e, narration, j::STATE_OPEN_DIARY);
}

fn open_text(e: &Entry, narration: bool, plain: i32) {
    let name = translate(&e.name);
    let text = translate(&e.text);
    if narration {
        change_state(j::STATE_OPEN_NARRATED);
        load_narration(&name, &text);
    } else {
        change_state(plain);
        load_text(&name, &text);
    }
    set_state_bg(&e.image);
}

pub fn set_diary_as_last_read(h: u64) {
    if let Some(i) = g() {
        i.last_type = j::LAST_DIARY;
        let counts: Vec<usize> = i.conts.iter().map(|c| c.diaries.len()).collect();
        let items: Vec<usize> = i
            .conts
            .iter()
            .flat_map(|c| c.diaries.iter().map(|h| h.to_bits() as usize))
            .collect();
        let (cat, entry) = j::find_diary(&counts, &items, h as usize);
        i.last_cat = cat;
        i.last_entry = entry;
    }
}

pub fn open_last_read() {
    let _f = host::journal_scope();
    let Some((kind, cat, entry, notes, counts)) = g().map(|i| {
        (
            i.last_type,
            i.last_cat,
            i.last_entry,
            i.note_order.len(),
            i.conts.iter().map(|c| c.diaries.len()).collect::<Vec<_>>(),
        )
    }) else {
        return;
    };
    match j::last_read(kind, cat, entry, notes, &counts) {
        1 => {
            set_force_instant_exit(true);
            host::game_s(game::SET_CONTAINER, b"Journal");
            if let Some(h) = g().and_then(|i| {
                usize::try_from(entry)
                    .ok()
                    .and_then(|e| i.note_order.get(e).copied())
            }) {
                open_note(h.to_bits(), false);
            }
        }
        2 => {
            set_force_instant_exit(true);
            host::game_s(game::SET_CONTAINER, b"Journal");
            let h = g().and_then(|i| {
                let c = i.conts.get(usize::try_from(cat).ok()?)?;
                c.diaries.get(usize::try_from(entry).ok()?).copied()
            });
            if let Some(h) = h {
                open_diary(h.to_bits(), false);
            }
        }
        _ => {}
    }
}

pub fn open_note_idx(idx: i32, narration: bool) {
    let h = g().and_then(|i| {
        usize::try_from(idx)
            .ok()
            .and_then(|n| i.note_order.get(n).copied())
    });
    if let Some(h) = h {
        open_note(h.to_bits(), narration);
    }
}

pub fn open_diary_idx(cat: i32, idx: i32, narration: bool) {
    let h = g().and_then(|i| {
        let c = i.conts.get(usize::try_from(cat).ok()?)?;
        c.diaries.get(usize::try_from(idx).ok()?).copied()
    });
    if let Some(h) = h {
        open_diary(h.to_bits(), narration);
    }
}

// ---- pages ----------------------------------------------------------------------------------

fn wrap_rows(font: usize, width: f32, line: f32, size: f32, text: &[u32]) -> Vec<Vec<u32>> {
    let mut w = A::new();
    w.p[0] = font;
    w.f[0] = width;
    w.f[1] = line;
    w.f[2] = size;
    w.w[0] = WStr::of(text);
    host::call(op::FONT_WRAP, &mut w);
    let n = usize::try_from(w.i[0]).unwrap_or(0);
    (0..n)
        .map(|r| {
            let mut a = A::new();
            a.i[0] = r as i32;
            host::call(op::FONT_ROW, &mut a);
            a.ret_wide()
        })
        .collect()
}

fn load_text(name: &[u32], text: &[u32]) {
    let Some((font, width, max_rows)) = g().map(|i| (i.font_default, i.text_width, i.max_rows))
    else {
        return;
    };
    if let Some(i) = g() {
        i.header = name.to_vec();
    }
    let rows = wrap_rows(font, width, 20.0, 20.0, text);
    if let Some(i) = g() {
        i.pages.load_text(&rows, max_rows);
    }
    set_note_page(0);
}

fn load_narration(name: &[u32], text: &[u32]) {
    let Some((font, width)) = g().map(|i| (i.font_default, i.text_width)) else {
        return;
    };
    if let Some(i) = g() {
        i.header = name.to_vec();
    }
    let rows = wrap_rows(font, width, 20.0, 20.0, text);
    let errors = g()
        .map(|i| i.pages.load_narration(&rows))
        .unwrap_or_default();
    for (row, n) in errors {
        let msg = [
            b"Voice lines '".as_slice(),
            &row,
            b"' has bad syntax! Found ",
            n.to_string().as_bytes(),
            b" arguments!\n",
        ]
        .concat();
        host::log(0, &msg);
    }
    set_note_page(0);
}

fn set_widget_shown(w: usize, on: bool) {
    if w != 0 {
        host::widget_enabled(w, on);
        host::widget_visible(w, on);
    }
}

fn set_note_page(page: i32) {
    let Some((state, pages)) = g().map(|i| {
        i.cur_page = page;
        (i.state, i.pages.pages.len())
    }) else {
        return;
    };
    if state == j::STATE_OPEN_NARRATED {
        if j::narrated_page_plays(page, pages) {
            let (voice, id, file) = g().map_or((0, 0, Vec::new()), |i| {
                (
                    i.voice,
                    i.voice_id,
                    usize::try_from(page)
                        .ok()
                        .and_then(|p| i.pages.pages.get(p))
                        .map(|p| p.voice.clone())
                        .unwrap_or_default(),
                )
            });
            if voice != 0 && voice_valid(voice, id) {
                voice_fade(voice);
            }
            let mut a = A::new();
            a.i[0] = jg::VOICE_PLAY;
            s(&mut a, 0, &file);
            host::call(op::JGAME, &mut a);
            if let Some(i) = g() {
                i.voice = a.r;
                if a.r != 0 {
                    i.voice_id = a.i[1];
                }
            }
        }
        return;
    }
    let (back, fwd) = j::note_arrows(page, pages);
    let (ib, ifw) = g().map_or((0, 0), |i| {
        (at(&i.img_back, state, 0), at(&i.img_fwd, state, 0))
    });
    set_widget_shown(ib, back);
    set_widget_shown(ifw, fwd);
}

fn set_note_list_page(page: i32, state: i32) {
    let idx = j::list_index(state);
    let Some((pages, ib, ifw, ret)) = g().map(|i| {
        put(&mut i.cur_list, idx, page);
        (
            usize::try_from(idx)
                .ok()
                .and_then(|n| i.lists.get(n))
                .map_or(0, Vec::len),
            at(&i.img_back, state, 0),
            at(&i.img_fwd, state, 0),
            at(&i.ret, state, 0),
        )
    }) else {
        return;
    };
    let r = j::set_list_page(&j::ListPageIn { page, pages, state });
    if r.no_pages {
        set_widget_shown(ib, false);
        set_widget_shown(ifw, false);
        if let Some(i) = g() {
            put(&mut i.default_nav, state, ret);
        }
        return;
    }
    let all: Vec<Vec<usize>> = g()
        .and_then(|i| usize::try_from(idx).ok().and_then(|n| i.lists.get(n)))
        .map(|l| {
            l.iter()
                .map(|p| p.entries.iter().flatten().copied().collect())
                .collect()
        })
        .unwrap_or_default();
    for (n, ws) in all.iter().enumerate() {
        let on = page as i64 == n as i64;
        for w in ws {
            host::widget_enabled(*w, on);
            host::widget_visible(*w, on);
        }
    }
    let first = g()
        .and_then(|i| {
            let l = i.lists.get(usize::try_from(idx).ok()?)?;
            let p = l.get(usize::try_from(page).ok()?)?;
            p.entries.first()?.first().copied()
        })
        .unwrap_or(0);
    let nav = if r.default_is_return { ret } else { first };
    if let Some(i) = g() {
        put(&mut i.default_nav, state, nav);
    }
    set_widget(set::DEFAULT_NAV, nav);
    set_widget(set::FOCUSED_WIDGET, nav);
    set_widget_shown(ib, r.back);
    set_widget_shown(ifw, r.forward);
}

// ---- GUI session ----------------------------------------------------------------------------

fn add_session(state: i32, w: usize) {
    if let Some(st) = g().and_then(|i| usize::try_from(state).ok().and_then(|n| i.st.get_mut(n))) {
        st.session.push(w);
    }
}

fn register(w: usize, data: WData) {
    if let Some(i) = g() {
        i.wdata.insert(w, data);
        if data.text.is_some() {
            i.wdata_order.push(w);
        }
    }
}

fn setup_label(label: usize, size: [f32; 2], idx: i32, state: i32, font: usize, align: i32) {
    let font = if font == 0 {
        g().map_or(0, |i| i.font_menu)
    } else {
        font
    };
    let mut a = A::new();
    a.i[0] = wx::SETUP_LABEL;
    a.p[0] = label;
    a.p[1] = font;
    a.i[1] = align;
    a.f[..4].copy_from_slice(&[1.0; 4]);
    a.f[4..6].copy_from_slice(&size);
    host::call(op::WIDGET_X, &mut a);
    add_session(state, label);
    register(
        label,
        WData {
            user: idx,
            text: Some(TextData {
                ty: state,
                effect: 0.0,
                extra: 0,
            }),
        },
    );
}

fn setup_image(image: usize, idx: i32, state: i32) {
    add_session(state, image);
    register(
        image,
        WData {
            user: idx,
            text: Some(TextData {
                ty: state,
                effect: 0.0,
                extra: 0,
            }),
        },
    );
}

fn label(pos: [f32; 3], size: [f32; 2], key: &[u8], root: usize) -> usize {
    host::widget_label(pos, size, &translate(key), root)
}

fn create_gui() {
    create_main_gui();
    create_notes_gui();
    create_diaries_gui();
    create_open_note_gui();
    create_quest_gui();
    change_state(j::STATE_MAIN);
}

fn destroy_gui() {
    for n in 0..j::STATE_COUNT {
        let session = g()
            .and_then(|i| usize::try_from(n).ok().and_then(|n| i.st.get_mut(n)))
            .map(|st| std::mem::take(&mut st.session))
            .unwrap_or_default();
        for w in &session {
            let mut a = A::new();
            a.i[0] = wx::DESTROY;
            a.p[0] = *w;
            host::call(op::WIDGET_X, &mut a);
        }
        if let Some(i) = g() {
            for w in &session {
                i.wdata.remove(w);
            }
        }
    }
    if let Some(i) = g() {
        let live: Vec<usize> = i
            .wdata_order
            .iter()
            .copied()
            .filter(|w| i.wdata.contains_key(w))
            .collect();
        i.wdata_order = live;
    }
    set_state_bg(b"");
}

fn root_of(state: i32) -> usize {
    g().map_or(0, |i| {
        usize::try_from(state)
            .ok()
            .and_then(|n| i.st.get(n))
            .map_or(0, |s| s.root)
    })
}

type MainLabel = (&'static [u8], [f32; 3], [f32; 2], i32);

fn create_main_gui() {
    let root = root_of(j::STATE_MAIN);
    let (back_y, back_font) = (j::BACK_TEXT_Y, [j::BACK_TEXT_FONT; 2]);
    let spec: [MainLabel; 4] = [
        (b"Notes", [300.0, 200.0, 3.0], [30.0; 2], 0),
        (b"Diaries", [300.0, 250.0, 3.0], [30.0; 2], 1),
        (b"Quest Log", [300.0, 300.0, 3.0], [30.0; 2], 2),
        (b"MainBack", [300.0, back_y, 3.0], back_font, 3),
    ];
    let mut labels = Vec::new();
    for (key, pos, font, idx) in spec {
        let l = label(pos, [200.0, 25.0], key, root);
        setup_label(l, font, idx, j::STATE_MAIN, 0, ALIGN_CENTER);
        add_cb(l, MSG_MOUSE_DOWN, cb::MAIN_CLICK);
        add_cb(l, MSG_DRAW, cb::MAIN_DRAW);
        if idx == 0
            && let Some(i) = g()
        {
            put(&mut i.default_nav, j::STATE_MAIN, l);
        }
        labels.push(l);
    }
    for (n, w) in labels.iter().enumerate() {
        add_cb(*w, MSG_MOUSE_DOWN, cb::MAIN_CLICK);
        add_cb(*w, MSG_UI_PRESS, cb::MAIN_UI_BUTTON);
        add_cb(*w, MSG_DRAW, cb::MAIN_DRAW);
        if let Some(prev) = n.checked_sub(1).and_then(|p| labels.get(p)) {
            nav(*w, ARROW_UP, *prev);
        }
        if let Some(next) = labels.get(n + 1) {
            nav(*w, ARROW_DOWN, *next);
        }
    }
    if let (Some(first), Some(i)) = (labels.first(), g()) {
        put(&mut i.default_nav, j::STATE_MAIN, *first);
    }
}

fn link_entries(list: usize) {
    let pages: Vec<Vec<usize>> = g()
        .and_then(|i| i.lists.get(list))
        .map(|l| {
            l.iter()
                .map(|p| {
                    p.entries
                        .iter()
                        .filter_map(|e| e.first().copied())
                        .collect()
                })
                .collect()
        })
        .unwrap_or_default();
    for p in pages {
        for (n, w) in p.iter().enumerate() {
            if let Some(prev) = n.checked_sub(1).and_then(|k| p.get(k)) {
                nav(*w, ARROW_UP, *prev);
            }
            if let Some(next) = p.get(n + 1) {
                nav(*w, ARROW_DOWN, *next);
            }
        }
    }
}

fn header_label(root: usize, key: &[u8], state: i32) {
    let y = j::NOTE_LIST_HEADER_Y;
    let l = label([250.0, y, 3.0], [300.0, 32.0], key, root);
    setup_label(l, [28.0; 2], 0, state, 0, ALIGN_CENTER);
}

fn empty_label(root: usize, pos: [f32; 3], key: &[u8], state: i32) {
    let font = g().map_or(0, |i| i.font_default);
    let l = label(pos, [300.0, 24.0], key, root);
    setup_label(l, [24.0; 2], 0, state, font, ALIGN_CENTER);
}

fn add_entry(list: usize, page: usize, widgets: Vec<usize>) {
    if let Some(p) = g()
        .and_then(|i| i.lists.get_mut(list))
        .and_then(|l| l.get_mut(page))
    {
        p.entries.push(widgets);
    }
}

fn start_list(list: usize, pages: usize) {
    if let Some(l) = g().and_then(|i| i.lists.get_mut(list)) {
        l.clear();
        l.extend((0..pages).map(|_| ListPage::default()));
    }
}

fn clear_list(list: usize) {
    if let Some(l) = g().and_then(|i| i.lists.get_mut(list)) {
        l.clear();
    }
}

fn finish_list(list: usize, state: i32) {
    setup_navigation(
        state,
        if list < 3 { list as i32 } else { -1 },
        2,
        3,
        root_of(state),
    );
    if let Some(i) = g() {
        let pages = i.lists.get(list).map_or(0, Vec::len);
        if let Some(c) = i.cur_list.get_mut(list) {
            *c = j::start_page(pages);
        }
    }
}

fn create_notes_gui() {
    let root = root_of(j::STATE_NOTES);
    let state = j::STATE_NOTES;
    clear_list(0);
    header_label(root, b"Notes", state);
    let notes: Vec<Entry> = g()
        .map(|i| {
            i.note_order
                .iter()
                .filter_map(|h| i.notes.get(*h).cloned())
                .collect()
        })
        .unwrap_or_default();
    if notes.is_empty() {
        empty_label(root, [250.0, 270.0, 3.0], b"NotesEmpty", state);
    } else {
        let (placed, pages) = j::layout_list(notes.len());
        start_list(0, pages);
        let font = g().map_or(0, |i| i.font_default);
        for (n, (note, at_)) in notes.iter().zip(placed.iter()).enumerate() {
            let pos = [250.0, at_.y, 3.0];
            let l = label(pos, [300.0, 24.0], &note.name, root);
            setup_label(l, [24.0; 2], n as i32, state, font, ALIGN_LEFT);
            add_cb(l, MSG_MOUSE_DOWN, cb::NOTE_CLICK);
            add_cb(l, MSG_DRAW, cb::LIST_DRAW);
            add_cb(l, MSG_UI_PRESS, cb::ITEM_PRESS);
            let im = host::widget_image(
                &note.icon,
                [pos[0] - 25.0, pos[1] + 2.0, pos[2] + -0.05],
                [20.0, 20.0],
                root,
            );
            setup_image(im, n as i32, state);
            add_entry(0, at_.page, vec![l, im]);
        }
    }
    link_entries(0);
    finish_list(0, state);
}

fn create_diaries_gui() {
    let root = root_of(j::STATE_DIARIES);
    let state = j::STATE_DIARIES;
    clear_list(1);
    header_label(root, b"Diaries", state);
    let conts: Vec<Vec<(Entry, u64)>> = g()
        .map(|i| {
            i.conts
                .iter()
                .map(|c| {
                    c.diaries
                        .iter()
                        .filter_map(|h| i.diaries.get(*h).cloned().map(|e| (e, h.to_bits())))
                        .collect()
                })
                .collect()
        })
        .unwrap_or_default();
    if conts.is_empty() {
        empty_label(root, [250.0, 270.0, 3.0], b"DiariesEmpty", state);
    } else {
        let total: usize = conts.iter().map(Vec::len).sum();
        let (placed, pages) = j::layout_list(total);
        start_list(1, pages);
        let font = g().map_or(0, |i| i.font_default);
        let mut flat = placed.iter();
        for c in &conts {
            for (n, (d, bits)) in c.iter().enumerate() {
                let Some(at_) = flat.next() else { continue };
                let pos = [250.0, at_.y, 3.0];
                let l = label(pos, [300.0, 24.0], &d.name, root);
                setup_label(l, [24.0; 2], n as i32, state, font, ALIGN_LEFT);
                add_cb(l, MSG_MOUSE_DOWN, cb::DIARY_CLICK);
                add_cb(l, MSG_UI_PRESS, cb::ITEM_PRESS);
                add_cb(l, MSG_DRAW, cb::LIST_DRAW);
                if let Some(t) = g()
                    .and_then(|i| i.wdata.get_mut(&l))
                    .and_then(|d| d.text.as_mut())
                {
                    t.extra = *bits;
                }
                let im = host::widget_image(
                    &d.icon,
                    [pos[0] - 23.0, pos[1], pos[2] - 1.0],
                    [20.0, 20.0],
                    root,
                );
                setup_image(im, n as i32, state);
                add_entry(1, at_.page, vec![l, im]);
            }
        }
    }
    link_entries(1);
    finish_list(1, state);
}

fn create_quest_gui() {
    let root = root_of(j::STATE_QUEST_LOG);
    let state = j::STATE_QUEST_LOG;
    clear_list(2);
    header_label(root, b"Quest Log", state);
    let quests: Vec<(usize, bool, Vec<u8>)> = g()
        .map(|i| {
            i.quests
                .iter()
                .enumerate()
                .map(|(n, q)| (n, q.active, q.text_entry.clone()))
                .collect()
        })
        .unwrap_or_default();
    let font = g().map_or(0, |i| i.font_default);
    if !quests.iter().any(|q| q.1) {
        empty_label(root, [150.0, 270.0, 3.0], b"QuestsEmpty", state);
    } else {
        let mut shown = Vec::new();
        let mut all_rows = Vec::new();
        for (n, active, text) in quests.iter().rev() {
            if !*active {
                continue;
            }
            let rows = wrap_rows(font, 500.0, 21.0, 19.0, &translate(text));
            shown.push(*n);
            all_rows.push(rows);
        }
        let counts: Vec<usize> = all_rows.iter().map(Vec::len).collect();
        let (placed, pages) = j::layout_quests(&counts);
        start_list(2, pages);
        for ((n, rows), at_) in shown.iter().zip(all_rows.iter()).zip(placed.iter()) {
            let mut pos = [150.0, at_.y, 3.0];
            let mut widgets = Vec::new();
            let dash = [u32::from(b'-')];
            let l = host::widget_label(
                [pos[0] - 15.0, pos[1], pos[2] - 1.0],
                [12.0, 24.0],
                &dash,
                root,
            );
            setup_label(l, [19.0; 2], *n as i32, state, font, ALIGN_LEFT);
            widgets.push(l);
            for row in rows {
                let l = host::widget_label(pos, [500.0, 24.0], row, root);
                setup_label(l, [19.0; 2], *n as i32, state, font, ALIGN_LEFT);
                widgets.push(l);
                pos[1] += 21.0;
            }
            add_entry(2, at_.page, widgets);
        }
    }
    finish_list(2, state);
}

fn create_open_note_gui() {
    for state in [
        j::STATE_OPEN_NOTE,
        j::STATE_OPEN_DIARY,
        j::STATE_OPEN_NARRATED,
    ] {
        setup_navigation(state, -1, 0, 1, root_of(state));
        if let Some(i) = g() {
            let r = at(&i.ret, state, 0);
            put(&mut i.default_nav, state, r);
        }
    }
    let root = root_of(j::STATE_OPEN_NARRATED);
    let (start, size) = g().map_or(([0.0; 3], [0.0; 2]), |i| (i.start_pos, i.set_size));
    let frame = host::widget_frame(start, size, root);
    add_cb(frame, MSG_UI_PRESS, cb::FRAME_CLICK);
    host::widget_set(frame, wd::SET_GLOBAL_UI, &[], 1, 0);
    let (b, f) = g().map_or((0, 0), |i| {
        (
            at(&i.img_back, j::STATE_OPEN_NARRATED, 0),
            at(&i.img_fwd, j::STATE_OPEN_NARRATED, 0),
        )
    });
    host::widget_visible(b, false);
    host::widget_enabled(b, false);
    host::widget_visible(f, false);
    host::widget_visible(f, false);
}

fn setup_navigation(state: i32, list: i32, fwd_idx: i32, back_idx: i32, root: usize) {
    let Some((tw, tsy, rows, rd)) =
        g().map(|i| (i.text_width, i.text_start_y, i.max_rows, i.row_dist))
    else {
        return;
    };
    let size = [65.0, 46.0];
    let (mut pos, back_x) = j::nav_positions(tw, tsy, rows, rd);
    let fwd = host::widget_image(b"journal/note_forward.tga", pos, size, root);
    add_cb(fwd, MSG_DRAW, cb::IMAGE_DRAW);
    add_cb(fwd, MSG_MOUSE_DOWN, cb::ARROW_CLICK);
    setup_image(fwd, fwd_idx, state);
    if let Some(i) = g() {
        put(&mut i.img_fwd, state, fwd);
    }

    let ret = if state == j::STATE_DIARIES || state == j::STATE_NOTES || state == j::STATE_QUEST_LOG
    {
        let l = label(
            [350.0, j::BACK_TEXT_Y, 3.0],
            [100.0, 24.0],
            b"MainBack",
            root,
        );
        let font = g().map_or(0, |i| i.font_menu);
        setup_label(l, [j::BACK_TEXT_FONT; 2], state, state, font, ALIGN_CENTER);
        add_cb(l, MSG_MOUSE_DOWN, cb::NOTE_BACK_CLICK);
        add_cb(l, MSG_DRAW, cb::LIST_DRAW);
        add_cb(l, MSG_UI_PRESS, cb::JOURNAL_PRESS);
        l
    } else {
        let im = host::widget_image(b"journal/note_return.tga", [0.0; 3], [-1.0; 2], root);
        if state != j::STATE_OPEN_NARRATED {
            let sz = host::widget_get(im, wd::GET_SIZE).f;
            host::widget_set(
                im,
                wd::SET_POSITION,
                &[400.0 - sz[0] / 2.0, j::BACK_TEXT_Y, 3.0],
                0,
                0,
            );
            add_cb(im, MSG_DRAW, cb::IMAGE_DRAW);
            add_cb(im, MSG_MOUSE_DOWN, cb::NOTE_BACK_CLICK);
            add_cb(im, MSG_UI_PRESS, cb::JOURNAL_PRESS);
            setup_image(im, state, state);
        } else {
            host::widget_visible(im, false);
        }
        im
    };
    add_cb(ret, MSG_UI_PRESS, cb::BACK_UI_PRESS);
    host::widget_set(ret, wd::SET_GLOBAL_UI, &[], 1, 0);
    if let Some(i) = g() {
        put(&mut i.ret, state, ret);
    }

    pos[0] = back_x;
    let back = host::widget_image(b"journal/note_backward.tga", pos, size, root);
    add_cb(back, MSG_DRAW, cb::IMAGE_DRAW);
    add_cb(back, MSG_MOUSE_DOWN, cb::ARROW_CLICK);
    setup_image(back, back_idx, state);
    if let Some(i) = g() {
        put(&mut i.img_back, state, back);
    }

    let listener = host::widget_dummy([0.0; 3], root);
    register(
        listener,
        WData {
            user: state,
            text: None,
        },
    );
    host::widget_set(listener, wd::SET_GLOBAL_UI, &[], 1, 0);
    add_cb(listener, MSG_UI_ARROW, cb::ARROW_PRESS);
    let _ = list;
}

// ---- background -----------------------------------------------------------------------------

fn create_background() {
    let mut a = A::new();
    host::ll(host::ll::SCREEN_SIZE_INT, &mut a);
    let (w, h) = (a.i[1], a.i[2]);
    let screen_tex = host::texture_new(b"Screen", true, w, h);
    let bg_tex = host::texture_new(b"ScreenBlur", false, w, h);
    let screen_gfx = host::gfx_texture(screen_tex, MAT_DIFFUSE);
    let bg_gfx = host::gfx_texture(bg_tex, MAT_ALPHA);
    let Some(size) = g().map(|i| {
        i.screen_tex = screen_tex;
        i.bg_tex = bg_tex;
        i.screen_gfx = screen_gfx;
        i.bg_gfx = bg_gfx;
        i.screen
    }) else {
        return;
    };
    host::render_background(screen_tex, bg_tex, size);
}

fn destroy_background() {
    let Some((sg, st, bg, bt)) = g().map(|i| {
        (
            std::mem::take(&mut i.screen_gfx),
            std::mem::take(&mut i.screen_tex),
            std::mem::take(&mut i.bg_gfx),
            std::mem::take(&mut i.bg_tex),
        )
    }) else {
        return;
    };
    if sg != 0 {
        host::gfx_destroy(sg);
    }
    if st != 0 {
        host::texture_destroy(st);
    }
    if bg != 0 {
        host::gfx_destroy(bg);
    }
    if bt != 0 {
        host::texture_destroy(bt);
    }
}

// ---- callbacks ------------------------------------------------------------------------------

fn user_of(w: usize) -> i32 {
    g().and_then(|i| i.wdata.get(&w)).map_or(0, |d| d.user)
}

fn text_of(w: usize) -> Option<TextData> {
    g().and_then(|i| i.wdata.get(&w)).and_then(|d| d.text)
}

/// Entry of every widget callback: `token` as registered by [`add_cb`].
pub fn dispatch(token: usize, w: usize, lval: i32) -> bool {
    let _f = host::journal_scope();
    match token {
        cb::MAIN_CLICK => main_click(w),
        cb::MAIN_UI_BUTTON => main_ui_button(w, lval),
        cb::MAIN_DRAW => draw_effect(w, true),
        cb::LIST_DRAW => draw_effect(w, false),
        cb::NOTE_CLICK => note_click(w),
        cb::NOTE_BACK_CLICK => note_back_click(lval),
        cb::JOURNAL_PRESS => journal_press(w, lval),
        cb::DIARY_CLICK => diary_click(w),
        cb::IMAGE_DRAW => true,
        cb::ARROW_CLICK => arrow_click(w),
        cb::FRAME_CLICK => {
            exit_pressed(false);
            true
        }
        cb::ITEM_PRESS => item_press(w, lval),
        cb::ARROW_PRESS => arrow_press(w, lval),
        _ => false,
    }
}

fn main_click(w: usize) -> bool {
    match j::main_click(user_of(w)) {
        -2 => exit_pressed(false),
        -1 => {}
        state => {
            change_state(state);
            if let Some(i) = g() {
                put(&mut i.default_nav, j::STATE_MAIN, w);
            }
        }
    }
    true
}

fn main_ui_button(w: usize, lval: i32) -> bool {
    match j::main_ui_button(lval) {
        2 => {
            if !ui_listener_button(w, lval) {
                return note_back_click(MOUSE_LEFT);
            }
            true
        }
        1 => main_click(w),
        _ => false,
    }
}

fn note_back_click(lval: i32) -> bool {
    if lval != j::BUTTON_PRIMARY && lval != j::BUTTON_SECONDARY {
        return true;
    }
    exit_pressed(false);
    true
}

fn journal_press(w: usize, lval: i32) -> bool {
    if lval == j::BUTTON_PRIMARY {
        ui_listener_button(w, lval);
    } else if lval == j::BUTTON_SECONDARY {
        exit_pressed(false);
    }
    true
}

fn visible(w: usize) -> bool {
    w != 0 && host::wx(wx::IS_VISIBLE, w).r != 0
}

fn arrows_of(kind: i32) -> (usize, usize) {
    g().map_or((0, 0), |i| {
        (at(&i.img_back, kind, 0), at(&i.img_fwd, kind, 0))
    })
}

fn do_button(action: i32, back: usize, fwd: usize) -> bool {
    match action {
        j::BTN_BACK_ARROW => arrow_click(back),
        j::BTN_FWD_ARROW => arrow_click(fwd),
        _ => {
            exit_pressed(false);
            true
        }
    }
}

fn ui_listener_button(w: usize, lval: i32) -> bool {
    let kind = user_of(w);
    if !j::is_open_state(kind) {
        return false;
    }
    let (back, fwd) = arrows_of(kind);
    match j::ui_button(kind, lval, visible(back), visible(fwd)) {
        Some(a) => do_button(a, back, fwd),
        None => false,
    }
}

fn note_click(w: usize) -> bool {
    let h = g().and_then(|i| {
        usize::try_from(user_of(w))
            .ok()
            .and_then(|n| i.note_order.get(n).copied())
    });
    if let Some(h) = h {
        open_note(h.to_bits(), false);
    }
    true
}

fn diary_click(w: usize) -> bool {
    if let Some(t) = text_of(w) {
        open_diary(t.extra, false);
    }
    true
}

fn item_press(w: usize, lval: i32) -> bool {
    if lval != j::BUTTON_PRIMARY {
        return false;
    }
    match text_of(w).map(|t| t.ty) {
        Some(j::STATE_DIARIES) => diary_click(w),
        Some(j::STATE_NOTES) => note_click(w),
        _ => false,
    }
}

fn arrow_click(w: usize) -> bool {
    match j::arrow_click(user_of(w)) {
        Some((0, d)) => {
            let p = g().map_or(0, |i| i.cur_page);
            set_note_page(p.wrapping_add(d));
        }
        Some((_, d)) => {
            let state = g().map_or(0, |i| i.state);
            let p = g().map_or(0, |i| at(&i.cur_list, j::list_index(state), 0));
            set_note_list_page(p.wrapping_add(d), state);
        }
        None => {}
    }
    sound(b"journal_page");
    true
}

fn arrow_press(w: usize, lval: i32) -> bool {
    let kind = user_of(w);
    let (back, fwd) = arrows_of(kind);
    let (bv, fv) = (visible(back), visible(fwd));
    let down_ok = lval == j::ARROW_DOWN && host::jgame(jg::DPAD_DOWN_OK).r != 0;
    match j::ui_arrow(kind, lval, bv, fv, down_ok) {
        Some(a) => do_button(a, back, fwd),
        None => false,
    }
}

fn draw_effect(w: usize, main: bool) -> bool {
    let Some(t) = text_of(w) else { return true };
    if t.effect <= 0.0 {
        return true;
    }
    let Some((pulse, menu)) = g().map(|i| (i.pulse, i.font_menu)) else {
        return true;
    };
    let alpha = host::widget_get(w, wd::GET_COLOR_MUL).f[3];
    let e = j::hover_effect(pulse, alpha, t.effect);
    let size = host::widget_get(w, wd::GET_SIZE).f;
    let gp = host::widget_get(w, wd::GET_GLOBAL).f;
    let gpos = [gp[0], gp[1], gp[2]];
    let fsz = host::wx(wx::GET_FONT_SIZE, w).f;
    let fsz = [fsz[0], fsz[1]];
    let text = host::wx(wx::GET_TEXT, w).ret_wide();
    let vsize = j::effect_size(fsz, e.x_add);
    let color = [1.0, 0.0, 0.0, e.alpha];
    if main {
        let pos = j::centered_effect_pos(gpos, size[0], e.x_add);
        host::draw_font(menu, &text, pos, vsize, color, ALIGN_CENTER);
        return true;
    }
    let font = host::wx(wx::GET_FONT, w).r;
    let align = host::wx(wx::GET_ALIGN, w).r as i32;
    if align == ALIGN_CENTER {
        let pos = j::centered_effect_pos(gpos, size[0], e.x_add);
        host::draw_font(font, &text, pos, vsize, color, ALIGN_CENTER);
    } else if align == ALIGN_LEFT {
        let len = |sz: [f32; 2]| {
            let mut a = A::new();
            a.i[0] = wx::FONT_LENGTH;
            a.p[0] = font;
            a.f[..2].copy_from_slice(&sz);
            a.w[0] = WStr::of(&text);
            host::call(op::WIDGET_X, &mut a);
            a.f[0]
        };
        let add = len(vsize) - len(fsz);
        let pos = j::left_effect_pos(gpos, add);
        host::draw_font(font, &text, pos, vsize, color, ALIGN_LEFT);
    }
    true
}

// ---- save data ------------------------------------------------------------------------------

pub fn last_read() -> (i32, i32, i32) {
    g().map_or((0, -1, -1), |i| (i.last_cat, i.last_entry, i.last_type))
}

pub fn note_count() -> usize {
    g().map_or(0, |i| i.note_order.len())
}

fn fields(e: &Entry, f: i32) -> Vec<u8> {
    match f {
        0 => e.name.clone(),
        1 => e.text.clone(),
        2 => e.icon.clone(),
        _ => e.image.clone(),
    }
}

pub fn note_field(n: usize, f: i32) -> Vec<u8> {
    g().and_then(|i| i.note_order.get(n).and_then(|h| i.notes.get(*h)))
        .map(|e| fields(e, f))
        .unwrap_or_default()
}

pub fn cont_count() -> usize {
    g().map_or(0, |i| i.conts.len())
}

pub fn cont_type(c: usize) -> Vec<u8> {
    g().and_then(|i| i.conts.get(c))
        .map(|c| c.ty.clone())
        .unwrap_or_default()
}

pub fn diary_count(c: usize) -> usize {
    g().and_then(|i| i.conts.get(c))
        .map_or(0, |c| c.diaries.len())
}

pub fn diary_field(c: usize, n: usize, f: i32) -> Vec<u8> {
    g().and_then(|i| {
        let h = i.conts.get(c)?.diaries.get(n)?;
        i.diaries.get(*h)
    })
    .map(|e| fields(e, f))
    .unwrap_or_default()
}

pub fn quest_count() -> usize {
    g().map_or(0, |i| i.quests.len())
}

pub fn quest_field(n: usize, f: i32) -> Vec<u8> {
    g().and_then(|i| i.quests.get(n))
        .map(|q| match f {
            0 => q.name.clone(),
            1 => q.name_entry.clone(),
            _ => q.text_entry.clone(),
        })
        .unwrap_or_default()
}

pub fn quest_active(n: usize) -> bool {
    g().and_then(|i| i.quests.get(n)).is_some_and(|q| q.active)
}

/// `ToJournal`: replaces the last-read state and every note, diary and quest.
pub fn load_begin(cat: i32, entry: i32, ty: i32) {
    if let Some(i) = g() {
        i.last_cat = cat;
        i.last_entry = entry;
        i.last_type = ty;
        i.notes = Arena::default();
        i.note_order.clear();
        i.diaries = Arena::default();
        i.conts.clear();
        i.quests.clear();
    }
}

pub fn load_note(e: Entry) {
    if let Some(i) = g()
        && let Some(h) = i.notes.insert(e)
    {
        i.note_order.push(h);
    }
}

pub fn load_container(ty: Vec<u8>) {
    if let Some(i) = g() {
        i.conts.push(Cont {
            ty,
            diaries: Vec::new(),
        });
    }
}

pub fn load_diary(e: Entry) {
    if let Some(i) = g()
        && let Some(h) = i.diaries.insert(e)
        && let Some(c) = i.conts.last_mut()
    {
        c.diaries.push(h);
    }
}

pub fn load_quest(q: Quest) {
    if let Some(i) = g() {
        i.quests.push(q);
    }
}
