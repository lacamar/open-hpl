//! Debug, general, timers, script variables, credits, saving, map changes, achievements.

use super::super::a1::{Arg, raw_bytes};
use super::super::{tables, vars};
use super::{GLOBAL_TEMP, LOCAL_NULL, NULL_STR, RStr, S, call, error, ops, s, sf, temp};
use crate::sound::own::handler;
use crate::{math, string};

fn map_var(name: &[u8]) -> Option<*const std::ffi::c_void> {
    if !call(ops::MAP_EXISTS, &[]).as_b() {
        return None;
    }
    let v = call(ops::MAP_VAR, &[s(name)]).p;
    if v.is_null() {
        error(&format!("Couldn't find local var '{}'\n", lossy(name)));
        return None;
    }
    Some(v)
}

fn global_var(name: &[u8], word: &str) -> Option<*const std::ffi::c_void> {
    let v = call(ops::GLOBAL_VAR, &[s(name)]).p;
    if v.is_null() {
        error(&format!("Couldn't find {word} var '{}'\n", lossy(name)));
        return None;
    }
    Some(v)
}

fn lossy(b: &[u8]) -> String {
    String::from_utf8_lossy(b).into_owned()
}

fn var_set(v: *const std::ffi::c_void, val: &[u8]) {
    call(ops::VAR_SET, &[Arg::ptr(v), s(val)]);
}

fn var_text<'a>(v: *const std::ffi::c_void) -> &'a [u8] {
    // SAFETY: `v` is a script variable's value string, alive for the call.
    unsafe { raw_bytes(v) }
}

fn cur_empty_map_var(name: &[u8]) -> Vec<u8> {
    if !call(ops::MAP_EXISTS, &[]).as_b() {
        return Vec::new();
    }
    let v = call(ops::MAP_VAR, &[s(name)]).p;
    if v.is_null() {
        error(&format!("Couldn't find local var '{}'\n", lossy(name)));
        return Vec::new();
    }
    var_text(v).to_vec()
}

fn cur_global(name: &[u8]) -> Vec<u8> {
    let v = call(ops::GLOBAL_VAR, &[s(name)]).p;
    if v.is_null() {
        error(&format!("Couldn't find global var '{}'\n", lossy(name)));
        return Vec::new();
    }
    var_text(v).to_vec()
}

sf!(Print(text: S) {
    call(ops::LOG, &[s(text.0)]);
});

sf!(AddDebugMessage(text: S, dup: bool) {
    call(ops::DEBUG_MSG, &[s(text.0), Arg::b(dup)]);
});

sf!(ProgLog(level: S, text: S) {
    call(ops::PROG_LOG, &[Arg::i(tables::progress_level(level.0)), s(text.0)]);
});

sf!(ScriptDebugOn() -> bool {
    call(ops::DEBUG_SCRIPT_ON, &[]).as_b()
});

sf!(RandFloat(min: f32, max: f32) -> f32 {
    math::rand_rectf(min, max)
});

sf!(RandInt(min: i32, max: i32) -> i32 {
    math::rand_rectl(min, max)
});

sf!(StringSub(text: S, start: i32, count: i32) -> RStr {
    temp(GLOBAL_TEMP, string::sub(text.0, start, count, false).unwrap_or_default())
});

sf!(AddTimer(name: S, time: f32, func: S) {
    if call(ops::MAP_EXISTS, &[]).as_b() {
        call(ops::MAP_ADD_TIMER, &[s(name.0), Arg::f(time), s(func.0)]);
    }
});

sf!(RemoveTimer(name: S) {
    if call(ops::MAP_EXISTS, &[]).as_b() {
        call(ops::MAP_REMOVE_TIMER, &[s(name.0)]);
    }
});

sf!(GetTimerTimeLeft(name: S) -> f32 {
    if !call(ops::MAP_EXISTS, &[]).as_b() {
        return 0.0;
    }
    call(ops::MAP_TIMER_LEFT, &[s(name.0)]).as_f()
});

sf!(SetLocalVarInt(name: S, val: i32) {
    if let Some(v) = map_var(name.0) {
        var_set(v, &vars::from_int(val));
    }
});

sf!(SetLocalVarFloat(name: S, val: f32) {
    if let Some(v) = map_var(name.0) {
        var_set(v, &vars::from_float(val));
    }
});

sf!(SetLocalVarString(name: S, val: S) {
    if let Some(v) = map_var(name.0) {
        var_set(v, val.0);
    }
});

sf!(AddLocalVarInt(name: S, val: i32) {
    if let Some(v) = map_var(name.0) {
        var_set(v, &vars::add_int(var_text(v), val));
    }
});

sf!(AddLocalVarFloat(name: S, val: f32) {
    if let Some(v) = map_var(name.0) {
        var_set(v, &vars::add_float(var_text(v), val));
    }
});

sf!(AddLocalVarString(name: S, val: S) {
    if let Some(v) = map_var(name.0) {
        let mut cur = var_text(v).to_vec();
        cur.extend_from_slice(val.0);
        var_set(v, &cur);
    }
});

sf!(GetLocalVarInt(name: S) -> i32 {
    vars::get_int(&cur_empty_map_var(name.0))
});

sf!(GetLocalVarFloat(name: S) -> f32 {
    vars::get_float(&cur_empty_map_var(name.0))
});

sf!(GetLocalVarString(name: S) -> RStr {
    if !call(ops::MAP_EXISTS, &[]).as_b() {
        return temp(LOCAL_NULL, b"");
    }
    let v = call(ops::MAP_VAR, &[s(name.0)]).p;
    if v.is_null() {
        error(&format!("Couldn't find local var '{}'\n", lossy(name.0)));
        return temp(NULL_STR, b"");
    }
    RStr(v)
});

sf!(SetGlobalVarInt(name: S, val: i32) {
    if let Some(v) = global_var(name.0, "Global") {
        var_set(v, &vars::from_int(val));
    }
});

sf!(SetGlobalVarFloat(name: S, val: f32) {
    if let Some(v) = global_var(name.0, "Global") {
        var_set(v, &vars::from_float(val));
    }
});

sf!(SetGlobalVarString(name: S, val: S) {
    if let Some(v) = global_var(name.0, "Global") {
        var_set(v, val.0);
    }
});

sf!(AddGlobalVarInt(name: S, val: i32) {
    if let Some(v) = global_var(name.0, "Global") {
        var_set(v, &vars::add_int(var_text(v), val));
    }
});

sf!(AddGlobalVarFloat(name: S, val: f32) {
    if let Some(v) = global_var(name.0, "Global") {
        var_set(v, &vars::add_float(var_text(v), val));
    }
});

sf!(AddGlobalVarString(name: S, val: S) {
    if let Some(v) = global_var(name.0, "global") {
        let mut cur = var_text(v).to_vec();
        cur.extend_from_slice(val.0);
        var_set(v, &cur);
    }
});

sf!(GetGlobalVarInt(name: S) -> i32 {
    vars::get_int(&cur_global(name.0))
});

sf!(GetGlobalVarFloat(name: S) -> f32 {
    vars::get_float(&cur_global(name.0))
});

sf!(GetGlobalVarString(name: S) -> RStr {
    let v = call(ops::GLOBAL_VAR, &[s(name.0)]).p;
    if v.is_null() {
        error(&format!("Couldn't find global var '{}'\n", lossy(name.0)));
        return temp(NULL_STR, b"");
    }
    RStr(v)
});

sf!(StartCredits(music: S, looped: bool, cat: S, entry: S, end_num: i32) {
    call(ops::CREDITS_SETUP, &[s(music.0), Arg::b(looped), s(cat.0), s(entry.0), Arg::i(end_num)]);
    call(ops::UPDATER_CONTAINER, &[s(b"Credits")]);
});

sf!(AddKeyPart(part: i32) {
    call(ops::CREDITS_KEY_PART, &[Arg::i(part)]);
});

sf!(StartDemoEnd() {
    if call(ops::DEMO_END_EXISTS, &[]).as_b() {
        call(ops::UPDATER_CONTAINER, &[s(b"DemoEnd")]);
    }
});

sf!(AutoSave() {
    call(ops::AUTO_SAVE, &[]);
});

fn checkpoint(name: S, start: S, cb: S, cat: S, entry: S) {
    if call(ops::MAP_EXISTS, &[]).as_b() {
        call(ops::MAP_CHECKPOINT, &[s(name.0), s(start.0), s(cb.0)]);
    }
    call(ops::DEATH_HINT, &[s(cat.0), s(entry.0)]);
}

sf!(CheckPoint(name: S, start: S, cb: S, cat: S, entry: S) {
    checkpoint(name, start, cb, cat, entry);
});

sf!(CheckPointAmfp(name: S, start: S, cb: S, cat: S, entry: S, extra: bool) {
    let _ = extra;
    checkpoint(name, start, cb, cat, entry);
});

sf!(AmfpStub_GetEntityActive(name: S) -> bool {
    super::warning(&format!("AMFP stub: GetEntityActive('{}') not implemented, returning true\n", lossy(name.0)));
    true
});

sf!(AmfpStub_GetJournalDisabled() -> bool {
    super::warning("AMFP stub: GetJournalDisabled() not implemented\n");
    false
});

sf!(AmfpStub_GetPlayerInfection() -> f32 {
    super::warning("AMFP stub: GetPlayerInfection() not implemented\n");
    0.0
});

sf!(ChangeMap(map: S, start: S, start_sound: S, end_sound: S) {
    call(ops::CHANGE_MAP, &[s(map.0), s(start.0), s(start_sound.0), s(end_sound.0)]);
});

sf!(ClearSavedMaps() {
    call(ops::CLEAR_SAVED_MAPS, &[]);
});

sf!(CreateDataCache() {
    call(ops::DATA_CACHE, &[Arg::b(true)]);
});

sf!(DestroyDataCache() {
    call(ops::DATA_CACHE, &[Arg::b(false)]);
});

sf!(SetMapDisplayNameEntry(entry: S) {
    if call(ops::MAP_EXISTS, &[]).as_b() {
        call(ops::MAP_DISPLAY_NAME, &[s(entry.0)]);
    }
});

sf!(SetupLoadScreen(cat: S, entry: S, random: i32, image: S) {
    call(ops::LOAD_SCREEN, &[s(cat.0), s(entry.0), Arg::i(random), s(image.0)]);
});

const ACH_MASOCHIST: i32 = 11;

sf!(UnlockAchievement(name: S) {
    let ach = tables::achievement(name.0);
    if ach != tables::ACH_NONE {
        call(ops::ACHIEVEMENT, &[Arg::i(ach)]);
    }
    if call(ops::HARD_MODE, &[]).as_b() && tables::achievement_hardmode(ach) {
        call(ops::ACHIEVEMENT, &[Arg::i(ACH_MASOCHIST)]);
    }
});

sf!(PlayGuiSound(file: S, volume: f32) {
    if tables::sound_is_entity(file.0) {
        call(ops::GUI_SOUND_DATA, &[s(file.0), Arg::f(volume)]);
    } else if let Some(h) = handler() {
        h.play_gui(file.0, false, volume, false);
    }
});
