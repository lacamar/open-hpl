use super::widget::*;
use super::widgets_base::*;
use crate::math::v3;
use crate::types::{Color, Vector2f, Vector3f};

fn w(s: &str) -> Vec<u32> {
    s.chars().map(u32::from).collect()
}

fn metrics() -> [GfxMetrics; GFX_SLOTS] {
    let mut m = [GfxMetrics::default(); GFX_SLOTS];
    for (i, g) in m.iter_mut().enumerate() {
        let a = (i % 4 + 2) as f32;
        g.active = v2(a, a + 1.0);
        g.image = v2(a + 1.0, a + 2.0);
    }
    m
}

fn run(f: impl FnOnce(&mut Ops)) -> Vec<Op> {
    let mut buf = [Op::default(); 32];
    let mut o = Ops::new(&mut buf);
    f(&mut o);
    let n = o.len();
    buf.iter().take(n).copied().collect()
}

#[test]
fn global_position_chain() {
    let p = ParentPos {
        global: v3(10.0, 20.0, 1.0),
        children_offset: v3(1.0, 2.0, 0.0),
        scroll: v3(5.0, 6.0, 0.0),
    };
    let local = v3(100.0, 200.0, 0.5);
    assert_eq!(global_position(&p, true, local), v3(106.0, 216.0, 1.5));
    assert_eq!(global_position(&p, false, local), v3(111.0, 222.0, 1.5));
    assert_eq!(
        global_to_local(v3(111.0, 222.0, 1.5), Some(&p)),
        v3(100.0, 200.0, 0.5)
    );
    assert_eq!(global_to_local(local, None), local);
}

#[test]
fn center_clamps_at_zero() {
    let c = center_in_set(v2(800.0, 600.0), v2(200.0, 100.0), 3.0);
    assert_eq!(c, v3(300.0, 250.0, 3.0));
    let c = center_in_set(v2(100.0, 100.0), v2(500.0, 500.0), 0.0);
    assert_eq!(c, v3(0.0, 0.0, 0.0));
}

#[test]
fn point_tests() {
    assert_eq!(point_stage(false, false, true), PointStage::Outside);
    assert_eq!(point_stage(true, true, false), PointStage::Inside);
    assert_eq!(point_stage(true, true, true), PointStage::CheckBox);
    assert_eq!(point_stage(true, false, false), PointStage::CheckBox);
    let g = v3(10.0, 10.0, 0.0);
    let s = v2(5.0, 5.0);
    assert!(point_in_box(v2(15.0, 10.0), g, s));
    assert!(!point_in_box(v2(15.1, 10.0), g, s));
    assert!(!point_in_box(v2(9.9, 12.0), g, s));
}

#[test]
fn arrow_slots() {
    let slots: Vec<i32> = [1, 2, 4, 8, 3, 6, 9, 10, 12, 0, -4]
        .iter()
        .map(|&a| ui_arrow_slot(a))
        .collect();
    assert_eq!(slots, [0, 1, 2, 3, 2, 3, 3, 3, -1, -1, 0]);
}

#[test]
fn borders_inside_without_background() {
    let m = metrics();
    let ops = run(|o| borders_and_corners(o, false, false, v3(1.0, 2.0, 0.0), v2(100.0, 50.0), &m));
    assert_eq!(ops.len(), 10);
    assert_eq!(
        ops.first().map(|o| (o.kind, o.pos)),
        Some((OP_OFFSET, v3(1.0, 2.0, 0.0)))
    );
    assert_eq!(
        ops.last().map(|o| (o.kind, o.pos)),
        Some((OP_OFFSET, v3(0.0, 0.0, 0.0)))
    );
    // right border: x = width - active.x, y = corner RU active.y; size = (image.x, h - (RD.y + RU.y))
    let right = ops.get(1).copied().unwrap_or_default();
    let (b, ru, rd) = (m[1], m[6], m[7]);
    assert_eq!(right.gfx, 1);
    assert_eq!(right.pos, v3(100.0 - b.active.x, ru.active.y, 0.0));
    assert_eq!(
        right.size,
        v2(b.image.x, 50.0 - (rd.active.y + ru.active.y))
    );
    assert_eq!(right.flags, HAS_SIZE);
}

#[test]
fn borders_outside_with_background() {
    let m = metrics();
    let ops = run(|o| borders_and_corners(o, true, true, v3(0.0, 0.0, 0.0), v2(40.0, 30.0), &m));
    assert_eq!(ops.len(), 11);
    let bg = ops.get(1).copied().unwrap_or_default();
    assert_eq!(
        (bg.gfx, bg.pos, bg.size),
        (0, v3(0.0, 0.0, 0.0), v2(40.0, 30.0))
    );
    assert_eq!(bg.flags, HAS_SIZE | HAS_COLOR);
    assert_eq!(bg.color, WHITE);
    let lu = ops.get(6).copied().unwrap_or_default();
    assert_eq!(lu.pos, v3(-m[5].active.x, -m[5].active.y, 0.0));
    assert_eq!(lu.flags, 0);
}

#[test]
fn ops_overflow_is_counted_not_written() {
    let mut buf = [Op::default(); 3];
    let mut o = Ops::new(&mut buf);
    borders_and_corners(
        &mut o,
        false,
        false,
        v3(0.0, 0.0, 0.0),
        v2(1.0, 1.0),
        &metrics(),
    );
    assert_eq!(o.len(), 3);
}

#[test]
fn button_press_release_cycle() {
    let idle = ButtonState {
        repeat_period: 1.0,
        ..ButtonState::default()
    };
    let down = button_mouse_down(idle, true, MOUSE_LEFT);
    assert!(down.ret && down.state.pressed && !down.fire && down.store_data);
    assert!(!button_mouse_down(idle, false, MOUSE_LEFT).ret);
    assert!(!button_mouse_down(idle, true, MOUSE_RIGHT).ret);
    // releasing over the button fires, off it does nothing and returns false
    let up = button_mouse_up(down.state, true, MOUSE_LEFT, true);
    assert!(up.ret && up.fire && !up.state.pressed);
    let off = button_mouse_up(down.state, true, MOUSE_LEFT, false);
    assert!(!off.ret && !off.fire && off.state.pressed);
    // a repeat already fired suppresses the release callback
    let repeated = ButtonState {
        repeat_times: 2,
        ..down.state
    };
    let up = button_mouse_up(repeated, true, MOUSE_LEFT, true);
    assert!(up.ret && !up.fire && up.state.repeat_times == 0);
}

#[test]
fn button_toggle_fires_on_down() {
    let s = ButtonState {
        toggleable: true,
        ..ButtonState::default()
    };
    let a = button_mouse_down(s, true, MOUSE_LEFT);
    assert!(a.state.pressed && a.fire);
    let b = button_mouse_down(a.state, true, MOUSE_LEFT);
    assert!(!b.state.pressed && b.fire);
    let up = button_mouse_up(a.state, true, MOUSE_LEFT, true);
    assert!(up.state.pressed && !up.fire && up.ret);
    assert!(button_release(true, true));
    assert!(!button_release(false, true));
}

#[test]
fn button_repeat_timer() {
    let s = ButtonState {
        pressed: true,
        repeat_active: true,
        repeat_period: 0.5,
        ..ButtonState::default()
    };
    let a = button_update(s, 0.25);
    assert!(!a.fire && a.state.timer == 0.25);
    let b = button_update(a.state, 0.25);
    assert!(!b.fire, "equal to the period does not fire");
    let c = button_update(b.state, 0.01);
    assert!(c.fire && c.state.timer == 0.0 && c.state.repeat_times == 1);
    assert!(!button_update(ButtonState::default(), 9.0).fire);
    assert_eq!(button_repeat_period(4.0), 0.25);
}

#[test]
fn button_draw_order_and_pressed_offset() {
    let m = metrics();
    let d = ButtonDraw {
        global: v3(10.0, 10.0, 0.0),
        size: v2(40.0, 20.0),
        pressed: true,
        enabled: false,
        has_image: true,
        has_default_font: true,
        pressed_offset: v3(1.0, 2.0, 0.0),
        font_size: v2(8.0, 10.0),
        font_color: Color {
            r: 1.0,
            g: 0.5,
            b: 0.25,
            a: 1.0,
        },
        color_mul: Color {
            r: 0.5,
            g: 0.5,
            b: 0.5,
            a: 0.5,
        },
        has_background: false,
    };
    let ops = run(|o| button_draw(o, &d, &m));
    assert_eq!(ops.len(), 2 + 10);
    let (img, text) = (
        ops.first().copied().unwrap_or_default(),
        ops.get(1).copied().unwrap_or_default(),
    );
    assert_eq!(
        (img.kind, img.gfx, img.size),
        (OP_GFX, GFX_IMAGE, v2(-1.0, -1.0))
    );
    let isz = m[GFX_IMAGE as usize].image;
    assert_eq!(
        img.pos,
        v3(
            10.0 + (20.0 - isz.x / 2.0) + 1.0,
            10.0 + (10.0 - isz.y / 2.0) + 2.0,
            0.2
        )
    );
    assert_eq!(
        img.color,
        Color {
            r: 1.0,
            g: 1.0,
            b: 1.0,
            a: 0.2
        }
    );
    assert_eq!((text.kind, text.align), (OP_FONT, 2));
    assert_eq!(
        text.pos,
        v3(10.0 + 20.0 + 1.0, 10.0 + (10.0 - 5.0) + 2.0, 0.5)
    );
    assert_eq!(
        text.color,
        Color {
            r: 0.25,
            g: 0.25,
            b: 0.25,
            a: 0.5
        }
    );
}

#[test]
fn check_box_layout() {
    let l = check_layout(12.0, 10.0, 15.0, v2(30.0, 11.0));
    assert_eq!(l.box_size, v2(12.0, 10.0));
    assert_eq!(l.size, v2(12.0 + 3.0 + 15.0 + 30.0, 11.0));
    assert_eq!(check_label_pos(12.0, 10.0, 8.0), v3(15.0, 1.0, 0.0));
    assert_eq!(
        check_box_pos(v3(1.0, 2.0, 3.0), 10.0, 6.0),
        v3(1.0, 4.0, 3.0)
    );
    assert!(check_mouse_down(MOUSE_LEFT | 8) && !check_mouse_down(MOUSE_RIGHT));
    assert_eq!(check_label_forward(MOUSE_RIGHT, true), (true, true));
    assert_eq!(check_label_forward(MOUSE_RIGHT, false), (true, false));
    assert_eq!(check_label_forward(8, true), (false, false));
}

fn label(word_wrap: bool, max_chars: i32) -> LabelDraw {
    LabelDraw {
        global: v3(100.0, 50.0, 0.0),
        size: v2(80.0, 40.0),
        align: 2,
        word_wrap,
        max_chars,
        enabled: true,
        has_default_font: true,
        font_size: v2(8.0, 10.0),
        font_color: Color {
            r: 1.0,
            g: 1.0,
            b: 1.0,
            a: 0.75,
        },
        color_mul: Color {
            r: 1.0,
            g: 1.0,
            b: 1.0,
            a: 1.0,
        },
        wrap_offset: 4.0,
        text_len: 20,
    }
}

#[test]
fn label_wrapped_rows_truncate_at_max_chars() {
    let d = label(true, 7);
    let mut height = None;
    let ops = run(|o| height = label_draw_text(o, &d, &[5, 5, 5]));
    // rows of 5: the second is cut to 2 chars and ends the loop
    assert_eq!(ops.len(), 2);
    assert_eq!(height, Some(11.0 * 3.0));
    let second = ops.get(1).copied().unwrap_or_default();
    assert_eq!((second.text, second.text_len), (2, 2));
    assert_eq!(second.pos, v3(140.0, 50.0 + 12.0 - 4.0, 0.0));
}

#[test]
fn label_disabled_unwrapped_text() {
    let mut d = label(false, 5);
    d.enabled = false;
    let mut height = Some(1.0);
    let ops = run(|o| height = label_draw_text(o, &d, &[]));
    assert_eq!(height, None);
    let t = ops.first().copied().unwrap_or_default();
    assert_eq!((t.text, t.text_len, t.align), (0, 5, 2));
    assert_eq!(
        t.color,
        Color {
            r: 0.5,
            g: 0.5,
            b: 0.5,
            a: 0.75
        }
    );
    assert_eq!(t.pos, v3(140.0, 50.0, 0.0));
    d.has_default_font = false;
    assert!(
        run(|o| {
            label_draw_text(o, &d, &[]);
        })
        .is_empty()
    );
}

#[test]
fn label_scroll_bounces() {
    let s = LabelScroll {
        timer: 0.5,
        offset: 0.0,
        down: true,
    };
    assert_eq!(label_update(s, 0.25, 10.0, 40.0, 100.0).timer, 0.25);
    assert_eq!(
        label_update(s, 0.25, 10.0, 40.0, 100.0).offset,
        0.0,
        "still waiting"
    );
    let moving = LabelScroll { timer: 0.0, ..s };
    let a = label_update(moving, 1.0, 10.0, 40.0, 100.0);
    assert_eq!((a.offset, a.down), (10.0, true));
    let end = LabelScroll {
        offset: 55.0,
        ..moving
    };
    assert!(!label_update(end, 1.0, 10.0, 40.0, 100.0).down);
    let up = LabelScroll {
        offset: 5.0,
        down: false,
        timer: 0.0,
    };
    assert!(label_update(up, 1.0, 10.0, 40.0, 100.0).down);
    assert_eq!(
        label_update(
            LabelScroll {
                offset: 20.0,
                down: false,
                timer: 0.0
            },
            1.0,
            10.0,
            40.0,
            100.0
        )
        .offset,
        10.0
    );
    assert_eq!(
        label_background(v3(10.0, 10.0, 1.0), v2(5.0, 6.0)),
        (v3(8.0, 8.0, 1.0 - 0.001), v2(9.0, 10.0))
    );
}

#[test]
fn image_ignores_wheel() {
    assert!(!image_mouse(MOUSE_WHEEL_UP) && !image_mouse(MOUSE_WHEEL_DOWN));
    assert!(image_mouse(MOUSE_LEFT) && image_mouse(MOUSE_WHEEL_UP | 1));
}

#[test]
fn dummy_encloses_children() {
    assert_eq!(dummy_enclose(&[]), v2(-9999.9 - 9999.9, -9999.9 - 9999.9));
    let c = [
        DummyChild {
            pos: v3(10.0, 20.0, 0.0),
            size: v2(5.0, 5.0),
        },
        DummyChild {
            pos: v3(2.0, 40.0, 0.0),
            size: v2(1.0, 10.0),
        },
    ];
    assert_eq!(dummy_enclose(&c), v2(15.0 - 2.0, 50.0 - 20.0));
}

fn window_look() -> WindowLook {
    WindowLook {
        size: v2(200.0, 100.0),
        corner_lu: v2(4.0, 3.0),
        corner_ru: v2(5.0, 3.0),
        label_font_height: 12.0,
        label_offset: v3(2.0, 1.0, 0.0),
    }
}

#[test]
fn window_drag_hit_and_math() {
    let l = window_look();
    let g = v3(100.0, 100.0, 2.0);
    // label rect: x 104.., y 103.., w 191, h 14
    assert!(window_mouse_down(&l, g, v2(104.0, 103.0), MOUSE_LEFT));
    assert!(window_mouse_down(&l, g, v2(295.0, 117.0), MOUSE_LEFT));
    assert!(!window_mouse_down(&l, g, v2(295.1, 117.0), MOUSE_LEFT));
    assert!(!window_mouse_down(&l, g, v2(150.0, 110.0), MOUSE_RIGHT));
    assert_eq!(
        window_drag_pos(v3(-5.0, -7.0, 2.0), v2(50.0, 60.0)),
        v3(45.0, 53.0, 2.0)
    );
    assert_eq!(window_menu_pos(&l), v3(0.0, 17.0, 0.1));
    assert_eq!(window_button_pos(200.0, 16.0), v3(179.0, 5.0, 0.2));
}

#[test]
fn window_keys_and_focus() {
    assert!(window_key_closes(true, true, KEY_ESCAPE, 0));
    assert!(!window_key_closes(false, true, KEY_ESCAPE, 0));
    assert!(!window_key_closes(true, false, KEY_ESCAPE, 0));
    assert!(!window_key_closes(true, true, KEY_ESCAPE, 1));
    assert!(!window_key_closes(true, true, 6, 0));
    assert!(window_focus_lost(true, true, true));
    assert!(window_focus_lost(false, false, false));
    assert!(!window_focus_lost(false, true, false));
    assert!(!window_focus_lost(false, false, true));
}

#[test]
fn window_draw_sequence() {
    let m = metrics();
    let d = WindowDraw {
        look: window_look(),
        global: v3(10.0, 20.0, 0.0),
        draw_label: true,
        focused: false,
        has_background: true,
        borders_outside: false,
        font_size: v2(7.0, 12.0),
        font_color: WHITE,
        color_mul: WHITE,
    };
    let ops = run(|o| window_draw(o, &d, &m));
    assert_eq!(ops.len(), 2 + 11);
    let label = ops.first().copied().unwrap_or_default();
    assert_eq!(
        (label.gfx, label.pos, label.size),
        (GFX_LABEL, v3(14.0, 23.0, 0.2), v2(191.0, 14.0))
    );
    assert_eq!(
        label.color,
        Color {
            r: 0.6,
            g: 0.6,
            b: 0.6,
            a: 0.3
        }
    );
    let text = ops.get(1).copied().unwrap_or_default();
    assert_eq!(
        (text.font, text.pos),
        (FONT_WINDOW_LABEL, v3(16.0, 24.0, 0.4))
    );
    let none = WindowDraw {
        draw_label: false,
        has_background: false,
        ..d
    };
    assert_eq!(run(|o| window_draw(o, &none, &m)).len(), 1 + 10);
}

#[test]
fn frame_layout_and_wheel() {
    let l = frame_slider_layout(v2(100.0, 80.0), true, true);
    assert_eq!(l.h_size, v2(82.0, 18.0));
    assert_eq!(l.v_size, v2(18.0, 62.0));
    assert_eq!(l.h_pos, v3(0.0, 62.0, 0.7));
    assert_eq!(l.v_pos, v3(82.0, 0.0, 0.7));
    let l = frame_slider_layout(v2(100.0, 80.0), false, false);
    assert_eq!((l.h_size.x, l.v_size.y), (100.0, 80.0));
    assert_eq!(frame_wheel(MOUSE_WHEEL_UP, 100, 10), Some(70));
    assert_eq!(frame_wheel(MOUSE_WHEEL_DOWN, 100, 10), Some(130));
    assert_eq!(
        frame_wheel(MOUSE_WHEEL_UP | MOUSE_WHEEL_DOWN, 100, 10),
        Some(70)
    );
    assert_eq!(frame_wheel(MOUSE_WHEEL_DOWN, 100, 0), None);
    assert_eq!(frame_wheel(1, 100, 10), None);
}

fn scroll_in() -> ScrollIn {
    ScrollIn {
        size: v2(100.0, 100.0),
        margin: v2(0.0, 0.0),
        offset: v3(0.0, 0.0, 0.0),
        max_coord: v3(-999_999.9, -999_999.9, -999_999.9),
        h_size: v2(100.0, 18.0),
        v_size: v2(18.0, 100.0),
        has_h: true,
        has_v: true,
    }
}

#[test]
fn frame_scroll_plan_vertical_overflow() {
    let kids = [
        ChildBox {
            pos: v3(0.0, 0.0, 0.0),
            size: v2(50.0, 50.0),
            visible: true,
        },
        ChildBox {
            pos: v3(0.0, 150.0, 0.0),
            size: v2(50.0, 100.0),
            visible: true,
        },
        ChildBox {
            pos: v3(900.0, 900.0, 0.0),
            size: v2(50.0, 100.0),
            visible: false,
        },
    ];
    let p = frame_scroll_plan(&scroll_in(), &kids);
    assert_eq!((p.max_coord.x, p.max_coord.y), (50.0, 250.0));
    assert!(p.v.active && !p.h.active);
    // end = 250 - 100 = 150; bar = 100 * 150 / 250 = 60
    assert_eq!(
        (p.v.max_value, p.v.bar_value_size, p.v.button_add),
        (151, 60, 10)
    );
    assert!(!p.v.set_value);
    // horizontal: nothing wider than the frame, so it resets to 0 and hides
    assert!(p.h.set_value && p.h.value == 0);
    assert_eq!(p.h.max_value, round_to_int(50.0 - 100.0 + 18.0) + 1);
}

#[test]
fn frame_scroll_plan_clamps_offset() {
    let kids = [ChildBox {
        pos: v3(0.0, 0.0, 0.0),
        size: v2(100.0, 300.0),
        visible: true,
    }];
    let mut i = scroll_in();
    i.offset = v3(0.0, 400.0, 0.0);
    let p = frame_scroll_plan(&i, &kids);
    // end = 200, offset 400 > limit 200: the slider is pulled back
    assert!(p.v.active && p.v.set_value && p.v.value == 200);
    // a frame with no sliders plans nothing
    i.has_h = false;
    i.has_v = false;
    let p = frame_scroll_plan(&i, &kids);
    assert_eq!((p.h, p.v), (SliderPlan::default(), SliderPlan::default()));
}

#[test]
fn frame_draw_passes() {
    let m = metrics();
    let d = FrameDraw {
        global: v3(50.0, 60.0, 0.0),
        size: v2(100.0, 80.0),
        draw_frame: true,
        draw_background: true,
        background_z: -0.5,
        background_color: WHITE,
    };
    let ops = run(|o| frame_draw(o, &d, &m));
    assert_eq!(ops.len(), 10);
    let off = ops.first().copied().unwrap_or_default();
    assert_eq!(off.pos, v3(50.0 - m[5].active.x, 60.0 - m[5].active.y, 0.0));
    let bg = run(|o| frame_draw_background(o, &d));
    assert_eq!(bg.len(), 1);
    assert_eq!(bg.first().map(|o| o.pos), Some(v3(50.0, 60.0, -0.5)));
    assert!(
        run(|o| frame_draw(
            o,
            &FrameDraw {
                draw_frame: false,
                ..d
            },
            &m
        ))
        .is_empty()
    );
}

#[test]
fn group_header_gap() {
    let m = metrics();
    let d = GroupDraw {
        global: v3(10.0, 10.0, 0.0),
        size: v2(100.0, 50.0),
        header_pos: v3(8.0, -6.0, 0.1),
        header_size: v2(30.0, 12.0),
        has_background: true,
        header_breaks_border: true,
    };
    let ops = run(|o| group_draw(o, &d, &m));
    // offset, background, right, left, two up pieces, down, four corners, offset
    assert_eq!(ops.len(), 12);
    let pre = ops.get(4).copied().unwrap_or_default();
    let post = ops.get(5).copied().unwrap_or_default();
    assert_eq!((pre.gfx, pre.size.x), (3, 8.0 - 4.0));
    assert_eq!(post.pos, v3(8.0 + 30.0 + 4.0, 0.0, 0.0));
    assert_eq!(post.size.x, (100.0 - 8.0 - 30.0) - m[6].active.x - 4.0);
    let plain = GroupDraw {
        header_breaks_border: false,
        has_background: false,
        ..d
    };
    assert_eq!(run(|o| group_draw(o, &plain, &m)).len(), 10);
    assert_eq!(group_header_pos(6.0, 10.0), v3(6.0, -5.0, 0.1));
}

#[test]
fn file_browser_helpers() {
    assert_eq!(file_type_of_extension(&w("tga")), Some(0));
    assert_eq!(file_type_of_extension(&w("ent")), Some(1));
    assert_eq!(file_type_of_extension(&w("wav")), Some(2));
    assert_eq!(file_type_of_extension(&w("log")), Some(3));
    assert_eq!(file_type_of_extension(&w("TGA")), None, "caller lowercases");
    assert_eq!(file_type_of_extension(&w("")), None);
    assert_eq!(file_type_of_extension(&w("tgaa")), None);
    let sizes = [
        (0, (0, 0)),
        (1024, (1024, 0)),
        (1025, (1, 1)),
        (1 << 20, (1024, 1)),
        (1 << 30, (1024, 2)),
        (1 << 40, (1024, 3)),
    ];
    for (n, want) in sizes {
        assert_eq!(human_readable_size(n), want, "size {n}");
    }
    assert_eq!(
        human_readable_size(u64::MAX).1,
        3,
        "past TB clamps to GB where C++ read out of bounds"
    );
}

#[test]
fn drives_and_paths() {
    let mut out = [0u32; 4];
    assert_eq!(drive_letters(0b1_0101, &mut out), 3);
    assert_eq!(out, [65, 67, 69, 0]);
    assert_eq!(drive_letters(0, &mut out), 0);
    assert_eq!(drive_letters(u64::MAX, &mut out), 4, "capped to the buffer");
    let targets = [
        ("<System Root>", NavTarget::SystemRoot),
        ("<Game Folder>", NavTarget::GameFolder),
        ("<Personal Folder>", NavTarget::PersonalFolder),
        ("<Disk Drive> C:", NavTarget::DiskDrive),
        ("<Disk Drive>", NavTarget::DiskDrive),
        ("<Disk Driv", NavTarget::Path),
        ("..", NavTarget::Up),
        (".", NavTarget::Here),
        ("", NavTarget::Path),
        ("a/b", NavTarget::Path),
    ];
    for (p, want) in targets {
        assert_eq!(navigate_target(&w(p)), want, "path {p:?}");
    }
    assert_eq!(path_piece(&w(""), 3), PathPiece::Skip);
    assert_eq!(path_piece(&w("."), 3), PathPiece::Skip);
    assert_eq!(path_piece(&w(".."), 2), PathPiece::Pop);
    assert_eq!(
        path_piece(&w(".."), 0),
        PathPiece::Skip,
        "pop_back on empty is UB in C++"
    );
    assert_eq!(path_piece(&w("usr"), 0), PathPiece::Push);
}

#[test]
fn ffi_rejects_null_and_small_buffers() {
    use super::ffi_widget::*;
    let mut v = Vector3f::default();
    // SAFETY: null inputs are documented no-ops; `v` is writable.
    unsafe { ohpl_wd_global_position(std::ptr::null(), true, std::ptr::null(), &mut v) };
    assert_eq!(v, Vector3f::default());
    // SAFETY: null ops buffer is documented to return 0.
    let n = unsafe {
        ohpl_wd_borders(
            std::ptr::null_mut(),
            8,
            true,
            false,
            std::ptr::null(),
            std::ptr::null(),
            std::ptr::null(),
        )
    };
    assert_eq!(n, 0);
    let _ = Vector2f::default();
    assert_eq!(ohpl_wd_ui_arrow_slot(8), 3);
}
