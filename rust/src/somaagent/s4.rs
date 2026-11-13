//! S4: pathfinder save blob, nav-path and agent debug strings.

use crate::string::{float_to_string, int_to_string};
use std::ffi::c_void;

const MAX_TRACK: u32 = 4096;

#[derive(Clone, Default, PartialEq, Debug)]
pub struct Track {
    pub node: Vec<u8>,
    pub min_wait: f32,
    pub max_wait: f32,
    pub anim: Vec<u8>,
    pub looped: bool,
}

#[derive(Clone, Default, PartialEq, Debug)]
pub struct PathBlob {
    pub moving: bool,
    pub goal: [f32; 3],
    pub exact: bool,
    pub result: Vec<u8>,
    pub in_map: bool,
    pub end_cb: Vec<u8>,
    pub tracks: Vec<Track>,
    pub track_idx: i32,
    pub track_active: bool,
    pub track_paused: bool,
    pub track_loop: bool,
    pub at_node: bool,
    pub track_wait: f32,
    pub track_freq: f32,
    pub track_cb: Vec<u8>,
}

fn put_str(out: &mut Vec<u8>, s: &[u8]) {
    out.extend_from_slice(&(s.len() as u32).to_ne_bytes());
    out.extend_from_slice(s);
}

fn put_f32(out: &mut Vec<u8>, f: f32) {
    out.extend_from_slice(&f.to_ne_bytes());
}

/// `SomaAgentSavePath` byte layout.
pub fn path_save(p: &PathBlob) -> Vec<u8> {
    let mut o = Vec::new();
    o.push(u8::from(p.moving));
    p.goal.iter().for_each(|&f| put_f32(&mut o, f));
    o.push(u8::from(p.exact));
    put_str(&mut o, &p.result);
    o.push(u8::from(p.in_map));
    put_str(&mut o, &p.end_cb);
    o.extend_from_slice(&(p.tracks.len() as u32).to_ne_bytes());
    for n in &p.tracks {
        put_str(&mut o, &n.node);
        put_f32(&mut o, n.min_wait);
        put_f32(&mut o, n.max_wait);
        put_str(&mut o, &n.anim);
        o.push(u8::from(n.looped));
    }
    o.extend_from_slice(&p.track_idx.to_ne_bytes());
    o.extend([p.track_active, p.track_paused, p.track_loop, p.at_node].map(u8::from));
    put_f32(&mut o, p.track_wait);
    put_f32(&mut o, p.track_freq);
    put_str(&mut o, &p.track_cb);
    o
}

struct Rd<'a> {
    d: &'a [u8],
    p: usize,
}

impl Rd<'_> {
    // A field that runs past the end keeps its old value but still advances.
    fn raw<const N: usize>(&mut self) -> Option<[u8; N]> {
        let r = self
            .d
            .get(self.p..self.p.saturating_add(N))
            .and_then(|s| s.try_into().ok());
        self.p = self.p.saturating_add(N);
        r
    }
    fn bool_(&mut self, x: &mut bool) {
        if let Some([b]) = self.raw::<1>() {
            *x = b != 0;
        }
    }
    fn f32_(&mut self, x: &mut f32) {
        if let Some(b) = self.raw::<4>() {
            *x = f32::from_ne_bytes(b);
        }
    }
    fn i32_(&mut self, x: &mut i32) {
        if let Some(b) = self.raw::<4>() {
            *x = i32::from_ne_bytes(b);
        }
    }
    fn u32_(&mut self) -> u32 {
        self.raw::<4>().map_or(0, u32::from_ne_bytes)
    }
    fn str_(&mut self, x: &mut Vec<u8>) {
        let n = self.u32_() as usize;
        let end = self.p.saturating_add(n);
        *x = self
            .d
            .get(self.p..end)
            .map(<[u8]>::to_vec)
            .unwrap_or_default();
        self.p = end;
    }
}

/// `SomaAgentLoadPath` parse; `cur` supplies the fields a truncated blob leaves untouched.
pub fn path_load(data: &[u8], cur: &PathBlob) -> PathBlob {
    let mut r = Rd { d: data, p: 0 };
    let mut o = PathBlob {
        track_idx: cur.track_idx,
        track_active: cur.track_active,
        track_paused: cur.track_paused,
        track_loop: cur.track_loop,
        at_node: cur.at_node,
        track_wait: cur.track_wait,
        track_freq: cur.track_freq,
        ..PathBlob::default()
    };
    r.bool_(&mut o.moving);
    o.goal.iter_mut().for_each(|f| r.f32_(f));
    r.bool_(&mut o.exact);
    r.str_(&mut o.result);
    r.bool_(&mut o.in_map);
    r.str_(&mut o.end_cb);
    let n = r.u32_();
    o.tracks = vec![Track::default(); n.min(MAX_TRACK) as usize];
    for t in &mut o.tracks {
        r.str_(&mut t.node);
        r.f32_(&mut t.min_wait);
        r.f32_(&mut t.max_wait);
        r.str_(&mut t.anim);
        r.bool_(&mut t.looped);
    }
    r.i32_(&mut o.track_idx);
    r.bool_(&mut o.track_active);
    r.bool_(&mut o.track_paused);
    r.bool_(&mut o.track_loop);
    r.bool_(&mut o.at_node);
    r.f32_(&mut o.track_wait);
    r.f32_(&mut o.track_freq);
    r.str_(&mut o.track_cb);
    o
}

fn f20(x: f32) -> Vec<u8> {
    float_to_string(x, 20, false)
}

fn int(x: i32) -> Vec<u8> {
    int_to_string(x, 0)
}

fn flag(x: bool) -> Vec<u8> {
    int(i32::from(x))
}

/// `SomaNavPath`: one "x y z" line per node, path order reversed.
pub fn nav_path_text(nodes: &[[f32; 3]]) -> Vec<u8> {
    let mut s = Vec::new();
    for n in nodes.iter().rev() {
        for (i, &f) in n.iter().enumerate() {
            if i > 0 {
                s.push(b' ');
            }
            s.extend(f20(f));
        }
        s.push(b'\n');
    }
    s
}

fn vec3_text(v: [f32; 3]) -> Vec<u8> {
    let f = |x| float_to_string(x, 6, false);
    [f(v[0]), b" : ".to_vec(), f(v[1]), b" : ".to_vec(), f(v[2])].concat()
}

pub struct DebugSm<'a> {
    pub cur: &'a [u8],
    pub prev: &'a [u8],
    pub next: &'a [u8],
    pub timers: i32,
}

pub struct DebugPf {
    pub moving: bool,
    pub path_idx: i32,
    pub path_len: i32,
    pub goal: [f32; 3],
    pub track_idx: i32,
    pub track_len: i32,
    pub track_active: bool,
    pub paused: bool,
    pub at_node: bool,
    pub wait: f32,
}

pub struct DebugCm<'a> {
    pub moving: bool,
    pub goal: [f32; 3],
    pub speed_state: i32,
    pub fwd: f32,
    pub stuck: f32,
    pub turning: bool,
    pub anim: &'a [u8],
    pub idle_extra_count: i32,
    pub idle_extra_wait: f32,
    pub wall: bool,
    pub wall_hits: i32,
    pub roll: f32,
    pub object_avoid: bool,
}

/// `SomaAgentDebug` line.
pub fn debug_text(sm: Option<&DebugSm>, pf: Option<&DebugPf>, cm: Option<&DebugCm>) -> Vec<u8> {
    let mut s = Vec::new();
    let mut add = |parts: &[&[u8]]| parts.iter().for_each(|p| s.extend_from_slice(p));
    if let Some(m) = sm {
        add(&[
            b"state=",
            m.cur,
            b" prev=",
            m.prev,
            b" next=",
            m.next,
            b" timers=",
            &int(m.timers),
        ]);
    }
    if let Some(p) = pf {
        add(&[
            b" pf_moving=",
            &flag(p.moving),
            b" path=",
            &int(p.path_idx),
            b"/",
            &int(p.path_len),
            b" goal=",
            &vec3_text(p.goal),
            b" track=",
            &int(p.track_idx),
            b"/",
            &int(p.track_len),
            b" track_active=",
            &flag(p.track_active),
            b" paused=",
            &flag(p.paused),
            b" at_node=",
            &flag(p.at_node),
            b" wait=",
            &f20(p.wait),
        ]);
    }
    if let Some(m) = cm {
        add(&[
            b" cm_moving=",
            &flag(m.moving),
            b" cm_goal=",
            &vec3_text(m.goal),
            b" speed_state=",
            &int(m.speed_state),
            b" fwd=",
            &f20(m.fwd),
            b" stuck=",
            &f20(m.stuck),
            b" turning=",
            &flag(m.turning),
            b" anim=",
            m.anim,
            b" idle_extra=",
            &int(m.idle_extra_count),
            b"/",
            &f20(m.idle_extra_wait),
            b" wall=",
            &flag(m.wall),
            b"/",
            &int(m.wall_hits),
            b" roll=",
            &f20(m.roll),
            b" doa=",
            &flag(m.object_avoid),
        ]);
    }
    s
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct OhplS4Str {
    ptr: *const u8,
    len: usize,
}

impl OhplS4Str {
    /// # Safety
    /// `ptr` is null or valid for `len` bytes.
    unsafe fn bytes<'a>(self) -> &'a [u8] {
        if self.ptr.is_null() {
            return &[];
        }
        // SAFETY: caller contract.
        unsafe { std::slice::from_raw_parts(self.ptr, self.len) }
    }
}

#[repr(C)]
pub struct OhplS4Track {
    node: OhplS4Str,
    min_wait: f32,
    max_wait: f32,
    anim: OhplS4Str,
    loop_anim: bool,
}

#[repr(C)]
pub struct OhplS4PathPod {
    moving: bool,
    exact: bool,
    in_map: bool,
    track_active: bool,
    track_paused: bool,
    track_loop: bool,
    at_node: bool,
    goal: [f32; 3],
    track_idx: i32,
    track_wait: f32,
    track_freq: f32,
}

type Sink = unsafe extern "C" fn(*mut c_void, *const u8, usize);
type StrSink = unsafe extern "C" fn(*mut c_void, u32, *const u8, usize);
type TrackSink = unsafe extern "C" fn(*mut c_void, *const OhplS4Track);

fn pod_to_blob(p: &OhplS4PathPod) -> PathBlob {
    PathBlob {
        moving: p.moving,
        goal: p.goal,
        exact: p.exact,
        in_map: p.in_map,
        track_idx: p.track_idx,
        track_active: p.track_active,
        track_paused: p.track_paused,
        track_loop: p.track_loop,
        at_node: p.at_node,
        track_wait: p.track_wait,
        track_freq: p.track_freq,
        ..PathBlob::default()
    }
}

/// # Safety
/// `pod` valid; `tracks` valid for `ntracks`; strings valid; `sink` accepts `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_s4_path_save(
    pod: *const OhplS4PathPod,
    result: OhplS4Str,
    end_cb: OhplS4Str,
    track_cb: OhplS4Str,
    tracks: *const OhplS4Track,
    ntracks: usize,
    ctx: *mut c_void,
    sink: Sink,
) {
    // SAFETY: caller contract.
    let Some(pod) = (unsafe { pod.as_ref() }) else {
        return;
    };
    let mut b = pod_to_blob(pod);
    // SAFETY: caller contract.
    unsafe {
        b.result = result.bytes().to_vec();
        b.end_cb = end_cb.bytes().to_vec();
        b.track_cb = track_cb.bytes().to_vec();
        if !tracks.is_null() {
            b.tracks = std::slice::from_raw_parts(tracks, ntracks)
                .iter()
                .map(|t| Track {
                    node: t.node.bytes().to_vec(),
                    min_wait: t.min_wait,
                    max_wait: t.max_wait,
                    anim: t.anim.bytes().to_vec(),
                    looped: t.loop_anim,
                })
                .collect();
        }
    }
    let out = path_save(&b);
    // SAFETY: sink contract.
    unsafe { sink(ctx, out.as_ptr(), out.len()) };
}

/// Parses `data` into `pod` (in/out) and replays strings (kind 0 result, 1 end cb, 2 track cb)
/// then tracks through the sinks.
///
/// # Safety
/// `data` valid for `len`; `pod` valid; sinks accept `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_s4_path_load(
    data: *const u8,
    len: usize,
    pod: *mut OhplS4PathPod,
    ctx: *mut c_void,
    on_str: StrSink,
    on_track: TrackSink,
) {
    // SAFETY: caller contract.
    let Some(pod) = (unsafe { pod.as_mut() }) else {
        return;
    };
    let d = if data.is_null() {
        &[][..]
    } else {
        // SAFETY: caller contract.
        unsafe { std::slice::from_raw_parts(data, len) }
    };
    let o = path_load(d, &pod_to_blob(pod));
    pod.moving = o.moving;
    pod.exact = o.exact;
    pod.in_map = o.in_map;
    pod.goal = o.goal;
    pod.track_idx = o.track_idx;
    pod.track_active = o.track_active;
    pod.track_paused = o.track_paused;
    pod.track_loop = o.track_loop;
    pod.at_node = o.at_node;
    pod.track_wait = o.track_wait;
    pod.track_freq = o.track_freq;
    for (k, s) in [&o.result, &o.end_cb, &o.track_cb].into_iter().enumerate() {
        // SAFETY: sink contract.
        unsafe { on_str(ctx, k as u32, s.as_ptr(), s.len()) };
    }
    for t in &o.tracks {
        let ft = OhplS4Track {
            node: OhplS4Str {
                ptr: t.node.as_ptr(),
                len: t.node.len(),
            },
            min_wait: t.min_wait,
            max_wait: t.max_wait,
            anim: OhplS4Str {
                ptr: t.anim.as_ptr(),
                len: t.anim.len(),
            },
            loop_anim: t.looped,
        };
        // SAFETY: sink contract.
        unsafe { on_track(ctx, &ft) };
    }
}

/// # Safety
/// `nodes` valid for `n` triples; sink accepts `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_s4_nav_path_text(
    nodes: *const f32,
    n: usize,
    ctx: *mut c_void,
    sink: Sink,
) {
    let v: Vec<[f32; 3]> = if nodes.is_null() {
        Vec::new()
    } else {
        // SAFETY: caller contract.
        unsafe { std::slice::from_raw_parts(nodes.cast::<[f32; 3]>(), n) }.to_vec()
    };
    let out = nav_path_text(&v);
    // SAFETY: sink contract.
    unsafe { sink(ctx, out.as_ptr(), out.len()) };
}

#[repr(C)]
pub struct OhplS4DebugIn {
    has_sm: bool,
    has_pf: bool,
    has_cm: bool,
    pf_moving: bool,
    pf_track_active: bool,
    pf_paused: bool,
    pf_at_node: bool,
    cm_moving: bool,
    cm_turning: bool,
    cm_wall: bool,
    cm_object_avoid: bool,
    sm_cur: OhplS4Str,
    sm_prev: OhplS4Str,
    sm_next: OhplS4Str,
    cm_anim: OhplS4Str,
    sm_timers: i32,
    pf_path_idx: i32,
    pf_path_len: i32,
    pf_track_idx: i32,
    pf_track_len: i32,
    cm_speed_state: i32,
    cm_idle_extra_count: i32,
    cm_wall_hits: i32,
    pf_goal: [f32; 3],
    cm_goal: [f32; 3],
    pf_wait: f32,
    cm_fwd: f32,
    cm_stuck: f32,
    cm_idle_extra_wait: f32,
    cm_roll: f32,
}

/// # Safety
/// `d` valid with valid strings; sink accepts `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_s4_debug_text(d: *const OhplS4DebugIn, ctx: *mut c_void, sink: Sink) {
    // SAFETY: caller contract.
    let Some(d) = (unsafe { d.as_ref() }) else {
        return;
    };
    // SAFETY: caller contract on the string fields.
    let out = unsafe {
        let sm = DebugSm {
            cur: d.sm_cur.bytes(),
            prev: d.sm_prev.bytes(),
            next: d.sm_next.bytes(),
            timers: d.sm_timers,
        };
        let pf = DebugPf {
            moving: d.pf_moving,
            path_idx: d.pf_path_idx,
            path_len: d.pf_path_len,
            goal: d.pf_goal,
            track_idx: d.pf_track_idx,
            track_len: d.pf_track_len,
            track_active: d.pf_track_active,
            paused: d.pf_paused,
            at_node: d.pf_at_node,
            wait: d.pf_wait,
        };
        let cm = DebugCm {
            moving: d.cm_moving,
            goal: d.cm_goal,
            speed_state: d.cm_speed_state,
            fwd: d.cm_fwd,
            stuck: d.cm_stuck,
            turning: d.cm_turning,
            anim: d.cm_anim.bytes(),
            idle_extra_count: d.cm_idle_extra_count,
            idle_extra_wait: d.cm_idle_extra_wait,
            wall: d.cm_wall,
            wall_hits: d.cm_wall_hits,
            roll: d.cm_roll,
            object_avoid: d.cm_object_avoid,
        };
        debug_text(
            d.has_sm.then_some(&sm),
            d.has_pf.then_some(&pf),
            d.has_cm.then_some(&cm),
        )
    };
    // SAFETY: sink contract.
    unsafe { sink(ctx, out.as_ptr(), out.len()) };
}
