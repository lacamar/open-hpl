//! Resource registry data structures: file searcher, resource map, loader dispatch.
//! Resource objects and loaders stay C++ and are opaque pointers here.

pub mod config;
pub mod ffi;
#[cfg(test)]
mod golden_tests;
pub mod manager;
pub mod manager_ffi;
#[cfg(test)]
mod manager_golden;
pub mod searcher;

use crate::string;
use std::collections::BTreeMap;
use std::ffi::c_void;

struct SearchEntry {
    id: u32,
    dirs: Vec<Vec<u32>>,
}

/// `std::multimap<lowercase name, entry>`: equal keys keep insertion order, keys sort bytewise.
/// Entry ids index the path strings kept by the C++ side.
#[derive(Default)]
pub struct FileSearcher {
    files: BTreeMap<Vec<u8>, Vec<SearchEntry>>,
    next_id: u32,
}

pub struct Found {
    pub id: u32,
    /// `Some` where C++ writes `*apEqualCount`.
    pub equal_count: Option<i32>,
}

const SEPS: [u32; 2] = ['/' as u32, '\\' as u32];

fn dirs_of(path: &[u32]) -> Vec<Vec<u32>> {
    string::split(path, Some(&SEPS), true)
        .map(<[u32]>::to_vec)
        .collect()
}

/// Wanted dirs matched while scanning the entry's dirs from the back.
fn common_suffix(wanted: &[Vec<u32>], entry: &[Vec<u32>]) -> i32 {
    let mut count = 0;
    let mut remaining = wanted.iter().rev().peekable();
    for dir in entry.iter().rev() {
        let Some(want) = remaining.peek() else {
            break;
        };
        if *want == dir {
            count += 1;
            remaining.next();
        }
    }
    count
}

impl FileSearcher {
    /// Skipped (`None`) when the first entry under `key` already has this path
    /// (`same_as(id)`); otherwise the new entry id.
    pub fn add(
        &mut self,
        key: &[u8],
        path: &[u32],
        same_as: impl FnOnce(u32) -> bool,
    ) -> Option<u32> {
        let list = self.files.entry(key.to_vec()).or_default();
        if list.first().is_some_and(|e| same_as(e.id)) {
            return None;
        }
        let id = self.next_id;
        self.next_id += 1;
        list.push(SearchEntry {
            id,
            dirs: dirs_of(path),
        });
        Some(id)
    }

    pub fn clear(&mut self) {
        self.files.clear();
        self.next_id = 0;
    }

    /// `None` when no file has that name.
    pub fn get(&self, name_and_path: &[u8], want_count: bool) -> Option<Found> {
        let low = string::to_lower(string::file_name(name_and_path));
        let list = self.files.get(&low)?;
        let first = list.first()?.id;
        if list.len() == 1 && !want_count {
            return Some(Found {
                id: first,
                equal_count: None,
            });
        }
        let wanted = string::utf8_to_wide(string::file_path(name_and_path));
        if wanted.is_empty() {
            return Some(Found {
                id: first,
                equal_count: None,
            });
        }
        let wanted = dirs_of(&wanted);
        let mut best = 0;
        let mut best_id = first;
        for e in list {
            let c = common_suffix(&wanted, &e.dirs).max(common_suffix(&e.dirs, &wanted));
            if c > best {
                best = c;
                best_id = e.id;
            }
        }
        Some(Found {
            id: best_id,
            equal_count: want_count.then_some(best),
        })
    }
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct MapEntry {
    pub first: u32,
    pub second: *mut c_void,
}

/// `std::multimap<unsigned, iResourceBase*>` as a sorted vector; equal keys keep insertion order.
// ponytail: O(n) insert; a B-tree with index access if a manager ever holds >50k resources.
#[derive(Default)]
pub struct ResMap {
    v: Vec<MapEntry>,
}

impl ResMap {
    pub fn len(&self) -> usize {
        self.v.len()
    }

    pub fn is_empty(&self) -> bool {
        self.v.is_empty()
    }

    pub fn at(&mut self, i: usize) -> *mut MapEntry {
        self.v
            .get_mut(i)
            .map_or(std::ptr::null_mut(), |e| e as *mut MapEntry)
    }

    pub fn insert(&mut self, hash: u32, res: *mut c_void) {
        let pos = self.v.partition_point(|e| e.first <= hash);
        self.v.insert(
            pos,
            MapEntry {
                first: hash,
                second: res,
            },
        );
    }

    fn range(&self, hash: u32) -> std::ops::Range<usize> {
        let lo = self.v.partition_point(|e| e.first < hash);
        let hi = self.v.partition_point(|e| e.first <= hash);
        lo..hi
    }

    pub fn find(&self, hash: u32, mut matches: impl FnMut(*mut c_void) -> bool) -> *mut c_void {
        self.v
            .get(self.range(hash))
            .unwrap_or_default()
            .iter()
            .map(|e| e.second)
            .find(|&r| matches(r))
            .unwrap_or(std::ptr::null_mut())
    }

    /// Erases the first entry under `hash` holding `res`.
    pub fn remove(&mut self, hash: u32, res: *mut c_void) -> bool {
        let r = self.range(hash);
        let found = self
            .v
            .get(r.clone())
            .unwrap_or_default()
            .iter()
            .position(|e| e.second == res);
        match found {
            Some(p) => {
                self.v.remove(r.start + p);
                true
            }
            None => false,
        }
    }

    pub fn entries(&self) -> impl Iterator<Item = *mut c_void> {
        self.v.iter().map(|e| e.second)
    }
}

/// libstdc++ `std::sort` (introsort + insertion sort), which leaves equal elements in an
/// order DestroyUnused depends on.
pub(crate) fn std_sort<T: Copy>(v: &mut [T], less: &mut impl FnMut(T, T) -> bool) {
    let n = v.len();
    if n < 2 {
        return;
    }
    let depth = 2 * (usize::BITS - 1 - n.leading_zeros()) as usize;
    introsort_loop(v, 0, n, depth, less);
    if n > 16 {
        insertion_sort(v, 0, 16, less);
        for i in 16..n {
            unguarded_linear_insert(v, i, less);
        }
    } else {
        insertion_sort(v, 0, n, less);
    }
}

fn introsort_loop<T: Copy>(
    v: &mut [T],
    first: usize,
    mut last: usize,
    mut depth: usize,
    less: &mut impl FnMut(T, T) -> bool,
) {
    while last - first > 16 {
        if depth == 0 {
            if let Some(part) = v.get_mut(first..last) {
                heap_sort(part, less);
            }
            return;
        }
        depth -= 1;
        let mid = first + ((last - first) >> 1);
        move_median_to_first(v, first, first + 1, mid, last - 1, less);
        let cut = unguarded_partition(v, first + 1, last, first, less);
        introsort_loop(v, cut, last, depth, less);
        last = cut;
    }
}

#[expect(
    clippy::indexing_slicing,
    reason = "std_sort's comparator is a strict weak order, so the partition and insertion sentinels bound every index"
)]
fn move_median_to_first<T: Copy>(
    v: &mut [T],
    result: usize,
    a: usize,
    b: usize,
    c: usize,
    less: &mut impl FnMut(T, T) -> bool,
) {
    let pick = if less(v[a], v[b]) {
        if less(v[b], v[c]) {
            b
        } else if less(v[a], v[c]) {
            c
        } else {
            a
        }
    } else if less(v[a], v[c]) {
        a
    } else if less(v[b], v[c]) {
        c
    } else {
        b
    };
    v.swap(result, pick);
}

#[expect(
    clippy::indexing_slicing,
    reason = "std_sort's comparator is a strict weak order, so the partition and insertion sentinels bound every index"
)]
fn unguarded_partition<T: Copy>(
    v: &mut [T],
    mut first: usize,
    mut last: usize,
    pivot: usize,
    less: &mut impl FnMut(T, T) -> bool,
) -> usize {
    loop {
        while less(v[first], v[pivot]) {
            first += 1;
        }
        last -= 1;
        while less(v[pivot], v[last]) {
            last -= 1;
        }
        if first >= last {
            return first;
        }
        v.swap(first, last);
        first += 1;
    }
}

#[expect(
    clippy::indexing_slicing,
    reason = "std_sort's comparator is a strict weak order, so the partition and insertion sentinels bound every index"
)]
fn insertion_sort<T: Copy>(
    v: &mut [T],
    first: usize,
    last: usize,
    less: &mut impl FnMut(T, T) -> bool,
) {
    for i in first + 1..last {
        if less(v[i], v[first]) {
            let val = v[i];
            v.copy_within(first..i, first + 1);
            v[first] = val;
        } else {
            unguarded_linear_insert(v, i, less);
        }
    }
}

#[expect(
    clippy::indexing_slicing,
    reason = "std_sort's comparator is a strict weak order, so the partition and insertion sentinels bound every index"
)]
fn unguarded_linear_insert<T: Copy>(
    v: &mut [T],
    mut last: usize,
    less: &mut impl FnMut(T, T) -> bool,
) {
    let val = v[last];
    while less(val, v[last - 1]) {
        v[last] = v[last - 1];
        last -= 1;
    }
    v[last] = val;
}

#[expect(
    clippy::indexing_slicing,
    reason = "std_sort's comparator is a strict weak order, so the partition and insertion sentinels bound every index"
)]
fn heap_sort<T: Copy>(v: &mut [T], less: &mut impl FnMut(T, T) -> bool) {
    let len = v.len();
    if len >= 2 {
        let mut parent = (len - 2) >> 1;
        loop {
            let value = v[parent];
            adjust_heap(v, parent, len, value, less);
            if parent == 0 {
                break;
            }
            parent -= 1;
        }
    }
    for end in (2..=len).rev() {
        let last = end - 1;
        let val = v[last];
        v[last] = v[0];
        adjust_heap(v, 0, last, val, less);
    }
}

#[expect(
    clippy::indexing_slicing,
    reason = "std_sort's comparator is a strict weak order, so the partition and insertion sentinels bound every index"
)]
fn adjust_heap<T: Copy>(
    v: &mut [T],
    mut hole: usize,
    len: usize,
    value: T,
    less: &mut impl FnMut(T, T) -> bool,
) {
    let top = hole;
    let mut child = hole;
    while child < len.saturating_sub(1) >> 1 {
        child = 2 * (child + 1);
        if less(v[child], v[child - 1]) {
            child -= 1;
        }
        v[hole] = v[child];
        hole = child;
    }
    if len & 1 == 0 && child == (len - 2) >> 1 {
        child = 2 * (child + 1);
        v[hole] = v[child - 1];
        hole = child - 1;
    }
    let mut parent = hole.wrapping_sub(1) >> 1;
    while hole > top && less(v[parent], value) {
        v[hole] = v[parent];
        hole = parent;
        parent = hole.wrapping_sub(1) >> 1;
    }
    v[hole] = value;
}

/// Eviction order of `DestroyUnused`: most users first, then newest.
pub fn eviction_order(
    res: &ResMap,
    mut key: impl FnMut(*mut c_void) -> (u32, u64),
) -> Vec<*mut c_void> {
    let ptrs: Vec<*mut c_void> = res.entries().collect();
    let keys: Vec<(u32, u64)> = ptrs.iter().map(|&r| key(r)).collect();
    let mut order: Vec<usize> = (0..ptrs.len()).collect();
    std_sort(&mut order, &mut |a, b| {
        let (Some(&x), Some(&y)) = (keys.get(a), keys.get(b)) else {
            return false;
        };
        if x.0 != y.0 { x.0 > y.0 } else { x.1 > y.1 }
    });
    order
        .into_iter()
        .filter_map(|i| ptrs.get(i).copied())
        .collect()
}

/// Extensions a loader supports, in the order they were added.
#[derive(Default)]
pub struct ExtList(pub Vec<Vec<u8>>);

impl ExtList {
    pub fn supports(&self, ext: &[u8]) -> bool {
        self.0.iter().any(|e| e == ext)
    }
}

/// Loaders in registration order; dispatch picks the first whose extensions match.
#[derive(Default)]
pub struct LoaderTable {
    loaders: Vec<(*mut c_void, *const ExtList)>,
}

impl LoaderTable {
    /// # Safety
    /// `exts` outlives every later `find` on this table.
    pub unsafe fn add(&mut self, loader: *mut c_void, exts: *const ExtList) {
        self.loaders.push((loader, exts));
    }

    pub fn len(&self) -> usize {
        self.loaders.len()
    }

    pub fn is_empty(&self) -> bool {
        self.loaders.is_empty()
    }

    pub fn at(&self, i: usize) -> *mut c_void {
        self.loaders.get(i).map_or(std::ptr::null_mut(), |l| l.0)
    }

    /// # Safety
    /// Every registered `ExtList` pointer is still live.
    pub unsafe fn find(&self, ext: &[u8]) -> *mut c_void {
        self.loaders
            .iter()
            // SAFETY: caller contract.
            .find(|(_, e)| unsafe { &**e }.supports(ext))
            .map_or(std::ptr::null_mut(), |l| l.0)
    }
}

/// Bookkeeping embedded by value in `iResourceBase`; layout mirrored in RustResources.h.
#[repr(C)]
pub struct ResMeta {
    pub handle: u64,
    pub time: u64,
    pub size: u64,
    pub prio: u32,
    pub user_count: u32,
    pub log_destruction: bool,
}

impl ResMeta {
    pub fn new(prio: u32, now: u64) -> Self {
        Self {
            handle: 0,
            time: now,
            size: 0,
            prio,
            user_count: 0,
            log_destruction: false,
        }
    }

    pub fn inc_user(&mut self, now: u64) {
        self.user_count += 1;
        self.time = now;
    }

    pub fn dec_user(&mut self) {
        self.user_count = self.user_count.saturating_sub(1);
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn ptr(n: usize) -> *mut c_void {
        std::ptr::without_provenance_mut(n)
    }

    type Case = (&'static str, bool, Option<(u32, Option<i32>)>);

    fn w(s: &str) -> Vec<u32> {
        s.chars().map(u32::from).collect()
    }

    fn keys(n: usize) -> Vec<u32> {
        let mut s: u32 = 12345;
        (0..n)
            .map(|_| {
                s = s.wrapping_mul(1103515245).wrapping_add(12345);
                (s >> 16) % 4
            })
            .collect()
    }

    // Expected orders recorded from libstdc++ std::sort / make_heap+sort_heap on the same keys.
    #[test]
    fn sort_matches_libstdcxx_on_ties() {
        let cases: [(usize, &[usize]); 3] = [
            (5, &[4, 3, 2, 0, 1]),
            (
                17,
                &[14, 13, 4, 11, 3, 12, 7, 8, 16, 9, 6, 5, 2, 10, 1, 15, 0],
            ),
            (
                40,
                &[
                    20, 39, 37, 36, 33, 30, 22, 14, 13, 11, 4, 31, 12, 8, 7, 29, 38, 3, 34, 35, 21,
                    5, 28, 24, 6, 2, 18, 16, 9, 1, 32, 10, 27, 26, 25, 23, 0, 19, 17, 15,
                ],
            ),
        ];
        for (n, want) in cases {
            let k = keys(n);
            let mut v: Vec<usize> = (0..n).collect();
            std_sort(&mut v, &mut |a, b| k[a] > k[b]);
            assert_eq!(v, want, "n={n}");
        }
    }

    #[test]
    fn heap_sort_matches_libstdcxx_on_ties() {
        let cases: [(usize, &[usize]); 2] = [
            (
                17,
                &[4, 14, 11, 13, 7, 8, 3, 12, 16, 9, 5, 2, 6, 15, 0, 1, 10],
            ),
            (
                40,
                &[
                    11, 22, 33, 13, 39, 36, 20, 30, 4, 37, 14, 7, 31, 38, 35, 8, 34, 3, 12, 29, 16,
                    18, 9, 21, 24, 5, 2, 6, 28, 15, 32, 17, 1, 19, 10, 23, 0, 25, 26, 27,
                ],
            ),
        ];
        for (n, want) in cases {
            let k = keys(n);
            let mut v: Vec<usize> = (0..n).collect();
            heap_sort(&mut v, &mut |a, b| k[a] > k[b]);
            assert_eq!(v, want, "n={n}");
        }
    }

    fn searcher() -> FileSearcher {
        let mut fs = FileSearcher::default();
        for (key, path) in [
            ("a.png", "/g/textures/a/a.png"),
            ("a.png", "/g/textures/b/a.png"),
            ("a.png", "/g/other/c/b/a.png"),
            ("solo.txt", "/g/x/solo.txt"),
        ] {
            assert!(fs.add(key.as_bytes(), &w(path), |_| false).is_some());
        }
        fs
    }

    #[test]
    fn lookup_picks_best_matching_dirs() {
        let fs = searcher();
        let cases: [Case; 9] = [
            ("a.png", false, Some((0, None))),
            ("A.PNG", true, Some((0, None))),
            ("b/a.png", false, Some((1, None))),
            ("b/a.png", true, Some((1, Some(1)))),
            ("textures/b/a.png", true, Some((1, Some(2)))),
            ("c/b/a.png", true, Some((2, Some(2)))),
            ("zz/a.png", true, Some((0, Some(0)))),
            ("solo.txt", false, Some((3, None))),
            ("nope.png", true, None),
        ];
        for (q, want_count, want) in cases {
            let got = fs
                .get(q.as_bytes(), want_count)
                .map(|f| (f.id, f.equal_count));
            assert_eq!(got, want, "query {q:?}");
        }
    }

    #[test]
    fn add_skips_only_when_first_entry_has_same_path() {
        let mut fs = searcher();
        assert_eq!(
            fs.add(b"a.png", &w("/g/textures/a/a.png"), |id| id == 0),
            None
        );
        assert_eq!(
            fs.add(b"a.png", &w("/g/other/c/b/a.png"), |id| id == 1),
            Some(4)
        );
        fs.clear();
        assert!(fs.get(b"a.png", false).is_none());
        assert_eq!(fs.add(b"a.png", &w("/x/a.png"), |_| false), Some(0));
    }

    #[test]
    fn res_map_keeps_insertion_order_within_a_hash() {
        let p = ptr;
        let mut m = ResMap::default();
        for (h, r) in [(5, 1), (3, 2), (5, 3), (5, 4), (1, 5)] {
            m.insert(h, p(r));
        }
        assert_eq!(
            m.entries().collect::<Vec<_>>(),
            [p(5), p(2), p(1), p(3), p(4)]
        );
        assert_eq!(m.find(5, |r| r == p(3)), p(3));
        assert!(m.find(5, |r| r == p(2)).is_null());
        assert!(m.remove(5, p(3)));
        assert!(!m.remove(5, p(3)));
        assert_eq!(m.len(), 4);
    }

    #[test]
    fn eviction_order_is_users_then_newest() {
        let p = ptr;
        let mut m = ResMap::default();
        for r in 1..=4 {
            m.insert(r as u32, p(r));
        }
        let order = eviction_order(&m, |r| match r as usize {
            1 => (0, 10),
            2 => (2, 5),
            3 => (0, 20),
            _ => (2, 5),
        });
        assert_eq!(order, [p(2), p(4), p(3), p(1)]);
    }

    #[test]
    fn loader_dispatch_takes_first_match() {
        let (a, b) = (
            ExtList(vec![b"png".to_vec(), b"jpg".to_vec()]),
            ExtList(vec![b"jpg".to_vec()]),
        );
        let mut t = LoaderTable::default();
        // SAFETY: `a` and `b` outlive the table.
        unsafe {
            t.add(ptr(1), &a);
            t.add(ptr(2), &b);
            assert_eq!(t.find(b"jpg"), ptr(1));
            assert!(t.find(b"JPG").is_null());
            assert!(t.find(b"").is_null());
        }
        assert_eq!(t.at(1), ptr(2));
    }

    #[test]
    fn user_count_saturates_and_inc_stamps_time() {
        let mut m = ResMeta::new(3, 100);
        m.dec_user();
        assert_eq!((m.user_count, m.prio, m.time, m.size), (0, 3, 100, 0));
        m.inc_user(200);
        m.inc_user(300);
        m.dec_user();
        assert_eq!((m.user_count, m.time), (1, 300));
    }
}
