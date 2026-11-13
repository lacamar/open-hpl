//! Replays the A4 golden vectors (recorded from the real C++ shells in the differential harness): every
//! entry is called with its recorded arguments and state; each host call must match the recorded op and
//! registers, and the final return value, outputs, POD, enemy state and queue must be byte-equal.

use super::a4_replay_gen::{CTX_SIZES, ENTRY_NAMES, OUT_BYTES, Reader, dispatch};
use super::a4_state::{Base, Io, Table};
use super::ctx::Ctx;
use super::machine::{Msg, Queue};
use std::ffi::c_void;

const GOLDEN: &[u8] = include_bytes!("../../tests/golden/A4/golden.bin");
const BASE_BYTES: usize = std::mem::size_of::<Base>();
const IO_BYTES: usize = std::mem::size_of::<Io>();
const MSG_BYTES: usize = std::mem::size_of::<Msg>();
const GA3_IO: usize = 84;
/// Only reached from `ohpl_a4_on_update`, whose vectors cover them.
const INSIDE_ON_UPDATE: &[&str] = &[
    "ohpl_a4_update_sound_state",
    "ohpl_a4_update_animation",
    "ohpl_a4_update_darkness_glow",
    "ohpl_a4_update_hallucination",
    "ohpl_a4_update_align_ground_ray",
];

struct Event {
    op: u32,
    io_in: Vec<u8>,
    io_out: Vec<u8>,
    ret: i32,
    ga3: Option<(Vec<u8>, Vec<u8>)>,
    deltas: Vec<(u32, Vec<u8>)>,
}

struct Snap {
    fields: Vec<Vec<u8>>,
    base: Vec<u8>,
    queue: Vec<u8>,
}

struct Mem {
    fields: Vec<Vec<u32>>,
    ctx: Box<Ctx>,
    base: Box<Base>,
    queue: Box<Queue>,
}

struct Replay {
    events: Vec<Event>,
    pos: usize,
    mem: *mut Mem,
    fail: Option<String>,
}

fn bytes_of<T>(v: &T) -> Vec<u8> {
    // SAFETY: plain-old-data read for comparison only.
    unsafe {
        std::slice::from_raw_parts(std::ptr::from_ref(v).cast::<u8>(), std::mem::size_of::<T>())
            .to_vec()
    }
}

fn rd_snap(r: &mut Reader) -> Snap {
    let fields = std::iter::once(Vec::new())
        .chain(CTX_SIZES.iter().skip(1).map(|&n| r.bytes(n).to_vec()))
        .collect();
    let base = r.bytes(BASE_BYTES).to_vec();
    let n = r.u32() as usize;
    let queue = r.bytes(n * MSG_BYTES).to_vec();
    Snap {
        fields,
        base,
        queue,
    }
}

fn queue_from(b: &[u8]) -> Queue {
    let msgs = b
        .chunks_exact(MSG_BYTES)
        .map(|c| {
            let mut m = Msg::default();
            // SAFETY: `c` holds one `Msg` image.
            unsafe {
                std::ptr::copy_nonoverlapping(
                    c.as_ptr(),
                    std::ptr::from_mut(&mut m).cast::<u8>(),
                    MSG_BYTES,
                )
            };
            m
        })
        .collect();
    Queue { msgs }
}

fn queue_bytes(q: &Queue) -> Vec<u8> {
    q.msgs.iter().flat_map(bytes_of).collect()
}

impl Mem {
    fn new(s: &Snap) -> Self {
        let fields: Vec<Vec<u32>> = CTX_SIZES
            .iter()
            .map(|&n| vec![0u32; n.div_ceil(4).max(1)])
            .collect();
        // SAFETY: `Ctx` holds only raw pointers, for which all-zero is valid.
        let mut ctx: Box<Ctx> = Box::new(unsafe { std::mem::zeroed() });
        // SAFETY: a zeroed `Base` is a valid POD (null handles, zero scalars).
        let base: Box<Base> = Box::new(unsafe { std::mem::zeroed() });
        let mut queue = Box::new(Queue::default());
        let slots = std::ptr::from_mut(&mut *ctx).cast::<*mut u8>();
        for (i, f) in fields.iter().enumerate() {
            let p = if i == 0 {
                std::ptr::from_mut(&mut *queue).cast::<u8>()
            } else {
                f.as_ptr().cast_mut().cast::<u8>()
            };
            // SAFETY: `Ctx` is an array of `CTX_SIZES.len()` pointers.
            unsafe { *slots.add(i) = p };
        }
        let mut m = Self {
            fields,
            ctx,
            base,
            queue,
        };
        m.load(s);
        m
    }

    fn load(&mut self, s: &Snap) {
        for (i, f) in self.fields.iter_mut().enumerate().skip(1) {
            let src = s.fields.get(i).map_or(&[][..], Vec::as_slice);
            // SAFETY: the destination holds at least `src.len()` bytes.
            unsafe {
                std::ptr::copy_nonoverlapping(src.as_ptr(), f.as_mut_ptr().cast::<u8>(), src.len())
            };
        }
        self.set_base(&s.base);
        *self.queue = queue_from(&s.queue);
    }

    fn set_base(&mut self, b: &[u8]) {
        let c = std::ptr::from_mut(&mut *self.ctx);
        // SAFETY: `b` is a full `Base` image; the context pointer is restored afterwards.
        unsafe {
            std::ptr::copy_nonoverlapping(
                b.as_ptr(),
                std::ptr::from_mut(&mut *self.base).cast::<u8>(),
                BASE_BYTES,
            );
        }
        self.base.c = c;
    }

    fn snap(&self) -> Snap {
        let fields = self
            .fields
            .iter()
            .zip(CTX_SIZES)
            .enumerate()
            .map(|(i, (f, n))| {
                if i == 0 {
                    Vec::new()
                } else {
                    // SAFETY: the buffer holds at least `n` bytes.
                    unsafe { std::slice::from_raw_parts(f.as_ptr().cast::<u8>(), n).to_vec() }
                }
            })
            .collect();
        let mut base = bytes_of(&*self.base);
        if let Some(h) = base.get_mut(..8) {
            h.fill(0);
        }
        Snap {
            fields,
            base,
            queue: queue_bytes(&self.queue),
        }
    }

    fn apply(&mut self, idx: u32, bytes: &[u8]) {
        match idx {
            200 => self.set_base(bytes),
            201 => *self.queue = queue_from(bytes),
            i => {
                if let Some(f) = self.fields.get_mut(i as usize) {
                    // SAFETY: the destination holds at least `bytes.len()` bytes.
                    unsafe {
                        std::ptr::copy_nonoverlapping(
                            bytes.as_ptr(),
                            f.as_mut_ptr().cast::<u8>(),
                            bytes.len(),
                        )
                    };
                }
            }
        }
    }
}

unsafe extern "C" fn host_call(ctx: *mut c_void, op: u32, io: *mut Io) -> i32 {
    // SAFETY: `ctx` is the `Replay` the test passed; `io` is the caller's register file.
    let (rp, io) = unsafe { (&mut *ctx.cast::<Replay>(), &mut *io) };
    let Some(ev) = rp.events.get(rp.pos) else {
        rp.fail
            .get_or_insert_with(|| format!("extra host call op {op}"));
        return 0;
    };
    rp.pos += 1;
    let pos = rp.pos;
    let ga3 = op == 1;
    let ga3_ptr = io.p[0];
    let mut cmp = bytes_of(io);
    if let Some(h) = cmp.get_mut(..8).filter(|_| ga3) {
        h.fill(0);
    }
    if ev.op != op || cmp != ev.io_in {
        rp.fail.get_or_insert_with(|| {
            format!("host call {pos} op {op} vs {}: registers differ", ev.op)
        });
    }
    if ga3 {
        let Some((gin, gout)) = &ev.ga3 else {
            return ev.ret;
        };
        // SAFETY: GA3 passthrough carries a pointer to the caller's 84-byte register file.
        let g = unsafe { std::slice::from_raw_parts_mut(ga3_ptr.cast::<u8>(), GA3_IO) };
        if g != gin.as_slice() {
            rp.fail.get_or_insert_with(|| {
                format!("host call {pos} ga3 op {} registers differ", io_op(io))
            });
        }
        g.copy_from_slice(gout);
    }
    // SAFETY: `io_out` is a full register-file image.
    unsafe {
        std::ptr::copy_nonoverlapping(
            ev.io_out.as_ptr(),
            std::ptr::from_mut(io).cast::<u8>(),
            IO_BYTES,
        )
    };
    if ga3 {
        io.p[0] = ga3_ptr;
    }
    // SAFETY: the memory outlives the replay.
    let mem = unsafe { &mut *rp.mem };
    for (i, b) in &ev.deltas {
        mem.apply(*i, b);
    }
    ev.ret
}

fn io_op(io: &Io) -> i32 {
    io.i[0]
}

fn rd_event(r: &mut Reader) -> Event {
    let op = r.u32();
    let io_in = r.bytes(IO_BYTES).to_vec();
    let io_out = r.bytes(IO_BYTES).to_vec();
    let ret = r.u32() as i32;
    let ga3 = (op == 1).then(|| (r.bytes(GA3_IO).to_vec(), r.bytes(GA3_IO).to_vec()));
    let nd = r.u32();
    let deltas = (0..nd)
        .map(|_| {
            let i = r.u32();
            let n = r.u32() as usize;
            (i, r.bytes(n).to_vec())
        })
        .collect();
    Event {
        op,
        io_in,
        io_out,
        ret,
        ga3,
        deltas,
    }
}

fn replay_one(rec: &[u8]) -> Result<(), String> {
    let mut r = Reader(rec, 0);
    let id = r.u32();
    let alen = r.u32() as usize;
    let args = r.bytes(alen).to_vec();
    let pre = rd_snap(&mut r);
    let mut events = Vec::new();
    while r.u32() == 1 {
        events.push(rd_event(&mut r));
    }
    let ret = r.u32();
    let outs = r
        .bytes(OUT_BYTES.get(id as usize).copied().unwrap_or(0))
        .to_vec();
    let post = rd_snap(&mut r);

    let mut mem = Mem::new(&pre);
    let mut rp = Replay {
        events,
        pos: 0,
        mem: std::ptr::from_mut(&mut mem),
        fail: None,
    };
    let table = Table {
        ctx: std::ptr::from_mut(&mut rp).cast(),
        call: host_call,
    };
    let mut got_outs = Vec::new();
    let b = std::ptr::from_mut(&mut *mem.base);
    // SAFETY: `b` and `table` outlive the call; `Reader` yields the recorded arguments.
    let got_ret = unsafe { dispatch(id, &mut Reader(&args, 0), b, &table, &mut got_outs) };
    let name = ENTRY_NAMES.get(id as usize).copied().unwrap_or("?");
    if let Some(f) = rp.fail.take() {
        return Err(format!("{name}: {f}"));
    }
    if rp.pos != rp.events.len() {
        return Err(format!(
            "{name}: {} of {} host calls",
            rp.pos,
            rp.events.len()
        ));
    }
    if got_ret != ret || got_outs != outs {
        return Err(format!(
            "{name}: return/outputs differ ({got_ret:#x} vs {ret:#x})"
        ));
    }
    let got = mem.snap();
    for (i, (a, e)) in got.fields.iter().zip(&post.fields).enumerate() {
        if a != e {
            return Err(format!("{name}: ctx field {i} differs"));
        }
    }
    if got.base != post.base {
        return Err(format!("{name}: POD differs"));
    }
    if got.queue != post.queue {
        return Err(format!("{name}: queue differs"));
    }
    Ok(())
}

#[test]
fn golden_replay() {
    let mut pos = 0;
    let (mut n, mut seen) = (0, [0u32; 64]);
    while let Some(h) = GOLDEN.get(pos..pos + 4) {
        let len = u32::from_le_bytes([h[0], h[1], h[2], h[3]]) as usize;
        let rec = &GOLDEN[pos + 4..pos + 4 + len];
        pos += 4 + len;
        if let Err(e) = replay_one(rec) {
            panic!("entry {n}: {e}");
        }
        let id = u32::from_le_bytes([rec[0], rec[1], rec[2], rec[3]]) as usize;
        seen[id] += 1;
        n += 1;
    }
    assert_eq!(pos, GOLDEN.len());
    assert!(n >= 300, "{n} entries");
    let missing: Vec<_> = ENTRY_NAMES
        .iter()
        .zip(seen)
        .filter(|(n, s)| *s == 0 && !INSIDE_ON_UPDATE.contains(n))
        .map(|(n, _)| *n)
        .collect();
    assert!(missing.is_empty(), "no vector for {missing:?}");
}
