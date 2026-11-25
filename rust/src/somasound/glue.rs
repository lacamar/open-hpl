//! `cSomaSoundEvents`: event projects, resolving an event to sound entity data, GUI and 3D plays.

use super::extract;
use super::game::{game, warn};
use super::host::{self, Engine, ops};
use crate::sound::own::data_of;
use crate::string::{file_ext, to_lower};
use std::collections::{BTreeMap, BTreeSet};

fn dir_of(path: &[u8]) -> &[u8] {
    let i = path
        .iter()
        .rposition(|&c| c == b'/' || c == b'\\')
        .map_or(0, |i| i + 1);
    path.get(..i).unwrap_or_default()
}

fn game_id() -> Vec<u8> {
    crate::somascript::s2_host::game_id()
}

fn read_fdp(path: &[u8]) -> Option<(Vec<u8>, bool)> {
    use std::os::unix::ffi::OsStrExt;
    let p = std::path::Path::new(std::ffi::OsStr::from_bytes(path));
    match std::fs::read(p) {
        Ok(d) => Some((d, true)),
        Err(_) => p.exists().then(|| (Vec::new(), false)),
    }
}

fn exists(path: &[u8]) -> bool {
    use std::os::unix::ffi::OsStrExt;
    std::path::Path::new(std::ffi::OsStr::from_bytes(path)).exists()
}

pub fn load_project(project: &[u8]) {
    let key = to_lower(project);
    if !game().bank.events.mark_project(&key) {
        return;
    }
    let fdp = host::find_file(&[project, b".fdp"].concat());
    let loaded = fdp
        .as_deref()
        .and_then(read_fdp)
        .and_then(|(data, ok)| game().bank.events.load_fdp(&data, ok, project));
    let Some((name, count)) = loaded else {
        warn(&format!(
            "SOMA sound: no event project '{}'\n",
            String::from_utf8_lossy(project)
        ));
        return;
    };
    let fdp = fdp.unwrap_or_default();
    let cache = extract::cache_dir(&game_id(), b"events-v4");
    let wanted: BTreeMap<Vec<u8>, BTreeSet<Vec<u8>>> = game().bank.events.wanted(&key);
    let mut files: BTreeMap<Vec<u8>, Vec<u8>> = BTreeMap::new();
    for (bank, samples) in &wanted {
        let mut path = host::find_file(&[bank.as_slice(), b".fsb"].concat()).unwrap_or_default();
        let local = [dir_of(&fdp), bank, b".fsb"].concat();
        let mut prefix = [bank.as_slice(), b"__"].concat();
        if local != path && exists(&local) {
            path = local;
            prefix = [key.as_slice(), b"__", &prefix].concat();
        }
        let list: Vec<Vec<u8>> = samples.iter().cloned().collect();
        let mut out = BTreeMap::new();
        extract::extract_samples(&path, &cache, &prefix, &list, &mut out);
        for (sample, file) in out {
            files.insert([bank.as_slice(), b"__", &sample].concat(), file);
        }
    }
    game().bank.events.assign_files(&key, &files);
    if files.values().any(|f| host::find_file(f).is_none())
        && let Some(o) = ops()
    {
        // SAFETY: `cache` is valid for the call.
        unsafe { (o.add_resource_dir)(cache.as_ptr(), cache.len()) };
    }
    host::log(
        format!(
            "SOMA sound: {count} events in project '{}', {} samples\n",
            String::from_utf8_lossy(&name),
            files.len()
        )
        .as_bytes(),
    );
}

pub fn get_event(name: &[u8]) -> Option<usize> {
    let len = super::events::Events::project_of(name)?.len();
    load_project(name.get(..len).unwrap_or_default());
    game().bank.events.find(name)
}

/// `cSomaSoundEvents::Resolve`: the sound entity data of an event, 0 when it has none.
pub fn resolve(event: &[u8]) -> usize {
    let Some(idx) = get_event(event) else {
        return 0;
    };
    let ev = game().bank.events.list.get(idx);
    let Some(ev) = ev else { return 0 };
    if ev.data != 0 {
        return ev.data;
    }
    if !ev.has_files() {
        if !ev.defs.is_empty() {
            warn(&format!(
                "SOMA sound: event '{}' has no playable samples\n",
                String::from_utf8_lossy(event)
            ));
        }
        return 0;
    }
    let Some(o) = ops() else { return 0 };
    let lower = to_lower(event);
    // SAFETY: plain calls; the strings are valid for each call.
    let data = unsafe { (o.data_new)(lower.as_ptr(), lower.len()) };
    // SAFETY: `data` is the `cSoundEntityData*` just created; its record is registered.
    if let Some(d) = unsafe { data_of(data).as_mut() } {
        for f in ev.files() {
            d.add_name(0, f);
        }
        d.setup(ev.min_dist, ev.max_dist, !ev.one_shot, ev.is_3d);
    }
    game().bank.events.set_data(idx, data);
    let g = game();
    if !std::mem::replace(&mut g.factory, true) {
        // SAFETY: plain call.
        unsafe { (o.install_hooks)() };
    }
    data
}

pub fn event_name(idx: usize) -> Vec<u8> {
    game()
        .bank
        .events
        .list
        .get(idx)
        .map(|e| e.name.clone())
        .unwrap_or_default()
}

fn new_shell(event: usize, ty: i32) -> usize {
    ops().map_or(0, |o| {
        // SAFETY: plain call.
        unsafe { (o.shell_new)(event, 0, ty) }
    })
}

fn handle_of(shell: usize) -> usize {
    ops().map_or(0, |o| {
        // SAFETY: plain call.
        unsafe { (o.shell_handle)(shell) }
    })
}

fn delete_shell(shell: usize) {
    if let Some(o) = ops() {
        // SAFETY: `shell` came from `shell_new`.
        unsafe { (o.shell_delete)(shell) };
    }
}

fn start(shell: usize) -> usize {
    let h = handle_of(shell);
    game().bank.play(&mut Engine, h);
    if game().state(h).is_some_and(|s| s.stopped) {
        delete_shell(shell);
        return 0;
    }
    shell
}

pub fn play_gui(name: &[u8], volume: f32, ty: i32, looped: bool, stream: bool) -> usize {
    let mut event = get_event(name);
    if event.is_none() && file_ext(name).is_empty() && name.contains(&b'/') {
        return 0;
    }
    let idx = *event.get_or_insert_with(|| game().bank.events.file_event(name, looped, stream));
    if !game()
        .bank
        .events
        .list
        .get(idx)
        .is_some_and(|e| e.has_files())
    {
        return 0;
    }
    let shell = new_shell(idx, ty);
    let h = handle_of(shell);
    if let Some(i) = game().bank.inst_mut(h) {
        i.st.volume = volume;
    }
    start(shell)
}

pub fn play_3d(name: &[u8], volume: f32, pos: [f32; 3], ty: i32) -> usize {
    let Some(idx) = get_event(name) else { return 0 };
    let Some(ev) = game().bank.events.list.get(idx).filter(|e| e.has_files()) else {
        return 0;
    };
    let is_3d = ev.is_3d;
    let shell = new_shell(idx, ty);
    let h = handle_of(shell);
    set_position(shell, h, pos);
    if let Some(i) = game().bank.inst_mut(h) {
        i.st.volume = volume;
        i.st.play_3d = is_3d;
    }
    start(shell)
}

pub fn set_position(shell: usize, h: usize, pos: [f32; 3]) {
    if let Some(i) = game().bank.inst_mut(h) {
        i.st.pos = pos;
    }
    if let Some(o) = ops() {
        // SAFETY: `pos` is valid for the call.
        unsafe { (o.shell_set_position)(shell, pos.as_ptr()) };
    }
}

pub fn event_of_data(data: usize) -> Option<usize> {
    game().bank.events.event_of_data(data)
}
