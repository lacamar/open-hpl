//! The file picker's browser (`iFileBrowser`): path navigation over the platform shims of
//! `RustGuiG3.cpp`. Its two virtuals report to the popup.

use super::g2_popup::{G2Popup, ohpl_g2_popup_on_add_filter, ohpl_g2_popup_on_navigate};
use super::widgets_base::{self as wb, NavTarget, PathPiece};
use crate::string as s;
use std::ffi::c_void;
use std::sync::Mutex;
use std::sync::atomic::{AtomicPtr, Ordering};

type Sink = unsafe extern "C" fn(*mut c_void, *const u32, usize);

/// Platform and resource calls of `RustGuiG3.cpp`, registered once at start-up.
#[repr(C)]
pub struct PlatHost {
    exists: unsafe extern "C" fn(i32, *const u32, usize) -> bool,
    list: unsafe extern "C" fn(i32, *const u32, usize, *const u32, usize, bool, Sink, *mut c_void),
    info: unsafe extern "C" fn(*const u32, usize, *mut u64, Sink, *mut c_void),
    special: unsafe extern "C" fn(i32, Sink, *mut c_void),
    drives: unsafe extern "C" fn() -> u64,
    assign: unsafe extern "C" fn(*mut c_void, i32, *const u32, usize),
    add_resource_dir: unsafe extern "C" fn(*mut c_void, *const u32, usize),
}

static HOST: AtomicPtr<PlatHost> = AtomicPtr::new(std::ptr::null_mut());

/// # Safety
/// `h` is a static table that outlives every call into the file browser.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g2fb_set_host(h: *mut PlatHost) {
    HOST.store(h, Ordering::Release);
}

fn host() -> Option<&'static PlatHost> {
    // SAFETY: `ohpl_g2fb_set_host` contract.
    unsafe { HOST.load(Ordering::Acquire).as_ref() }
}

unsafe fn ohpl_g2fb_exists(kind: i32, p: *const u32, n: usize) -> bool {
    // SAFETY: caller contract; the table entries have these signatures.
    host().is_some_and(|h| unsafe { (h.exists)(kind, p, n) })
}

#[allow(clippy::too_many_arguments)]
unsafe fn ohpl_g2fb_list(
    kind: i32,
    dir: *const u32,
    dir_n: usize,
    mask: *const u32,
    mask_n: usize,
    hidden: bool,
    sink: Sink,
    ctx: *mut c_void,
) {
    if let Some(h) = host() {
        // SAFETY: caller contract.
        unsafe { (h.list)(kind, dir, dir_n, mask, mask_n, hidden, sink, ctx) };
    }
}

unsafe fn ohpl_g2fb_info(p: *const u32, n: usize, size: *mut u64, sink: Sink, ctx: *mut c_void) {
    if let Some(h) = host() {
        // SAFETY: caller contract.
        unsafe { (h.info)(p, n, size, sink, ctx) };
    }
}

unsafe fn ohpl_g2fb_special(kind: i32, sink: Sink, ctx: *mut c_void) {
    if let Some(h) = host() {
        // SAFETY: caller contract.
        unsafe { (h.special)(kind, sink, ctx) };
    }
}

unsafe fn ohpl_g2fb_drives() -> u64 {
    // SAFETY: the table entry has this signature.
    host().map_or(0, |h| unsafe { (h.drives)() })
}

/// Assigns the text to a C++ `tWString` (kind 0) or makes a `tWStringVec` hold only it (kind 1).
///
/// # Safety
/// `dest` is that C++ object; `p` is readable for `n` units.
pub(super) unsafe fn ohpl_g2fb_assign(dest: *mut c_void, kind: i32, p: *const u32, n: usize) {
    if let Some(h) = host() {
        // SAFETY: caller contract.
        unsafe { (h.assign)(dest, kind, p, n) };
    }
}

/// `cResources::AddResourceDir`.
///
/// # Safety
/// `res` is the set's `cResources`; `p` is readable for `n` units.
pub(super) unsafe fn add_resource_dir(res: *mut c_void, p: *const u32, n: usize) {
    if let Some(h) = host() {
        // SAFETY: caller contract.
        unsafe { (h.add_resource_dir)(res, p, n) };
    }
}

const SLASH: u32 = b'/' as u32;
const BACKSLASH: u32 = b'\\' as u32;

static DIRS: Mutex<[Vec<u32>; 2]> = Mutex::new([Vec::new(), Vec::new()]);

/// `iFileBrowser::msGameDir` (0) and `msPersonalDir` (1).
///
/// # Safety
/// `p` is readable for `n` units.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g2fb_set_dir(kind: i32, p: *const u32, n: usize) {
    // SAFETY: caller contract.
    let v = unsafe { std::slice::from_raw_parts(p, n) }.to_vec();
    if let Some(slot) = DIRS
        .lock()
        .ok()
        .as_deref_mut()
        .and_then(|d| d.get_mut(usize::from(kind != 0)))
    {
        *slot = v;
    }
}

pub fn dir(kind: usize) -> Vec<u32> {
    DIRS.lock()
        .ok()
        .and_then(|d| d.get(kind).cloned())
        .unwrap_or_default()
}

unsafe extern "C" fn push(ctx: *mut c_void, p: *const u32, n: usize) {
    // SAFETY: `ctx` is the `Vec<Vec<u32>>` of the caller; `p` is readable for `n` units.
    unsafe {
        (*ctx.cast::<Vec<Vec<u32>>>()).push(std::slice::from_raw_parts(p, n).to_vec());
    }
}

fn ws(t: &str) -> Vec<u32> {
    t.chars().map(u32::from).collect()
}

pub fn exists(folder: bool, p: &[u32]) -> bool {
    // SAFETY: `p` is readable for its length.
    unsafe { ohpl_g2fb_exists(i32::from(folder), p.as_ptr(), p.len()) }
}

fn list(kind: i32, dir: &[u32], mask: &[u32], hidden: bool) -> Vec<Vec<u32>> {
    let mut out: Vec<Vec<u32>> = Vec::new();
    // SAFETY: the slices are readable; `push` fills `out`.
    unsafe {
        ohpl_g2fb_list(
            kind,
            dir.as_ptr(),
            dir.len(),
            mask.as_ptr(),
            mask.len(),
            hidden,
            push,
            (&raw mut out).cast(),
        );
    }
    out
}

fn special(kind: i32) -> Vec<u32> {
    let mut out: Vec<Vec<u32>> = Vec::new();
    // SAFETY: `push` fills `out`.
    unsafe { ohpl_g2fb_special(kind, push, (&raw mut out).cast()) };
    out.pop().unwrap_or_default()
}

pub struct Category {
    pub name: Vec<u32>,
    pub filters: Vec<Vec<u32>>,
}

pub struct FileBrowser {
    popup: *mut G2Popup,
    hidden: bool,
    start: Vec<u32>,
    pub cats: Vec<Category>,
    pub path: Vec<Vec<u32>>,
    pub roots: Vec<Vec<u32>>,
    pub files: Vec<Vec<u32>>,
    pub save_dest: usize,
    pub load_dest: usize,
    pub up_gfx: usize,
    pub icons: [usize; 6],
}

impl FileBrowser {
    pub fn new(popup: *mut G2Popup, start: &[u32], hidden: bool) -> FileBrowser {
        let mut roots = vec![ws("<Personal Folder>")];
        let mut letters = [0u32; 64];
        // SAFETY: plain call.
        let n = wb::drive_letters(unsafe { ohpl_g2fb_drives() }, &mut letters);
        for &l in letters.iter().take(n) {
            let mut d = ws("<Disk Drive> A:");
            if let Some(c) = d.get_mut(13) {
                *c = l;
            }
            roots.push(d);
        }
        FileBrowser {
            popup,
            hidden,
            start: s::file_path(start).to_vec(),
            cats: Vec::new(),
            path: Vec::new(),
            roots,
            files: Vec::new(),
            save_dest: 0,
            load_dest: 0,
            up_gfx: 0,
            icons: [0; 6],
        }
    }

    pub fn add_category(&mut self, name: &[u32], filter: &[u32]) -> (i32, bool) {
        self.cats.push(Category {
            name: name.to_vec(),
            filters: Vec::new(),
        });
        let idx = self.cats.len() as i32 - 1;
        let added = !filter.is_empty() && self.add_filter_state(idx, filter);
        (idx, added)
    }

    fn add_filter_state(&mut self, cat: i32, filter: &[u32]) -> bool {
        match usize::try_from(cat).ok().and_then(|i| self.cats.get_mut(i)) {
            Some(c) => {
                c.filters.push(filter.to_vec());
                true
            }
            None => false,
        }
    }

    pub fn full_path(&self) -> Vec<u32> {
        let mut p = vec![SLASH];
        for d in &self.path {
            p.extend(s::add_slash_at_end(d, SLASH));
        }
        p
    }

    /// Moves to `p`: whether it worked and whether the popup is told (`OnNavigate`).
    fn resolve(&mut self, p: &[u32]) -> (bool, bool) {
        let mut p = p.to_vec();
        loop {
            match wb::navigate_target(&p) {
                NavTarget::SystemRoot => self.path.clear(),
                NavTarget::GameFolder => self.set_path(&dir(0)),
                NavTarget::PersonalFolder => {
                    let mut v = special(0);
                    v.extend(dir(1));
                    self.set_path(&v);
                }
                NavTarget::DiskDrive => {
                    p = s::sub(&p, 13, -1, true).unwrap_or(&[]).to_vec();
                    continue;
                }
                NavTarget::Up => {
                    self.path.pop();
                }
                NavTarget::Here => {}
                NavTarget::Path => {
                    let mut full = self.full_path();
                    full.extend_from_slice(&p);
                    let mut rel = s::add_slash_at_end(&special(1), SLASH);
                    rel.extend_from_slice(&p);
                    let child = if self.path.is_empty() {
                        p.first().is_some_and(|&c| c != SLASH)
                    } else {
                        true
                    };
                    if child && exists(true, &full) {
                        self.path.push(p);
                    } else if exists(true, &rel) {
                        self.set_path(&rel);
                    } else if exists(true, &p) {
                        self.set_path(&p);
                    } else {
                        return (false, false);
                    }
                }
            }
            return (true, true);
        }
    }

    fn set_path(&mut self, p: &[u32]) {
        let p = s::replace_char_to(p, &[BACKSLASH], &[SLASH]);
        if !(exists(true, &p) || p.is_empty()) {
            return;
        }
        self.path.clear();
        for piece in s::split(&p, Some(&[SLASH]), false) {
            match wb::path_piece(piece, self.path.len()) {
                PathPiece::Pop => {
                    self.path.pop();
                }
                PathPiece::Push => self.path.push(piece.to_vec()),
                PathPiece::Skip => {}
            }
        }
    }

    /// Folders then the sorted files of the category, stored for [`row`](Self::row); the count.
    pub fn list_current(&mut self, cat: i32) -> usize {
        let Some(c) = usize::try_from(cat).ok().and_then(|i| self.cats.get(i)) else {
            return self.files.len();
        };
        let dir = self.full_path();
        let mut out = list(1, &dir, &[], self.hidden);
        let mut files: Vec<Vec<u32>> = Vec::new();
        for f in &c.filters {
            files.extend(list(0, &dir, f, self.hidden));
        }
        files.sort();
        out.extend(files);
        self.files = out;
        self.files.len()
    }

    /// File type, name, size text and date text of listed entry `idx`.
    pub fn row(&self, idx: usize) -> (i32, Vec<u32>, Vec<u32>, Vec<u32>) {
        let name = self.files.get(idx).cloned().unwrap_or_default();
        let mut full = self.full_path();
        full.extend_from_slice(&name);
        let ty = if exists(true, &full) {
            5
        } else {
            let ext = s::to_lower(s::file_ext(&name));
            wb::file_type_of_extension(&ext).unwrap_or(4)
        };
        let mut size = 0u64;
        let mut date: Vec<Vec<u32>> = Vec::new();
        // SAFETY: `full` is readable; `push` fills `date`.
        unsafe {
            ohpl_g2fb_info(
                full.as_ptr(),
                full.len(),
                &mut size,
                push,
                (&raw mut date).cast(),
            );
        }
        let (n, unit) = wb::human_readable_size(size);
        let mut text = s::ulong_to_string_wide(n, 0)
            .iter()
            .map(|&b| u32::from(b))
            .collect::<Vec<_>>();
        text.push(u32::from(b' '));
        text.extend(ws(["bytes", "KB", "MB", "GB"]
            .get(unit)
            .copied()
            .unwrap_or("GB")));
        (ty, name, text, date.pop().unwrap_or_default())
    }
}

/// # Safety
/// `fb` is a live browser; no reference to it is held across the popup callbacks.
pub unsafe fn navigate_to(fb: *mut FileBrowser, p: &[u32]) -> bool {
    // SAFETY: caller contract.
    let (ok, notify) = unsafe { (*fb).resolve(p) };
    if notify {
        // SAFETY: the popup owns the browser and outlives it.
        unsafe { ohpl_g2_popup_on_navigate((*fb).popup) };
    }
    ok
}

/// # Safety
/// As [`navigate_to`].
pub unsafe fn navigate_up(fb: *mut FileBrowser) -> bool {
    // SAFETY: caller contract.
    unsafe { navigate_to(fb, &ws("..")) }
}

/// # Safety
/// As [`navigate_to`].
pub unsafe fn init(fb: *mut FileBrowser) {
    // SAFETY: caller contract.
    unsafe {
        let start = (*fb).start.clone();
        if !navigate_to(fb, &start) {
            navigate_to(fb, &ws("<System Root>"));
        }
    }
}

/// # Safety
/// As [`navigate_to`].
pub unsafe fn add_category(fb: *mut FileBrowser, name: &[u32], filter: &[u32]) -> i32 {
    // SAFETY: caller contract.
    let (idx, added) = unsafe { (*fb).add_category(name, filter) };
    if added {
        // SAFETY: the popup owns the browser.
        unsafe { ohpl_g2_popup_on_add_filter((*fb).popup) };
    }
    idx
}

/// # Safety
/// As [`navigate_to`].
pub unsafe fn add_filter(fb: *mut FileBrowser, cat: i32, filter: &[u32]) {
    // SAFETY: caller contract.
    if unsafe { (*fb).add_filter_state(cat, filter) } {
        // SAFETY: the popup owns the browser.
        unsafe { ohpl_g2_popup_on_add_filter((*fb).popup) };
    }
}
