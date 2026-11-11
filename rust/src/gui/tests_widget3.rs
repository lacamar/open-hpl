use super::popup::*;
use super::widget::v2;
use super::widgets_menu::*;
use crate::math::v3;
use crate::types::Color;

fn w(s: &str) -> Vec<u32> {
    s.chars().map(u32::from).collect()
}

fn item_in() -> ItemDrawIn {
    ItemDrawIn {
        kind: KIND_CONTEXT,
        separator: false,
        highlighted: false,
        enabled: true,
        widget_enabled: true,
        is_parent: false,
        checked: false,
        global: v3(10.0, 20.0, 1.0),
        size: v2(100.0, 16.0),
        separator_padding: 4.0,
        separator_height: 2.0,
        arrow_offset: v3(90.0, 1.0, 0.2),
        check_offset: v3(2.0, 1.0, 0.2),
        text_offset: v3(20.0, 3.0, 0.2),
    }
}

fn draw(i: &ItemDrawIn) -> Vec<MenuOp> {
    let mut out = [MenuOp::default(); MAX_ITEM_OPS];
    let n = item_draw(i, &mut out);
    out[..n].to_vec()
}

#[test]
fn separator_draws_only_the_line() {
    let mut i = item_in();
    i.separator = true;
    let ops = draw(&i);
    assert_eq!(ops.len(), 1);
    assert_eq!(ops[0].slot, GFX_LINE);
    assert_eq!(ops[0].pos, v3(14.0, 27.0, 1.2));
    assert_eq!(ops[0].size, v2(92.0, 2.0));
}

#[test]
fn context_item_draw_order_and_colour() {
    let mut i = item_in();
    i.checked = true;
    i.highlighted = true;
    let ops = draw(&i);
    let kinds: Vec<_> = ops.iter().map(|o| (o.kind, o.slot)).collect();
    assert_eq!(
        kinds,
        [
            (OP_TEXT, TEXT_SHORTCUT),
            (OP_GFX, GFX_CHECKED_ENABLED),
            (OP_GFX, GFX_SELECTION),
            (OP_TEXT, TEXT_LABEL)
        ]
    );
    assert_eq!(ops[0].align, ALIGN_RIGHT);
    assert_eq!(ops[0].pos, v3(100.0, 23.0, 1.2));
    assert_eq!(ops[3].color, COLOR_HIGHLIGHT);
    i.enabled = false;
    i.widget_enabled = false;
    i.is_parent = true;
    let ops = draw(&i);
    assert_eq!(ops[0].slot, GFX_ARROW);
    assert_eq!(ops[1].slot, GFX_CHECKED_DISABLED);
    assert_eq!(ops[3].color, COLOR_DISABLED);
}

#[test]
fn main_menu_item_draws_label_only() {
    let mut i = item_in();
    i.kind = KIND_MAIN;
    i.checked = true;
    assert_eq!(draw(&i).len(), 1);
    i.kind = KIND_OTHER;
    i.highlighted = true;
    assert_eq!(draw(&i).len(), 2);
}

#[test]
fn submenu_placement() {
    let g = v3(100.0, 50.0, 1.0);
    let ip = v3(7.0, 12.0, 0.1);
    let right = item_submenu_pos(KIND_CONTEXT, g, ip, 16.0, 80.0, 60.0, 400.0);
    assert_eq!(right, v3(180.0, 62.0, 1.1));
    let left = item_submenu_pos(KIND_CONTEXT, g, ip, 16.0, 80.0, 60.0, 200.0);
    assert_eq!(left, v3(40.0, 62.0, 1.1));
    let below = item_submenu_pos(KIND_MAIN, g, ip, 16.0, 80.0, 60.0, 400.0);
    assert_eq!(below, v3(107.0, 78.0, 1.1));
    assert_eq!(
        item_submenu_pos(KIND_OTHER, g, ip, 16.0, 80.0, 60.0, 400.0),
        v3(100.0, 62.0, 1.1)
    );
}

#[test]
fn item_timer_opens_submenu() {
    let u = item_update(0.4, 0.2, 0.5, true, KIND_CONTEXT, true, true, false);
    assert!(u.show_submenu && u.timer > 0.5);
    assert!(!item_update(0.4, 0.2, 0.5, true, KIND_CONTEXT, true, true, true).show_submenu);
    assert!(!item_update(0.4, 0.2, 0.5, true, KIND_CONTEXT, false, true, false).show_submenu);
    assert_eq!(
        item_update(0.1, 0.2, 0.5, true, KIND_MAIN, true, true, false).timer,
        0.1
    );
}

#[test]
fn item_mouse_decisions() {
    assert!(!item_mouse_up(true, true, false).ret);
    assert!(!item_mouse_up(false, false, false).ret);
    let leaf = item_mouse_up(false, true, false);
    assert!(leaf.ret && leaf.hide_topmost && leaf.press);
    assert!(!item_mouse_up(false, true, true).hide_topmost);
    assert!(item_mouse_move_highlights(true, true, false, true));
    assert!(!item_mouse_move_highlights(true, false, false, true));
    assert!(!item_mouse_leave(true, false).ret);
    let l = item_mouse_leave(false, true);
    assert!(l.ret && !l.clear_highlight && l.reset_timer);
}

#[test]
fn shortcut_text_joins_with_commas() {
    assert_eq!(join_shortcut(&[], &w("Ctrl+S")), w("Ctrl+S"));
    assert_eq!(join_shortcut(&w("A"), &w("B")), w("A,B"));
}

#[test]
fn context_layout_grows_with_items() {
    assert_eq!(ctx_item_width(120.0, 3.0, 4.0), 113.0);
    assert_eq!(ctx_item_pos(20.0, 3.0, 2.0, 1), v3(2.0, 23.0, 0.1));
    assert_eq!(ctx_item_pos(40.0, 3.0, 2.0, 2), v3(2.0, 37.0, 0.1));
    assert_eq!(ctx_separation(false, false, 2.0), 2.0);
    assert_eq!(ctx_separation(true, false, 2.0), 0.0);
    assert_eq!(ctx_empty_size(50.0, 3.0, 4.0), v2(50.0, 7.0));
    assert_eq!(ctx_grown_size(v2(50.0, 7.0), 16.0, 2.0), v2(50.0, 25.0));
}

#[test]
fn context_width_resizes_items_only_when_text_overflows() {
    let m = CtxMetrics {
        h_padding: 2.0,
        check_icon: 10.0,
        text_left_padding: 3.0,
        text_right_padding: 4.0,
        arrow_icon: 8.0,
        v_padding: 1.0,
        left_border_w: 3.0,
        right_border_w: 3.0,
    };
    let a = ctx_update_width(&m, 10.0, 100.0, 30.0);
    assert!(!a.resize_items);
    assert_eq!(a.menu_size, v2(106.0, 30.0));
    assert_eq!(a.arrow_pos, v3(90.0, 4.0, 0.2));
    assert_eq!(a.text_pos, v3(15.0, 1.0, 0.2));
    let b = ctx_update_width(&m, 90.0, 100.0, 30.0);
    assert!(b.resize_items);
    assert_eq!(b.item_width, 119.0);
    assert_eq!(b.menu_size.x, 125.0);
}

#[test]
fn context_hide_and_focus_rules() {
    assert_eq!(
        ctx_hide_plan(true, false),
        CtxHide {
            hide_child: true,
            notify_parent: false
        }
    );
    let u = ctx_update(0.0, 0.5, 0.0, true, true);
    assert!(u.hide_timer);
    let u = ctx_update(0.0, 0.5, 1.0, true, true);
    assert!(!u.hide_timer && u.timer == 0.5);
    assert_eq!(ctx_update(0.0, 0.5, 0.0, false, true).timer, 0.0);
    assert!(ctx_lost_focus_hides(true, true, true));
    assert!(ctx_lost_focus_hides(false, false, false));
    assert!(!ctx_lost_focus_hides(false, true, false));
    assert!(!ctx_lost_focus_hides(false, false, true));
    assert!(ctx_key_hides(KEY_ESCAPE) && !ctx_key_hides(0));
}

#[test]
fn main_menu_layout() {
    assert_eq!(main_item_pos(2.0, None, 5.0), v3(3.0, 2.0, 1.0));
    assert_eq!(
        main_item_pos(2.0, Some((3.0, 40.0)), 5.0),
        v3(51.0, 2.0, 1.0)
    );
    let s = main_item_size(30.0, 6.0, 12.0, 2.0);
    assert_eq!(s.size, v2(42.0, 16.0));
    assert_eq!(s.text_pos, v3(6.0, 2.0, 0.2));
    let r = main_init(PARENT_ROOT, 800.0, 0.0, 12.0);
    assert_eq!(r.size, v2(800.0, 20.0));
    let win = main_init(PARENT_WINDOW, 800.0, 300.0, 12.0);
    assert_eq!(win.size.x, 296.0);
    assert_eq!(win.pos_delta, v3(2.0, 0.0, 1.0));
    assert!(win.pos_delta.y.is_sign_negative());
    assert_eq!(main_init(PARENT_OTHER, 800.0, 300.0, 12.0).size.x, 300.0);
}

#[test]
fn popup_focus_bookkeeping() {
    let mut s = PopUpState::new(1, 2, 3);
    assert_eq!((s.new_attention, s.att_changed), (1, false));
    s.set_next_focus(9);
    assert!(s.focus_changed && s.new_focus == 9);
    s.set_next_focus(0);
    assert!(!s.focus_changed && s.new_focus == 0);
    s.after_callback([1, 2, 3], [1, 5, 3]);
    assert!(!s.att_changed && s.focus_changed && !s.default_focus_changed);
    assert_eq!(s.new_focus, 5);
    s.after_callback([1, 5, 3], [4, 5, 6]);
    assert!(s.att_changed && !s.focus_changed && s.default_focus_changed);
    assert_eq!((s.new_attention, s.new_default_focus), (4, 6));
    assert_eq!(s.new_focus, 5);
}

#[test]
fn popup_is_centred() {
    assert_eq!(
        popup_pos(v2(800.0, 600.0), v2(350.0, 250.0), 10.0),
        v3(225.0, 175.0, 10.0)
    );
}

#[test]
fn message_box_layout_single_and_double() {
    let one = msgbox_layout(50.0, 80.0, 12.0, v2(800.0, 600.0), false);
    assert_eq!(one.window_size, v2(200.0, 102.0));
    assert_eq!(one.window_pos, v3(300.0, 249.0, 100.0));
    assert_eq!(one.button0_pos, v3(60.0, 62.0, 1.0));
    assert_eq!(one.label_size, v2(190.0, 12.0));
    let two = msgbox_layout(50.0, 300.0, 12.0, v2(800.0, 600.0), true);
    assert_eq!(two.window_size.x, 340.0);
    assert_eq!(two.button0_pos.x, 170.0 - 90.0);
    assert_eq!(two.button1_pos.x, two.button0_pos.x + 100.0);
    let tiny = msgbox_layout(0.0, 0.0, 12.0, v2(10.0, 10.0), false);
    assert_eq!((tiny.window_pos.x, tiny.window_pos.y), (0.0, 0.0));
}

#[test]
fn message_box_gamepad() {
    assert_eq!(msgbox_gamepad(UI_BUTTON_PRIMARY, true), Some(false));
    assert_eq!(msgbox_gamepad(UI_BUTTON_SECONDARY, true), Some(true));
    assert_eq!(msgbox_gamepad(UI_BUTTON_SECONDARY, false), Some(false));
    assert_eq!(msgbox_gamepad(8, true), None);
}

#[test]
fn colour_component_conversion() {
    assert_eq!(color_slider_value(0.5), 500);
    assert_eq!(color_slider_value(0.9999), 999);
    assert_eq!(color_slider_value(f32::NAN), 0);
    assert_eq!(color_from_slider(250), 250.0f32 * 0.001);
    assert_eq!(color_component(&[false, false, true, false]), Some(2));
    assert_eq!(color_component(&[false; 4]), None);
    let a = Color {
        r: 1.0,
        g: 0.5,
        b: 0.0,
        a: 1.0,
    };
    assert!(!color_changed(a, a));
    assert!(color_changed(a, Color { a: 0.5, ..a }));
    assert!(color_changed(
        Color { r: f32::NAN, ..a },
        Color { r: f32::NAN, ..a }
    ));
}

#[test]
fn colour_layout_rows() {
    let l = color_layout();
    assert_eq!(l.slider_pos[2], v3(125.0, 110.0, 0.2));
    assert_eq!(l.input_pos[3], v3(245.0, 140.0, 0.2));
    assert_eq!(l.label_pos[1], v3(100.0, 85.0, 0.2));
}

#[test]
fn picker_start_filename() {
    assert_eq!(fp_start_filename(&w("/a/b.txt"), &w("/x")), w("b.txt"));
    assert_eq!(fp_start_filename(&[], &w("/x/dir.cfg")), w("dir.cfg"));
    assert_eq!(fp_start_filename(&[], &w("name")), Vec::<u32>::new());
    assert_eq!(fp_caption(PICKER_LOAD), w("Load file"));
    assert_eq!(fp_caption(PICKER_SAVE), w("Save file"));
    assert_eq!(fp_caption(7), w(" file"));
}

#[test]
fn picker_filter_text() {
    let a = w("*.txt");
    let b = w("*.cfg");
    assert_eq!(
        fp_filter_text(&w("Text"), &[&a, &b]),
        w("Text (*.txt, *.cfg)")
    );
    assert_eq!(fp_filter_text(&w("All"), &[]), w("All ()"));
}

#[test]
fn picker_directory_entries_and_selection() {
    assert_eq!(
        fp_dir_entries(true, false, 2),
        [DIR_ROOT, DIR_PERSONAL, 0, 1]
    );
    assert_eq!(fp_dir_entries(false, false, 0), [DIR_ROOT]);
    // items: root, personal, game, a, b
    assert_eq!(fp_dir_select(1, 5, 2), DirSelect { action: 1, pops: 0 });
    assert_eq!(fp_dir_select(4, 5, 2), DirSelect { action: 0, pops: 0 });
    assert_eq!(fp_dir_select(3, 5, 2), DirSelect { action: 2, pops: 2 });
    // nothing selected: the C++ popped past the end of the path
    assert_eq!(fp_dir_select(-1, 5, 2), DirSelect { action: 2, pops: 2 });
}

#[test]
fn picker_accept_and_extension() {
    assert_eq!(fp_accept(PICKER_SAVE, true, false), FP_ERROR_EMPTY);
    assert_eq!(fp_accept(PICKER_SAVE, false, true), FP_SAVE_CONFIRM);
    assert_eq!(fp_accept(PICKER_SAVE, false, false), FP_SAVE_DONE);
    assert_eq!(fp_accept(PICKER_LOAD, false, false), FP_LOAD_MISSING);
    assert_eq!(fp_accept(PICKER_LOAD, false, true), FP_LOAD_DONE);
    assert_eq!(fp_accept(9, false, true), FP_NONE);
    let f = w("*.map");
    assert_eq!(fp_save_extension(&w("/a/x.map"), Some(&f)), None);
    assert_eq!(fp_save_extension(&w("/a/x.txt"), Some(&f)), Some(w(".map")));
    assert_eq!(fp_save_extension(&w("/a/x"), Some(&f)), Some(w(".map")));
    assert_eq!(fp_save_extension(&w("/a/x"), None), None);
    assert!(fp_overwrite_confirmed(PICKER_SAVE, 0));
    assert!(!fp_overwrite_confirmed(PICKER_SAVE, 1));
    assert!(!fp_overwrite_confirmed(PICKER_LOAD, 0));
}

#[test]
fn keyboard_keys() {
    let k = kb_keys();
    assert_eq!(k.len(), KB_KEYS);
    assert_eq!((k[0].lower, k[0].upper), (48, 48));
    assert_eq!((k[10].lower, k[10].upper), (97, 65));
    assert_eq!(k[35].offset, 25);
    assert_eq!(k[36].kind, KEY_ENTER);
    assert!(!k[37].types_char);
    assert_eq!(k[38].size, v2(329.0, 32.0));
}

#[test]
fn keyboard_layout_wraps_rows() {
    let keys = kb_keys();
    let sizes: Vec<_> = keys.iter().map(|k| k.size).collect();
    let mut pos = vec![v3(0.0, 0.0, 0.0); sizes.len()];
    let mut row = vec![0; sizes.len()];
    let l = kb_layout(&sizes, &mut pos, &mut row);
    assert_eq!(pos[0], v3(0.0, 0.0, 2.0));
    assert_eq!(pos[9].x, 297.0);
    assert_eq!(row[10], 1);
    assert_eq!(pos[10], v3(0.0, 33.0, 2.0));
    assert_eq!(row[38], l.rows as i32 - 1);
    assert_eq!(l.window_size.x, 330.0);
    assert_eq!(l.window_size.y, pos[38].y + 33.0);
    assert_eq!(l.rows, 5);
}

#[test]
fn keyboard_nav_wraps() {
    let rows = [3usize, 3, 1];
    let mut nav = [KbNav {
        up: 0,
        right: 0,
        down: 0,
        left: 0,
    }; 7];
    kb_nav(&rows, &mut nav);
    assert_eq!(
        nav[0],
        KbNav {
            up: 6,
            right: 1,
            down: 3,
            left: 2
        }
    );
    assert_eq!(
        nav[5],
        KbNav {
            up: 2,
            right: 3,
            down: 6,
            left: 4
        }
    );
    assert_eq!(
        nav[6],
        KbNav {
            up: 3,
            right: 6,
            down: 0,
            left: 6
        }
    );
    let mut empty = [KbNav {
        up: 0,
        right: 0,
        down: 0,
        left: 0,
    }; 1];
    kb_nav(&[0, 1], &mut empty);
    assert_eq!(empty[0].up, -1);
    assert_eq!(empty[0].down, -1);
}

#[test]
fn keyboard_window_and_buttons() {
    assert_eq!(
        kb_window_pos(v3(100.0, 50.0, 1.0), v2(80.0, 20.0), 330.0),
        v3(100.0 + 40.0 - 165.0, 78.0, 1.0)
    );
    assert_eq!(kb_ui_press(UI_BUTTON_SECONDARY), KB_CANCEL);
    assert_eq!(kb_ui_press(UI_BUTTON_DELETE), KB_DELETE);
    assert_eq!(kb_ui_press(UI_BUTTON_CLEAR), KB_CLEAR);
    assert_eq!(kb_ui_press(UI_BUTTON_PREV_PAGE), KB_SHIFT_ON);
    assert_eq!(kb_ui_press(UI_BUTTON_PREV_PAGE | 1), KB_NONE);
    assert!(kb_ui_release(UI_BUTTON_PREV_PAGE | 1) && !kb_ui_release(1));
}
