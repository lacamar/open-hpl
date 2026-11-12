//! Profile window rules: list order, name trimming and the create/delete/select decisions.

use crate::graphics::stdsort;

pub const ILLEGAL_CHARS: [u32; 9] = [
    '*' as u32,
    '/' as u32,
    '\\' as u32,
    ':' as u32,
    '<' as u32,
    '>' as u32,
    '|' as u32,
    '"' as u32,
    '?' as u32,
];
pub const NAME_MAX: i32 = 20;

#[repr(C)]
#[derive(Clone, Copy, Default, Debug, PartialEq, Eq)]
pub struct Date {
    pub seconds: i32,
    pub minutes: i32,
    pub hours: i32,
    pub month_day: i32,
    pub month: i32,
    pub year: i32,
}

fn key(d: &Date) -> [i32; 6] {
    [d.year, d.month, d.month_day, d.hours, d.minutes, d.seconds]
}

pub fn date_less(a: &Date, b: &Date) -> bool {
    key(a) < key(b)
}

/// `std::sort` order of the profile folders by creation date.
pub fn sort_order(dates: &[Date]) -> Vec<u32> {
    let mut idx: Vec<u32> = (0..dates.len() as u32).collect();
    stdsort::sort(&mut idx, |&a, &b| {
        match (dates.get(a as usize), dates.get(b as usize)) {
            (Some(x), Some(y)) => date_less(x, y),
            _ => false,
        }
    });
    idx
}

/// Range of `name` left after dropping leading and trailing spaces.
pub fn trim(name: &[u32]) -> (usize, usize) {
    let sp = ' ' as u32;
    let start = name.iter().take_while(|&&c| c == sp).count();
    let rest = name.get(start..).unwrap_or(&[]);
    let end = start + rest.len() - rest.iter().rev().take_while(|&&c| c == sp).count();
    (start, end)
}

/// Index to select, or `None` when nothing is selected or it is past the list.
pub fn select_target(num: i32, count: usize) -> Option<usize> {
    usize::try_from(num).ok().filter(|&n| n < count)
}

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub struct DeletePlan {
    pub reset_profile: bool,
    pub remove_config: bool,
    pub new_selection: i32,
}

pub fn delete_plan(selected: i32, in_use: bool, config_path_differs: bool) -> DeletePlan {
    DeletePlan {
        reset_profile: in_use,
        remove_config: config_path_differs,
        new_selection: selected.wrapping_sub(1),
    }
}

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum Activate {
    CreateWindow,
    LockList,
}

pub fn activate(item_num: i32) -> Activate {
    if item_num == 0 {
        Activate::CreateWindow
    } else {
        Activate::LockList
    }
}

/// `WindowUIPress`: the secondary button closes the window unless the list or name box is active.
pub fn window_ui_press(is_secondary: bool, list_locked: bool, name_visible: bool) -> bool {
    is_secondary && !list_locked && !name_visible
}
