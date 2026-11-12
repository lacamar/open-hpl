//! `LuxDebugHandler`: debug flags and their user config, the message list, the batch map queue
//! and the debug window checkbox dispatch.

use super::cfg::Cfg;
use crate::string;
use std::collections::VecDeque;

#[derive(Clone, Debug, Default, PartialEq)]
pub struct Message {
    pub text: Vec<u32>,
    pub count: f32,
}

/// The flags the C++ reads directly (`OhplGa9DebugFlags`). The C++ left them, `mbFirstUpdateOnMap`
/// and `mlTempCount` uninitialised until the first user config / map load; here they start
/// false / 0.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Flags {
    pub show_fps: bool,
    pub show_sound_playing: bool,
    pub show_player_info: bool,
    pub show_entity_info: bool,
    pub show_debug_messages: bool,
    pub show_error_messages: bool,
    pub script_debug_on: bool,
    pub inspection_mode: bool,
    pub disable_flashbacks: bool,
    pub draw_physics: bool,
    pub reload_from_current_position: bool,
    pub allow_quick_save: bool,
    pub window_active: bool,
    pub fast_forward: bool,
    pub first_update_on_map: bool,
    pub temp_count: i32,
}

#[repr(C)]
#[derive(Clone, Debug, Default)]
pub struct Debug {
    pub f: Flags,
    pub batch_maps: VecDeque<Vec<u8>>,
    pub messages: VecDeque<Message>,
}

impl Debug {
    /// `LoadUserConfig`; ptest switches the overlays off.
    pub fn load_user_config(&mut self, c: &mut dyn Cfg, ptest: bool) {
        self.f.show_fps = c.bool(b"Debug", b"ShowFPS", true);
        self.f.show_sound_playing = c.bool(b"Debug", b"ShowSoundPlaying", true);
        self.f.show_player_info = c.bool(b"Debug", b"ShowPlayerInfo", true);
        self.f.show_entity_info = c.bool(b"Debug", b"ShowEntityInfo", true);
        self.f.show_debug_messages = c.bool(b"Debug", b"ShowDebugMessages", true);
        self.f.show_error_messages = c.bool(b"Debug", b"ShowErrorsAndWarnings", false);
        self.f.script_debug_on = c.bool(b"Debug", b"ScriptDebugOn", false);
        self.f.inspection_mode = c.bool(b"Debug", b"InspectionMode", false);
        self.f.disable_flashbacks = c.bool(b"Debug", b"DisableFlashBacks", false);
        self.f.draw_physics = c.bool(b"Debug", b"DrawPhysics", false);
        self.f.reload_from_current_position = c.bool(b"Debug", b"ReloadFromCurrentPosition", true);
        self.f.allow_quick_save = c.bool(b"Debug", b"AllowQuickSave", false);
        if ptest {
            self.f.show_fps = false;
            self.f.show_sound_playing = false;
            self.f.show_player_info = false;
            self.f.show_entity_info = false;
            self.f.show_debug_messages = false;
            self.f.show_error_messages = false;
            self.f.script_debug_on = false;
            self.f.inspection_mode = false;
            self.f.disable_flashbacks = false;
        }
    }

    pub fn save_user_config(&self, c: &mut dyn Cfg) {
        c.set_bool(b"Debug", b"ShowFPS", self.f.show_fps);
        c.set_bool(b"Debug", b"ShowSoundPlaying", self.f.show_sound_playing);
        c.set_bool(b"Debug", b"ShowPlayerInfo", self.f.show_player_info);
        c.set_bool(b"Debug", b"ShowEntityInfo", self.f.show_entity_info);
        c.set_bool(b"Debug", b"ShowDebugMessages", self.f.show_debug_messages);
        c.set_bool(
            b"Debug",
            b"ShowErrorsAndWarnings",
            self.f.show_error_messages,
        );
        c.set_bool(b"Debug", b"ScriptDebugOn", self.f.script_debug_on);
        c.set_bool(b"Debug", b"InspectionMode", self.f.inspection_mode);
        c.set_bool(b"Debug", b"DisableFlashBacks", self.f.disable_flashbacks);
        c.set_bool(b"Debug", b"DrawPhysics", self.f.draw_physics);
        c.set_bool(
            b"Debug",
            b"ReloadFromCurrentPosition",
            self.f.reload_from_current_position,
        );
        c.set_bool(b"Debug", b"AllowQuickSave", self.f.allow_quick_save);
    }

    pub fn reset(&mut self) {
        self.f.first_update_on_map = false;
    }

    pub fn on_map_enter(&mut self) {
        self.f.first_update_on_map = true;
        self.f.temp_count = 0;
    }

    fn push(&mut self, text: &[u32]) {
        self.messages.push_front(Message {
            text: text.to_vec(),
            count: 4.0,
        });
        if self.messages.len() > 100 {
            self.messages.pop_back();
        }
    }

    fn has(&self, text: &[u32]) -> bool {
        self.messages.iter().any(|m| m.text == text)
    }

    /// `AddErrorOrWarningMessage`; duplicates are dropped.
    pub fn add_error_or_warning(&mut self, text: &[u32]) {
        if !self.f.show_error_messages || self.has(text) {
            return;
        }
        self.push(text);
    }

    pub fn add_message(&mut self, text: &[u32], check_duplicates: bool) {
        if !self.f.show_debug_messages || (check_duplicates && self.has(text)) {
            return;
        }
        self.push(text);
    }

    pub fn update_messages(&mut self, time_step: f32) {
        self.messages.iter_mut().for_each(|m| m.count -= time_step);
        self.messages.retain(|m| m.count.is_nan() || m.count > 0.0);
    }

    /// `SetFastForward`: the engine speed multiplier when the flag changed.
    pub fn set_fast_forward(&mut self, on: bool) -> Option<f32> {
        if self.f.fast_forward == on {
            return None;
        }
        self.f.fast_forward = on;
        Some(if on { 4.0 } else { 1.0 })
    }

    /// The counters of `Update`: whether the first-update block runs and a batch map is due.
    pub fn update_tick(&mut self) -> (bool, bool) {
        let first = self.f.first_update_on_map;
        self.f.first_update_on_map = false;
        self.f.temp_count = self.f.temp_count.wrapping_add(1);
        let due = self.f.temp_count > 30 && !self.batch_maps.is_empty();
        if due {
            self.f.temp_count = 0;
        }
        (first, due)
    }

    pub fn batch_pop(&mut self) -> Vec<u8> {
        self.batch_maps.pop_front().unwrap_or_default()
    }

    /// `LoadBatchLoadFile`: every token of the file is a map.
    pub fn load_batch(&mut self, data: &[u8]) {
        self.batch_maps = string::split(data, None, true)
            .map(<[u8]>::to_vec)
            .collect();
    }

    /// `SetDebugWindowActive`: `None` when the debug menu is not loaded, else the new activity
    /// and whether the cursor lock changes (not fullscreen).
    pub fn window_change(
        load_debug_menu: bool,
        active: bool,
        fullscreen: bool,
    ) -> Option<(bool, bool)> {
        load_debug_menu.then_some((active, !fullscreen))
    }
}

#[derive(Clone, Copy, Debug, PartialEq)]
pub enum Toggle {
    None,
    OcclusionCulling(bool),
    LogCreateAndDelete(bool),
    FastPhysicsLoad(bool),
    FreeCam(bool),
    FreeCamSpeed(f32),
    FastForward(bool),
}

impl Debug {
    /// `ChangeDebugText`: widget `num` set to `val`; the flags it owns change here, the rest is
    /// returned for the engine.
    pub fn change_text(&mut self, num: i32, val: i32) -> Toggle {
        let on = val == 1;
        match num {
            0 => self.f.show_fps = on,
            1 => self.f.show_player_info = on,
            2 => self.f.show_entity_info = on,
            3 => self.f.show_sound_playing = on,
            4 => self.f.show_debug_messages = on,
            5 => self.f.inspection_mode = on,
            6 => return Toggle::OcclusionCulling(on),
            7 => return Toggle::LogCreateAndDelete(on),
            8 => self.f.reload_from_current_position = on,
            9 => return Toggle::FastPhysicsLoad(on),
            10 => self.f.disable_flashbacks = on,
            11 => self.f.draw_physics = on,
            12 => self.f.show_error_messages = on,
            13 => return Toggle::FreeCam(on),
            14 => {
                let v = (val as f32) / 100.0;
                return Toggle::FreeCamSpeed(if v > 0.001 { v } else { 0.001 });
            }
            17 => return Toggle::FastForward(on),
            _ => {}
        }
        Toggle::None
    }
}

/// `UpdateInspectionMeshEntity`: the end of the line and its bounding box (min, max).
pub fn inspection_line(start: [f32; 3], forward: [f32; 3]) -> ([f32; 3], [f32; 3], [f32; 3]) {
    let end = [
        forward[0].mul_add(15.0, start[0]),
        forward[1].mul_add(15.0, start[1]),
        forward[2].mul_add(15.0, start[2]),
    ];
    let min = [0, 1, 2].map(|i| pick_min(start.get(i), end.get(i)));
    let max = [0, 1, 2].map(|i| pick_max(start.get(i), end.get(i)));
    (end, min, max)
}

fn pick_min(a: Option<&f32>, b: Option<&f32>) -> f32 {
    let (a, b) = (a.copied().unwrap_or(0.0), b.copied().unwrap_or(0.0));
    if a < b { a } else { b }
}

fn pick_max(a: Option<&f32>, b: Option<&f32>) -> f32 {
    let (a, b) = (a.copied().unwrap_or(0.0), b.copied().unwrap_or(0.0));
    if a > b { a } else { b }
}
