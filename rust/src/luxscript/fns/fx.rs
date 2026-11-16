//! Effects, voices, messages, hints, journal, inventory, insanity and music: the Rust-owned handlers.

use super::super::a1::Arg;
use super::super::tables;
use super::{Ent, S, TYPE_ANY, call, entity, ops, s, sf};
use crate::luxfx::{ffi12, fxffi};
use crate::luxui::event::Str;
use crate::luxui::{handlers_ffi, invffi, jrn};
use crate::math::IDENTITY;

const H_FX: i32 = 0;
const H_HINT: i32 = 1;
const H_MSG: i32 = 2;
const H_INSANITY: i32 = 3;
const H_MUSIC: i32 = 4;

const POST_RADIAL: i32 = 0;
const POST_SEPIA: i32 = 1;
const POST_TRAIL: i32 = 2;
const LOG_HIGH: i32 = 2;

fn handle(which: i32) -> u64 {
    call(ops::HANDLE, &[Arg::i(which)]).u
}

fn st(b: &[u8]) -> Str {
    Str {
        p: b.as_ptr(),
        n: b.len(),
    }
}

fn translated(cat: &[u8], entry: &[u8]) -> (*const u32, usize) {
    let r = call(ops::TRANSLATE, &[s(cat), s(entry)]);
    (r.p.cast(), r.n)
}

sf!(FadeIn(time: f32) {
    fxffi::ohpl_a12a_fx_fade(handle(H_FX), 0, time);
});

sf!(FadeOut(time: f32) {
    fxffi::ohpl_a12a_fx_fade(handle(H_FX), 1, time);
});

sf!(FadeImageTrailTo(amount: f32, speed: f32) {
    fxffi::ohpl_a12a_fx_fade_to(handle(H_FX), POST_TRAIL, amount, speed);
});

sf!(FadeSepiaColorTo(amount: f32, speed: f32) {
    fxffi::ohpl_a12a_fx_fade_to(handle(H_FX), POST_SEPIA, amount, speed);
});

sf!(FadeRadialBlurTo(size: f32, speed: f32) {
    fxffi::ohpl_a12a_fx_fade_to(handle(H_FX), POST_RADIAL, size, speed);
});

sf!(SetRadialBlurStartDist(dist: f32) {
    fxffi::ohpl_a12a_fx_radial_dist(handle(H_FX), dist);
});

// the original passes the fade-out time for both fades
sf!(StartEffectFlash(_fade_in: f32, white: f32, fade_out: f32) {
    fxffi::ohpl_a12a_fx_flash_start(handle(H_FX), fade_out, white, fade_out);
});

sf!(StartEffectEmotionFlash(cat: S, entry: S, sound: S) {
    // SAFETY: the three strings live for the call.
    unsafe {
        fxffi::ohpl_a12a_fx_emotion_start(
            handle(H_FX), cat.0.as_ptr(), cat.0.len(), entry.0.as_ptr(), entry.0.len(), sound.0.as_ptr(), sound.0.len(),
        );
    }
});

sf!(StopAllEffectVoices(fade_out: f32) {
    fxffi::ohpl_a12a_fx_voice(handle(H_FX), 0, tables::inverse(fade_out));
});

sf!(GetEffectVoiceActive() -> bool {
    fxffi::ohpl_a12a_fx_active(handle(H_FX), 3) != 0
});

sf!(SetEffectVoiceOverCallback(func: S) {
    // SAFETY: `func` lives for the call.
    unsafe { fxffi::ohpl_a12a_fx_voice_cb_set(handle(H_FX), func.0.as_ptr(), func.0.len()) };
});

sf!(StartScreenShake(amount: f32, time: f32, fade_in: f32, fade_out: f32) {
    fxffi::ohpl_a12a_fx_shake(handle(H_FX), amount, time, fade_in, fade_out);
});

fn voice_pos(use_pos: bool, ent: S) -> [f32; 3] {
    let mut pos = [0.0; 3];
    if use_pos
        && let Some(e) = entity(ent.0, TYPE_ANY, -1)
        && call(ops::ENT_BODY_NUM, &[e.a()]).as_i() > 0
    {
        for (i, v) in pos.iter_mut().enumerate() {
            *v = call(ops::ENT_BODY_POS, &[e.a(), Arg::i(i as i32)]).as_f();
        }
    }
    pos
}

#[allow(clippy::too_many_arguments)]
fn voice(
    voice: S,
    effect: S,
    cat: S,
    subs: &[(S, f32)],
    use_pos: bool,
    pos_ent: S,
    min: f32,
    max: f32,
    prio: i32,
    remove_interrupted: bool,
) {
    let pos = voice_pos(use_pos, pos_ent);
    let empty = st(b"");
    let mut entries = [empty; 7];
    let mut delays = [0.0f32; 7];
    for ((slot, delay), (e, d)) in entries.iter_mut().zip(delays.iter_mut()).zip(subs) {
        *slot = st(e.0);
        *delay = *d;
    }
    let v = fxffi::VoiceIn {
        voice: fxffi::Str {
            p: voice.0.as_ptr(),
            n: voice.0.len(),
        },
        effect: fxffi::Str {
            p: effect.0.as_ptr(),
            n: effect.0.len(),
        },
        cat: fxffi::Str {
            p: cat.0.as_ptr(),
            n: cat.0.len(),
        },
        entries: entries.map(|e| fxffi::Str { p: e.p, n: e.n }),
        delays,
        use_pos: i32::from(use_pos),
        pos,
        min,
        max,
        priority: prio,
        remove_interrupted: i32::from(remove_interrupted),
    };
    // SAFETY: every string outlives the call.
    unsafe { fxffi::ohpl_a12a_fx_voice_add(handle(H_FX), &raw const v) };
}

sf!(AddEffectVoice(vf: S, ef: S, cat: S, entry: S, use_pos: bool, pos_ent: S, min: f32, max: f32) {
    voice(vf, ef, cat, &[(entry, 0.0)], use_pos, pos_ent, min, max, -1, true);
});

sf!(AddEffectVoiceExt(vf: S, ef: S, cat: S, entry: S, use_pos: bool, pos_ent: S, min: f32, max: f32, prio: i32, stop: bool) {
    voice(vf, ef, cat, &[(entry, 0.0)], use_pos, pos_ent, min, max, prio, stop);
});

sf!(AddEffectVoice2(
    vf: S, ef: S, cat: S, e1: S, d1: f32, e2: S, d2: f32, use_pos: bool, pos_ent: S, min: f32, max: f32
) {
    voice(vf, ef, cat, &[(e1, d1), (e2, d2)], use_pos, pos_ent, min, max, -1, true);
});

sf!(AddEffectVoice3(
    vf: S, ef: S, cat: S, e1: S, d1: f32, e2: S, d2: f32, e3: S, d3: f32, use_pos: bool, pos_ent: S, min: f32,
    max: f32
) {
    voice(vf, ef, cat, &[(e1, d1), (e2, d2), (e3, d3)], use_pos, pos_ent, min, max, -1, true);
});

sf!(AddEffectVoice4(
    vf: S, ef: S, cat: S, e1: S, d1: f32, e2: S, d2: f32, e3: S, d3: f32, e4: S, d4: f32, use_pos: bool,
    pos_ent: S, min: f32, max: f32
) {
    voice(vf, ef, cat, &[(e1, d1), (e2, d2), (e3, d3), (e4, d4)], use_pos, pos_ent, min, max, -1, true);
});

sf!(AddEffectVoice5(
    vf: S, ef: S, cat: S, e1: S, d1: f32, e2: S, d2: f32, e3: S, d3: f32, e4: S, d4: f32, e5: S, d5: f32,
    use_pos: bool, pos_ent: S, min: f32, max: f32
) {
    voice(
        vf, ef, cat, &[(e1, d1), (e2, d2), (e3, d3), (e4, d4), (e5, d5)], use_pos, pos_ent, min, max, -1, true,
    );
});

sf!(AddEffectVoice6(
    vf: S, ef: S, cat: S, e1: S, d1: f32, e2: S, d2: f32, e3: S, d3: f32, e4: S, d4: f32, e5: S, d5: f32,
    e6: S, d6: f32, use_pos: bool, pos_ent: S, min: f32, max: f32
) {
    voice(
        vf, ef, cat, &[(e1, d1), (e2, d2), (e3, d3), (e4, d4), (e5, d5), (e6, d6)], use_pos, pos_ent, min, max,
        -1, true,
    );
});

sf!(AddEffectVoice7(
    vf: S, ef: S, cat: S, e1: S, d1: f32, e2: S, d2: f32, e3: S, d3: f32, e4: S, d4: f32, e5: S, d5: f32,
    e6: S, d6: f32, e7: S, d7: f32, use_pos: bool, pos_ent: S, min: f32, max: f32
) {
    voice(
        vf, ef, cat, &[(e1, d1), (e2, d2), (e3, d3), (e4, d4), (e5, d5), (e6, d6), (e7, d7)], use_pos, pos_ent,
        min, max, -1, true,
    );
});

sf!(SetMessage(cat: S, entry: S, time: f32) {
    let (p, n) = translated(cat.0, entry.0);
    // SAFETY: the host's translation buffer is valid until the next translate call.
    unsafe { handlers_ffi::ohpl_a11b_msg_set_message(handle(H_MSG), p, n, time) };
});

fn add_hint(name: &[u8], cat: &[u8], entry: &[u8], time: f32) {
    let (p, n) = translated(cat, entry);
    // SAFETY: as in `SetMessage`.
    unsafe {
        handlers_ffi::ohpl_a11b_hint_add(handle(H_HINT), name.as_ptr(), name.len(), p, n, time)
    };
}

sf!(GiveHint(name: S, cat: S, entry: S, time: f32) {
    add_hint(name.0, cat.0, entry.0, time);
});

sf!(RemoveHint(name: S) {
    // SAFETY: `name` lives for the call.
    unsafe { handlers_ffi::ohpl_a11b_hint_remove(handle(H_HINT), name.0.as_ptr(), name.0.len()) };
});

sf!(BlockHint(name: S) {
    // SAFETY: as above.
    unsafe { handlers_ffi::ohpl_a11b_hint_block(handle(H_HINT), name.0.as_ptr(), name.0.len(), 1) };
});

sf!(UnBlockHint(name: S) {
    // SAFETY: as above.
    unsafe { handlers_ffi::ohpl_a11b_hint_block(handle(H_HINT), name.0.as_ptr(), name.0.len(), 0) };
});

sf!(AddNote(entry: S, image: S) {
    jrn::add_note(entry.0, image.0);
});

sf!(AddDiary(entry: S, image: S) {
    jrn::add_diary(entry.0, image.0);
});

sf!(ReturnOpenJournal(open: bool) {
    invffi::ohpl_inv_set_show_journal(i32::from(open));
});

fn progress_log(text: &[u8]) {
    call(ops::PROG_LOG, &[Arg::i(LOG_HIGH), s(text)]);
}

fn joined(head: &str, name: &[u8]) -> Vec<u8> {
    [head.as_bytes(), name].concat()
}

sf!(AddQuest(name: S, entry: S) {
    if jrn::add_quest(name.0, entry.0) {
        progress_log(&joined("Added Memento ", name.0));
        add_hint(b"QuestAdded", b"Hints", b"QuestAdded", 0.0);
        handlers_ffi::ohpl_a11b_msg_start_quest(handle(H_MSG));
    }
});

sf!(CompleteQuest(name: S, entry: S) {
    jrn::add_quest(name.0, entry.0);
    if jrn::disable_quest(name.0) {
        progress_log(&joined("Completed Quest ", name.0));
        if call(ops::MAP_EXISTS, &[]).as_b() {
            call(ops::MAP_QUEST_COMPLETION, &[Arg::f(6.0)]);
        }
        let ach = tables::quest_achievement(name.0);
        if ach != tables::ACH_NONE {
            call(ops::ACHIEVEMENT, &[Arg::i(ach)]);
        }
    }
});

sf!(QuestIsCompleted(name: S) -> bool {
    jrn::quest_state(name.0) == 0
});

sf!(QuestIsAdded(name: S) -> bool {
    jrn::quest_state(name.0) >= 0
});

sf!(SetNumberOfQuestsInMap(n: i32) {
    if call(ops::MAP_EXISTS, &[]).as_b() {
        call(ops::MAP_SET_QUESTS, &[Arg::i(n)]);
    }
});

sf!(ExitInventory() {
    if call(ops::INPUT_IN_INVENTORY, &[]).as_b() {
        invffi::ohpl_inv_exit_pressed();
    }
});

sf!(SetInventoryDisabled(off: bool) {
    invffi::ohpl_inv_set_disabled(i32::from(off));
});

sf!(SetInventoryMessage(cat: S, entry: S, time: f32) {
    let (p, n) = translated(cat.0, entry.0);
    // SAFETY: as in `SetMessage`.
    unsafe { invffi::ohpl_inv_set_message(p, n, time) };
});

sf!(GiveItem(name: S, ty: S, sub: S, image: S, amount: f32) {
    let t = invffi::ohpl_inv_type_from_string(st(ty.0));
    // SAFETY: the null `remove` flag is allowed.
    unsafe {
        invffi::ohpl_inv_add_item(st(name.0), t, st(sub.0), st(image.0), amount, st(b""), st(b""), std::ptr::null_mut());
    }
});

sf!(GiveItemFromFile(name: S, file: S) {
    if !call(ops::MAP_EXISTS, &[]).as_b() {
        return;
    }
    let identity = IDENTITY;
    let e = Ent(call(ops::MAP_CREATE_ENTITY, &[s(name.0), s(file.0), Arg::ptr(std::ptr::from_ref(&identity).cast())]).p);
    if e.0.is_null() {
        return;
    }
    if call(ops::ENT_IS_TYPE, &[e.a(), Arg::i(super::TYPE_PROP), Arg::i(ITEM_PROP)]).as_b() {
        let ty = call(ops::ITEM_FIELD, &[e.a(), Arg::i(0)]).as_i();
        let sub = call(ops::ITEM_FIELD, &[e.a(), Arg::i(1)]);
        let image = call(ops::ITEM_FIELD, &[e.a(), Arg::i(2)]);
        let amount = call(ops::ITEM_FIELD, &[e.a(), Arg::i(3)]).as_f();
        // SAFETY: the host strings live until the next host call; the null flag is allowed.
        unsafe {
            invffi::ohpl_inv_add_item(
                st(name.0), ty, st(super::raw_bytes(sub.p)), st(super::raw_bytes(image.p)), amount, st(b""), st(b""),
                std::ptr::null_mut(),
            );
        }
    }
    call(ops::MAP_DESTROY_ENTITY, &[e.a()]);
});

const ITEM_PROP: i32 = 7;

sf!(RemoveItem(name: S) {
    invffi::ohpl_inv_remove_by_name(st(name.0));
});

sf!(HasItem(name: S) -> bool {
    invffi::ohpl_inv_get_by_name(st(name.0)) != 0
});

sf!(AddCombineCallback(name: S, a: S, b: S, func: S, auto_destroy: bool) {
    invffi::ohpl_inv_add_combine(st(name.0), st(a.0), st(b.0), st(func.0), i32::from(auto_destroy));
});

sf!(RemoveCombineCallback(name: S) {
    invffi::ohpl_inv_remove_combine(st(name.0));
});

sf!(AddUseItemCallback(name: S, item: S, ent: S, func: S, auto_destroy: bool) {
    if call(ops::MAP_EXISTS, &[]).as_b() {
        call(ops::MAP_USE_ITEM_ADD, &[s(name.0), s(item.0), s(ent.0), s(func.0), Arg::b(auto_destroy)]);
    }
});

sf!(RemoveUseItemCallback(name: S) {
    if call(ops::MAP_EXISTS, &[]).as_b() {
        call(ops::MAP_USE_ITEM_REMOVE, &[s(name.0)]);
    }
});

sf!(SetInsanitySetEnabled(set: S, on: bool) {
    // SAFETY: `set` lives for the call.
    unsafe { ffi12::ohpl_a12b_ins_set(handle(H_INSANITY), i32::from(on), set.0.as_ptr(), set.0.len()) };
});

sf!(StartRandomInsanityEvent() {
    // SAFETY: the null name has length 0.
    unsafe { ffi12::ohpl_a12b_ins_start(handle(H_INSANITY), 0, 0, std::ptr::null(), 0) };
});

sf!(StartInsanityEvent(name: S) {
    // SAFETY: `name` lives for the call.
    unsafe { ffi12::ohpl_a12b_ins_start(handle(H_INSANITY), 2, 0, name.0.as_ptr(), name.0.len()) };
});

sf!(StopCurrentInsanityEvent() {
    ffi12::ohpl_a12b_ins_stop(handle(H_INSANITY));
});

sf!(InsanityEventIsActive() -> bool {
    ffi12::ohpl_a12b_ins_current(handle(H_INSANITY)) >= 0
});

sf!(PlayMusic(file: S, looped: bool, volume: f32, fade: f32, prio: i32, resume: bool) {
    // SAFETY: `file` lives for the call.
    unsafe {
        ffi12::ohpl_a12b_music_play(
            handle(H_MUSIC), file.0.as_ptr(), file.0.len(), i32::from(looped), volume, fade, prio,
            i32::from(resume), 0,
        );
    }
});

sf!(StopMusic(fade: f32, prio: i32) {
    ffi12::ohpl_a12b_music_stop(handle(H_MUSIC), fade, prio);
});
