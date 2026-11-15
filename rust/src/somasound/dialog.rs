//! `cSomaLuxDialogHandler`: scripted dialogs as a state machine over voice subjects, pauses and
//! response options, driven by the game update.

use super::dump;
use super::host::{CallArg, Str, ops};
use crate::somascript::s2_as::{Soma, str_bytes};
use std::collections::BTreeMap;
use std::ffi::c_void;

const EVENT_SET: i32 = 2;
const EVENT_ADD: i32 = 3;
const EVENT_EQUAL: i32 = 4;
const EVENT_LESS: i32 = 5;
const EVENT_GREATER: i32 = 6;
const EVENT_SCRIPT: i32 = 7;

#[derive(Clone)]
struct Event {
    kind: i32,
    val: i32,
    var: Vec<u8>,
    new_branch: Vec<u8>,
    only_end: bool,
}

#[derive(Clone)]
struct Item {
    subject: Vec<u8>,
    pause: f32,
    callback: Vec<u8>,
    events: Vec<Event>,
}

#[derive(Clone)]
struct Option_ {
    entry: Vec<u8>,
    branch: Vec<u8>,
    id: i32,
    callback: Vec<u8>,
}

#[derive(Clone, Default)]
struct Branch {
    name: Vec<u8>,
    next: Vec<u8>,
    items: Vec<Item>,
    options: Vec<Option_>,
}

#[derive(Clone)]
struct Dialog {
    uid: u64,
    name: Vec<u8>,
    callback: Vec<u8>,
    branches: Vec<Branch>,
    branch: i32,
    item: i32,
    waiting: bool,
    pause_left: f32,
    done: bool,
}

impl Dialog {
    fn new(name: &[u8], callback: Vec<u8>) -> Self {
        Self {
            uid: 0,
            name: name.to_vec(),
            callback,
            branches: Vec::new(),
            branch: -1,
            item: -1,
            waiting: false,
            pause_left: 0.0,
            done: false,
        }
    }

    fn find(&self, name: &[u8]) -> Option<i32> {
        self.branches
            .iter()
            .position(|b| b.name == name)
            .map(|i| i as i32)
    }

    fn cur_branch(&self) -> Option<&Branch> {
        usize::try_from(self.branch)
            .ok()
            .and_then(|i| self.branches.get(i))
    }

    fn cur_item(&self) -> Option<&Item> {
        let i = usize::try_from(self.item).ok()?;
        self.cur_branch()?.items.get(i)
    }
}

#[derive(Default)]
pub struct Dialogs {
    building: Option<Dialog>,
    pending_callback: Vec<u8>,
    active: Vec<Dialog>,
    vars: BTreeMap<Vec<u8>, i32>,
    next_uid: u64,
}

fn d() -> &'static mut Dialogs {
    &mut super::game::game().dialogs
}

fn building() -> &'static mut Dialog {
    d().building
        .get_or_insert_with(|| Dialog::new(b"", Vec::new()))
}

fn voice_op<R>(default: R, f: impl FnOnce(&super::host::S7Host) -> R) -> R {
    ops().map_or(default, f)
}

fn voice_present() -> bool {
    // SAFETY: plain call.
    voice_op(false, |h| unsafe { (h.voice_present)() })
}

fn script(decl: &str, args: &[CallArg], want_ret: bool) -> i32 {
    // SAFETY: `decl` and the argument strings are valid for the call.
    voice_op(-1, |h| unsafe {
        (h.script_call)(
            decl.as_ptr(),
            decl.len(),
            args.as_ptr(),
            args.len(),
            want_ret,
        )
    })
}

fn str_arg(s: &[u8]) -> CallArg {
    CallArg {
        kind: 0,
        i: 0,
        s: Str {
            p: s.as_ptr(),
            n: s.len(),
        },
    }
}

fn plain_arg(kind: u8, i: i32) -> CallArg {
    CallArg {
        kind,
        i,
        s: Str {
            p: std::ptr::null(),
            n: 0,
        },
    }
}

fn lossy(b: &[u8]) -> String {
    String::from_utf8_lossy(b).into_owned()
}

fn item_callback(func: &[u8], subject: &[u8], start: bool) {
    dump::text(
        b"DI",
        format!("{} {} {}", lossy(func), lossy(subject), u8::from(start)).as_bytes(),
    );
    if func.is_empty() {
        return;
    }
    script(
        &format!("void {}(const tString&in, bool)", lossy(func)),
        &[str_arg(subject), plain_arg(1, i32::from(start))],
        false,
    );
}

fn with<R>(uid: u64, f: impl FnOnce(&mut Dialog) -> R) -> Option<R> {
    d().active.iter_mut().find(|x| x.uid == uid).map(f)
}

unsafe extern "C" fn on_done(token: u64) {
    with(token, |x| x.waiting = false);
}

fn start_item(uid: u64) {
    let Some(item) = with(uid, |x| x.cur_item().cloned()).flatten() else {
        return;
    };
    item_callback(&item.callback, &item.subject, true);
    if item.pause >= 0.0 {
        with(uid, |x| {
            x.pause_left = item.pause;
            x.waiting = false;
        });
        return;
    }
    with(uid, |x| x.waiting = true);
    let ok = voice_present()
        && voice_op(false, |h| {
            // SAFETY: the subject is valid for the call; `on_done` accepts the token.
            unsafe { (h.voice_play)(item.subject.as_ptr(), item.subject.len(), uid, on_done) }
        });
    if !ok {
        with(uid, |x| x.waiting = false);
    }
}

fn finish(uid: u64) {
    let Some((name, callback)) = with(uid, |x| {
        x.done = true;
        (x.name.clone(), x.callback.clone())
    }) else {
        return;
    };
    dump::text(
        b"DF",
        format!("{} {}", lossy(&name), lossy(&callback)).as_bytes(),
    );
    if !callback.is_empty() {
        script(
            &format!("void {}(const tString&in)", lossy(&callback)),
            &[str_arg(&name)],
            false,
        );
    }
}

fn end_branch(uid: u64) {
    let Some(b) = with(uid, |x| x.cur_branch().cloned()).flatten() else {
        return;
    };
    let mut next = b.next.clone();
    if let Some(opt) = b.options.first() {
        host_log(&format!(
            "SOMA dialog: auto-selected option '{}'\n",
            lossy(&opt.entry)
        ));
        dump::text(
            b"DO",
            format!("{} {} {}", lossy(&opt.entry), lossy(&opt.branch), opt.id).as_bytes(),
        );
        if !opt.callback.is_empty() {
            script(
                &format!(
                    "void {}(const tString&in,const tString&in,int)",
                    lossy(&opt.callback)
                ),
                &[str_arg(&b.name), str_arg(&opt.entry), plain_arg(2, opt.id)],
                false,
            );
        }
        next = opt.branch.clone();
    }
    if !next.is_empty()
        && let Some(Some(idx)) = with(uid, |x| x.find(&next))
    {
        with(uid, |x| {
            x.branch = idx;
            x.item = -1;
        });
        return;
    }
    finish(uid);
}

fn host_log(s: &str) {
    super::host::log(s.as_bytes());
}

fn check_event(uid: u64, item: &Item, e: &Event) -> bool {
    let cur = d().vars.get(&e.var).copied().unwrap_or(0);
    match e.kind {
        EVENT_SET => {
            d().vars.insert(e.var.clone(), e.val);
            false
        }
        EVENT_ADD => {
            *d().vars.entry(e.var.clone()).or_insert(0) += e.val;
            false
        }
        EVENT_EQUAL => cur == e.val,
        EVENT_LESS => cur < e.val,
        EVENT_GREATER => cur > e.val,
        EVENT_SCRIPT => {
            if e.var.is_empty() {
                return false;
            }
            let Some(branch) = with(uid, |x| x.cur_branch().map(|b| b.name.clone())).flatten()
            else {
                return false;
            };
            let line = if voice_present() {
                // SAFETY: the subject is valid for the call.
                voice_op(0, |h| unsafe {
                    (h.voice_line_count)(item.subject.as_ptr(), item.subject.len())
                }) - 1
            } else {
                -1
            };
            script(
                &format!(
                    "bool {}(const tString&in, const tString&in, int, const tString&in)",
                    lossy(&e.var)
                ),
                &[
                    str_arg(&branch),
                    str_arg(&item.subject),
                    plain_arg(2, line),
                    str_arg(&e.new_branch),
                ],
                true,
            ) > 0
        }
        _ => false,
    }
}

fn check_end_events(uid: u64) -> bool {
    let Some(item) = with(uid, |x| x.cur_item().cloned()).flatten() else {
        return false;
    };
    for e in &item.events {
        if !e.only_end || !check_event(uid, &item, e) {
            continue;
        }
        let idx = (!e.new_branch.is_empty())
            .then(|| with(uid, |x| x.find(&e.new_branch)).flatten())
            .flatten();
        match idx {
            Some(i) => {
                with(uid, |x| {
                    x.branch = i;
                    x.item = -1;
                });
            }
            None => finish(uid),
        }
        return true;
    }
    false
}

fn next_item(uid: u64) {
    while with(uid, |x| !x.done).unwrap_or(false) {
        let Some((b, item)) = with(uid, |x| (x.cur_branch().cloned(), x.item)) else {
            return;
        };
        let Some(b) = b else { return };
        if item >= 0
            && let Some(prev) = b.items.get(item as usize)
        {
            item_callback(&prev.callback, &prev.subject, false);
            if check_end_events(uid) {
                continue;
            }
        }
        let started = with(uid, |x| {
            x.item += 1;
            usize::try_from(x.item)
                .ok()
                .is_some_and(|i| i < b.items.len())
        })
        .unwrap_or(false);
        if started {
            start_item(uid);
            return;
        }
        end_branch(uid);
        if with(uid, |x| x.done).unwrap_or(true) {
            return;
        }
    }
}

pub fn update(dt: f32) {
    let uids: Vec<u64> = d().active.iter().map(|x| x.uid).collect();
    for uid in uids {
        let Some((done, pause, waiting)) = with(uid, |x| (x.done, x.pause_left, x.waiting)) else {
            continue;
        };
        if done {
            continue;
        }
        if pause > 0.0 {
            let left = with(uid, |x| {
                x.pause_left -= dt;
                x.pause_left
            })
            .unwrap_or(1.0);
            if left <= 0.0 {
                next_item(uid);
            }
        } else if !waiting {
            next_item(uid);
        }
    }
    d().active.retain(|x| !x.done);
}

pub fn stop_all() {
    for x in &mut d().active {
        x.done = true;
    }
    if voice_present()
        && let Some(h) = ops()
    {
        // SAFETY: plain call.
        unsafe { (h.voice_stop_all)() };
    }
}

fn character_active(name: &[u8]) -> bool {
    if !voice_present() {
        return false;
    }
    let Some(h) = ops() else { return false };
    // SAFETY: `name` is valid for the call.
    if unsafe { (h.voice_scene_involving)(name.as_ptr(), name.len()) } {
        return true;
    }
    d().active.iter().filter(|x| !x.done).any(|x| {
        x.branches.iter().flat_map(|b| &b.items).any(|i| {
            // SAFETY: both strings are valid for the call.
            !i.subject.is_empty()
                && unsafe {
                    (h.voice_subject_involves)(
                        i.subject.as_ptr(),
                        i.subject.len(),
                        name.as_ptr(),
                        name.len(),
                    )
                }
        })
    })
}

fn bytes<'a>(p: *mut c_void) -> &'a [u8] {
    // SAFETY: the declarations below pass `const tString &in`.
    unsafe { str_bytes(p) }
}

fn begin(name: &[u8]) {
    let cb = std::mem::take(&mut d().pending_callback);
    d().building = Some(Dialog::new(name, cb));
}

fn end(start: &[u8]) {
    let mut dialog = d()
        .building
        .take()
        .unwrap_or_else(|| Dialog::new(b"", Vec::new()));
    if dialog.branches.is_empty() {
        return;
    }
    dialog.branch = 0;
    if !start.is_empty()
        && let Some(i) = dialog.find(start)
    {
        dialog.branch = i;
    }
    dialog.item = -1;
    d().next_uid += 1;
    dialog.uid = d().next_uid;
    let uid = dialog.uid;
    d().active.push(dialog);
    next_item(uid);
}

fn add_item(item: Item) {
    let b = building();
    if b.branches.is_empty() {
        b.branches.push(Branch::default());
    }
    if let Some(l) = b.branches.last_mut() {
        l.items.push(item);
    }
}

fn add_option(o: Option_) {
    if let Some(l) = building().branches.last_mut() {
        l.options.push(o);
    }
}

fn add_event(e: Event) {
    if let Some(i) = building()
        .branches
        .last_mut()
        .and_then(|b| b.items.last_mut())
    {
        i.events.push(e);
    }
}

pub(super) fn register(s: &Soma) {
    let t = "cLuxDialogHandler";
    s.method(t, "void Begin(const tString &in asName)", |fr| {
        begin(bytes(fr.ptr_arg(0)));
    });
    s.method(t, "void End(const tString&in asStartBranch)", |fr| {
        end(bytes(fr.ptr_arg(0)));
    });
    s.method(t, "void SetCallbackFunc(const tString&in asFunc)", |fr| {
        d().pending_callback = bytes(fr.ptr_arg(0)).to_vec();
    });
    s.method(
        t,
        "void AddBranch(const tString&in asName, const tString&in asNextBranch)",
        |fr| {
            let b = Branch {
                name: bytes(fr.ptr_arg(0)).to_vec(),
                next: bytes(fr.ptr_arg(1)).to_vec(),
                ..Branch::default()
            };
            building().branches.push(b);
        },
    );
    s.method(
        t,
        "void AddBranchSubject(const tString&in asSubject, const tString&in asCallback)",
        |fr| {
            add_item(Item {
                subject: bytes(fr.ptr_arg(0)).to_vec(),
                pause: -1.0,
                callback: bytes(fr.ptr_arg(1)).to_vec(),
                events: Vec::new(),
            });
        },
    );
    s.method(
        t,
        "void AddBranchPause(float afTime, const tString&in asCallback)",
        |fr| {
            add_item(Item {
                subject: Vec::new(),
                pause: fr.arg::<f32>(0),
                callback: bytes(fr.ptr_arg(1)).to_vec(),
                events: Vec::new(),
            });
        },
    );
    s.method(
        t,
        "void AddResponseOption(const tString &in asEntry, const tString&in asBranch,int alId, const tString&in asCallback)",
        |fr| {
            let o = Option_ {
                entry: bytes(fr.ptr_arg(0)).to_vec(),
                branch: bytes(fr.ptr_arg(1)).to_vec(),
                id: fr.arg::<i32>(2),
                callback: bytes(fr.ptr_arg(3)).to_vec(),
            };
            add_option(o);
        },
    );
    s.method(
        t,
        "void AddBranchEvent(eLuxDialogBranchEvent aType, float afVar, const tString&in asVar, const tString&in asNewBranch, bool abOnlyCheckEndOfSubject)",
        |fr| {
            let e = Event {
                kind: fr.arg::<i32>(0),
                val: fr.arg::<f32>(1).round() as i32,
                var: bytes(fr.ptr_arg(2)).to_vec(),
                new_branch: bytes(fr.ptr_arg(3)).to_vec(),
                only_end: fr.arg::<bool>(4),
            };
            add_event(e);
        },
    );
    s.method(t, "bool CharacterIsActive(const tString&in asName)", |fr| {
        let n = bytes(fr.ptr_arg(0)).to_vec();
        fr.ret::<bool>(character_active(&n));
    });
    s.method(t, "void Stop(const tString &in asName)", |fr| {
        let n = bytes(fr.ptr_arg(0)).to_vec();
        for x in d().active.iter_mut().filter(|x| x.name == n) {
            x.done = true;
        }
    });
    s.method(t, "void StopAll()", |_| stop_all());
    s.method(t, "void SetVar(const tString&in asName, int alX)", |fr| {
        let (n, x) = (bytes(fr.ptr_arg(0)).to_vec(), fr.arg::<i32>(1));
        d().vars.insert(n, x);
    });
    s.method(t, "void IncVar(const tString&in asName, int alX)", |fr| {
        let (n, x) = (bytes(fr.ptr_arg(0)).to_vec(), fr.arg::<i32>(1));
        *d().vars.entry(n).or_insert(0) += x;
    });
    s.method(t, "int GetVar(const tString&in asName)", |fr| {
        let n = bytes(fr.ptr_arg(0)).to_vec();
        fr.ret::<i32>(*d().vars.entry(n).or_insert(0));
    });
}

#[cfg(test)]
#[path = "dialog_tests.rs"]
mod tests;
