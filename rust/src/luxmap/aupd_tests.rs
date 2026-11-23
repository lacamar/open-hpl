//! Area updates against a fake host: call order of liquid, sticky and slime updates.

use super::a7_area::{A7Area, Pod, T_LIQUID, T_SLIME, T_STICKY};
use super::aupd::{self, aop};
use super::mhost::{self, A, body, op};
use std::sync::Mutex;

static LOG: Mutex<Vec<i32>> = Mutex::new(Vec::new());
static BODIES: [usize; 1] = [0x1000];
static COLLIDES: Mutex<bool> = Mutex::new(true);

unsafe extern "C" fn fake(o: i32, a: *mut A) {
    // SAFETY: the caller passes a live block.
    let a = unsafe { &mut *a };
    LOG.lock().unwrap().push(o);
    match o {
        aop::ENGINE_STEP => a.f[0] = 1.0 / 60.0,
        aop::BODIES => {
            a.i[0] = 1;
            a.r = BODIES.as_ptr() as usize;
        }
        op::BODY_INFO => {
            a.i[0] = body::COLLIDE | body::ACTIVE;
            a.f[0] = 1.0;
        }
        op::BODY_PAIR => a.i[0] = 1,
        aop::BODY_IN_AREA => a.i[0] = 1,
        aop::LQ_BODY_READ => a.i[1] = 1,
        aop::COLLIDES => a.i[0] = i32::from(*COLLIDES.lock().unwrap()),
        aop::ST_PRE_ATTACH => a.i[1] = -1,
        _ => {}
    }
}

fn setup() -> std::sync::MutexGuard<'static, ()> {
    let g = mhost::TEST_LOCK.lock().unwrap_or_else(|e| e.into_inner());
    mhost::set_host(Some(fake));
    LOG.lock().unwrap().clear();
    *COLLIDES.lock().unwrap() = true;
    g
}

fn ops() -> Vec<i32> {
    std::mem::take(&mut *LOG.lock().unwrap())
}

fn area(kind: i32) -> Box<A7Area> {
    Box::new(A7Area::new(kind).unwrap())
}

#[test]
fn liquid_sets_up_a_body_once_it_is_inside() {
    let _g = setup();
    let mut a = area(T_LIQUID);
    // SAFETY: `a` is live for the call; no shell or map is reached.
    unsafe { aupd::update(&raw mut *a, 1, std::ptr::null_mut(), 1.0 / 60.0) };
    assert_eq!(
        ops(),
        [
            aop::ENGINE_STEP,
            aop::ENV,
            aop::BODIES,
            op::BODY_INFO,
            op::BODY_PAIR,
            aop::LQ_BODY_READ,
            aop::LQ_BODY_SET
        ]
    );
}

#[test]
fn liquid_skips_a_step_shorter_than_the_engine_step() {
    let _g = setup();
    let mut a = area(T_LIQUID);
    // SAFETY: as above.
    unsafe { aupd::update(&raw mut *a, 1, std::ptr::null_mut(), 0.001) };
    assert_eq!(ops(), [aop::ENGINE_STEP]);
}

#[test]
fn sticky_attaches_a_body_then_detaches_it() {
    let _g = setup();
    let mut a = area(T_STICKY);
    let p = &raw mut *a;
    // SAFETY: as above.
    unsafe { aupd::update(p, 1, std::ptr::null_mut(), 1.0 / 60.0) };
    let seen = ops();
    assert!(seen.contains(&aop::ST_PRE_ATTACH) && seen.contains(&aop::ST_ATTACH_APPLY));
    let Pod::Sticky(s) = a.pod else { panic!() };
    assert_eq!((s.attached_body, s.last_attached), (0x1000, 0x1000));
    // SAFETY: as above.
    unsafe { aupd::detach_body(p, 1, std::ptr::null_mut()) };
    assert_eq!(*ops().last().unwrap(), aop::ST_DETACH_APPLY);
    let Pod::Sticky(s) = a.pod else { panic!() };
    assert_eq!(s.attached_body, 0);
}

#[test]
fn slime_attacks_in_order_and_stays_quiet_without_a_collision() {
    let _g = setup();
    let mut a = area(T_SLIME);
    if let Pod::Slime(s) = &mut a.pod {
        s.disable_after_attack = true;
    }
    // SAFETY: as above.
    unsafe { aupd::update(&raw mut *a, 1, std::ptr::null_mut(), 1.0) };
    assert_eq!(
        ops(),
        [
            aop::COLLIDES,
            aop::SLIME_DAMAGE,
            aop::PLAY_SOUND,
            aop::SHAKE,
            aop::SET_ACTIVE
        ]
    );
    *COLLIDES.lock().unwrap() = false;
    // SAFETY: as above.
    unsafe { aupd::update(&raw mut *a, 1, std::ptr::null_mut(), 1.0) };
    assert!(ops().iter().all(|o| *o != aop::SLIME_DAMAGE));
}
