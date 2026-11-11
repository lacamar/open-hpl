use super::scenemgr::*;
use super::soundentity::*;
use super::world::*;
use crate::types::{Color, Vector3f};

fn v3(x: f32, y: f32, z: f32) -> Vector3f {
    Vector3f { x, y, z }
}

#[derive(Default)]
struct Fake {
    entries: [bool; 3],
    valid: [bool; 3],
    result: Option<PlayResult>,
    emitter: bool,
    dist: f32,
    log: Vec<String>,
}

impl SoundHost for Fake {
    fn silent(&mut self) -> bool {
        false
    }
    fn has_entry(&mut self, t: i32) -> bool {
        self.entries[t as usize]
    }
    fn is_valid(&mut self, t: i32) -> bool {
        self.valid[t as usize]
    }
    fn clear_entry(&mut self, t: i32) {
        self.entries[t as usize] = false;
        self.log.push(format!("clear {t}"));
    }
    fn stop_entry(&mut self, t: i32) {
        self.log.push(format!("stop {t}"));
    }
    fn fade_in(&mut self, t: i32, _: f32) {
        self.log.push(format!("fade_in {t}"));
    }
    fn fade_out(&mut self, t: i32, _: f32) {
        self.log.push(format!("fade_out {t}"));
    }
    fn update_position(&mut self, t: i32) {
        self.log.push(format!("pos {t}"));
    }
    fn play_sound(&mut self, t: i32, looping: bool) -> PlayResult {
        self.log.push(format!("play {t} {looping}"));
        let r = self.result.unwrap_or(PlayResult::Played);
        self.entries[t as usize] = r == PlayResult::Played;
        self.valid[t as usize] = r == PlayResult::Played;
        r
    }
    fn on_start(&mut self) {
        self.log.push("start".into());
    }
    fn emitter(&mut self) -> bool {
        self.emitter
    }
    fn listener_sqr_dist(&mut self) -> f32 {
        self.dist
    }
}

fn cfg(looping: bool, interval: f32) -> SoundCfg {
    SoundCfg {
        interval,
        random: 0.0,
        looping,
        use_3d: true,
        has_start: false,
    }
}

#[test]
fn silent_init_is_stopped_and_removable() {
    let s = init(&cfg(false, 0.0), false, true);
    assert!(s.stopped && s.started && s.skip_start_end && s.remove_when_over);
    assert!(is_stopped(&s, false) && !is_stopped(&s, true));
}

#[test]
fn play_oneshot_then_entry_dies_stops_entity() {
    let c = cfg(false, 0.0);
    let mut s = init(&c, true, false);
    let mut h = Fake {
        emitter: true,
        ..Fake::default()
    };
    play(&mut s, &mut h, &c, true);
    assert!(s.started && h.entries[0]);
    h.valid[0] = false;
    update(&mut s, &mut h, &c, 0.1, 10.0);
    assert!(s.stopped && !h.entries[0]);
}

#[test]
fn failed_main_stops_but_priority_failure_of_loop_does_not() {
    let c = cfg(false, 0.0);
    let mut s = init(&c, false, false);
    let mut h = Fake {
        result: Some(PlayResult::Failed {
            not_enough_channels: false,
        }),
        ..Fake::default()
    };
    play(&mut s, &mut h, &c, false);
    assert!(s.stopped);

    let c = cfg(true, 0.0);
    let mut s = init(&c, false, false);
    h.result = Some(PlayResult::Failed {
        not_enough_channels: true,
    });
    play(&mut s, &mut h, &c, false);
    assert!(!s.stopped);
}

#[test]
fn priority_release_sleeps_then_restarts() {
    let c = cfg(true, 0.0);
    let mut s = init(&c, false, false);
    let mut h = Fake {
        emitter: true,
        dist: 0.0,
        ..Fake::default()
    };
    play(&mut s, &mut h, &c, false);
    priority_release(&mut s);
    h.valid[0] = false;
    update(&mut s, &mut h, &c, 0.1, 10.0);
    assert_eq!(s.sleep_count, 0.3);
    assert!(s.started && h.log.iter().filter(|l| *l == "start").count() == 1);
    update(&mut s, &mut h, &c, 0.1, 10.0);
    assert!((s.sleep_count - 0.2).abs() < 1e-6);
}

#[test]
fn looping_sound_out_of_range_stops_then_resumes() {
    let c = cfg(true, 0.0);
    let mut s = init(&c, false, false);
    let mut h = Fake {
        emitter: true,
        dist: 10_000.0,
        ..Fake::default()
    };
    play(&mut s, &mut h, &c, false);
    update(&mut s, &mut h, &c, 0.1, 10.0);
    assert!(s.out_of_range && !h.entries[0]);
    h.dist = 1.0;
    update(&mut s, &mut h, &c, 0.1, 10.0);
    assert!(!s.out_of_range && h.entries[0]);
}

#[test]
fn fade_out_without_entry_stops_and_with_stale_entry_clears() {
    let mut s = init(&cfg(true, 0.0), false, false);
    let mut h = Fake::default();
    fade_out(&mut s, &mut h, 1.0);
    assert!(s.fading_out && s.stopped);
    s.stopped = false;
    h.entries[0] = true;
    fade_out(&mut s, &mut h, 1.0);
    assert!(s.stopped && !h.entries[0]);
    s.stopped = false;
    h.entries[0] = true;
    h.valid[0] = true;
    fade_out(&mut s, &mut h, 1.0);
    assert_eq!(h.log.last().map(String::as_str), Some("fade_out 0"));
}

#[test]
fn stop_plays_end_sound_only_when_looping() {
    let c = cfg(true, 0.0);
    let mut s = init(&c, false, false);
    let mut h = Fake {
        emitter: true,
        ..Fake::default()
    };
    play(&mut s, &mut h, &c, false);
    h.log.clear();
    stop(&mut s, &mut h, &c, true);
    assert_eq!(h.log, ["play 2 false", "stop 0", "clear 0"]);
}

#[test]
fn lookups() {
    let names: Vec<&[u8]> = vec![b"a", b"b", b"a"];
    assert_eq!(find_by_name(names.iter().copied(), b"a"), 0);
    assert_eq!(find_last_by_name(names.iter().copied(), b"a"), 2);
    assert_eq!(find_by_name(names.iter().copied(), b"z"), -1);
    assert_eq!(find_by_id([4, 5, 6].into_iter(), 6), 2);
    assert_eq!(find_ptr([8usize, 9].into_iter(), 9), 1);
    let starts: Vec<&[u8]> = vec![b"b", b"a", b"a", b"\xff"];
    assert_eq!(first_by_name(starts.iter().copied()), 1);
    assert_eq!(first_by_name(std::iter::empty()), -1);
}

fn field(x: f32, radius: f32) -> FieldIn {
    FieldIn {
        min: v3(x, 0.0, 0.0),
        max: v3(x + 1.0, 1.0, 1.0),
        radius,
    }
}

#[test]
fn force_fields_sorted_and_capped_at_four() {
    let fields = [
        field(0.0, 1.0),
        field(0.0, 5.0),
        field(0.0, 3.0),
        field(50.0, 9.0),
        field(0.0, 2.0),
        field(0.0, 4.0),
        field(0.0, 0.5),
    ];
    let mut out = [0u32; 4];
    let n = force_fields(&fields, v3(0.0, 0.0, 0.0), v3(2.0, 2.0, 2.0), &mut out);
    assert_eq!(n, 4);
    assert_eq!(out, [1, 5, 2, 4]);
}

#[test]
fn skybox_color_applies_hpl3_square_and_brightness() {
    let c = Color {
        r: 0.5,
        g: 0.25,
        b: 1.0,
        a: 0.5,
    };
    let o = skybox_color(c, true, 2.0);
    assert_eq!((o.r, o.g, o.b, o.a), (0.5, 0.125, 2.0, 0.5));
    let o = skybox_color(c, false, 2.0);
    assert_eq!((o.r, o.g, o.b, o.a), (1.0, 0.5, 2.0, 0.5));
}

#[test]
fn pre_update_counts_and_never_hangs() {
    assert_eq!(pre_update_steps(1.0, 0.25), 4);
    assert_eq!(pre_update_steps(0.0, 0.25), 0);
    assert_eq!(pre_update_steps(1.0, 0.0), 1);
    assert_eq!(pre_update_steps(1.0, -1.0), 1);
    assert_eq!(pre_update_steps(1.0, f32::NAN), 1);
    assert_eq!(pre_update_steps(1e9, 1e-3), 1);
}

#[test]
fn bounds_and_destroy_plan() {
    let (a, b) = physics_bounds(v3(0.0, 1.0, 2.0), v3(3.0, 4.0, 5.0));
    assert_eq!(
        (a.x, a.y, a.z, b.x, b.y, b.z),
        (-10.0, -9.0, -8.0, 13.0, 14.0, 15.0)
    );
    assert_eq!(destroy_plan(0), DESTROY_STATIC | DESTROY_PHYSICS);
    assert_eq!(destroy_plan(1), DESTROY_PHYSICS);
    assert_eq!(destroy_plan(3), 0);
}

#[test]
fn viewport_plan_orders_steps() {
    let v = ViewportIn {
        visible: true,
        has_composite: true,
        has_renderer: true,
        has_world: true,
        has_frustum: true,
        post_effects_active: true,
    };
    let all = FLAG_WORLD | FLAG_GUI | FLAG_POST_EFFECTS;
    assert_eq!(
        plan_viewport(all, &v),
        STEP_PRE_CALLBACK
            | STEP_RENDER_WORLD
            | STEP_POST_CALLBACK
            | STEP_GUI_3D
            | STEP_POST_EFFECTS
            | STEP_POST_FLAG
            | STEP_GUI_SCREEN
    );
    let hidden = ViewportIn {
        visible: false,
        ..v
    };
    assert_eq!(plan_viewport(all, &hidden), STEP_SKIP);
    let bare = ViewportIn {
        has_renderer: false,
        ..v
    };
    let p = plan_viewport(all, &bare);
    assert!(p & STEP_FALLBACK_TARGET != 0 && p & STEP_POST_EFFECTS == 0 && p & STEP_POST_FLAG != 0);
    assert_eq!(plan_viewport(FLAG_GUI, &v), STEP_GUI_SCREEN);
    assert!(!wants_post_effects(FLAG_POST_EFFECTS, true));
}

#[test]
fn gui_order_is_stable() {
    let mut out = [0u32; 5];
    let n = gui_order(&[3, 1, 3, 0, 1], &mut out);
    assert_eq!(n, 5);
    assert_eq!(out, [3, 1, 4, 0, 2]);
}
