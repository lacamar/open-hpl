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

type Sink = unsafe extern "C" fn(*mut c_void, *const u8, usize);

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
