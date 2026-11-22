//! gcc layout (class size, member offset and size) of the remaining Amnesia save-data classes and
//! the engine classes `cScriptVar`, `cAreaEntity`, `cStartPosEntity`. Recorded from the original
//! C++ `kBeginSerialize` tables; the file is the layout authority now that those tables are gone.

use std::collections::HashMap;
use std::sync::OnceLock;

type Class = (usize, HashMap<&'static str, (usize, usize)>);
type Layout = HashMap<&'static str, Class>;

fn table() -> &'static Layout {
    static T: OnceLock<Layout> = OnceLock::new();
    T.get_or_init(|| {
        let mut t = Layout::new();
        for l in include_str!("layout_gcc.txt").lines() {
            let w: Vec<&str> = l.split(' ').collect();
            let num = |i: usize| w.get(i).and_then(|s| s.parse::<usize>().ok());
            match (w.first(), w.get(1), num(2)) {
                (Some(&"C"), Some(&c), Some(size)) => {
                    t.insert(c, (size, HashMap::new()));
                }
                (Some(&"F"), Some(&c), _) => {
                    if let (Some(f), Some(off), Some(sz), Some(e)) =
                        (w.get(2), num(3), num(4), t.get_mut(c))
                    {
                        e.1.insert(f, (off, sz));
                    }
                }
                _ => {}
            }
        }
        t
    })
}

fn class(name: &[u8]) -> Option<&'static Class> {
    table().get(std::str::from_utf8(name).ok()?)
}

pub fn size(name: &[u8]) -> Option<usize> {
    class(name).map(|c| c.0)
}

pub fn field(name: &[u8], member: &[u8]) -> Option<(usize, usize)> {
    class(name)?
        .1
        .get(std::str::from_utf8(member).ok()?)
        .copied()
}

pub fn classes() -> impl Iterator<Item = &'static str> {
    table().keys().copied()
}
