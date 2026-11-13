//! `cDevHud::Render` and `RenderBugReport`: the draw sequence. The GUI set, font and frame buffer
//! stay in C++ behind `HudHost`; the bug-report state stays with the shell and is passed in.

use super::devhud;
use crate::string;

pub trait HudHost {
    fn app_time_ms(&self) -> u64;
    /// Lazy font/set setup (`cDevHud::Init`).
    fn init(&self) -> bool;
    fn capture_bug_shot(&self);
    fn screen_size(&self) -> (i32, i32);
    fn set_virtual_size(&self, w: f32, h: f32);
    fn fps(&self) -> f32;
    fn avg_frame_ms(&self) -> f32;
    fn system_lines(&self) -> Vec<Vec<u8>>;
    fn font_length(&self, size: f32, text: &[u32]) -> f32;
    fn draw_gfx(&self, pos: [f32; 3], size: [f32; 2]);
    fn draw_font(&self, text: &[u32], pos: [f32; 3], size: f32, color: [f32; 4]);
    fn set_frame_buffer_null(&self);
    fn render_set(&self);
}

pub struct HudState<'a> {
    pub hud: bool,
    pub bug_open: bool,
    pub bug_capture: bool,
    pub saved_time: u64,
    pub bug_text: &'a [u32],
    pub last_saved: &'a [u8],
}

const TITLE_COLOR: [f32; 4] = [1.0, 0.75, 0.2, 1.0];
const TEXT_COLOR: [f32; 4] = [1.0; 4];
const BUG_TITLE: &str = "Bug report  -  Enter: save   Shift+Enter: new line   Esc: cancel";
const BUG_SAVED: &str = "Bug report saved";

fn wide(s: &str) -> Vec<u32> {
    s.chars().map(u32::from).collect()
}

fn max(a: f32, b: f32) -> f32 {
    if a > b { a } else { b }
}

/// Returns whether the pending bug capture was consumed.
pub fn render(host: &dyn HudHost, st: &HudState<'_>) -> bool {
    let toast = st.saved_time != 0 && devhud::toast_active(st.saved_time, host.app_time_ms());
    if !devhud::should_render(st.hud, st.bug_open, toast) {
        return false;
    }
    if !host.init() {
        return false;
    }
    let captured = st.bug_capture;
    if captured {
        host.capture_bug_shot();
    }

    let (sx, sy) = host.screen_size();
    host.set_virtual_size(sx as f32, sy as f32);

    if st.hud {
        let size = devhud::hud_font_size(sy);
        let mut lines: Vec<Vec<u32>> = host
            .system_lines()
            .iter()
            .map(|l| string::utf8_to_wide(l))
            .collect();
        let stats = devhud::stats_line(sx, sy, host.fps(), host.avg_frame_ms());
        lines.push(string::utf8_to_wide(&stats));

        let mut width = 0.0;
        for l in &lines {
            width = max(width, host.font_length(size, l));
        }
        let lay = devhud::hud_layout(size, width, lines.len());
        host.draw_gfx([lay.pad, lay.pad, 0.0], [lay.bg_w, lay.bg_h]);
        let mut y = lay.text_y;
        for (i, l) in lines.iter().enumerate() {
            let col = if i == 0 { TITLE_COLOR } else { TEXT_COLOR };
            host.draw_font(l, [lay.text_x, y, 1.0], size, col);
            y += lay.line_h;
        }
    }

    if st.bug_open || toast {
        render_bug_report(host, st, sx, sy);
    }

    host.set_frame_buffer_null();
    host.render_set();
    captured
}

fn render_bug_report(host: &dyn HudHost, st: &HudState<'_>, sx: i32, sy: i32) {
    let m = devhud::bug_metrics(sx, sy);
    let size = m.font;
    let mut box_w = m.box_w;
    let mut lines: Vec<Vec<u32>> = Vec::new();
    if st.bug_open {
        lines.push(wide(BUG_TITLE));
        let mut text = st.bug_text.to_vec();
        text.push(devhud::cursor_char(host.app_time_ms()));
        lines.extend(devhud::wrap_bug_text(&text, m.text_w, &mut |l| {
            host.font_length(size, l)
        }));
    } else {
        lines.push(wide(BUG_SAVED));
        lines.push(string::utf8_to_wide(st.last_saved));
        let w = host.font_length(size, lines.get(1).map_or(&[], Vec::as_slice));
        box_w = devhud::toast_box_w(&m, w);
    }

    let b = devhud::bug_box(&m, box_w, lines.len(), sx, sy);
    host.draw_gfx([b.x, b.y, 10.0], [b.w, b.h]);
    let mut y = b.text_y;
    for (i, l) in lines.iter().enumerate() {
        let col = if i == 0 { TITLE_COLOR } else { TEXT_COLOR };
        host.draw_font(l, [b.text_x, y, 11.0], size, col);
        y += m.line_h;
    }
}
