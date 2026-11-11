//! `.snt` sound entity files (`cSoundEntityData::CreateFromFile`) and its random sound pick.

use crate::math::rand_rectl;
use crate::string::{to_bool, token_to_float, token_to_int};
use crate::xml::{DOC, Doc, NodeId, c_str};

#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct Props {
    pub volume: f32,
    pub max_distance: f32,
    pub min_distance: f32,
    pub block_volume_mul: f32,
    pub random: f32,
    pub interval: f32,
    pub priority: i32,
    pub use_3d: bool,
    pub looping: bool,
    pub stream: bool,
    pub blockable: bool,
    pub fade_start: bool,
    pub fade_stop: bool,
}

#[derive(Debug, PartialEq, Eq)]
pub enum LoadError {
    Xml,
    NoRoot,
    NoSounds,
    NoProperties,
}

/// The sound file names of the `Main`, `Start` and `Stop` lists, in that order.
pub type Names = [Vec<Vec<u8>>; 3];

/// Last duplicate wins, as the attribute map of the C++ DOM keeps it.
fn attr<'a>(doc: &'a Doc, id: NodeId, name: &[u8]) -> Option<&'a [u8]> {
    doc.attrs(id)
        .filter(|a| c_str(&a.name) == name)
        .last()
        .map(|a| c_str(&a.value))
}

fn attr_float(doc: &Doc, id: NodeId, name: &[u8], default: f32) -> f32 {
    attr(doc, id, name).map_or(default, token_to_float)
}

fn attr_int(doc: &Doc, id: NodeId, name: &[u8], default: i32) -> i32 {
    attr(doc, id, name).map_or(default, token_to_int)
}

fn attr_bool(doc: &Doc, id: NodeId, name: &[u8], default: bool) -> bool {
    attr(doc, id, name).map_or(default, to_bool)
}

fn names_in(doc: &Doc, parent: NodeId, tag: &[u8], out: &mut Vec<Vec<u8>>) {
    let Some(list) = doc.first_child_element(parent, Some(tag)) else {
        return;
    };
    let mut child = doc.first_child_element(list, None);
    while let Some(c) = child {
        out.push(attr(doc, c, b"File").unwrap_or_default().to_vec());
        child = doc.next_sibling_element(c, None);
    }
}

/// Fills `names` as soon as the `SOUNDS` element is read, so a missing `PROPERTIES` leaves them set.
pub fn parse(doc: &Doc, names: &mut Names) -> Result<Props, LoadError> {
    let root = doc
        .first_child_element(DOC, None)
        .ok_or(LoadError::NoRoot)?;
    let sounds = doc
        .first_child_element(root, Some(b"SOUNDS"))
        .ok_or(LoadError::NoSounds)?;
    let [main, start, stop] = names;
    names_in(doc, sounds, b"Main", main);
    names_in(doc, sounds, b"Start", start);
    names_in(doc, sounds, b"Stop", stop);

    let p = doc
        .first_child_element(root, Some(b"PROPERTIES"))
        .ok_or(LoadError::NoProperties)?;
    Ok(Props {
        use_3d: attr_bool(doc, p, b"Use3D", true),
        looping: attr_bool(doc, p, b"Loop", true),
        stream: attr_bool(doc, p, b"Stream", true),
        blockable: attr_bool(doc, p, b"Blockable", false),
        block_volume_mul: attr_float(doc, p, b"BlockVolumeMul", 0.6),
        volume: attr_float(doc, p, b"Volume", 1.0),
        max_distance: attr_float(doc, p, b"MaxDistance", 1.0),
        min_distance: attr_float(doc, p, b"MinDistance", 1.0),
        fade_start: attr_bool(doc, p, b"FadeStart", true),
        fade_stop: attr_bool(doc, p, b"FadeStop", true),
        random: attr_float(doc, p, b"Random", 1.0),
        interval: attr_float(doc, p, b"Interval", 0.0),
        priority: attr_int(doc, p, b"Priority", 0),
    })
}

/// The index `GetRandomSoundName` returns for a list of `size` names, and whether it becomes the
/// new "previous" index (a one-name list leaves it alone). `None` for an empty list.
pub fn random_index(size: i32, previous: i32, skip_previous: bool) -> Option<(i32, bool)> {
    match size {
        ..=0 => return None,
        1 => return Some((0, false)),
        _ => {}
    }
    let (mut start, mut size_add) = (-1, -1);
    if skip_previous && size > 2 && previous < size && previous > 0 {
        start = previous;
        size_add = start - 1;
    }
    let mut rand = rand_rectl(start + 1, size + size_add);
    if rand >= size {
        rand -= size;
    }
    Some((rand, true))
}

/// `GetSoundName` and `SetSoundName` index: `None` when out of range. The C++ also read and
/// wrote at negative indices (out of bounds); those now count as out of range.
pub fn name_index(size: i32, idx: i32) -> Option<usize> {
    if idx < 0 || size <= idx {
        None
    } else {
        usize::try_from(idx).ok()
    }
}
