//! Pure logic of the HPL2 `iWidget` base class and the frame and window widgets: position
//! chains, hit tests, border/corner draw lists, scroll planning and drag math. The widget
//! objects, their virtuals and every `cGuiSet` call stay in C++; they pass snapshots in and
//! perform the returned [`Op`] list and decisions.

use crate::math::{sub3, v3};
use crate::types::{Color, Vector2f, Vector3f};

pub const MOUSE_LEFT: i32 = 1;
pub const MOUSE_RIGHT: i32 = 4;
pub const MOUSE_WHEEL_UP: i32 = 8;
pub const MOUSE_WHEEL_DOWN: i32 = 16;
pub const UI_BUTTON_PRIMARY: i32 = 1;
pub const KEY_ESCAPE: i32 = 5;

pub const GFX_BACKGROUND: u32 = 0;
pub const GFX_IMAGE: u32 = 9;
pub const GFX_LABEL: u32 = 10;
pub const GFX_SLOTS: usize = 11;

pub const FONT_DEFAULT: u32 = 0;
pub const FONT_WINDOW_LABEL: u32 = 1;

pub const OP_GFX: u32 = 0;
pub const OP_FONT: u32 = 1;
pub const OP_OFFSET: u32 = 2;

pub const HAS_SIZE: u32 = 1;
pub const HAS_COLOR: u32 = 2;

/// Gfx slots: 0 background, 1..=4 borders (right, left, up, down), 5..=8 corners (LU, RU, RD, LD),
/// 9 image, 10 label.
#[repr(C)]
#[derive(Clone, Copy, Default, Debug, PartialEq)]
pub struct GfxMetrics {
    pub active: Vector2f,
    pub image: Vector2f,
}

/// One `cGuiSet` call. `OP_GFX`: `DrawGfx(gfx slot, pos, size if HAS_SIZE, color if HAS_COLOR)` with
/// the C++ defaults for absent arguments. `OP_FONT`: `DrawFont(text slot[..text_len if >= 0], font
/// slot, pos, size, color, align)`. `OP_OFFSET`: `SetDrawOffset(pos)`.
#[repr(C)]
#[derive(Clone, Copy, Default, Debug, PartialEq)]
pub struct Op {
    pub kind: u32,
    pub gfx: u32,
    pub text: u32,
    pub font: u32,
    pub flags: u32,
    pub align: i32,
    pub text_len: i32,
    pub pos: Vector3f,
    pub size: Vector2f,
    pub color: Color,
}

const _: () = assert!(std::mem::size_of::<Op>() == 4 * 7 + 12 + 8 + 16);

/// Appends ops into a caller buffer; ops past the capacity are counted but dropped.
pub struct Ops<'a> {
    buf: &'a mut [Op],
    n: usize,
}

impl<'a> Ops<'a> {
    pub fn new(buf: &'a mut [Op]) -> Self {
        Ops { buf, n: 0 }
    }

    pub fn len(&self) -> usize {
        self.n.min(self.buf.len())
    }

    pub fn is_empty(&self) -> bool {
        self.n == 0
    }

    fn push(&mut self, op: Op) {
        if let Some(slot) = self.buf.get_mut(self.n) {
            *slot = op;
        }
        self.n += 1;
    }

    pub fn offset(&mut self, pos: Vector3f) {
        self.push(Op {
            kind: OP_OFFSET,
            pos,
            ..Op::default()
        });
    }

    pub fn gfx(&mut self, slot: u32, pos: Vector3f, size: Option<Vector2f>, color: Option<Color>) {
        let mut flags = 0;
        if size.is_some() {
            flags |= HAS_SIZE;
        }
        if color.is_some() {
            flags |= HAS_COLOR;
        }
        self.push(Op {
            kind: OP_GFX,
            gfx: slot,
            flags,
            pos,
            size: size.unwrap_or_default(),
            color: color.unwrap_or_default(),
            ..Op::default()
        });
    }

    #[allow(
        clippy::too_many_arguments,
        reason = "mirrors the DrawFont argument list"
    )]
    pub fn font(
        &mut self,
        text: u32,
        text_len: i32,
        font: u32,
        pos: Vector3f,
        size: Vector2f,
        color: Color,
        align: i32,
    ) {
        self.push(Op {
            kind: OP_FONT,
            text,
            text_len,
            font,
            align,
            pos,
            size,
            color,
            ..Op::default()
        });
    }
}

pub fn add3(a: Vector3f, b: Vector3f) -> Vector3f {
    v3(a.x + b.x, a.y + b.y, a.z + b.z)
}

pub fn add2(a: Vector2f, b: Vector2f) -> Vector2f {
    Vector2f {
        x: a.x + b.x,
        y: a.y + b.y,
    }
}

pub fn sub2(a: Vector2f, b: Vector2f) -> Vector2f {
    Vector2f {
        x: a.x - b.x,
        y: a.y - b.y,
    }
}

pub fn v2(x: f32, y: f32) -> Vector2f {
    Vector2f { x, y }
}

pub fn color_mul(a: Color, b: Color) -> Color {
    Color {
        r: a.r * b.r,
        g: a.g * b.g,
        b: a.b * b.b,
        a: a.a * b.a,
    }
}

pub const WHITE: Color = Color {
    r: 1.0,
    g: 1.0,
    b: 1.0,
    a: 1.0,
};

/// `cMath::RoundToInt`; the double floor result saturates like the aarch64 `fcvtzs`.
pub fn round_to_int(v: f32) -> i32 {
    (f64::from(v) + 0.5).floor() as i32
}

/// `cMath::CheckPointInRectIntersection`.
pub fn point_in_rect(p: Vector2f, x: f32, y: f32, w: f32, h: f32) -> bool {
    !(p.x < x || p.x > x + w || p.y < y || p.y > y + h)
}

#[repr(C)]
#[derive(Clone, Copy, Default, Debug, PartialEq)]
pub struct ParentPos {
    pub global: Vector3f,
    pub children_offset: Vector3f,
    pub scroll: Vector3f,
}

/// `iWidget::GetGlobalPosition` of a widget that has a parent.
pub fn global_position(p: &ParentPos, affected_by_scroll: bool, local: Vector3f) -> Vector3f {
    let scroll = if affected_by_scroll {
        p.scroll
    } else {
        Vector3f::default()
    };
    add3(sub3(add3(p.global, p.children_offset), scroll), local)
}

/// `iWidget::SetGlobalPosition`; `parent` is `None` for a root widget.
pub fn global_to_local(pos: Vector3f, parent: Option<&ParentPos>) -> Vector3f {
    match parent {
        Some(p) => sub3(sub3(pos, p.global), p.children_offset),
        None => pos,
    }
}

/// `iWidget::CenterGlobalPositionInSet`.
pub fn center_in_set(set: Vector2f, size: Vector2f, global_z: f32) -> Vector3f {
    let mut x = set.x / 2.0 - size.x / 2.0;
    let mut y = set.y / 2.0 - size.y / 2.0;
    if x < 0.0 {
        x = 0.0;
    }
    if y < 0.0 {
        y = 0.0;
    }
    v3(x, y, global_z)
}

/// `iWidget::GetPosRelativeToMouse`.
pub fn rel_to_mouse(global: Vector3f, mouse: Vector2f) -> Vector2f {
    v2(global.x - mouse.x, global.y - mouse.y)
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum PointStage {
    Outside = 0,
    Inside = 1,
    CheckBox = 2,
}

/// First half of `iWidget::PointIsInside`; `CheckBox` means the global position box test decides.
pub fn point_stage(parent_ok: bool, only_clipped: bool, clips: bool) -> PointStage {
    if !parent_ok {
        PointStage::Outside
    } else if only_clipped && !clips {
        PointStage::Inside
    } else {
        PointStage::CheckBox
    }
}

pub fn point_in_box(p: Vector2f, global: Vector3f, size: Vector2f) -> bool {
    !(p.x < global.x || p.x > global.x + size.x || p.y < global.y || p.y > global.y + size.y)
}

/// Index into the focus-navigation array for an arrow flag, or -1 where C++ indexes out of bounds
/// (`cMath::Log2ToInt` of 0 or of a flag combination above 11).
pub fn ui_arrow_slot(arrow: i32) -> i32 {
    match arrow {
        i32::MIN..=-1 => 0,
        0 => -1,
        1 => 0,
        2 => 1,
        3..=5 => 2,
        6..=11 => 3,
        _ => -1,
    }
}

/// `iWidget::DrawBordersAndCorners`.
pub fn borders_and_corners(
    o: &mut Ops,
    has_bg: bool,
    outside: bool,
    pos: Vector3f,
    size: Vector2f,
    m: &[GfxMetrics; GFX_SLOTS],
) {
    let act = |i: usize| m.get(i).map(|g| g.active).unwrap_or_default();
    let img = |i: usize| m.get(i).map(|g| g.image).unwrap_or_default();
    let (c0, c1, c2, c3) = (act(5), act(6), act(7), act(8));
    let b0 = act(1);
    let b3 = act(4);
    let (i0, i1, i2, i3) = (img(1), img(2), img(3), img(4));
    o.offset(pos);
    if has_bg {
        let (p, s) = if outside {
            (v3(0.0, 0.0, 0.0), size)
        } else {
            (v3(c0.x, c0.y, 0.0), sub2(sub2(size, c2), c0))
        };
        o.gfx(GFX_BACKGROUND, p, Some(s), Some(WHITE));
    }
    if outside {
        o.gfx(1, v3(size.x, 0.0, 0.0), Some(v2(i0.x, size.y)), None);
        o.gfx(2, v3(-c0.x, 0.0, 0.0), Some(v2(i1.x, size.y)), None);
        o.gfx(3, v3(0.0, -c0.y, 0.0), Some(v2(size.x, i2.y)), None);
        o.gfx(4, v3(0.0, size.y, 0.0), Some(v2(size.x, i3.y)), None);
        o.gfx(5, v3(-c0.x, -c0.y, 0.0), None, None);
        o.gfx(6, v3(size.x, -c1.y, 0.0), None, None);
        o.gfx(7, v3(size.x, size.y, 0.0), None, None);
        o.gfx(8, v3(-c3.x, size.y, 0.0), None, None);
    } else {
        o.gfx(
            1,
            v3(size.x - b0.x, c1.y, 0.0),
            Some(v2(i0.x, size.y - (c2.y + c1.y))),
            None,
        );
        o.gfx(
            2,
            v3(0.0, c0.y, 0.0),
            Some(v2(i1.x, size.y - (c3.y + c0.y))),
            None,
        );
        o.gfx(
            3,
            v3(c0.x, 0.0, 0.0),
            Some(v2(size.x - (c0.x + c1.x), i2.y)),
            None,
        );
        o.gfx(
            4,
            v3(c3.x, size.y - b3.y, 0.0),
            Some(v2(size.x - (c2.x + c3.x), i3.y)),
            None,
        );
        o.gfx(5, v3(0.0, 0.0, 0.0), None, None);
        o.gfx(6, v3(size.x - c1.x, 0.0, 0.0), None, None);
        o.gfx(7, v3(size.x - c2.x, size.y - c2.y, 0.0), None, None);
        o.gfx(8, v3(0.0, size.y - c3.y, 0.0), None, None);
    }
    o.offset(v3(0.0, 0.0, 0.0));
}

// ---------------------------------------------------------------- window

#[repr(C)]
#[derive(Clone, Copy, Default, Debug, PartialEq)]
pub struct WindowLook {
    pub size: Vector2f,
    pub corner_lu: Vector2f,
    pub corner_ru: Vector2f,
    pub label_font_height: f32,
    pub label_offset: Vector3f,
}

impl WindowLook {
    fn label_size(&self) -> Vector2f {
        v2(
            self.size.x - (self.corner_lu.x + self.corner_ru.x),
            self.label_font_height + self.label_offset.y * 2.0,
        )
    }
}

#[repr(C)]
#[derive(Clone, Copy, Default, Debug, PartialEq)]
pub struct WindowDraw {
    pub look: WindowLook,
    pub global: Vector3f,
    pub draw_label: bool,
    pub focused: bool,
    pub has_background: bool,
    pub borders_outside: bool,
    pub font_size: Vector2f,
    pub font_color: Color,
    pub color_mul: Color,
}

/// `cWidgetWindow::OnDraw`.
pub fn window_draw(o: &mut Ops, d: &WindowDraw, m: &[GfxMetrics; GFX_SLOTS]) {
    let l = &d.look;
    if d.draw_label {
        let col = if d.focused {
            WHITE
        } else {
            Color {
                r: 0.6,
                g: 0.6,
                b: 0.6,
                a: 0.3,
            }
        };
        o.gfx(
            GFX_LABEL,
            add3(d.global, v3(l.corner_lu.x, l.corner_lu.y, 0.2)),
            Some(l.label_size()),
            Some(col),
        );
    }
    o.font(
        0,
        -1,
        FONT_WINDOW_LABEL,
        add3(
            d.global,
            v3(
                l.corner_lu.x + l.label_offset.x,
                l.corner_lu.y + l.label_offset.y,
                0.4,
            ),
        ),
        d.font_size,
        color_mul(d.font_color, d.color_mul),
        0,
    );
    borders_and_corners(o, d.has_background, d.borders_outside, d.global, l.size, m);
}

/// `cWidgetWindow::OnMouseDown` after the static check: whether the label was hit with the left button.
pub fn window_mouse_down(l: &WindowLook, global: Vector3f, mouse: Vector2f, val: i32) -> bool {
    let ls = l.label_size();
    let x = global.x + l.corner_lu.x;
    let y = global.y + l.corner_lu.y;
    point_in_rect(mouse, x, y, ls.x, ls.y) && val & MOUSE_LEFT != 0
}

/// `mvRelMousePos + cVector3f(mouse.x, mouse.y, 0)` of the drag handlers.
pub fn window_drag_pos(rel: Vector3f, mouse: Vector2f) -> Vector3f {
    add3(rel, v3(mouse.x, mouse.y, 0.0))
}

/// `cWidgetWindow::OnKeyPress`: whether the window must send `WindowClose`.
pub fn window_key_closes(focused: bool, close_on_escape: bool, key: i32, modifier: i32) -> bool {
    focused && close_on_escape && key == KEY_ESCAPE && modifier == 0
}

/// `cWidgetWindow::OnChildLostFocus`: whether `mbFocused` drops.
pub fn window_focus_lost(
    no_focused_widget: bool,
    connected_down: bool,
    connected_up: bool,
) -> bool {
    no_focused_widget || (!connected_down && !connected_up)
}

/// Position of a main menu attached to a window.
pub fn window_menu_pos(l: &WindowLook) -> Vector3f {
    v3(0.0, l.label_size().y + 3.0, 0.1)
}

/// Position of the close button for a window width.
pub fn window_button_pos(width: f32, button_size: f32) -> Vector3f {
    v3(width - button_size - 5.0, 5.0, 0.2)
}

// ---------------------------------------------------------------- frame

#[repr(C)]
#[derive(Clone, Copy, Default, Debug, PartialEq)]
pub struct SliderLayout {
    pub h_size: Vector2f,
    pub h_pos: Vector3f,
    pub v_size: Vector2f,
    pub v_pos: Vector3f,
}

/// `cWidgetFrame::OnChangeSize`: slider sizes and positions.
pub fn frame_slider_layout(size: Vector2f, h_visible: bool, v_visible: bool) -> SliderLayout {
    let mut h = size.x;
    if v_visible {
        h -= 18.0;
    }
    let mut v = size.y;
    if h_visible {
        v -= 18.0;
    }
    SliderLayout {
        h_size: v2(h, 18.0),
        h_pos: v3(0.0, size.y - 18.0, 0.7),
        v_size: v2(18.0, v),
        v_pos: v3(size.x - 18.0, 0.0, 0.7),
    }
}

#[repr(C)]
#[derive(Clone, Copy, Default, Debug, PartialEq)]
pub struct ChildBox {
    pub pos: Vector3f,
    pub size: Vector2f,
    pub visible: bool,
}

#[repr(C)]
#[derive(Clone, Copy, Default, Debug, PartialEq)]
pub struct ScrollIn {
    pub size: Vector2f,
    pub margin: Vector2f,
    pub offset: Vector3f,
    pub max_coord: Vector3f,
    pub h_size: Vector2f,
    pub v_size: Vector2f,
    pub has_h: bool,
    pub has_v: bool,
}

#[repr(C)]
#[derive(Clone, Copy, Default, Debug, PartialEq)]
pub struct SliderPlan {
    pub max_value: i32,
    pub bar_value_size: i32,
    pub button_add: i32,
    pub set_value: bool,
    pub value: i32,
    pub active: bool,
}

#[repr(C)]
#[derive(Clone, Copy, Default, Debug, PartialEq)]
pub struct ScrollPlan {
    pub max_coord: Vector3f,
    pub h: SliderPlan,
    pub v: SliderPlan,
}

/// `cWidgetFrame::OnUpdate` scrollbar pass: what each slider must be set to.
pub fn frame_scroll_plan(i: &ScrollIn, children: &[ChildBox]) -> ScrollPlan {
    let mut hori = false;
    let mut vert = false;
    let safe = sub2(add2(v2(i.offset.x, i.offset.y), i.size), i.margin);
    let mut max = v3(-999_999.0, -999_999.0, -999_999.0);
    for c in children.iter().filter(|c| c.visible) {
        let lr = v3(c.pos.x + c.size.x, c.pos.y + c.size.y, c.pos.z);
        if c.pos.x < i.offset.x || lr.x > safe.x {
            hori = true;
        }
        if c.pos.y < i.offset.y || lr.y > safe.y {
            vert = true;
        }
        if max.x < lr.x {
            max.x = lr.x;
        }
        if max.y < lr.y {
            max.y = lr.y;
        }
    }
    let mut keep = i.max_coord;
    if max.x != keep.x {
        keep.x = max.x;
    }
    if max.y != keep.y {
        keep.y = max.y;
    }
    let mut plan = ScrollPlan {
        max_coord: keep,
        ..ScrollPlan::default()
    };
    let mut limit = i.offset;
    if i.has_h {
        let mut end = keep.x - i.size.x;
        let bar = i.size.y * end / keep.y;
        if vert && i.has_v {
            end += i.v_size.x;
        }
        let h = &mut plan.h;
        h.max_value = round_to_int(end).wrapping_add(1);
        h.bar_value_size = round_to_int(bar);
        h.button_add = round_to_int(i.size.x * 0.1);
        if end > 0.0 && limit.x > end {
            limit.x = end;
        }
        if !hori {
            h.set_value = true;
            h.value = 0;
        } else {
            let off = round_to_int(i.offset.x);
            let lim = round_to_int(limit.x);
            if off > lim {
                h.set_value = true;
                h.value = lim;
            }
        }
        h.active = hori;
    }
    if i.has_v {
        let mut end = keep.y - i.size.y;
        let bar = i.size.y * end / keep.y;
        if hori && i.has_h {
            end += i.h_size.y;
        }
        let v = &mut plan.v;
        v.max_value = round_to_int(end).wrapping_add(1);
        v.bar_value_size = round_to_int(bar);
        v.button_add = round_to_int(i.size.y * 0.1);
        if end > 0.0 {
            if limit.y > end {
                limit.y = end;
            }
        } else {
            vert = false;
        }
        if !vert {
            v.set_value = true;
            v.value = 0;
        } else {
            let off = round_to_int(i.offset.y);
            let lim = round_to_int(limit.y);
            if off > lim {
                v.set_value = true;
                v.value = lim;
            }
        }
        v.active = vert;
    }
    plan
}

/// `cWidgetFrame::OnMouseDown` wheel handling: `Some(value)` when the vertical slider moves.
pub fn frame_wheel(val: i32, value: i32, button_add: i32) -> Option<i32> {
    let add = button_add.wrapping_mul(3);
    let mut v = value;
    if val & MOUSE_WHEEL_UP != 0 {
        v = v.wrapping_sub(add);
    } else if val & MOUSE_WHEEL_DOWN != 0 {
        v = v.wrapping_add(add);
    }
    (v != value).then_some(v)
}

#[repr(C)]
#[derive(Clone, Copy, Default, Debug, PartialEq)]
pub struct FrameDraw {
    pub global: Vector3f,
    pub size: Vector2f,
    pub draw_frame: bool,
    pub draw_background: bool,
    pub background_z: f32,
    pub background_color: Color,
}

/// `cWidgetFrame::OnDraw` (border pass).
pub fn frame_draw(o: &mut Ops, d: &FrameDraw, m: &[GfxMetrics; GFX_SLOTS]) {
    if d.draw_frame {
        let c0 = m.get(5).map(|g| g.active).unwrap_or_default();
        let c2 = m.get(7).map(|g| g.active).unwrap_or_default();
        borders_and_corners(
            o,
            false,
            false,
            sub3(d.global, v3(c0.x, c0.y, 0.0)),
            add2(add2(d.size, c0), c2),
            m,
        );
    }
}

/// `cWidgetFrame::OnDrawAfterClip` (background pass).
pub fn frame_draw_background(o: &mut Ops, d: &FrameDraw) {
    if d.draw_background {
        o.gfx(
            GFX_BACKGROUND,
            add3(d.global, v3(0.0, 0.0, d.background_z)),
            Some(d.size),
            Some(d.background_color),
        );
    }
}
