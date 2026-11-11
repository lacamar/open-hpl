//! `LuxSaveHandler.cpp`: save-slot naming and autosave rotation.

use crate::string;

#[repr(C)]
#[derive(Clone, Copy, Default, PartialEq, Eq, Debug)]
pub struct Date {
    pub seconds: i32,
    pub minutes: i32,
    pub hours: i32,
    pub month_day: i32,
    pub month: i32,
    pub year: i32,
}

impl Date {
    fn key(&self) -> [i32; 6] {
        [
            self.year,
            self.month,
            self.month_day,
            self.hours,
            self.minutes,
            self.seconds,
        ]
    }

    /// `cDate::operator<`.
    pub fn before(&self, o: &Date) -> bool {
        self.key() < o.key()
    }

    /// `cDate::operator>`.
    pub fn after(&self, o: &Date) -> bool {
        self.key() > o.key()
    }
}

#[repr(C)]
#[derive(Default)]
pub struct NameState {
    pub count: i32,
    pub latest: Date,
}

fn widen(s: &[u8]) -> Vec<u32> {
    s.iter().map(|&c| u32::from(c)).collect()
}

fn w(s: &str) -> Vec<u32> {
    widen(s.as_bytes())
}

/// `GetSaveName`: `Prefix_map_Y_M_D_h_m_s_n.sav`; `d.month` is the 0-based tm month.
pub fn save_name(st: &mut NameState, prefix: &[u32], map_wide: &[u32], d: &Date) -> Vec<u32> {
    let mut name = prefix.to_vec();
    name.push(u32::from(b'_'));
    if map_wide.is_empty() {
        name.extend(w("NULL"));
    } else {
        name.extend_from_slice(map_wide);
    }
    for x in [
        d.year,
        d.month.wrapping_add(1),
        d.month_day,
        d.hours,
        d.minutes,
        d.seconds,
        st.count,
    ] {
        name.push(u32::from(b'_'));
        name.extend(widen(&string::int_to_string(x, 0)));
    }
    st.count = st.count.wrapping_add(1);
    if st.count >= 100 || *d != st.latest {
        st.count = 0;
    }
    st.latest = *d;
    name.extend(w(".sav"));
    name
}

/// `GetProperSaveName`: `Prefix_map_Y_M_D_h_m_s_n` becomes `Prefix - Level - D/M-Y h:mm:ss`.
pub fn proper_name(file: &[u32], translate_level: &mut dyn FnMut(&[u32]) -> Vec<u32>) -> Vec<u32> {
    let sep = [u32::from(b'_')];
    let mut parts: Vec<Vec<u32>> = string::split(file, Some(&sep), true)
        .map(<[u32]>::to_vec)
        .collect();
    if parts.len() != 9 {
        return w("INVALID SAVE FILE SYNTAX");
    }
    let g = |parts: &[Vec<u32>], i: usize| parts.get(i).cloned().unwrap_or_default();
    let mut name = g(&parts, 0);
    name.extend(w(" - "));
    name.extend(translate_level(parts.get(1).map_or(&[], Vec::as_slice)));
    name.extend(w(" - "));
    for p in parts.iter_mut().skip(5).take(3) {
        if p.len() == 1 {
            p.insert(0, u32::from(b'0'));
        }
    }
    name.extend(g(&parts, 4));
    for (s, i) in [('/', 3), ('-', 2), (' ', 5), (':', 6), (':', 7)] {
        name.push(u32::from(s));
        name.extend(g(&parts, i));
    }
    name
}

/// `GetNewestSaveFile`: first file with the strictly latest date; `None` for an empty list.
pub fn newest(dates: &[Date]) -> Option<usize> {
    let mut best: Option<usize> = None;
    for (i, d) in dates.iter().enumerate() {
        let better = match best.and_then(|b| dates.get(b)) {
            None => true,
            Some(cur) => d.after(cur),
        };
        if better {
            best = Some(i);
        }
    }
    best
}

/// `DeleteOldestSaveFiles`: file indices in the order they are removed. `dates` are creation
/// dates; the original re-reads them every pass, the result is the same.
pub fn rotation_victims(dates: &[Date], max: i32) -> Vec<usize> {
    let mut left: Vec<usize> = (0..dates.len()).collect();
    let mut out = Vec::new();
    let mut last = 0usize;
    while i32::try_from(left.len()).is_ok_and(|n| n >= max) && last != left.len() {
        let mut oldest: Option<usize> = None;
        for (pos, &f) in left.iter().enumerate() {
            let Some(d) = dates.get(f) else { continue };
            let older = match oldest
                .and_then(|o| left.get(o))
                .and_then(|&of| dates.get(of))
            {
                None => true,
                Some(cur) => d.before(cur),
            };
            if older {
                oldest = Some(pos);
            }
        }
        last = left.len();
        if let Some(pos) = oldest {
            out.push(left.remove(pos));
        }
    }
    out
}

/// `AutoLoad` hard-mode gate: a hard-mode game may only load a file named `HardMode...`.
pub fn hardmode_file_ok(file: &[u32]) -> bool {
    let n = w("HardMode");
    file.windows(n.len()).any(|x| x == n.as_slice())
}
