use super::host::{Io, OP_BODY_POS, OP_BODY_SIZE, v_of};
use super::nav::*;
use crate::math::{Vector3f, v3};

struct Engine {
    scratch: Vec<usize>,
    path: Vec<usize>,
    used: Vec<usize>,
    at_center: bool,
    astar_start: Vector3f,
    ok: bool,
}

fn put_ptr(io: &mut Io, slot: usize, p: usize) {
    io.i[slot] = p as u32 as i32;
    io.i[slot + 1] = (p >> 32) as u32 as i32;
}

fn get_ptr(io: &Io, slot: usize) -> usize {
    (io.i[slot] as u32 as usize) | ((io.i[slot + 1] as u32 as usize) << 32)
}

impl Engine {
    fn new() -> Self {
        Self {
            scratch: Vec::new(),
            path: vec![0x1_0000_0001, 0x1_0000_0002, 0x1_0000_0003],
            used: Vec::new(),
            at_center: true,
            astar_start: v3(0.0, 0.0, 0.0),
            ok: true,
        }
    }

    fn call(&mut self, op: u32, io: &mut Io) -> i32 {
        match op {
            OP_BODY_POS => io.v[0] = [1.0, 2.0, 3.0],
            OP_BODY_SIZE => io.v[0] = [0.5, 1.8, 0.5],
            OP_NC_AT_CENTER => return i32::from(self.at_center),
            OP_ASTAR_PATH => {
                self.astar_start = v_of(io.v[0]);
                self.scratch = self.path.clone();
                io.i[2] = self.scratch.len() as i32;
                return i32::from(self.ok);
            }
            OP_NC_ITER => {
                self.scratch = vec![0x10, 0x20, 0x30];
                return 3;
            }
            OP_SCRATCH_AT => put_ptr(io, 0, self.scratch[io.i[0] as usize]),
            OP_NODE_POS => io.v[0] = [get_ptr(io, 0) as f32, 0.0, 0.0],
            OP_NODE_USED_AS_GOAL => return i32::from(self.used.contains(&get_ptr(io, 0))),
            _ => {}
        }
        0
    }
}

fn run(nav: *mut Nav, e: &mut Engine, op: u32, io: &mut Io) -> i32 {
    let mut inner = |op: u32, io: &mut Io| e.call(op, io);
    // SAFETY: `nav` comes from a live `Box<Nav>` held by the test.
    unsafe { intercept(nav, &mut inner, op, io) }.unwrap_or(-99)
}

fn with_astar() -> Box<Nav> {
    let mut n = Box::new(Nav::new());
    n.nc = 0xAA;
    n.astar = 0xBB;
    n
}

#[test]
fn move_to_fills_path_and_pops_from_the_back() {
    let mut n = with_astar();
    let (p, mut e) = (&raw mut *n, Engine::new());
    n.win.len = 7;
    let mut io = Io::default();
    io.v[0] = [4.0, 5.0, 6.0];
    assert_eq!(run(p, &mut e, OP_PF_MOVE_TO, &mut io), 1);
    assert!(n.moving && n.win.len == 0);
    assert_eq!(n.goal, [4.0, 5.0, 6.0]);
    assert_eq!(e.astar_start, v3(1.0, 2.01, 3.0));
    assert_eq!(run(p, &mut e, OP_PF_PATH_LEN, &mut Io::default()), 3);
    let mut back = Io::default();
    run(p, &mut e, OP_PF_PATH_BACK, &mut back);
    assert_eq!(back.v[0][0], 0x1_0000_0003_u64 as f32);
    run(p, &mut e, OP_PF_PATH_POP, &mut Io::default());
    assert_eq!(run(p, &mut e, OP_PF_PATH_LEN, &mut Io::default()), 2);
    assert_eq!(n.path.first().copied(), Some(0x1_0000_0001));
    run(p, &mut e, OP_PF_PATH_CLEAR, &mut Io::default());
    assert!(n.path.is_empty() && n.moving);
    n.win.len = 3;
    run(p, &mut e, OP_PF_STOP, &mut Io::default());
    assert!(!n.moving && n.win.len == 0);
}

#[test]
fn node_container_below_feet_lowers_the_start() {
    let mut n = with_astar();
    let (p, mut e) = (&raw mut *n, Engine::new());
    e.at_center = false;
    e.ok = false;
    assert_eq!(run(p, &mut e, OP_PF_MOVE_TO, &mut Io::default()), 0);
    assert_eq!(e.astar_start, v3(1.0, 2.0 - 0.9 + 0.01, 3.0));
    assert_eq!(n.path.len(), 3);
}

#[test]
fn no_astar_sets_the_goal_and_finds_no_path() {
    let mut n = Box::new(Nav::new());
    let (p, mut e) = (&raw mut *n, Engine::new());
    let mut io = Io::default();
    io.v[0] = [9.0, 8.0, 7.0];
    assert_eq!(run(p, &mut e, OP_PF_MOVE_TO, &mut io), 0);
    assert!(n.moving && n.path.is_empty());
    assert_eq!(n.goal, [9.0, 8.0, 7.0]);
    assert_eq!(run(p, &mut e, OP_NC_PRESENT, &mut Io::default()), 0);
}

#[test]
fn node_iteration_skips_the_last_search_and_goal_nodes() {
    let mut n = with_astar();
    let (p, mut e) = (&raw mut *n, Engine::new());
    e.used = vec![0x30];
    let mut io = Io::default();
    io.i[0] = 1;
    assert_eq!(run(p, &mut e, OP_NC_ITER_BEGIN, &mut io), 3);
    let node = |i: i32, skip_used: i32| Io {
        i: [i, skip_used, 0, 0, 0, 0, 0, 0],
        ..Io::default()
    };
    let mut a = node(1, 1);
    assert_eq!(run(p, &mut e, OP_NC_ITER_NODE, &mut a), 1);
    assert_eq!(a.v[0][0], 32.0);
    assert_eq!(run(p, &mut e, OP_NC_ITER_NODE, &mut node(2, 1)), 0);
    assert_eq!(run(p, &mut e, OP_NC_ITER_NODE, &mut node(2, 0)), 1);
    run(p, &mut e, OP_NC_COMMIT_LAST, &mut node(1, 0));
    assert_eq!(run(p, &mut e, OP_NC_ITER_NODE, &mut node(1, 0)), 0);
    let mut other = Io::default();
    other.i[0] = 0;
    run(p, &mut e, OP_NC_ITER_BEGIN, &mut other);
    assert_eq!(run(p, &mut e, OP_NC_ITER_NODE, &mut node(1, 0)), 1);
}

#[test]
fn handles_resolve_until_destroyed() {
    let bits = create();
    let p = ptr(bits);
    assert!(!p.is_null());
    // SAFETY: `p` is live until `destroy`.
    unsafe { (*p).astar = 0x77 };
    assert_eq!(destroy(bits), 0x77);
    assert!(ptr(bits).is_null());
    assert_eq!(destroy(bits), 0);
    assert!(ptr(0).is_null());
}

fn field<'a>(line: &'a str, key: &str) -> &'a str {
    line.split_once(key)
        .map_or("", |(_, r)| r.split(' ').next().unwrap_or(""))
}

fn golden_replay(dump: &str) -> (usize, usize) {
    let mut last: std::collections::HashMap<&str, (Vec<&str>, i32)> =
        std::collections::HashMap::new();
    let (mut pops, mut fresh) = (0, 0);
    for line in dump.lines().take_while(|l| *l != "END") {
        let name = line.split(' ').next().unwrap_or("");
        let ids: Vec<&str> = field(line, "ids=")
            .split(',')
            .filter(|s| !s.is_empty())
            .collect();
        let win: i32 = field(line, "win=").parse().unwrap_or(-1);
        if let Some((pids, pwin)) = last.get(name) {
            if ids == *pids {
                assert!(win == pwin + 1 || win == 0, "{line}");
            } else if !pids.is_empty() && ids[..] == pids[..pids.len() - 1] {
                pops += 1;
                assert_eq!(win, 0, "{line}");
            } else {
                fresh += 1;
            }
        }
        last.insert(name, (ids, win));
    }
    (pops, fresh)
}

#[test]
fn golden_dumps_pop_from_the_back_and_restart_the_window() {
    assert_eq!(
        golden_replay(include_str!("../../tests/golden/A4b/a4b-prison.txt")),
        (12, 12)
    );
    assert_eq!(
        golden_replay(include_str!("../../tests/golden/A4b/a4b-archives.txt")),
        (3, 1)
    );
}
