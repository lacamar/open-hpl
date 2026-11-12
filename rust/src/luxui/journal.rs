//! `cLuxJournal`: note pages and text wrap decisions, note/diary/quest list layout and paging,
//! state navigation and input handling, and the hover effect math. Note, diary and quest
//! containers (the save layer reads them), widgets, fonts, sound and draws stay C++.

use crate::math::sin;
use crate::string;

pub const STATE_MAIN: i32 = 0;
pub const STATE_NOTES: i32 = 1;
pub const STATE_DIARIES: i32 = 2;
pub const STATE_QUEST_LOG: i32 = 3;
pub const STATE_OPEN_NOTE: i32 = 4;
pub const STATE_OPEN_DIARY: i32 = 5;
pub const STATE_OPEN_NARRATED: i32 = 6;
pub const STATE_COUNT: i32 = 7;

pub const BUTTON_PRIMARY: i32 = 1;
pub const BUTTON_SECONDARY: i32 = 2;
pub const ARROW_UP: i32 = 1;
pub const ARROW_RIGHT: i32 = 2;
pub const ARROW_DOWN: i32 = 4;
pub const ARROW_LEFT: i32 = 8;

pub const MAX_NOTE_LIST_Y: f32 = 450.0;
pub const NOTE_LIST_HEADER_Y: f32 = 40.0;
pub const BACK_TEXT_Y: f32 = 530.0;
pub const BACK_TEXT_FONT: f32 = 28.0;

pub const EXIT_NONE: i32 = 0;
pub const EXIT_TO_INVENTORY: i32 = 1;
pub const EXIT_TO_DEFAULT: i32 = 2;

pub const PRESS_NONE: i32 = 0;
pub const PRESS_NEXT_PAGE: i32 = 1;
pub const PRESS_EXIT_HINT: i32 = 2;
pub const PRESS_EXIT: i32 = 3;
pub const PRESS_CHANGE_STATE: i32 = 4;

const EXIT_STATE: [i32; 7] = [
    STATE_COUNT,
    STATE_MAIN,
    STATE_MAIN,
    STATE_MAIN,
    STATE_NOTES,
    STATE_DIARIES,
    STATE_DIARIES,
];

const BACKGROUND: [&[u8]; 7] = [
    b"journal/bg_main.tga",
    b"journal/bg_notes.tga",
    b"journal/bg_diaries.tga",
    b"journal/bg_questlog.tga",
    b"",
    b"",
    b"",
];

pub fn background(state: i32) -> &'static [u8] {
    usize::try_from(state)
        .ok()
        .and_then(|i| BACKGROUND.get(i))
        .copied()
        .unwrap_or(b"")
}

// ---- alphas ---------------------------------------------------------------------------------

/// Journal alpha step; returns the container to switch to once fully faded out.
pub fn update_alpha(alpha: &mut f32, active: bool, from_inventory: bool, dt: f32) -> i32 {
    if active {
        *alpha = dt.mul_add(2.0, *alpha);
        if *alpha > 1.0 {
            *alpha = 1.0;
        }
        return EXIT_NONE;
    }
    *alpha = (-dt).mul_add(3.0, *alpha);
    if *alpha < 0.0 {
        *alpha = 0.0;
        return if from_inventory {
            EXIT_TO_INVENTORY
        } else {
            EXIT_TO_DEFAULT
        };
    }
    EXIT_NONE
}

pub fn state_alpha(alpha: f32, is_current: bool, dt: f32) -> f32 {
    if is_current {
        let a = dt.mul_add(2.0, alpha);
        if a > 1.0 { 1.0 } else { a }
    } else {
        let a = (-dt).mul_add(5.0, alpha);
        if a < 0.0 { 0.0 } else { a }
    }
}

pub fn hover_alpha(alpha: f32, hovered: bool, dt: f32) -> f32 {
    if hovered {
        let a = dt.mul_add(4.0, alpha);
        if a > 1.0 { 1.0 } else { a }
    } else {
        let a = (-dt).mul_add(2.0, alpha);
        if a < 0.0 { 0.0 } else { a }
    }
}

/// Whether the narrated diary has run out of voice and should advance.
pub fn narration_done(state: i32, active: bool, voice_valid: bool) -> bool {
    state == STATE_OPEN_NARRATED && active && !voice_valid
}

// ---- exit / state ---------------------------------------------------------------------------

/// `ExitPressed`: `(action, new_state)`.
pub fn exit_pressed(
    active: bool,
    instant: bool,
    force_instant: bool,
    state: i32,
    page: i32,
    pages: usize,
) -> (i32, i32) {
    if !active {
        return (PRESS_NONE, 0);
    }
    if state == STATE_OPEN_NARRATED && i64::from(page) < pages as i64 - 1 {
        return (PRESS_NEXT_PAGE, page.wrapping_add(1));
    }
    if instant || force_instant {
        return (PRESS_EXIT_HINT, 0);
    }
    let new = usize::try_from(state)
        .ok()
        .and_then(|i| EXIT_STATE.get(i))
        .copied()
        .unwrap_or(STATE_COUNT);
    if new == STATE_COUNT {
        (PRESS_EXIT, 0)
    } else {
        (PRESS_CHANGE_STATE, new)
    }
}

pub fn root_visible(root: i32, current: i32) -> bool {
    root == current
}

pub fn list_index(state: i32) -> i32 {
    match state {
        STATE_NOTES => 0,
        STATE_DIARIES => 1,
        _ => 2,
    }
}

// ---- notes, diaries, quests -----------------------------------------------------------------

pub fn note_entries(name: &[u8]) -> (Vec<u8>, Vec<u8>) {
    (
        [b"Note_".as_slice(), name, b"_Name"].concat(),
        [b"Note_".as_slice(), name, b"_Text"].concat(),
    )
}

pub fn diary_entries(name: &[u8], num: i32) -> (Vec<u8>, Vec<u8>) {
    let n = string::int_to_string(num, -1);
    (
        [b"Diary_".as_slice(), name, b"_Name", &n].concat(),
        [b"Diary_".as_slice(), name, b"_Text", &n].concat(),
    )
}

pub fn quest_entries(name: &[u8]) -> (Vec<u8>, Vec<u8>) {
    (
        [b"Quest_".as_slice(), name, b"_Name"].concat(),
        [b"Quest_".as_slice(), name, b"_Text"].concat(),
    )
}

/// `(large image, icon)` file names of a note or diary image.
pub fn image_files(image: &[u8]) -> (Vec<u8>, Vec<u8>) {
    let ext = string::file_ext(image);
    let base = string::set_file_ext(image, b"");
    (
        string::set_file_ext(&[base.as_slice(), b"_large"].concat(), ext),
        string::set_file_ext(&[base.as_slice(), b"_icon"].concat(), ext),
    )
}

pub const ARCHIVIST_NONE: i32 = 0;
pub const ARCHIVIST_TDD: i32 = 1;
pub const ARCHIVIST_JUSTINE: i32 = 2;

/// Master archivist bookkeeping after a note is added.
pub fn archivist(game: &[u8], notes: usize) -> i32 {
    if game == b"Amnesia - The Dark Descent -" && notes == 21 {
        ARCHIVIST_TDD
    } else if game == b"Amnesia - Justine -" && notes == 9 {
        ARCHIVIST_JUSTINE
    } else {
        ARCHIVIST_NONE
    }
}

pub const QUEST_MISSING: i32 = 0;
pub const QUEST_DISABLED: i32 = 1;
pub const QUEST_ALREADY_OFF: i32 = 2;

pub fn disable_quest(found: bool, active: bool) -> i32 {
    if !found {
        QUEST_MISSING
    } else if active {
        QUEST_DISABLED
    } else {
        QUEST_ALREADY_OFF
    }
}

/// Position of `target` in a flat list of item identities.
pub fn find_index(items: &[usize], target: usize) -> i32 {
    items
        .iter()
        .position(|&p| p == target)
        .map_or(-1, |i| i as i32)
}

/// `(category, entry)` of `target` in diary containers flattened as `counts` and `items`.
pub fn find_diary(counts: &[usize], items: &[usize], target: usize) -> (i32, i32) {
    let mut at = 0usize;
    for (cat, &n) in counts.iter().enumerate() {
        let end = (at + n).min(items.len());
        if let Some(e) = items
            .get(at..end)
            .and_then(|s| s.iter().position(|&p| p == target))
        {
            return (cat as i32, e as i32);
        }
        at += n;
    }
    (-1, -1)
}

pub const LAST_NOTE: i32 = 0;
pub const LAST_DIARY: i32 = 1;

/// `OpenLastReadText`: 0 nothing to open, 1 open note `entry`, 2 open diary `(cat, entry)`.
pub fn last_read(kind: i32, cat: i32, entry: i32, notes: usize, counts: &[usize]) -> i32 {
    if entry < 0 || kind < 0 {
        return 0;
    }
    if kind == LAST_NOTE {
        return if entry as usize >= notes { 0 } else { 1 };
    }
    if cat < 0 {
        return 0;
    }
    match counts.get(cat as usize) {
        None => 0,
        Some(&n) if entry as usize >= n => 0,
        Some(_) => 2,
    }
}

// ---- pages ----------------------------------------------------------------------------------

#[derive(Default, Clone)]
pub struct Page {
    pub voice: Vec<u8>,
    pub rows: Vec<Vec<u32>>,
}

#[derive(Default)]
pub struct Pages {
    pub pages: Vec<Page>,
}

fn wide(s: &[u8]) -> Vec<u32> {
    s.iter().map(|&c| u32::from(c)).collect()
}

fn is_voice_prefix(row: &[u32]) -> bool {
    string::sub(row, 0, 6, true).is_some_and(|p| p == wide(b"[voice").as_slice())
}

impl Pages {
    /// `LoadText`: `rows` are the font-wrapped lines of the text.
    pub fn load_text(&mut self, rows: &[Vec<u32>], max_page_rows: i32) {
        self.pages.clear();
        let new_page = wide(b"[new_page]");
        let mut count = 0i32;
        for row in rows {
            if is_voice_prefix(row) {
                if let Some(p) = self.pages.last_mut() {
                    p.rows.push(Vec::new());
                    count += 1;
                }
                continue;
            }
            let tag = *row == new_page;
            if self.pages.is_empty() || count >= max_page_rows || tag {
                self.pages.push(Page::default());
                count = 0;
                if tag {
                    continue;
                }
            }
            if let Some(p) = self.pages.last_mut() {
                p.rows.push(row.clone());
            }
            count += 1;
        }
    }

    /// `LoadNarrationText`: returns `(row, argument count)` of every malformed voice line.
    pub fn load_narration(&mut self, draft: &[Vec<u32>]) -> Vec<(Vec<u8>, usize)> {
        self.pages.clear();
        let mut errors = Vec::new();
        let mut rows: Vec<Vec<u32>> = Vec::new();
        let (open, close) = (u32::from(b'['), u32::from(b']'));
        let mut i = 0;
        while i < draft.len() {
            let Some(row) = draft.get(i) else { break };
            if row.is_empty() {
                rows.push(row.clone());
            } else if row.first() == Some(&open) && row.last() != Some(&close) {
                rows.push(row.clone());
                i += 1;
                if let (Some(next), Some(last)) = (draft.get(i), rows.last_mut()) {
                    last.extend_from_slice(next);
                }
            } else {
                rows.push(row.clone());
            }
            i += 1;
        }
        let new_page = wide(b"[new_page]");
        let seps = wide(b" []");
        for row in &rows {
            let voice = row.len() > 6 && is_voice_prefix(row);
            if *row == new_page {
                continue;
            }
            if self.pages.is_empty() || voice {
                self.pages.push(Page::default());
                if voice {
                    let args: Vec<&[u32]> = string::split(row, Some(&seps), true).collect();
                    match (args.len(), args.get(1)) {
                        (2, Some(v)) => {
                            if let Some(p) = self.pages.last_mut() {
                                p.voice = string::wide_to_utf8(v);
                            }
                        }
                        _ => errors.push((string::wide_to_utf8(row), args.len())),
                    }
                    continue;
                }
            }
            if let Some(p) = self.pages.last_mut() {
                p.rows.push(row.clone());
            }
        }
        errors
    }
}

/// Arrow visibility after `SetNotePage`: `(backward, forward)`.
pub fn note_arrows(page: i32, pages: usize) -> (bool, bool) {
    (page != 0, i64::from(page) < pages as i64 - 1)
}

/// Whether `SetNotePage` starts a voice for the narrated diary; the page index may be out of
/// range for a diary without pages.
pub fn narrated_page_plays(page: i32, pages: usize) -> bool {
    page >= 0 && (page as usize) < pages
}

pub fn text_page_start_y(row_count: usize, row_dist: f32) -> f32 {
    let half = row_count as f32 * 0.5;
    (-row_dist).mul_add(half, 300.0)
}

pub fn nav_positions(
    text_width: f32,
    text_start_y: f32,
    max_rows: i32,
    row_dist: f32,
) -> ([f32; 3], f32) {
    let size_x = 65.0f32;
    let fwd_x = 400.0 + text_width / 2.0 - size_x - 80.0;
    let y = (max_rows as f32 + 1.0).mul_add(row_dist, text_start_y);
    ([fwd_x, y, 3.0], 400.0 - text_width / 2.0 + 80.0)
}

// ---- lists ----------------------------------------------------------------------------------

#[derive(Clone, Copy, PartialEq, Debug)]
pub struct Placed {
    pub page: usize,
    pub y: f32,
}

/// Note and diary list: one row per entry, a new page once the next row would pass the limit.
pub fn layout_list(entries: usize) -> (Vec<Placed>, usize) {
    let mut out = Vec::new();
    if entries == 0 {
        return (out, 0);
    }
    let mut page = 0usize;
    let mut y = 100.0f32;
    for _ in 0..entries {
        if y > MAX_NOTE_LIST_Y {
            y = 100.0;
            page += 1;
        }
        out.push(Placed { page, y });
        y += 30.0;
    }
    (out, page + 1)
}

/// Quest list: `rows[i]` is the wrapped row count of the i-th shown quest.
pub fn layout_quests(rows: &[usize]) -> (Vec<Placed>, usize) {
    let mut out = Vec::new();
    if rows.is_empty() {
        return (out, 0);
    }
    let mut page = 0usize;
    let mut y = 100.0f32;
    for &n in rows {
        let extra = n.wrapping_sub(1).wrapping_mul(21) as f32;
        if y > 200.0 && y + extra > MAX_NOTE_LIST_Y {
            y = 100.0;
            page += 1;
        }
        out.push(Placed { page, y });
        for _ in 0..n {
            y += 21.0;
        }
        y += 30.0;
    }
    (out, page + 1)
}

/// Page shown when a list state is entered (the last page; none gives -1).
pub fn start_page(pages: usize) -> i32 {
    (pages as i64 - 1) as i32
}

pub struct ListPageIn {
    pub page: i32,
    pub pages: usize,
    pub state: i32,
}

pub struct ListPage {
    pub no_pages: bool,
    pub back: bool,
    pub forward: bool,
    pub default_is_return: bool,
}

pub fn set_list_page(p: &ListPageIn) -> ListPage {
    if p.page < 0 || p.page as usize >= p.pages {
        return ListPage {
            no_pages: true,
            back: false,
            forward: false,
            default_is_return: true,
        };
    }
    ListPage {
        no_pages: false,
        back: p.page != 0,
        forward: i64::from(p.page) < p.pages as i64 - 1,
        default_is_return: p.state == STATE_QUEST_LOG,
    }
}

// ---- input ----------------------------------------------------------------------------------

pub const MAIN_NOTES: i32 = 0;
pub const MAIN_DIARIES: i32 = 1;
pub const MAIN_QUEST: i32 = 2;
pub const MAIN_BACK: i32 = 3;

/// `MainMenuTextClick`: the state to enter for a label index, or -1 (and -2 for back).
pub fn main_click(user: i32) -> i32 {
    match user {
        MAIN_NOTES => STATE_NOTES,
        MAIN_DIARIES => STATE_DIARIES,
        MAIN_QUEST => STATE_QUEST_LOG,
        MAIN_BACK => -2,
        _ => -1,
    }
}

pub const ARROW_NOTE_FWD: i32 = 0;
pub const ARROW_NOTE_BACK: i32 = 1;
pub const ARROW_LIST_FWD: i32 = 2;
pub const ARROW_LIST_BACK: i32 = 3;

/// `NoteArrowClick`: the page change for a button index: `(0 note page, 1 list page, delta)`.
pub fn arrow_click(user: i32) -> Option<(i32, i32)> {
    match user {
        ARROW_NOTE_FWD => Some((0, 1)),
        ARROW_NOTE_BACK => Some((0, -1)),
        ARROW_LIST_FWD => Some((1, 1)),
        ARROW_LIST_BACK => Some((1, -1)),
        _ => None,
    }
}

pub fn is_open_state(t: i32) -> bool {
    t == STATE_OPEN_NOTE || t == STATE_OPEN_DIARY || t == STATE_OPEN_NARRATED
}

pub const BTN_NONE: i32 = 0;
pub const BTN_BACK_ARROW: i32 = 1;
pub const BTN_FWD_ARROW: i32 = 2;
pub const BTN_EXIT: i32 = 3;

/// `UIListenerButtonPress`: `None` when the press is not handled.
pub fn ui_button(kind: i32, button: i32, back_visible: bool, fwd_visible: bool) -> Option<i32> {
    if !is_open_state(kind) {
        return None;
    }
    match button {
        BUTTON_SECONDARY => Some(if back_visible {
            BTN_BACK_ARROW
        } else {
            BTN_EXIT
        }),
        BUTTON_PRIMARY if fwd_visible => Some(BTN_FWD_ARROW),
        _ => None,
    }
}

/// `UIListenerArrowPress`: `BTN_*` for the action, `None` when not handled. `down_ok` is the
/// gamepad D-pad check.
pub fn ui_arrow(
    kind: i32,
    arrow: i32,
    back_visible: bool,
    fwd_visible: bool,
    down_ok: bool,
) -> Option<i32> {
    match arrow {
        ARROW_LEFT if back_visible => Some(BTN_BACK_ARROW),
        ARROW_RIGHT if fwd_visible => Some(BTN_FWD_ARROW),
        ARROW_DOWN if down_ok && is_open_state(kind) => Some(BTN_EXIT),
        _ => None,
    }
}

/// `MainMenuUIButtonPress`: 0 not handled, 1 primary click, 2 secondary (button then back).
pub fn main_ui_button(button: i32) -> i32 {
    match button {
        BUTTON_SECONDARY => 2,
        BUTTON_PRIMARY => 1,
        _ => 0,
    }
}

// ---- hover effect ---------------------------------------------------------------------------

pub struct Effect {
    pub t: f32,
    pub x_add: f32,
    pub alpha: f32,
}

/// Pulsing red hover overlay on a label.
pub fn hover_effect(pulse: f32, color_alpha: f32, effect_alpha: f32) -> Effect {
    let t = 0.3 + (sin(pulse * 2.5) + 1.0) * 0.7 * 0.5;
    let x_add = (1.0 - t).mul_add(5.0, 1.0);
    Effect {
        t,
        x_add,
        alpha: color_alpha * t * effect_alpha,
    }
}

pub fn centered_effect_pos(gpos: [f32; 3], size_x: f32, x_add: f32) -> [f32; 3] {
    [
        gpos[0] - x_add / 2.0 + size_x / 2.0,
        gpos[1] - 2.0,
        gpos[2] - 1.0,
    ]
}

pub fn left_effect_pos(gpos: [f32; 3], size_add: f32) -> [f32; 3] {
    [gpos[0] - size_add / 2.0, gpos[1] - 2.0, gpos[2] - 1.0]
}

pub fn effect_size(font: [f32; 2], x_add: f32) -> [f32; 2] {
    [font[0] + x_add, font[1] + 4.0]
}

pub fn fade_alpha(alpha: f32) -> f32 {
    (-alpha).mul_add(alpha, 1.0)
}
