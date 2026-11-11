//! Game pause/resume and saving-lock bookkeeping (`SomaSetGamePaused`).
//!
//! Official pause (`cLuxMap::PauseSoundsAndMusic`): music and sounds pause once each, ambience
//! follows the sounds unless it is excluded, and resuming clears every flag.

#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct Pause {
    pub music: bool,
    pub sounds: bool,
    pub ambience: bool,
    pub saving_enabled: bool,
    pub saving_disabled_time: f32,
}

impl Default for Pause {
    fn default() -> Self {
        Pause {
            music: false,
            sounds: false,
            ambience: false,
            saving_enabled: true,
            saving_disabled_time: 0.0,
        }
    }
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Op {
    /// `mbScriptGamePaused = arg != 0`.
    SetScriptPaused(bool),
    ClearUpdateWhenPaused,
    /// Current map's world active = `mbActive && arg`.
    WorldActive(bool),
    MusicResume,
    MusicPause,
    /// Resume every sound entry and instance.
    SoundsResumeAll,
    /// Pause the sound entries and instances of these types.
    SoundsPause(u32),
    SoundscapePaused(bool),
    ReapplyVoicePauses,
}

pub fn set_paused(
    st: &mut Pause,
    script_paused: bool,
    pause: bool,
    music: bool,
    sounds: bool,
    ambience: bool,
    types: u32,
) -> Vec<Op> {
    let mut ops = Vec::new();
    if !pause && !script_paused {
        return ops;
    }
    ops.push(Op::SetScriptPaused(pause));
    if !pause {
        ops.push(Op::ClearUpdateWhenPaused);
    }
    ops.push(Op::WorldActive(!pause));
    if !pause {
        ops.extend([
            Op::MusicResume,
            Op::SoundsResumeAll,
            Op::SoundscapePaused(false),
            Op::ReapplyVoicePauses,
        ]);
        st.music = false;
        st.sounds = false;
        st.ambience = false;
        return ops;
    }
    if !st.music && music {
        ops.push(Op::MusicPause);
        st.music = true;
    }
    if !st.sounds && sounds {
        ops.push(Op::SoundsPause(types));
        st.sounds = true;
        st.ambience = true;
        if !ambience {
            ops.push(Op::SoundscapePaused(false));
            st.ambience = false;
        }
    } else if !st.ambience && ambience {
        ops.push(Op::SoundscapePaused(true));
        st.ambience = true;
    }
    ops
}

pub fn disable_saving(st: &mut Pause, duration: f32) {
    st.saving_enabled = false;
    st.saving_disabled_time = duration;
}

pub fn tick_saving(st: &mut Pause, dt: f32) {
    if !st.saving_enabled && st.saving_disabled_time > 0.0 {
        st.saving_disabled_time -= dt;
        if st.saving_disabled_time <= 0.0 {
            st.saving_enabled = true;
        }
    }
}

/// What `cSomaLuxUpdater::Update` does after the map-change check.
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct Frame {
    pub game_input: bool,
    pub game_update: bool,
    pub game_paused: bool,
    pub map_update: bool,
    pub update_when_paused: bool,
}

pub fn frame_plan(
    has_game: bool,
    has_map: bool,
    map_active: bool,
    uses_real_player: bool,
    paused: bool,
) -> Frame {
    Frame {
        game_input: has_map && uses_real_player,
        game_update: has_game,
        game_paused: paused,
        map_update: has_map && map_active && !paused,
        update_when_paused: paused,
    }
}

/// `cSomaLuxUpdater::PostUpdate`: (game post-update, map post-update).
pub fn post_plan(
    updated: bool,
    scripts_held: bool,
    paused: bool,
    has_game: bool,
    has_map: bool,
    map_active: bool,
) -> (bool, bool) {
    if !updated || scripts_held || paused {
        return (false, false);
    }
    (has_game, has_map && map_active)
}
