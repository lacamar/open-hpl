//! Map handler against a fake host: call order of a map change, current-map switch and pause bookkeeping.

use super::a7_ffi::Ctx;
use super::mhandler::{H, Handler, hop};
use super::mhost::{self, A, op};
use std::sync::Mutex;

static LOG: Mutex<Vec<i32>> = Mutex::new(Vec::new());
static CTX: Mutex<usize> = Mutex::new(0);

unsafe extern "C" fn fake(o: i32, a: *mut A) {
    let a = unsafe { &mut *a };
    LOG.lock().unwrap().push(o);
    match o {
        hop::MAP_CTX => a.r = *CTX.lock().unwrap(),
        hop::FADE if a.i[0] == 2 => a.i[0] = 0,
        op::LOG => {}
        _ => {}
    }
}

fn setup() -> (H, std::sync::MutexGuard<'static, ()>) {
    let g = mhost::TEST_LOCK.lock().unwrap_or_else(|e| e.into_inner());
    mhost::set_host(Some(fake));
    LOG.lock().unwrap().clear();
    *CTX.lock().unwrap() = Box::into_raw(Box::<Ctx>::default()) as usize;
    let h = Handler::create();
    // SAFETY: just created, leaked for the test.
    let h = unsafe { H::new(h) };
    h.init(0x99);
    (h, g)
}

fn ops() -> Vec<i32> {
    std::mem::take(&mut *LOG.lock().unwrap())
}

#[test]
fn change_map_fades_out_and_disables_the_player() {
    let (h, _g) = setup();
    h.change_map(b"03_dungeon", b"start", b"snd_a", b"snd_b");
    assert_eq!(ops(), [hop::PLAY_GUI_SOUND, hop::FADE, hop::PLAYER_ACTIVE]);
}

#[test]
fn pause_and_resume_reach_the_engine_once() {
    let (h, _g) = setup();
    h.pause_sounds();
    h.pause_sounds();
    h.resume_sounds();
    h.resume_sounds();
    assert_eq!(ops(), [hop::SOUND_ALL, hop::SOUND_ALL]);
}

#[test]
fn first_map_goes_through_the_enter_plan() {
    let (h, _g) = setup();
    h.set_current_map(0x10, false, true, b"start");
    let kinds: Vec<i32> = ops()
        .into_iter()
        .filter(|o| matches!(*o, hop::MODULE_MSG | hop::SET_CURRENT | hop::VIEW))
        .collect();
    assert_eq!(
        kinds,
        [
            hop::SET_CURRENT,
            hop::MODULE_MSG,
            hop::MODULE_MSG,
            hop::VIEW
        ]
    );
}

#[test]
fn no_change_pending_does_not_start_a_map_change() {
    let (h, _g) = setup();
    h.update(0.016);
    let o = ops();
    assert_eq!(o.first(), Some(&hop::CLEAR_RENDER_LISTS));
    assert!(!o.contains(&hop::SAVE_PENDING));
}
