use super::*;

fn ev(kind: u32) -> Event {
    Event {
        kind,
        sym: 0,
        key_mod: 0,
        state: 0,
        button: 0,
        axis_value: 0,
        x: 0,
        y: 0,
        xrel: 0,
        yrel: 0,
        wheel_y: 0,
        which: 0,
        text: [0; 32],
    }
}

fn key(sym: i32, state: u8) -> Event {
    Event {
        sym,
        state,
        ..ev(KIND_KEY)
    }
}

fn axis(which: i32, axis: u8, v: i16) -> Event {
    Event {
        which,
        button: axis,
        axis_value: v,
        ..ev(KIND_PAD_AXIS)
    }
}

fn button(which: i32, b: u8, state: u8) -> Event {
    Event {
        which,
        button: b,
        state,
        ..ev(KIND_PAD_BUTTON)
    }
}

const KEY_A: i32 = 44;
const KEY_SPACE: i32 = 6;

#[test]
fn key_tables() {
    assert_eq!(key_name(0), "BackSpace");
    assert_eq!(key_name(KEY_SPACE), "Space");
    assert_eq!(key_name(133), "None");
    assert_eq!(key_name(-1), "Unknown");
    assert_eq!(key_name(i32::from(KEY_LAST)), "Unknown");
    assert_eq!(key_from_name(b"Space"), KEY_SPACE);
    assert_eq!(key_from_name(b"space"), i32::from(KEY_LAST));
    assert_eq!(key_from_name(b""), i32::from(KEY_LAST));
    assert_eq!(key_name(KEY_A), "A");
    assert_eq!(i32::from(sdl_to_key(i32::from(b'a'))), KEY_A);
    assert_eq!(sdl_to_key(0x7fff_ffff), KEY_NONE);
}

#[test]
fn backquote_maps_to_backslash() {
    let backslash = key_from_name(b"BackSlash");
    assert_eq!(i32::from(sdl_to_key(i32::from(b'`'))), backslash);
    assert_eq!(i32::from(sdl_to_key(i32::from(b'\\'))), backslash);
    assert_ne!(backslash, key_from_name(b"BackQuote"));
}

#[test]
fn device_name_tables() {
    assert_eq!(modifier_name(0), "");
    assert_eq!(modifier_name(4), "ALT");
    assert_eq!(modifier_name(3), "Unknown");
    assert_eq!(mouse_button_name(3), "WheelUp");
    assert_eq!(mouse_button_name(9), "Unknown");
    assert_eq!(mouse_button_from_name(b"Mouse9"), 8);
    assert_eq!(mouse_button_from_name(b"x"), 9);
    assert_eq!(pad_button_name(14), "Button DpadRight");
    assert_eq!(pad_button_name(15), "Unknown");
    assert_eq!(pad_button_from_name(b"Button Guide"), 5);
    assert_eq!(pad_button_from_name(b"Button 0"), 30);
    assert_eq!(pad_axis_name(5), "Axis RightTrigger");
    assert_eq!(pad_axis_name(6), "None");
    assert_eq!(pad_axis_from_name(b"nope"), 10);
    assert_eq!(pad_axis_range_name(0), "None");
    assert_eq!(pad_axis_range_from_name(b"Negative"), 2);
    assert_eq!(pad_axis_range_from_name(b"Dead"), 3);
    assert_eq!(pad_axis_range_from_value(-0.0), 0);
    assert_eq!(pad_axis_range_from_value(-0.5), 2);
    assert_eq!(pad_hat_name(9), "Hat 9");
    assert_eq!(pad_hat_from_name(b"Hat 10"), 10);
    assert_eq!(pad_hat_state_name(12), "LeftDown");
    assert_eq!(pad_hat_state_name(5), "None");
    assert_eq!(pad_hat_state_from_name(b"LeftUp"), 9);
    assert_eq!(pad_hat_state_from_name(b"x"), 0x10);
    assert_eq!(pad_ball_name(0), "Trackball 0");
    assert_eq!(pad_ball_from_name(b"x"), 1);
}

#[test]
fn same_frame_release_defers() {
    let mut kb = Keyboard::new();
    kb.update(&[key(97, 1), key(97, 0)], 0);
    assert!(kb.key_is_down(KEY_A));
    assert_eq!(kb.pop_pressed().map(|p| p.key), Some(KEY_A));
    assert_eq!(kb.pop_released().map(|p| p.key), Some(KEY_A));
    kb.update(&[], 0);
    assert!(!kb.key_is_down(KEY_A));
    assert!(!kb.key_is_pressed() && !kb.key_is_released());
}

#[test]
fn release_then_press_in_frame_stays_down() {
    let mut kb = Keyboard::new();
    kb.update(&[key(97, 1)], 0);
    kb.update(&[key(97, 0), key(97, 1)], 0);
    assert!(kb.key_is_down(KEY_A));
    assert!(kb.key_is_pressed() && kb.key_is_released());
}

#[test]
fn unknown_key_is_none_and_modifiers_map() {
    let mut kb = Keyboard::new();
    let e = Event {
        key_mod: 0x0041 | 0x0300,
        ..key(0x7000_0000, 1)
    };
    kb.update(&[e], 0);
    assert!(kb.key_is_down(i32::from(KEY_NONE)));
    assert_eq!(
        kb.pop_pressed(),
        Some(KeyPress {
            key: i32::from(KEY_NONE),
            unicode: 0,
            modifier: 7
        })
    );
    assert_eq!(kb.pop_pressed(), None);
    assert!(!kb.key_is_down(-1) && !kb.key_is_down(i32::from(KEY_LAST)));
}

#[test]
fn text_input_uses_mod_state() {
    let mut text = [0u8; 32];
    text[..5].copy_from_slice(b"a\xe2\x82\xac\0");
    let mut kb = Keyboard::new();
    kb.update(
        &[Event {
            text,
            ..ev(KIND_TEXT)
        }],
        0x0002,
    );
    let none = i32::from(KEY_NONE);
    let press = |unicode| KeyPress {
        key: none,
        unicode,
        modifier: 2,
    };
    assert_eq!(kb.pop_pressed(), Some(press(97)));
    assert_eq!(kb.pop_pressed(), Some(press(0x20ac)));
    assert_eq!(kb.pop_pressed(), None);
    assert!(!kb.key_is_down(none));
}

#[test]
fn mouse_buttons_and_wheel() {
    let mut m = Mouse::new();
    let down = |b: u8| Event {
        button: b,
        state: 1,
        ..ev(KIND_MOUSE_BUTTON)
    };
    let wheel = |y: i32| Event {
        wheel_y: y,
        ..ev(KIND_WHEEL)
    };
    let none = Vector2l::default();
    m.update(&[down(1), down(4), down(5), down(9)], false, none);
    assert!([0, 5, 6].iter().all(|&b| m.button_is_down(b)));
    assert!(!m.button_is_down(1) && !m.button_is_down(7) && !m.button_is_down(9));
    m.update(&[wheel(1), down(3)], false, none);
    assert!(m.button_is_down(3) && !m.button_is_down(4) && !m.button_is_down(2));
    m.update(&[wheel(0)], false, none);
    assert!(!m.button_is_down(3) && m.button_is_down(4));
    m.update(&[], false, none);
    assert!(!m.button_is_down(4) && !m.button_is_down(-1));
}

#[test]
fn mouse_motion_and_relative() {
    let mut m = Mouse::new();
    let motion = Event {
        x: 10,
        y: -3,
        xrel: 4,
        yrel: 5,
        ..ev(KIND_MOUSE_MOTION)
    };
    let rel = Vector2l { x: 1, y: 2 };
    m.update(&[motion, motion], false, rel);
    assert_eq!(m.abs_position(), Vector2l { x: 10, y: -3 });
    assert_eq!(m.take_rel_position(), rel);
    assert_eq!(m.take_rel_position(), Vector2l::default());
    m.update(&[motion, motion], true, rel);
    assert_eq!(m.take_rel_position(), Vector2l { x: 9, y: 12 });
    assert!(m.take_first_time() && !m.take_first_time());
}

fn pad() -> Gamepad {
    Gamepad::new(0, 100, 21, 6, 27)
}

#[test]
fn pad_axis_scaling_and_dead_zone() {
    let mut g = pad();
    g.update(
        &[
            axis(100, 0, 32767),
            axis(100, 1, 0),
            axis(100, 4, 5000),
            axis(100, 5, 20000),
        ],
        5,
    );
    assert_eq!(g.axis_value(0), 32767.0 * INV_AXIS_MAX);
    assert_eq!(g.axis_value(4), 0.0);
    assert!((g.axis_value(5) - 20000.0 / 32767.5).abs() < 1e-6);
    g.update(&[axis(100, 0, 7000), axis(100, 1, 7000)], 6);
    assert_eq!((g.axis_value(0), g.axis_value(1)), (0.0, 0.0));
    g.update(&[axis(100, 0, -32768)], 7);
    assert_eq!(g.axis_value(0), -1.0);
    assert_eq!(g.axis_value(6), 0.0);
    assert_eq!(g.axis_value(-1), 0.0);
}

#[test]
fn pad_fused_dead_zone_boundary() {
    // Inside the dead zone only when v0*v0 + v1*v1 is evaluated as a fused multiply-add.
    let mut g = pad();
    g.update(&[axis(100, 0, 4865), axis(100, 1, 9767)], 0);
    assert_eq!((g.axis_value(0), g.axis_value(1)), (0.0, 0.0));
}

#[test]
fn pad_events_for_other_instances_are_ignored() {
    let mut g = pad();
    g.update(&[axis(101, 2, 32767), button(7, 0, 1)], 0);
    assert!(!g.has_input_updates() && !g.button_is_down(0));
}

#[test]
fn pad_button_edges_and_queues() {
    let mut g = pad();
    g.update(
        &[
            button(100, 0, 1),
            button(100, 0, 1),
            button(100, 3, 1),
            button(100, 3, 0),
        ],
        10,
    );
    assert!(g.button_is_pressed() && g.button_is_released() && !g.button_is_down(3));
    let d = |id, value| InputData {
        pad_index: 0,
        input_type: INPUT_BUTTON,
        input_id: id,
        value,
    };
    assert_eq!(g.pop_input_update(), Some(d(0, 1.0)));
    assert_eq!(g.pop_button(), Some(d(3, 1.0)));
    assert!(!g.button_is_pressed());
    assert_eq!(g.pop_released_button(), Some(d(3, 0.0)));
    assert_eq!(g.pop_input_update(), None);
    assert_eq!(g.pop_button(), None);
    assert!(g.button_is_down(0) && !g.button_is_down(21) && !g.button_is_down(-1));
}

#[test]
fn pad_axis_change_queue_outlives_update() {
    let mut g = pad();
    g.update(&[axis(100, 2, 30000)], 0);
    g.update(&[], 1);
    assert!(g.axes_updated() && !g.has_input_updates());
    let a = g.pop_updated_axis();
    assert_eq!(a.map(|a| (a.input_type, a.input_id)), Some((INPUT_AXIS, 2)));
    assert!(!g.axes_updated());
}

#[test]
fn pad_removed_and_flush_reset_state() {
    let mut g = pad();
    g.update(&[button(100, 1, 1), axis(100, 2, 30000)], 0);
    g.update(&[ev(KIND_PAD_REMOVED)], 0);
    assert!(g.button_is_down(1), "instance 0 is not this pad");
    g.update(
        &[Event {
            which: 100,
            ..ev(KIND_PAD_REMOVED)
        }],
        0,
    );
    assert!(!g.button_is_down(1) && g.axis_value(2) == 0.0);
    g.update(&[button(100, 1, 1)], 0);
    assert!(g.button_is_down(1));
    let flush: Vec<Event> = (0..27).map(|i| button(100, i % 21, 0)).collect();
    g.update(&flush, 0);
    assert!(!g.button_is_down(1) && !g.button_is_released() && !g.has_input_updates());
    g.update(&[button(100, 1, 1)], 0);
    g.update(&flush[..26], 0);
    assert!(!g.button_is_down(1) && g.button_is_released());
}

#[test]
fn pad_without_controller_has_no_state() {
    let mut g = Gamepad::new(0, 0, 0, 0, 27);
    g.update(&[axis(0, 0, 100), button(0, 0, 1)], 0);
    assert_eq!((g.num_buttons(), g.num_axes()), (0, 0));
    assert!(!g.has_input_updates() && !g.button_is_down(0));
}

#[test]
fn pad_last_active_tracking() {
    let mut g = pad();
    assert_eq!(g.time_since_last_active(1000), f32::MAX);
    g.update(&[axis(100, 4, 32767)], 2000);
    assert_eq!(g.time_since_last_active(3500), 1.5);
    g.update(&[axis(100, 4, 4000)], 4000);
    assert_eq!(g.time_since_last_active(4000), 2.0);
    g.update(&[button(100, 2, 1)], 0x1_0000_0005);
    assert_eq!(
        g.time_since_last_active(0x1_0000_0005),
        4_294_967.5,
        "last_active keeps 32 bits"
    );
    g.clear_state();
    assert!(!g.button_is_down(2));
}

#[test]
fn action_edges() {
    let mut a = ActionState::default();
    assert!(!a.is_down() && !a.became_triggered() && !a.was_triggered());
    a.update(true, 0.1);
    assert!(a.is_down() && a.became_triggered() && !a.became_triggered());
    a.update(false, 0.1);
    assert!(a.was_triggered() && !a.was_triggered());
    a.update(true, 0.1);
    assert!(a.became_triggered());
}

#[test]
fn action_double_trigger() {
    let mut a = ActionState::default();
    a.update(true, 0.0);
    assert!(!a.double_triggered(0.5), "first press only arms the timer");
    a.update(false, 0.2);
    a.update(true, 0.0);
    assert!(a.double_triggered(0.5));
    a.update(false, 0.1);
    a.update(true, 0.0);
    assert!(!a.double_triggered(0.5), "a double trigger cannot chain");
    a.update(false, 0.6);
    a.update(true, 0.0);
    assert!(!a.double_triggered(0.5), "too slow");
}

#[test]
fn action_reset_to_current_state() {
    let mut a = ActionState::default();
    a.update(true, 0.0);
    a.reset_to_current_state(true);
    assert!(!a.became_triggered() && !a.double_triggered(1.0));
    a.update(false, 0.0);
    assert!(a.was_triggered());
    a.reset_to_current_state(false);
    assert!(!a.was_triggered());
}
