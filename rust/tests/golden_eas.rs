//! E-as: the array/string add-ons against what the C++ add-ons did, recorded from the real engine on
//! 79957c1 (`tests/golden/E-as/suite-amnesia.out`, from `suite-amnesia.as`).

use ohpl::script::addons::sort::std_sort;
use std::fs;
use std::path::{Path, PathBuf};
use std::process::Command;

fn golden() -> PathBuf {
    Path::new(env!("CARGO_MANIFEST_DIR")).join("tests/golden/E-as")
}

struct Rng(u32);

impl Rng {
    fn next(&mut self, n: u32) -> u32 {
        self.0 = self.0.wrapping_mul(1_103_515_245).wrapping_add(12345);
        (self.0 >> 8) % n
    }
}

struct Items(Vec<(i32, i32)>);

impl Items {
    fn make(r: &mut Rng, n: u32, range: u32) -> Items {
        Items((0..n).map(|i| (r.next(range) as i32, i as i32)).collect())
    }

    /// The object-array sort of the add-on: `std::sort` over the handles with `opCmp(a, b) < 0`.
    fn sort(&self, order: &mut [usize], start: usize, count: usize, asc: bool) {
        let k = |h: usize| self.0.get(h - 1).map_or(0, |x| x.0);
        let end = (start + count).min(order.len());
        if let Some(range) = order.get_mut(start..end) {
            std_sort(range, |a, b| {
                let (a, b) = if asc { (a, b) } else { (b, a) };
                k(a) < k(b)
            });
        }
    }

    fn line(&self, order: &[usize]) -> String {
        let mut s = format!("{}:", order.len());
        for &h in order {
            let (k, id) = self.0.get(h - 1).copied().unwrap_or_default();
            s += &format!(" {k}/{id}");
        }
        s
    }

    fn ids(&self, order: &[usize]) -> String {
        order
            .iter()
            .map(|&h| format!("{} ", self.0.get(h - 1).map_or(0, |x| x.1)))
            .collect()
    }
}

#[test]
fn item_sort_order_matches_the_recorded_engine() {
    let text = fs::read_to_string(golden().join("suite-amnesia.out")).unwrap_or_default();
    let section: Vec<&str> = text
        .lines()
        .skip_while(|l| *l != "-- items")
        .skip(1)
        .take_while(|l| !l.starts_with("live"))
        .collect();
    assert!(section.len() >= 14, "recording missing: {}", section.len());
    let mut r = Rng(12345);
    for _ in 0..100 {
        r.next(1000);
    }
    let seq = |n: usize| (1..=n).collect::<Vec<usize>>();
    let mut want = section.iter();
    let mut check = |got: String| assert_eq!(Some(&got.as_str()), want.next(), "line");

    let a = Items::make(&mut r, 5, 3);
    let mut o = seq(5);
    check(a.line(&o));
    a.sort(&mut o, 0, 5, true);
    check(a.line(&o));
    a.sort(&mut o, 0, 5, false);
    check(a.line(&o));

    let b = Items::make(&mut r, 17, 4);
    let mut o = seq(17);
    b.sort(&mut o, 0, 17, true);
    check(b.line(&o));

    let c = Items::make(&mut r, 40, 6);
    let mut o = seq(40);
    c.sort(&mut o, 0, 40, true);
    check(c.line(&o));
    c.sort(&mut o, 0, 40, false);
    check(c.line(&o));
    c.sort(&mut o, 5, 20, true);
    check(c.line(&o));

    let d = Items::make(&mut r, 300, 10);
    let mut o = seq(300);
    d.sort(&mut o, 0, 300, true);
    check(d.ids(&o));
    d.sort(&mut o, 0, 300, false);
    check(d.ids(&o));

    let e = Items::make(&mut r, 2000, 5);
    let mut o = seq(2000);
    e.sort(&mut o, 0, 2000, true);
    let hash = o
        .iter()
        .map(|&h| e.0.get(h - 1).map_or(0, |x| x.1))
        .fold(0i32, |h, id| h.wrapping_mul(31).wrapping_add(id));
    check(format!("hash {hash}"));
}

#[test]
fn suite_replays_through_the_engine() {
    let dir = std::env::var_os("OPENHPL_BUILD_DIR")
        .map(PathBuf::from)
        .unwrap_or_else(|| Path::new(env!("CARGO_MANIFEST_DIR")).join("../amnesia/src/build"));
    let bin = dir.join("Soma.bin.aarch64");
    if !bin.exists() {
        eprintln!("skipped: {} not built", bin.display());
        return;
    }
    let out = Command::new(&bin)
        .env("OPENHPL_AS_SUITE", golden().join("suite-amnesia.as"))
        .output()
        .expect("run the suite");
    let want = fs::read_to_string(golden().join("suite-amnesia.out")).unwrap_or_default();
    // live counts follow when the GC destroys objects, which depends on heap addresses
    let got: String = String::from_utf8_lossy(&out.stdout)
        .lines()
        .map(|l| match l.find("live ") {
            Some(i) => {
                let head = &l[..i + 5];
                let rest = l[i + 5..].trim_start_matches("after ");
                let tail = rest
                    .trim_start_matches(['-', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9']);
                let after = if rest.len() != l[i + 5..].len() {
                    "after "
                } else {
                    ""
                };
                format!("{head}{after}N{tail}\n")
            }
            None => format!("{l}\n"),
        })
        .collect();
    assert_eq!(got, want);
}
