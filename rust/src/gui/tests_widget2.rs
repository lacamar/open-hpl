use super::widget::{MOUSE_WHEEL_DOWN, MOUSE_WHEEL_UP, v2};
use super::widgets_list::*;
use super::widgets_slider::*;
use super::widgets_text::{self as wt, Edit, Event, Ret, TextEnv, TextHost, TextState};
use crate::math::v3;

fn w(s: &str) -> Vec<u32> {
    s.chars().map(u32::from).collect()
}

// ---------------------------------------------------------------- slider

fn slider(vertical: bool) -> SliderState {
    let mut s = SliderState::new(vertical, 10.0);
    s.update_bar(
        v2(20.0, 120.0),
        v3(5.0, 7.0, 0.0),
        v2(3.0, 3.0),
        v2(3.0, 3.0),
    );
    s
}

#[test]
fn slider_value_clamps_and_reports_changes() {
    let mut s = slider(true);
    assert_eq!(s.set_value(-4), None);
    assert_eq!(s.set_value(4), Some(4));
    assert_eq!(s.set_value(4), None);
    assert_eq!(s.set_value(99), Some(10));
    assert!(s.set_max_value(3));
    assert!(!s.set_max_value(3));
    assert_eq!(s.max_value, 3);
    assert!(s.set_max_value(-5));
    assert_eq!(s.max_value, 0);
    assert_eq!(s.bar_value_size, 0);
    s.set_bar_value_size(9);
    assert_eq!(s.bar_value_size, 1);
}

#[test]
fn slider_bar_geometry_vertical() {
    let mut s = slider(true);
    s.max_value = 10;
    s.bar_value_size = 1;
    s.value = 5;
    s.update_bar(
        v2(20.0, 120.0),
        v3(5.0, 7.0, 0.0),
        v2(3.0, 3.0),
        v2(3.0, 3.0),
    );
    assert_eq!(s.slider_size, 100.0);
    assert_eq!(s.bar_size.y, (1.0f32 / 11.0) * 100.0);
    assert_eq!(s.min_pos, 10.0);
    assert!(s.max_pos > s.bar_pos.y);
    assert_eq!(s.rect[0], 5.0);
    assert_eq!(s.rect[1], 7.0 + s.bar_pos.y);
}

#[test]
fn slider_min_bar_size_and_zero_range() {
    let mut s = SliderState::new(false, 10.0);
    s.max_value = 0;
    s.bar_value_size = 1;
    s.update_bar(
        v2(100.0, 10.0),
        v3(0.0, 0.0, 0.0),
        v2(4.0, 0.0),
        v2(4.0, 0.0),
    );
    assert_eq!(s.value_step, 0.0);
    assert_eq!(s.bar_size.x, 80.0);
    s.bar_value_size = 0;
    s.update_bar(
        v2(100.0, 10.0),
        v3(0.0, 0.0, 0.0),
        v2(4.0, 0.0),
        v2(4.0, 0.0),
    );
    assert_eq!(s.bar_size.x, 10.0);
}

#[test]
fn slider_drag_follows_mouse() {
    let mut s = slider(true);
    let g = v3(5.0, 7.0, 0.0);
    let (ret, fire) = s.mouse_down(
        1,
        v2(10.0, s.rect[1] + 1.0),
        v3(5.0, s.bar_pos.y + 1.0, 0.0),
        g,
    );
    assert!(ret && fire.is_none() && s.pressed);
    let fired = s.mouse_move(v3(5.0, 60.0, 0.0), g);
    assert!(fired.is_some());
    assert!(s.bar_pos.y <= s.max_pos && s.bar_pos.y >= s.min_pos);
    assert!(s.release());
    assert!(!s.release());
}

#[test]
fn slider_bar_click_pages_and_wheel_is_ignored() {
    let mut s = SliderState::new(false, 10.0);
    s.update_bar(
        v2(120.0, 20.0),
        v3(5.0, 7.0, 0.0),
        v2(3.0, 3.0),
        v2(3.0, 3.0),
    );
    let g = v3(5.0, 7.0, 0.0);
    assert_eq!(
        s.mouse_down(MOUSE_WHEEL_UP, v2(0.0, 0.0), v3(0.0, 0.0, 0.0), g),
        (false, None)
    );
    let (ret, fire) = s.mouse_down(1, v2(500.0, 500.0), v3(95.0, 0.0, 0.0), g);
    assert!(ret);
    assert!(fire.is_some());
    assert!(s.value > 0);
}

#[test]
fn slider_arrow_buttons() {
    assert_eq!(slider_arrow_button(true, UI_ARROW_UP), 0);
    assert_eq!(slider_arrow_button(true, UI_ARROW_DOWN), 1);
    assert_eq!(slider_arrow_button(true, UI_ARROW_LEFT), -1);
    assert_eq!(slider_arrow_button(false, UI_ARROW_LEFT), 0);
    assert_eq!(slider_arrow_button(false, UI_ARROW_RIGHT), 1);
    assert_eq!(slider_arrow_button(false, 3), -1);
}

// ---------------------------------------------------------------- tab frame

#[test]
fn tab_visibility_hides_and_places() {
    let mut st = TabFrameState {
        first_visible: 1,
        button_size: 10.0,
        first_tab_offset: 4.0,
        tab_separation: 2.0,
        ..TabFrameState::default()
    };
    let widths = [50.0, 60.0, 70.0];
    let mut items = [TabItem::default(); 3];
    let arrows = st.visibility(120.0, &widths, &mut items);
    assert!(!items[0].visible && !items[0].set_pos);
    assert!(items[1].visible && items[1].pos_x == 4.0);
    assert!(items[2].pos_x == 66.0);
    assert_eq!(st.last_visible, 1);
    assert!(arrows.left_enabled && arrows.right_enabled && arrows.shown);
}

#[test]
fn tab_visibility_past_the_end_is_defined() {
    let mut st = TabFrameState {
        first_visible: 5,
        ..TabFrameState::default()
    };
    let mut items = [TabItem::default(); 2];
    st.visibility(100.0, &[10.0, 10.0], &mut items);
    assert!(items.iter().all(|i| !i.visible));
}

#[test]
fn tab_arrow_moves_first_visible() {
    let mut st = TabFrameState::default();
    assert!(!st.arrow(false, 4));
    st.last_visible = 1;
    assert!(st.arrow(true, 4));
    assert_eq!(st.first_visible, 1);
    st.last_visible = 3;
    assert!(!st.arrow(true, 4));
    assert!(st.arrow(false, 4));
}

#[test]
fn tab_layout_and_page_targets() {
    let l = tab_layout(v2(200.0, 100.0), 12.0, 3.0);
    assert_eq!(l.label_pos.y, 3.0);
    assert_eq!(l.frame_pos.y, 19.0);
    assert_eq!(l.frame_size.y, 100.0 - 23.0);
    assert_eq!(tab_page_target(4, 2), Some(1));
    assert_eq!(tab_page_target(8, 2), Some(3));
    assert_eq!(tab_page_target(1, 2), None);
    assert!(tab_point_inside(
        v2(5.0, 5.0),
        true,
        v3(0.0, 0.0, 0.0),
        v2(10.0, 10.0),
        v3(100.0, 100.0, 0.0),
        v2(10.0, 10.0)
    ));
    assert!(!tab_point_inside(
        v2(5.0, 5.0),
        false,
        v3(0.0, 0.0, 0.0),
        v2(10.0, 10.0),
        v3(100.0, 100.0, 0.0),
        v2(10.0, 10.0)
    ));
}

// ---------------------------------------------------------------- list box

#[test]
fn list_update_and_cursor_window() {
    let mut l = ListState::new();
    let u = l.update(100.0, 8.0, 20);
    assert_eq!(l.max_items, 9);
    assert!(u.big && u.bar_value_size == 9 && u.max_value == 11);
    let s = l.cursor_step1(15, true, true);
    assert_eq!(
        s,
        SliderSet {
            set: true,
            value: 7
        }
    );
    assert_eq!(l.first_item, 7);
    l.first_item = 20;
    assert_eq!(
        l.cursor_step2(true),
        SliderSet {
            set: true,
            value: 15
        }
    );
    assert_eq!(l.cursor_step1(-3, false, false), SliderSet::default());
    assert_eq!(l.cursor_pos, -1);
    assert_eq!(l.last_cursor_pos, 15);
    let small = l.update(100.0, 8.0, 3);
    assert!(!small.big);
}

#[test]
fn list_cursor_window_with_nonpositive_rows_terminates() {
    let mut l = ListState::new();
    l.update(5.0, 8.0, 10);
    assert_eq!(l.max_items, -1);
    let s = l.cursor_step1(4, true, true);
    assert!(s.set);
    assert_eq!(l.first_item, 6);
}

#[test]
fn list_select_actions_and_ranges() {
    let mut l = ListState::new();
    l.update(100.0, 8.0, 20);
    assert_eq!(l.select_action(SEL_TOGGLE, 3, true, true).kind, 1);
    assert!(l.select_action(SEL_TOGGLE, 3, true, true).flag);
    l.allow_multi = true;
    assert!(!l.select_action(SEL_TOGGLE, 3, true, true).flag);
    assert_eq!(l.select_action(SEL_SELECT, 3, false, false).kind, 0);
    l.anchor_pos = 2;
    let a = l.select_action(SEL_FROM_ANCHOR, 6, true, false);
    assert_eq!((a.kind, a.start, a.end), (2, 2, 6));
    assert_eq!(
        range_plan(2, 6, 20),
        RangePlan {
            first: 2,
            count: 4,
            step: 1,
            end_valid: true
        }
    );
    assert_eq!(
        range_plan(6, 2, 20),
        RangePlan {
            first: 6,
            count: 4,
            step: -1,
            end_valid: true
        }
    );
    assert_eq!(
        range_plan(-1, 3, 20),
        RangePlan {
            first: 0,
            count: 3,
            step: 1,
            end_valid: true
        }
    );
    assert_eq!(range_plan(4, 4, 20).count, 0);
    assert!(!range_plan(2, 25, 20).end_valid);
    assert_eq!(range_plan(2, 25, 20).count, 18);
}

#[test]
fn list_multi_selection_negative_index_is_defined() {
    assert_eq!(multi_selection(&[4, 9], 1), 9);
    assert_eq!(multi_selection(&[4, 9], 2), -1);
    assert_eq!(multi_selection(&[4, 9], -1), -1);
}

#[test]
fn list_prune_drops_stale_indices() {
    let mut sel = [1, 7, 3, 9];
    let n = prune_selection(&mut sel, 5);
    assert_eq!(&sel[..n], &[1, 3]);
}

#[test]
fn list_mouse_and_keys() {
    let mut l = ListState::new();
    l.update(100.0, 8.0, 20);
    assert_eq!(ListState::wheel(MOUSE_WHEEL_UP), 0);
    assert_eq!(ListState::wheel(MOUSE_WHEEL_DOWN), 1);
    assert_eq!(ListState::wheel(MOUSE_WHEEL_UP | MOUSE_WHEEL_DOWN), -1);
    let m = l.mouse_down(32.0, 8.0, MOD_CTRL);
    assert!(m.proceed && m.index == 3 && m.ty == SEL_TOGGLE && !m.clear);
    assert_eq!(l.mouse_down_finish(false, 3), 3);
    assert_eq!(l.num_clicks, 1);
    assert_eq!(l.anchor_pos, 3);
    assert!(!l.mouse_down(-25.0, 8.0, 0).proceed);
    assert!(l.mouse_down(-5.0, 8.0, 0).proceed);
    let k = l.key_press(KEY_DOWN, 0);
    assert!(k.handled && k.arrow);
    let f = l.key_press_finish(0);
    assert!(f.select && f.ty == SEL_SELECT && f.clear);
    assert!(l.key_press(KEY_SPACE, MOD_CTRL).select);
    assert!(!l.key_press(9, 0).handled);
}

#[test]
fn list_double_click_and_locked_buttons() {
    let mut l = ListState::new();
    l.num_clicks = 2;
    assert_eq!(l.double_click(95.0, 100.0, 10.0), (false, false));
    assert_eq!(l.double_click(10.0, 100.0, 10.0), (true, true));
    assert_eq!(l.num_clicks, 0);
    let b = l.ui_button(true, 1, -1);
    assert_eq!((b.ret, b.init_select, b.check_locked), (-1, true, false));
    l.locked = true;
    assert_eq!(l.ui_button(true, 2, 0).ret, 1);
    assert!(!l.locked);
    assert_eq!(l.ui_button(true, 2, 0).ret, 0);
    assert_eq!(l.ui_button(false, 1, 0).ret, 0);
    l.update(100.0, 8.0, 5);
    assert!(!l.ui_arrow(UI_ARROW_UP).handled);
    l.locked = true;
    l.cursor_pos = 4;
    let a = l.ui_arrow(UI_ARROW_DOWN);
    assert!(a.handled && a.unlock);
}

#[test]
fn list_rows_skip_null_items_without_advancing() {
    let rows = [0, ROW_NULL, ROW_SELECTED | ROW_UNSELECTABLE, 0];
    let mut out = [RowOp::default(); 12];
    let n = list_rows(0, 10, 4, v3(3.0, 0.0, 0.0), 80.0, 8.0, &rows, &mut out);
    assert_eq!(n, 5);
    assert_eq!(out[0].kind, OP_TEXT);
    assert_eq!(out[1].kind, OP_BACKGROUND);
    assert_eq!(out[1].pos.y, 10.0);
    assert_eq!(out[3].kind, OP_TEXT_HIGHLIGHT);
    assert_eq!(out[4].pos.y, 20.0);
}

#[test]
fn list_cursor_lines_only_in_window() {
    let mut l = ListState::new();
    l.cursor_pos = 3;
    l.first_item = 3;
    let lines = cursor_lines(&l, v3(0.0, 2.0, 0.0), 100.0, 10.0, 8.0).unwrap();
    assert_eq!(lines[0].1, v2(90.0, 1.0));
    l.first_item = 4;
    assert!(cursor_lines(&l, v3(0.0, 2.0, 0.0), 100.0, 10.0, 8.0).is_none());
}

// ---------------------------------------------------------------- combo box

#[test]
fn combo_select_moves_the_window() {
    let mut c = ComboState::new();
    c.max_items = 3;
    assert!(c.select_step1(9, false, 5).is_none());
    assert_eq!(
        c.select_step1(4, true, 5),
        Some(SliderSet {
            set: true,
            value: 2
        })
    );
    assert!(c.select_step1(4, true, 5).is_none());
    c.first_item = 6;
    assert_eq!(
        c.select_step2(true),
        SliderSet {
            set: true,
            value: 4
        }
    );
    assert!(c.update(8.0, 2));
    assert_eq!(c.items_shown, 2);
    assert_eq!(c.menu_height, 2.0 + 10.0 * 2.0 + 2.0);
}

#[test]
fn combo_open_places_the_menu_inside_the_set() {
    let mut c = ComboState::new();
    c.update(8.0, 20);
    c.selected = 2;
    assert!(c.can_open(20));
    assert!(!c.can_open(0));
    let o = c.open(
        20,
        v2(100.0, 14.0),
        v2(2.0, 2.0),
        v2(2.0, 2.0),
        560.0,
        10.0,
        600.0,
    );
    assert!(c.menu_open && c.mouse_over == 2);
    assert_eq!(c.menu_pos.z, 11.0);
    assert!(c.menu_pos.y < 16.0);
    assert!(o.slider);
    assert_eq!(o.slider_size.x, 20.0);
    assert!(!c.can_open(20));
    assert!(c.close());
    assert!(!c.close());
}

#[test]
fn combo_mouse_handling() {
    let mut c = ComboState::new();
    c.update(8.0, 6);
    c.open(
        6,
        v2(100.0, 14.0),
        v2(2.0, 2.0),
        v2(2.0, 2.0),
        0.0,
        0.0,
        600.0,
    );
    assert!(c.mouse_move(true, v3(10.0, 40.0, 0.0), 100.0, false, 2.0, 8.0, 6));
    assert!(c.mouse_over >= 1);
    assert!(!c.mouse_move(false, v3(10.0, 40.0, 0.0), 100.0, false, 2.0, 8.0, 6));
    assert_eq!(c.mouse_down_pre(MOUSE_WHEEL_UP), (true, -1));
    assert!(c.mouse_up_pre(1));
    assert!(!c.mouse_up_pre(MOUSE_WHEEL_DOWN));
    assert!(c.mouse_down_closes(0.0));
    assert_eq!(c.button_press(1, v3(5.0, 20.0, 0.0), 100.0), 1);
    assert_eq!(c.button_press(1, v3(5.0, 500.0, 0.0), 100.0), 2);
    assert_eq!(c.button_press(4, v3(5.0, 20.0, 0.0), 100.0), 0);
    c.close();
    assert_eq!(c.button_press(1, v3(5.0, 20.0, 0.0), 100.0), 3);
}

#[test]
fn combo_ui_arrow_with_zero_button_add_is_defined() {
    let mut c = ComboState::new();
    c.update(8.0, 20);
    c.open(
        20,
        v2(100.0, 14.0),
        v2(2.0, 2.0),
        v2(2.0, 2.0),
        0.0,
        0.0,
        600.0,
    );
    let (ret, set) = c.ui_arrow(UI_ARROW_DOWN, 20, 3, 0);
    assert!(ret);
    assert_eq!(set, None);
    c.mouse_over = 19;
    let (_, set) = c.ui_arrow(UI_ARROW_UP, 20, 0, 1);
    assert_eq!(set, Some(7));
    c.mouse_over = 15;
    let (_, set) = c.ui_arrow(UI_ARROW_DOWN, 20, 0, 1);
    assert_eq!(set, Some(5));
}

#[test]
fn combo_ui_buttons_and_slider_arrows() {
    let mut c = ComboState::new();
    c.update(8.0, 4);
    assert!(!c.ui_button(false, 1).ret);
    let o = c.ui_button(true, 1);
    assert_eq!((o.action, o.ret), (2, true));
    assert!(c.open_by_ui);
    c.open(
        4,
        v2(10.0, 10.0),
        v2(0.0, 0.0),
        v2(0.0, 0.0),
        0.0,
        0.0,
        600.0,
    );
    let s = c.ui_button(true, 1);
    assert_eq!((s.action, s.clear_open_by_ui), (3, true));
    let esc = c.ui_button(true, 2);
    assert_eq!((esc.action, esc.ret), (1, true));
    assert!(!c.open_by_ui);
    c.mouse_over = 0;
    assert!(c.slider_arrow(UI_ARROW_UP, 4));
    assert_eq!(c.mouse_over, 3);
    assert!(c.slider_arrow(UI_ARROW_DOWN, 4));
    assert_eq!(c.mouse_over, 0);
    assert!(!c.slider_arrow(2, 4));
}

#[test]
fn combo_rows_highlight_hovered_item() {
    let mut c = ComboState::new();
    c.update(8.0, 3);
    c.max_items = 3;
    c.mouse_over = 1;
    let mut out = [RowOp::default(); 8];
    let n = combo_rows(&c, v3(0.0, 0.0, 0.0), 80.0, 8.0, 3, &mut out);
    assert_eq!(n, 4);
    assert_eq!(out[1].kind, OP_SELECTION);
    assert_eq!(out[2].kind, OP_TEXT_HIGHLIGHT);
    assert_eq!(out[3].kind, OP_TEXT);
}

// ---------------------------------------------------------------- text box

#[derive(Default)]
struct Host {
    clip: Vec<u32>,
    copied: Vec<Vec<u32>>,
    events: Vec<Event>,
    local_x: f32,
}

impl TextHost for Host {
    fn glyph(&mut self, num: i32) -> Option<f32> {
        (0..127).contains(&num).then_some(1.0)
    }
    fn length(&mut self, text: &[u32]) -> f32 {
        text.len() as f32 * 6.0
    }
    fn local_x(&mut self) -> f32 {
        self.local_x
    }
    fn clipboard(&mut self) -> Vec<u32> {
        self.clip.clone()
    }
    fn copy(&mut self, text: &[u32]) {
        self.copied.push(text.to_vec());
    }
    fn to_float(&mut self, text: &[u32], _decimals: i32) -> f32 {
        String::from_utf32(text).parse().unwrap_or(0.0)
    }
    fn float_text(&mut self, v: f32, decimals: i32) -> Vec<u32> {
        w(&format!("{v:.*}", usize::try_from(decimals).unwrap_or(0)))
    }
    fn emit(&mut self, e: Event) {
        self.events.push(e);
    }
}

trait FromU32 {
    fn from_utf32(t: &[u32]) -> String;
}

impl FromU32 for String {
    fn from_utf32(t: &[u32]) -> String {
        t.iter().filter_map(|&c| char::from_u32(c)).collect()
    }
}

fn env() -> TextEnv {
    TextEnv {
        font_first: 0,
        font_last: 126,
        font_size: v2(6.0, 8.0),
        corner0: v2(2.0, 2.0),
        default_decimals: 3,
    }
}

fn state() -> TextState {
    let mut st = TextState::new(false);
    st.text_max_size = 1000.0;
    st
}

struct Session {
    st: TextState,
    host: Host,
    text: Vec<u32>,
}

impl Session {
    fn new(text: &str) -> Self {
        Session {
            st: state(),
            host: Host::default(),
            text: w(text),
        }
    }

    fn run<R>(&mut self, f: impl FnOnce(&mut Edit<'_, Host>) -> R) -> R {
        let mut e = Edit {
            st: &mut self.st,
            env: env(),
            text: self.text.clone(),
            illegal: &[],
            host: &mut self.host,
        };
        let r = f(&mut e);
        let t = e.text.clone();
        drop(e);
        self.text = t;
        r
    }

    fn key(&mut self, key: i32, modifier: i32, ch: char) -> Ret {
        self.run(|e| e.key_press(key, modifier, u32::from(ch)).0)
    }

    fn text(&self) -> String {
        String::from_utf32(&self.text)
    }
}

#[test]
fn text_typing_inserts_at_marker() {
    let mut s = Session::new("ac");
    s.st.marker = 1;
    assert_eq!(s.key(200, 0, 'b'), Ret::True);
    assert_eq!(s.text(), "abc");
    assert_eq!(s.st.marker, 2);
    assert!(s.st.changed_since_enter);
    assert!(matches!(s.host.events.last(), Some(Event::SetText { .. })));
}

#[test]
fn text_without_marker_ignores_keys() {
    let mut s = Session::new("ac");
    assert_eq!(s.key(200, 0, 'b'), Ret::False);
    s.st.marker = 0;
    s.st.can_edit = false;
    assert_eq!(s.key(200, 0, 'b'), Ret::False);
    assert_eq!(s.text(), "ac");
}

#[test]
fn text_characters_outside_the_font_are_dropped() {
    let mut s = Session::new("");
    s.st.marker = 0;
    s.key(200, 0, '\u{e9}');
    assert_eq!(s.text(), "");
    s.key(200, 0, 'x');
    assert_eq!(s.text(), "x");
}

#[test]
fn text_selection_edit_and_clipboard() {
    let mut s = Session::new("hello world");
    s.st.marker = 5;
    s.st.sel_end = 0;
    assert_eq!(s.key(wt::KEY_C, wt::MOD_CTRL, 'c'), Ret::True);
    assert_eq!(s.host.copied, vec![w("hello")]);
    assert_eq!(s.key(wt::KEY_X, wt::MOD_CTRL, 'x'), Ret::True);
    assert_eq!(s.text(), " world");
    assert_eq!(s.st.sel_end, -1);
    s.host.clip = w("big ");
    s.st.marker = 0;
    s.key(wt::KEY_V, wt::MOD_CTRL, 'v');
    assert_eq!(s.text(), "big  world");
    assert_eq!(s.st.marker, 4);
}

#[test]
fn text_ctrl_with_other_keys_is_not_handled() {
    let mut s = Session::new("ab");
    s.st.marker = 1;
    assert_eq!(s.key(wt::KEY_LEFT, wt::MOD_CTRL, 'x'), Ret::False);
    assert_eq!(s.st.marker, 1);
    s.key(wt::KEY_A, wt::MOD_CTRL, 'a');
    assert_eq!((s.st.sel_end, s.st.marker), (0, 2));
}

#[test]
fn text_paste_filters_illegal_and_respects_max() {
    let mut s = Session::new("ab");
    s.st.marker = 1;
    s.st.max_chars = 4;
    s.host.clip = w("xyz");
    s.key(wt::KEY_V, wt::MOD_CTRL, 'v');
    assert_eq!(s.text(), "ab");
    assert!(s.st.changed_since_enter);
    s.host.clip = w("xy");
    s.key(wt::KEY_V, wt::MOD_CTRL, 'v');
    assert_eq!(s.text(), "axyb");
    let mut e = Edit {
        st: &mut s.st,
        env: env(),
        text: w("q"),
        illegal: &w("z"),
        host: &mut s.host,
    };
    e.st.max_chars = -1;
    e.st.marker = 1;
    e.host.clip = w("azb");
    e.key_press(wt::KEY_V, wt::MOD_CTRL, 0);
    assert_eq!(e.text, w("qab"));
}

#[test]
fn text_backspace_delete_and_selection_replace() {
    let mut s = Session::new("abcd");
    s.st.marker = 2;
    s.key(wt::KEY_BACKSPACE, 0, ' ');
    assert_eq!((s.text().as_str(), s.st.marker), ("acd", 1));
    s.key(wt::KEY_DELETE, 0, ' ');
    assert_eq!(s.text(), "ad");
    s.st.marker = 2;
    s.key(wt::KEY_DELETE, 0, ' ');
    assert_eq!(s.text(), "ad");
    s.st.marker = 0;
    s.key(wt::KEY_BACKSPACE, 0, ' ');
    assert_eq!(s.text(), "ad");
    s.st.marker = 2;
    s.st.sel_end = 0;
    s.key(200, 0, 'z');
    assert_eq!((s.text().as_str(), s.st.marker, s.st.sel_end), ("z", 1, -1));
}

#[test]
fn text_arrows_home_end_and_shift_selection() {
    let mut s = Session::new("abcd");
    s.st.marker = 2;
    s.key(wt::KEY_RIGHT, wt::MOD_SHIFT, ' ');
    assert_eq!((s.st.marker, s.st.sel_end), (3, 2));
    s.key(wt::KEY_LEFT, 0, ' ');
    assert_eq!((s.st.marker, s.st.sel_end), (2, -1));
    s.key(wt::KEY_HOME, wt::MOD_SHIFT, ' ');
    assert_eq!((s.st.marker, s.st.sel_end), (0, 2));
    s.key(wt::KEY_END, 0, ' ');
    assert_eq!((s.st.marker, s.st.sel_end), (4, -1));
    s.key(wt::KEY_RIGHT, 0, ' ');
    assert_eq!(s.st.marker, 4);
}

#[test]
fn text_enter_fires_only_after_a_change() {
    let mut s = Session::new("ab");
    s.st.marker = 1;
    assert_eq!(s.key(wt::KEY_RETURN, 0, ' '), Ret::True);
    assert!(!s.host.events.contains(&Event::Enter));
    s.key(200, 0, 'c');
    assert_eq!(s.key(wt::KEY_KP_ENTER, 0, ' '), Ret::Enter);
    assert_eq!(s.host.events.last(), Some(&Event::Enter));
    assert!(!s.st.changed_since_enter);
    s.st.force_enter = true;
    assert_eq!(s.key(wt::KEY_RETURN, 0, ' '), Ret::Enter);
}

#[test]
fn text_numeric_input_rules() {
    let mut s = Session::new("");
    s.st = TextState::new(true);
    s.st.text_max_size = 1000.0;
    s.st.marker = 0;
    s.key(200, 0, 'a');
    assert_eq!(s.text(), "");
    s.key(200, 0, '5');
    s.key(200, 0, '.');
    s.key(200, 0, '.');
    s.key(200, 0, '2');
    assert_eq!(s.text(), "5.2");
    s.st.marker = 0;
    s.key(200, 0, '-');
    s.key(200, 0, '-');
    assert_eq!(s.text(), "-5.2");
    s.st.marker = 4;
    s.st.sel_end = 1;
    s.key(200, 0, '.');
    assert_eq!(s.text(), "-.");
    assert!(s.st.numeric_updated);
}

#[test]
fn text_numeric_value_roundtrip_and_bounds() {
    let mut s = Session::new("");
    s.st = TextState::new(true);
    s.st.has_upper = true;
    s.st.upper = 10.0;
    s.run(|e| e.set_numeric_value(25.0));
    assert_eq!(s.st.numeric_value, 10.0);
    assert_eq!(s.text(), "10.000");
    assert!(!s.st.numeric_updated);
    s.text = w("2.5");
    s.st.numeric_updated = true;
    assert_eq!(s.run(|e| e.numeric_value()), 2.5);
    assert_eq!(s.run(|e| e.button_pressed(0, true)), Ret::Enter);
    assert_eq!(s.text(), "3.000");
    assert_eq!(s.run(|e| e.button_pressed(0, false)), Ret::False);
    assert_eq!(s.run(|e| e.button_pressed(-1, true)), Ret::True);
}

#[test]
fn text_max_length_truncation_nests_the_set_text() {
    let mut s = Session::new("abcdef");
    s.st.marker = 6;
    s.st.sel_end = 5;
    s.run(|e| e.set_max_text_length(3));
    assert_eq!(s.text(), "abc");
    assert_eq!((s.st.marker, s.st.sel_end), (2, 2));
    s.host.events.clear();
    s.st.marker = 3;
    s.st.numeric = true;
    s.st.numeric_updated = false;
    let t = w("abcdef");
    s.run(|e| e.set_text(t));
    assert!(s.st.numeric_updated);
    match s.host.events.as_slice() {
        [
            Event::SetText {
                text,
                nested: Some(n),
            },
        ] => {
            assert_eq!(text.len(), 6);
            assert_eq!(n.len(), 3);
        }
        other => panic!("{other:?}"),
    }
}

#[test]
fn text_visible_window_follows_the_marker() {
    let mut s = Session::new(&"abcdefghij".repeat(3));
    s.st.text_max_size = 30.0;
    s.run(|e| e.on_change_text(false));
    assert_eq!(s.st.visible_size, 4);
    s.st.marker = 0;
    s.run(|e| e.set_marker_pos(20));
    assert!(s.st.first_visible > 10);
    s.run(|e| e.set_marker_pos(1));
    assert_eq!(s.st.first_visible, 1);
}

#[test]
fn text_marker_on_empty_text_is_not_clamped_and_reads_nul() {
    let mut s = Session::new("");
    s.run(|e| e.set_selected_text(0, 5));
    assert_eq!(s.st.marker, 5);
    s.host.local_x = 0.0;
    s.run(|e| e.double_click(1));
    assert!(s.st.marker >= 0);
}

#[test]
fn text_mouse_select_and_double_click_word() {
    let mut s = Session::new("one two three");
    s.host.local_x = 4.0 + 6.0 * 5.0;
    let (r, att) = s.run(|e| e.mouse_down(1));
    assert!(r == Ret::True && att && s.st.pressed);
    assert_eq!(s.st.sel_end, -1);
    s.host.local_x = 4.0 + 6.0 * 8.0;
    s.run(|e| e.mouse_move());
    assert_ne!(s.st.sel_end, -1);
    let saved = s.run(|e| e.mouse_up(1));
    assert!(saved.is_some());
    assert!(!s.st.pressed);
    s.run(|e| e.mouse_up_restore(2, 7));
    assert_eq!((s.st.marker, s.st.sel_end), (2, 7));
    s.host.local_x = 4.0 + 6.0 * 5.0;
    s.run(|e| e.double_click(1));
    assert_eq!((s.st.sel_end, s.st.marker), (4, 7));
    assert_eq!(s.run(|e| e.mouse_down(4)).0, Ret::False);
    s.st.can_edit = false;
    assert!(s.run(|e| e.mouse_up(1)).is_none());
}

#[test]
fn text_focus_transitions() {
    let mut s = Session::new("ab");
    s.run(|e| e.got_focus());
    assert!(s.st.got_focus_recently);
    s.run(|e| e.got_tab_focus());
    assert_eq!(
        (s.st.marker, s.st.sel_end, s.st.got_focus_recently),
        (2, 0, false)
    );
    s.st.changed_since_enter = true;
    s.run(|e| e.lost_focus());
    assert_eq!((s.st.marker, s.st.sel_end), (-1, -1));
    assert_eq!(s.host.events, vec![Event::Enter]);
    assert!(s.run(|e| e.ui_button(true, wt::UI_BUTTON_PRIMARY)));
    assert!(!s.run(|e| e.ui_button(false, wt::UI_BUTTON_PRIMARY)));
}

#[test]
fn text_layout_and_draw_plan() {
    let st = TextState::new(false);
    let (h, wd) = wt::layout(
        &st,
        100.0,
        v2(1.0, 1.0),
        v2(1.0, 1.0),
        v2(1.0, 1.0),
        8.0,
        0.0,
    );
    assert_eq!((h, wd), (14.0, 92.0));
    let mut s = Session::new("hello");
    s.st.text_max_size = 100.0;
    s.run(|e| e.on_change_text(false));
    s.st.marker = 3;
    s.st.sel_end = 1;
    let d = s.run(|e| e.draw(v3(10.0, 20.0, 0.0)));
    assert!(d.has_marker && d.has_selection);
    assert_eq!(d.text_pos, v3(15.0, 24.0, 0.2));
    assert_eq!(d.marker_pos.x, 15.0 + 18.0);
    assert_eq!(d.sel_size.x, 12.0);
    assert_eq!((d.hl_start, d.hl_len), (1, 2));
}
