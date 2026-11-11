//! Save-slot naming, listing, sorting and autosave rotation (`SomaSave.cpp`).

use crate::graphics::stdsort;
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
    /// `cDate::operator<`: field by field, year first.
    pub fn before(&self, o: &Date) -> bool {
        let a = [
            self.year,
            self.month,
            self.month_day,
            self.hours,
            self.minutes,
            self.seconds,
        ];
        let b = [o.year, o.month, o.month_day, o.hours, o.minutes, o.seconds];
        a < b
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
    s.bytes().map(u32::from).collect()
}

/// `GetSaveName`: "Prefix_map_Y_M_D_h_m_s_n[_suffix].sav", `month` is the 0-based tm month.
pub fn save_name(
    st: &mut NameState,
    prefix: &[u32],
    map_display: &[u8],
    d: &Date,
    suffix: &[u32],
) -> Vec<u32> {
    let map = if map_display.is_empty() {
        &b"NULL"[..]
    } else {
        map_display
    };
    let mut name = prefix.to_vec();
    name.push(u32::from(b'_'));
    name.extend(string::utf8_to_wide(map));
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
    st.count += 1;
    if st.count >= 100 || *d != st.latest {
        st.count = 0;
    }
    st.latest = *d;
    if !suffix.is_empty() {
        name.push(u32::from(b'_'));
        name.extend_from_slice(suffix);
    }
    name.extend(w(".sav"));
    name
}

pub trait NamesHost {
    /// `Translate("Levels", key)`.
    fn translate_level(&mut self, key: &[u8]) -> Vec<u32>;
    /// Local time of the file's mtime: day, month, year, hour, minute, second.
    fn local_mtime(&mut self, file: &[u32]) -> [i32; 6];
}

pub struct SaveInfo {
    /// Index in the input list.
    pub src: usize,
    pub date: Date,
    pub file: Vec<u32>,
    pub name: Vec<u32>,
    pub suffix: Vec<u32>,
    pub date_str: Vec<u8>,
}

/// `GetProperSaveName`: HPL3-style "Prefix_map_Y_M_D_h_m_s_n[_suffix]" slots get a readable title.
pub fn proper_name(h: &mut dyn NamesHost, file: &[u32]) -> (Vec<u32>, Vec<u8>, Vec<u32>) {
    let base = string::set_file_ext(file, &[]);
    let sep = [u32::from(b'_')];
    let mut parts: Vec<Vec<u32>> = string::split(&base, Some(&sep), true)
        .map(<[u32]>::to_vec)
        .collect();
    let suffix = if parts.len() == 10 {
        parts.get(9).cloned().unwrap_or_default()
    } else {
        Vec::new()
    };
    if parts.len() != 9 && parts.len() != 10 {
        let [dd, mo, yy, hh, mi, ss] = h.local_mtime(file);
        let date = format!("{dd}/{mo}/{yy} {hh}:{mi:02}:{ss:02}");
        return (base, date.into_bytes(), suffix);
    }
    let key = string::wide_to_utf8(parts.get(1).map_or(&[], Vec::as_slice));
    let mut name = parts.first().cloned().unwrap_or_default();
    name.extend(w(" - "));
    name.extend(h.translate_level(&key));
    name.extend(w(" - "));
    for p in parts.iter_mut().skip(5).take(3) {
        if p.len() == 1 {
            p.insert(0, u32::from(b'0'));
        }
    }
    let g = |i: usize| parts.get(i).cloned().unwrap_or_default();
    let mut d = g(4);
    for (sep, i) in [('/', 3), ('-', 2), (' ', 5), (':', 6), (':', 7)] {
        d.push(sep as u32);
        d.extend(g(i));
    }
    (name, string::wide_to_utf8(&d), suffix)
}

/// `ListSaves`: newest first; ties keep libstdc++ `std::sort` order.
pub fn list_saves(h: &mut dyn NamesHost, files: &[(Vec<u32>, Date)]) -> Vec<SaveInfo> {
    let mut infos: Vec<SaveInfo> = files
        .iter()
        .enumerate()
        .map(|(src, (f, d))| {
            let (name, date_str, suffix) = proper_name(h, f);
            SaveInfo {
                src,
                date: *d,
                file: f.clone(),
                name,
                suffix,
                date_str,
            }
        })
        .collect();
    let mut order: Vec<(usize, Date)> =
        infos.iter().enumerate().map(|(i, s)| (i, s.date)).collect();
    stdsort::sort(&mut order, |a, b| b.1.before(&a.1));
    let mut slots: Vec<Option<SaveInfo>> = infos.drain(..).map(Some).collect();
    order
        .iter()
        .filter_map(|&(i, _)| slots.get_mut(i).and_then(Option::take))
        .collect()
}

/// `DeleteOldestSaveFiles`: indices (into the sorted list) to remove.
pub fn rotation_victims(sorted_suffixes: &[&[u32]], suffix: &[u32], max: i32) -> Vec<usize> {
    let mut count = 0i32;
    let mut out = Vec::new();
    for (i, s) in sorted_suffixes.iter().enumerate() {
        if *s == suffix {
            count += 1;
            if count >= max {
                out.push(i);
            }
        }
    }
    out
}
