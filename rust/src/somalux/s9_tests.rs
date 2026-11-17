use super::*;
use std::cell::RefCell;

struct Mock(RefCell<Vec<String>>, fn(usize, &[u8]));

impl Ops for Mock {
    fn go(&self, code: u32, io: Io) -> (i32, Io) {
        if code == op::FIRE {
            let name = io
                .text()
                .split(|&c| c == 0)
                .next()
                .unwrap_or_default()
                .to_vec();
            self.0.borrow_mut().push(lossy(&name));
            (self.1)(io.p[0] as usize, &name);
        }
        (0, io)
    }
}

fn fired(m: usize, dt: f32, act: fn(usize, &[u8])) -> Vec<String> {
    let mock = Mock(RefCell::new(Vec::new()), act);
    timers_update(&mock, m, dt);
    mock.0.into_inner()
}

#[test]
fn timers_fire_in_list_order_and_expire() {
    let m = 0x1000;
    with_map(m, |t| {
        t.add(b"a", 1.0, b"fa");
        t.add(b"b", 0.5, b"fb");
    });
    assert!(fired(m, 0.25, |_, _| {}).is_empty());
    assert_eq!(fired(m, 0.25, |_, _| {}), ["b"]);
    assert_eq!(with_map(m, |t| t.timers.len()), 1);
    assert_eq!(fired(m, 0.5, |_, _| {}), ["a"]);
    assert_eq!(with_map(m, |t| (t.timers.len(), t.stamp)), (0, 3));
    ohpl_s9_map_free(m);
}

#[test]
fn restart_and_remove_inside_callback() {
    let m = 0x2000;
    with_map(m, |t| {
        t.add(b"r", 1.0, b"f");
        t.add(b"x", 1.0, b"g");
    });
    let f = fired(m, 1.0, |m, name| {
        with_map(m, |t| {
            if name == b"x" {
                t.restart_current(-1.0);
                t.remove(b"r");
                t.add(b"n", 0.0, b"h");
            }
        });
    });
    assert_eq!(f, ["x"]);
    let names = with_map(m, |t| {
        t.timers.iter().map(|x| x.name.clone()).collect::<Vec<_>>()
    });
    assert_eq!(names, [b"n".to_vec(), b"x".to_vec()]);
    assert_eq!(with_map(m, |t| t.find(b"n").map(|x| x.time)), Some(0.001));
    ohpl_s9_map_free(m);
}

#[test]
fn collide_groups() {
    let mut g = Vec::new();
    assert_eq!(collide_flag(&mut g, b""), 0);
    assert_eq!(collide_flag(&mut g, b"+player"), 0xFFFF_0001);
    assert_eq!(collide_flag(&mut g, b"-player"), 0xFFFE_FFFF);
    assert_eq!(collide_flag(&mut g, b"+a +b -c"), 0xFFF7_0006);
    assert_eq!(collide_flag(&mut g, b"bogus"), 0);
    assert!(collide_match(0xFFFF_0001, 0xFFFF_0003));
    assert!(!collide_match(0xFFFE_0001, 0xFFFF_0001));
}

#[test]
fn start_map_values() {
    assert_eq!(start_map_value(b"File:m.hpm, Pos:s", b""), b"m.hpm");
    assert_eq!(start_map_value(b"File:m.hpm, Pos:s", b"Pos"), b"s");
    assert_eq!(start_map_value(b"A:x, m.hpm", b"B"), b"m.hpm");
    assert_eq!(start_map_value(b"", b""), b"");
}

#[test]
fn pixel_format_bytes() {
    assert_eq!(bytes_per_pixel(0x1636_2004), 4);
    assert_eq!(bytes_per_pixel(0x1516_1802), 2);
    assert_eq!(bytes_per_pixel(0x3259_5559), 2);
    assert_eq!(bytes_per_pixel(0x3231_564E), 1);
    assert_eq!(bytes_per_pixel(0), 0);
}

#[test]
fn collide_golden_from_cpp() {
    let mut g = Vec::new();
    for l in include_str!("../../tests/golden/S9/collide.txt").lines() {
        let (spec, want) = l.rsplit_once('|').unwrap_or_default();
        let spec = spec.replace("\\t", "\t");
        let want = u32::from_str_radix(want, 16).unwrap_or(u32::MAX);
        assert_eq!(collide_flag(&mut g, spec.as_bytes()), want, "{l}");
    }
}

struct Replay {
    fired: RefCell<String>,
    count: std::cell::Cell<usize>,
}

impl Ops for Replay {
    fn go(&self, code: u32, io: Io) -> (i32, Io) {
        if code != op::FIRE {
            return (0, io);
        }
        let map = io.p[0] as usize;
        let name = io
            .text()
            .split(|&c| c == 0)
            .next()
            .unwrap_or_default()
            .to_vec();
        self.fired
            .borrow_mut()
            .push_str(&format!("{} ", lossy(&name)));
        let h = name
            .iter()
            .fold(0u32, |h, &c| h.wrapping_mul(31).wrapping_add(u32::from(c)));
        let c = self.count.get();
        let new = format!("n{}", c % 4).into_bytes();
        self.count.set(c + 1);
        with_map(map, |t| match (h as usize).wrapping_add(c) % 5 {
            0 => t.restart_current(-1.0),
            1 => t.restart_current(0.3),
            2 => t.add(&new, 0.2, b"f"),
            3 => t.remove(&new),
            _ => {}
        });
        (0, io)
    }
}

#[test]
fn timers_golden_from_cpp() {
    let m = 0x3000;
    let rp = Replay {
        fired: RefCell::new(String::new()),
        count: std::cell::Cell::new(0),
    };
    let mut lines = include_str!("../../tests/golden/S9/timers.txt").lines();
    while let Some(op_line) = lines.next() {
        let p: Vec<&str> = op_line.split(' ').collect();
        match p.as_slice() {
            ["A", n, t] => with_map(m, |x| x.add(n.as_bytes(), t.parse().unwrap_or(0.0), b"f")),
            ["R", n] => with_map(m, |x| x.remove(n.as_bytes())),
            ["P", n, b] => with_map(m, |x| {
                x.timers
                    .iter_mut()
                    .filter(|t| t.name == n.as_bytes())
                    .for_each(|t| t.paused = *b == "1")
            }),
            ["U", dt] => {
                rp.fired.borrow_mut().clear();
                timers_update(&rp, m, dt.parse().unwrap_or(0.0));
            }
            _ => panic!("bad op {op_line}"),
        }
        let got = with_map(m, |x| {
            let mut s = String::from("=");
            for t in &x.timers {
                s += &format!(
                    " {}:{:x}:{}{}",
                    lossy(&t.name),
                    t.time.to_bits(),
                    u8::from(t.paused),
                    u8::from(t.removed)
                );
            }
            format!("{s} |{}|{}", rp.fired.borrow(), x.stamp)
        });
        assert_eq!(Some(got.as_str()), lines.next(), "after {op_line}");
    }
    ohpl_s9_map_free(m);
}
