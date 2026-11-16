use super::*;
use std::rc::Rc;

#[derive(Default)]
struct Mock {
    log: RefCell<Vec<String>>,
    map_active: bool,
    fail_load: bool,
    next: Cell<usize>,
    #[allow(clippy::vec_box)]
    blocks: RefCell<Vec<Box<MoveBlock>>>,
}

impl Ops for Mock {
    fn go(&self, o: u32, mut io: Io) -> (i32, Io) {
        let text = String::from_utf8_lossy(&io.text()).replace('\0', "|");
        match o {
            op::MAP_ACTIVE => return (i32::from(self.map_active), io),
            op::NEW_STATE => {
                let n = self.next.get() + 1;
                self.next.set(n);
                io.p[0] = (0x1000 * n) as *mut c_void;
                let mut b = Box::new(MoveBlock {
                    id: io.i[1],
                    player: std::ptr::null_mut(),
                    speeds: MoveSpeeds::default(),
                });
                io.p[1] = std::ptr::from_mut(&mut *b).cast();
                self.blocks.borrow_mut().push(b);
                return (0, io);
            }
            op::LOAD_STATE => {
                return (i32::from(!self.fail_load), io);
            }
            op::CALL => {
                self.log.borrow_mut().push(format!(
                    "call {:x} {text} args={}",
                    io.p[0] as usize, io.i[0]
                ));
            }
            op::BASE => {
                self.log
                    .borrow_mut()
                    .push(format!("base {:x} {}", io.p[0] as usize, io.i[0]));
            }
            op::ERROR_LOG | op::DEAD_BROADCAST | op::DELETE => {
                self.log.borrow_mut().push(format!("op{o} {text}"));
            }
            op::BODY_SET if io.i[0] == body::SPEEDS => {
                // SAFETY: the call passes ten floats.
                let v = unsafe { std::slice::from_raw_parts(io.p[1].cast::<f32>(), 10) };
                self.log.borrow_mut().push(format!("speeds {v:?}"));
            }
            _ => {}
        }
        (0, io)
    }
}

fn setup(m: Mock) -> Rc<Mock> {
    let m = Rc::new(m);
    TEST_OPS.with(|t| *t.borrow_mut() = Some(m.clone()));
    new(0x10);
    m
}

fn take(m: &Mock) -> Vec<String> {
    std::mem::take(&mut *m.log.borrow_mut())
}

#[test]
fn state_machine_order() {
    let m = setup(Mock::default());
    add_state(b"A", 0, b"f", b"c", false);
    add_state(b"B", 3, b"f", b"c", false);
    add_state(b"A", 0, b"f", b"c", false);
    assert_eq!(state_id(false), 0);
    assert_eq!(take(&m).len(), 1);
    change_state(0, false);
    assert!(take(&m).is_empty());
    change_state(3, false);
    assert_eq!(
        take(&m),
        [
            "call 1000 void OnLeaveState(int alNextStateId) args=1",
            "call 2000 void OnEnterState(int alPrevStateId) args=1"
        ]
    );
    assert_eq!(state_id(false), 3);
    change_state(9, false);
    assert_eq!(take(&m).len(), 1);
    assert_eq!(state_id(false), 3);
}

#[test]
fn failed_load_adds_nothing() {
    let m = setup(Mock {
        fail_load: true,
        ..Mock::default()
    });
    add_state(b"A", 0, b"f", b"c", true);
    assert_eq!(state_id(true), -1);
    assert!(take(&m).is_empty());
}

#[test]
fn move_defaults_and_speeds() {
    let m = setup(Mock {
        map_active: true,
        ..Mock::default()
    });
    add_state(b"M", 0, b"f", b"c", true);
    let b = m.blocks.borrow()[0].as_ref() as *const MoveBlock;
    // SAFETY: the mock keeps the block alive.
    let s = unsafe { (*b).speeds };
    assert_eq!(
        (s.forward_opposite_mul, s.max_sideway_mul, s.max_forward),
        (1.0, 1.0, 0.0)
    );
    with(|p| p.body = 0x77);
    take(&m);
    update(0.016);
    let log = take(&m);
    assert_eq!(log[0], "base 10 0");
    assert_eq!(log[1], "base 1000 0");
    assert!(log[2].starts_with("speeds [0.0, -0.0, 0.0, -0.0,"));
}

#[test]
fn update_skips_without_map() {
    let m = setup(Mock::default());
    update(0.1);
    post_update(0.1);
    assert!(take(&m).is_empty());
}

#[test]
fn damage_ids_and_death() {
    let m = setup(Mock::default());
    with(|p| {
        p.health = 100.0;
        p.max_health = 100.0;
    });
    let src = SomaId::default();
    assert!(give_damage(1.0, 0, 0.0, src, 5));
    assert!(!give_damage(1.0, 0, 0.0, src, 5));
    assert!(give_damage(1.0, 0, 0.0, src, -1));
    assert!(give_damage(1.0, 0, 0.0, src, -1));
    for id in 10..18 {
        assert!(give_damage(1.0, 0, 0.0, src, id));
    }
    assert_eq!(with(|p| p.damage_ids.len()), Some(8));
    assert!(give_damage(1.0, 0, 0.0, src, 5), "id 5 was evicted");
    assert_eq!(with(|p| p.health), Some(100.0 - 12.0));
    take(&m);
    set_health(0.0);
    let log = take(&m);
    assert_eq!(log.len(), 3);
    assert!(log[0].starts_with("call 10 void OnPlayerDead"));
    assert!(log[1].starts_with("call 0 void OnPlayerDead"));
    assert!(is_dead());
    assert!(!give_damage(1.0, 0, 0.0, src, -1));
}

#[test]
fn map_enter_resets_to_state_zero() {
    let m = setup(Mock::default());
    add_state(b"A", 0, b"f", b"c", false);
    add_state(b"B", 2, b"f", b"c", false);
    change_state(2, false);
    take(&m);
    map_message(b"void OnMapEnter(cLuxMap @apMap)", 0);
    let log = take(&m);
    assert_eq!(state_id(false), 0);
    assert_eq!(
        log[0],
        "call 1000 void OnEnterState(int alPrevStateId) args=1"
    );
    assert_eq!(log.len(), 1 + 3);
}

#[test]
fn reset_message_drops_state_but_not_id_zero() {
    let m = setup(Mock::default());
    add_state(b"A", 0, b"f", b"c", false);
    add_state(b"B", 2, b"f", b"c", false);
    change_state(2, false);
    message(b"void Reset()");
    assert_eq!(state_id(false), -1);
    free();
    assert_eq!(take(&m).iter().filter(|l| l.starts_with("op6")).count(), 2);
}
