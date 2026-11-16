//! The game HUD's UI, script-owned UIs (`cLuxScriptImGui`) and their per-frame draw/render.

use super::s8d::{Host, ImGui, Ptr};
use std::cell::{Cell, RefCell};

/// The script object of `cLuxScriptImGui`: two engine words, the UI, a property slot.
#[repr(C)]
pub struct ScriptImGui {
    pad: [usize; 2],
    pub im: *mut ImGui,
    prop: usize,
}

const _: () = assert!(std::mem::offset_of!(ScriptImGui, im) == 16);

#[derive(Clone, Copy)]
pub struct HudGeom {
    pub size: [f32; 2],
    pub offset: [f32; 2],
    pub center: [f32; 2],
    pub start: [f32; 3],
}

thread_local! {
    static HUD: Cell<usize> = const { Cell::new(0) };
    static HUD_SETS: RefCell<Vec<Ptr>> = const { RefCell::new(Vec::new()) };
    static SCRIPT_IMS: RefCell<Vec<usize>> = const { RefCell::new(Vec::new()) };
    static LAST_SCREEN: Cell<[i32; 2]> = const { Cell::new([0, 0]) };
    static GEOM: Cell<usize> = const { Cell::new(0) };
}

/// Matches the official engine at 4:3, 16:10 and 16:9: a 1024x768 centre area, widened by 4/3 of
/// the extra width.
fn hud_virtual_size(h: &dyn Host) -> [f32; 2] {
    let (s, _) = h.screen_size();
    [(3072.0f32 * s[0] / s[1] - 1024.0) / 3.0, 768.0]
}

/// The recomputed HUD geometry; the pointer stays valid for the whole run.
pub fn geom(h: &dyn Host, refresh: bool) -> *mut HudGeom {
    let mut p = GEOM.with(Cell::get) as *mut HudGeom;
    if p.is_null() {
        p = Box::into_raw(Box::new(HudGeom {
            size: [0.0; 2],
            offset: [0.0; 2],
            center: [1024.0, 768.0],
            start: [0.0; 3],
        }));
        GEOM.with(|c| c.set(p as usize));
    }
    if refresh {
        let size = hud_virtual_size(h);
        let offset = [(size[0] - 1024.0) * 0.5, 0.0];
        // SAFETY: leaked box, single thread.
        unsafe {
            (*p).size = size;
            (*p).offset = offset;
            (*p).start = [-offset[0], 0.0, 0.0];
        }
    }
    p
}

fn set_hud_virtual(h: &dyn Host, set: Ptr) {
    let size = hud_virtual_size(h);
    h.set_hud_virtual(set, size, [(size[0] - 1024.0) * 0.5, 0.0]);
}

pub fn hud(h: &dyn Host) -> *mut ImGui {
    let p = HUD.with(Cell::get);
    if p != 0 {
        return p as *mut ImGui;
    }
    let set = h.create_set(b"GameHud");
    set_hud_virtual(h, set);
    HUD_SETS.with(|s| s.borrow_mut().push(set));
    let im = ImGui::new_boxed(h, b"GameHud", set);
    HUD.with(|c| c.set(im as usize));
    im
}

pub fn create_script_imgui(h: &dyn Host, name: &[u8], draw: bool) -> *mut ScriptImGui {
    let set = h.create_set(name);
    set_hud_virtual(h, set);
    HUD_SETS.with(|s| s.borrow_mut().push(set));
    let p = Box::into_raw(Box::new(ScriptImGui {
        pad: [0; 2],
        im: ImGui::new_boxed(h, name, set),
        prop: 0,
    }));
    if draw {
        SCRIPT_IMS.with(|v| v.borrow_mut().push(p as usize));
    }
    p
}

pub fn draw_imguis(h: &dyn Host) {
    let (_, screen) = h.screen_size();
    if LAST_SCREEN.with(Cell::get) != screen {
        LAST_SCREEN.with(|c| c.set(screen));
        for set in HUD_SETS.with(|s| s.borrow().clone()) {
            if !h.set_info(set).2 {
                set_hud_virtual(h, set);
            }
        }
    }
    let p = HUD.with(Cell::get) as *mut ImGui;
    if !p.is_null() {
        // SAFETY: leaked box, single thread.
        unsafe { (*p).draw_all(h) };
    }
    for s in SCRIPT_IMS.with(|v| v.borrow().clone()) {
        // SAFETY: leaked box, single thread.
        unsafe { (*(*(s as *mut ScriptImGui)).im).draw_all(h) };
    }
}

pub fn render_imguis(h: &dyn Host) {
    draw_imguis(h);
    for set in HUD_SETS.with(|s| s.borrow().clone()) {
        if !h.set_info(set).2 {
            h.set_render(set);
        }
    }
}
