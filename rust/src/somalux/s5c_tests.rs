use super::*;
use std::rc::Rc;

#[derive(Default)]
struct Mock {
    log: RefCell<Vec<String>>,
    paused: bool,
}

impl Ops for Mock {
    fn go(&self, o: u32, io: Io) -> (i32, Io) {
        if o == op::SC {
            self.log
                .borrow_mut()
                .push(format!("sc {:x} {}", io.p[0] as usize, io.i[0]));
        }
        if o == op::PAUSED {
            return (i32::from(self.paused), io);
        }
        (0, io)
    }
}

fn setup(m: Mock) -> Rc<Mock> {
    let m = Rc::new(m);
    TEST_OPS.with(|t| *t.borrow_mut() = Some(m.clone()));
    GAME.with(|g| *g.borrow_mut() = Some(Game::new()));
    m
}

fn entry(p: usize, id: i32, active: bool) -> Entry {
    Entry {
        p,
        name: format!("n{id}").into_bytes(),
        id,
        active,
    }
}

#[test]
fn dispatch_order_and_active_effects() {
    let m = setup(Mock::default());
    with(|g| {
        g.handlers.push(entry(1, -1, false));
        g.effects.push(entry(2, 5, false));
        g.effects.push(entry(3, 6, true));
        g.modules.push(entry(4, 7, false));
    });
    ohpl_s5c_game_post_update(0.5);
    let order: Vec<String> = m
        .log
        .borrow()
        .iter()
        .map(|s| s.split(' ').nth(1).unwrap_or("").to_string())
        .collect();
    assert_eq!(order, ["1", "3", "4", "1", "3", "4"]);
    m.log.borrow_mut().clear();
    ohpl_s5c_game_reset_scriptables();
    assert_eq!(m.log.borrow().len(), 4);
}

#[test]
fn paused_actions_reach_modules_only() {
    let m = setup(Mock {
        paused: true,
        ..Mock::default()
    });
    with(|g| {
        g.handlers.push(entry(1, -1, false));
        g.modules.push(entry(4, 7, false));
    });
    ohpl_s5c_game_broadcast_action(3, true);
    assert_eq!(m.log.borrow().as_slice(), ["sc 4 5"]);
    assert_eq!(ohpl_s5c_game_module_by_id(7) as usize, 4);
    assert!(ohpl_s5c_game_module_by_id(8).is_null());
}

#[test]
fn flat_xml_groups() {
    let rows = parse_flat(b"0\x1fG\x1fName\x1fa\x1e1\x1fT\x1fName\x1fb\x1fID\x1f12\x1e0\x1fH\x1e");
    let g = groups(&rows);
    assert_eq!(g.len(), 2);
    assert_eq!(g[0].1.len(), 1);
    assert_eq!(g[0].1[0].int(b"ID", -1), 12);
    assert_eq!(g[1].0.int(b"ID", -1), -1);
}
