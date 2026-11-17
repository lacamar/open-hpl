//! `cSomaSoundEvents::RegisterNatives`: sound entries, sound entities, world creation, GUI plays,
//! global volumes and music.

use super::game::game;
use super::host::{Engine, ops};
use super::{glue, music};
use crate::somascript::s2_api::{active_native_decl, is_dummy, release_handle_args};
use crate::somascript::s2_as::{self, Frame, Soma, str_bytes};
use crate::sound::own::{ent_fade_in, ent_fade_out, entity_of, fade_main_in, handler};
use crate::sound::own_ffi::music;
use std::ffi::c_void;

const ENTRY_WORLD: i32 = 1;

fn bytes<'a>(p: *mut c_void) -> &'a [u8] {
    // SAFETY: the declarations below pass `const tString &in`.
    unsafe { str_bytes(p) }
}

fn pos_of(fr: &Frame, i: usize) -> [f32; 3] {
    // SAFETY: a `const cVector3f &in` is three floats.
    unsafe { fr.ptr_arg(i).cast::<[f32; 3]>().read_unaligned() }
}

/// `Live(p)`: the instance handle behind a shell the script still holds.
fn live(p: usize) -> Option<usize> {
    game().shell_of(p).map(|s| s.h)
}

fn shell_string(shell: usize, which: u32, set: &[u8]) -> *mut c_void {
    ops()
        .map_or(std::ptr::null(), |o| {
            // SAFETY: `set` is valid for the call.
            unsafe { (o.shell_string)(shell, which, set.as_ptr(), set.len()) }
        })
        .cast_mut()
}

fn with_inst<R>(p: usize, default: R, f: impl FnOnce(usize) -> R) -> R {
    match live(p) {
        Some(h) => f(h),
        None => default,
    }
}

fn param_def(h: usize, idx: i32) -> Option<(f32, f32, Vec<u8>)> {
    let g = game();
    let ev = g
        .bank
        .inst(h)
        .and_then(|i| g.bank.events.list.get(i.st.event))?;
    let p = ev.params.get(usize::try_from(idx).ok()?)?;
    Some((p.min, p.max, p.name.clone()))
}

pub(super) fn param_count(h: usize) -> i32 {
    game()
        .bank
        .inst(h)
        .map_or(0, |i| i.param_value.len() as i32)
}

pub fn inst_state<R>(h: usize, default: R, f: impl FnOnce(&super::instance::State) -> R) -> R {
    game().state(h).map_or(default, f)
}

fn event_info<R>(h: usize, default: R, f: impl FnOnce(&super::events::Event) -> R) -> R {
    let g = game();
    g.bank
        .inst(h)
        .and_then(|i| g.bank.events.list.get(i.st.event))
        .map_or(default, f)
}

fn entity_event(o: usize) -> usize {
    if o == 0 {
        return 0;
    }
    // SAFETY: `o` is a live sound entity handed out by the script engine.
    unsafe { entity_of(o).as_ref() }.map_or(0, |e| e.event() as usize)
}

fn fade_in_to(h: usize, mul: f32, speed: f32) {
    game().bank.fade_in_to(&mut Engine, h, mul, speed);
}

fn set_volume_mul(h: usize, v: f32) {
    if let Some(i) = game().bank.inst_mut(h) {
        i.st.volume_mul = v;
        i.st.volume_mul_dest = v;
    }
}

fn set_speed_mul(h: usize, v: f32) {
    if let Some(i) = game().bank.inst_mut(h) {
        i.st.speed_mul = v;
        i.st.speed_mul_dest = v;
    }
}

fn set_param(h: usize, idx: i32, v: f32) {
    game().bank.set_param(h, idx, v);
}

pub(super) fn param_value(h: usize, idx: i32) -> f32 {
    usize::try_from(idx)
        .ok()
        .and_then(|k| {
            game()
                .bank
                .inst(h)
                .and_then(|i| i.param_value.get(k).copied())
        })
        .unwrap_or(0.0)
}

pub fn register_entries(s: &Soma) {
    let t = "cSoundEntry";
    s.method(t, "const tString& GetName()", |fr| {
        let p = fr.this as usize;
        fr.ret_ref(if live(p).is_some() {
            shell_string(p, 0, b"")
        } else {
            shell_string(0, 2, b"")
        });
    });
    s.method(t, "eSoundEntryType GetType()", |fr| {
        fr.ret::<i32>(with_inst(fr.this as usize, 0, |h| {
            inst_state(h, 0, |s| s.entry_type)
        }));
    });
    s.method(t, "int GetId()", |fr| {
        fr.ret::<i32>(with_inst(fr.this as usize, -1, |h| {
            inst_state(h, 0, |s| s.id)
        }));
    });
    s.method(t, "bool IsFirstTime()", |fr| {
        fr.ret::<bool>(with_inst(fr.this as usize, false, |h| {
            inst_state(h, false, |s| s.time == 0.0)
        }));
    });
    s.method(t, "void SetPosition(const cVector3f&in avPosition)", |fr| {
        let p = fr.this as usize;
        let v = pos_of(fr, 0);
        if let Some(h) = live(p) {
            glue::set_position(p, h, v);
        }
    });
    s.method(t, "const cVector3f& GetPosition()", |fr| {
        let p = fr.this as usize;
        let shell = if live(p).is_some() { p } else { 0 };
        fr.ret_ref(
            ops()
                .map_or(std::ptr::null(), |o| {
                    // SAFETY: plain call.
                    unsafe { (o.shell_position_ref)(shell) }
                })
                .cast_mut(),
        );
    });
    s.method(t, "bool IsPlaying()", |fr| {
        fr.ret::<bool>(with_inst(fr.this as usize, false, |h| {
            inst_state(h, false, |s| !s.stopped)
        }));
    });
    s.method(t, "float GetElapsedTime()", |fr| {
        fr.ret::<f32>(with_inst(fr.this as usize, 0.0, |h| {
            inst_state(h, 0.0, |s| s.time)
        }));
    });
    s.method(t, "float GetTotalTime()", |fr| {
        fr.ret::<f32>(with_inst(fr.this as usize, 0.0, super::game::total_time));
    });
    s.method(t, "float GetAudibility()", |fr| {
        fr.ret::<f32>(with_inst(fr.this as usize, 0.0, |h| {
            inst_state(h, 0.0, |s| s.audibility)
        }));
    });
    s.method(
        t,
        "void SetParam(const tString &in asName, float afValue)",
        |fr| {
            let (name, v) = (bytes(fr.ptr_arg(0)).to_vec(), fr.arg::<f32>(1));
            if let Some(h) = live(fr.this as usize) {
                let idx = game().bank.param_index(h, &name);
                set_param(h, idx, v);
            }
        },
    );
    s.method(t, "void SetParam(int alIdx, float afValue)", |fr| {
        let (i, v) = (fr.arg::<i32>(0), fr.arg::<f32>(1));
        if let Some(h) = live(fr.this as usize) {
            set_param(h, i, v);
        }
    });
    s.method(t, "int GetParamNum()", |fr| {
        fr.ret::<i32>(with_inst(fr.this as usize, 0, param_count))
    });
    s.method(t, "float GetParamValue(int alIdx)", |fr| {
        let i = fr.arg::<i32>(0);
        fr.ret::<f32>(with_inst(fr.this as usize, 0.0, |h| param_value(h, i)));
    });
    s.method(t, "float GetParamMin(int alIdx)", |fr| {
        let i = fr.arg::<i32>(0);
        fr.ret::<f32>(with_inst(fr.this as usize, 0.0, |h| {
            param_def(h, i).map_or(0.0, |d| d.0)
        }));
    });
    s.method(t, "float GetParamMax(int alIdx)", |fr| {
        let i = fr.arg::<i32>(0);
        fr.ret::<f32>(with_inst(fr.this as usize, 0.0, |h| {
            param_def(h, i).map_or(0.0, |d| d.1)
        }));
    });
    s.method(t, "const tString& GetParamName(int alIdx)", |fr| {
        let (p, i) = (fr.this as usize, fr.arg::<i32>(0));
        let def = live(p).and_then(|h| param_def(h, i));
        fr.ret_ref(match def {
            Some(d) => shell_string(p, 1, &d.2),
            None => shell_string(0, 2, b""),
        });
    });
    s.method(t, "void SetPaused(bool abX)", |fr| {
        let x = fr.arg::<bool>(0);
        if let Some(h) = live(fr.this as usize) {
            game().bank.set_paused(&mut Engine, h, x);
        }
    });
    s.method(t, "bool GetPaused()", |fr| {
        fr.ret::<bool>(with_inst(fr.this as usize, false, |h| {
            inst_state(h, false, |s| s.paused)
        }));
    });
    s.method(t, "void SetVolume(float afX)", |fr| {
        let x = fr.arg::<f32>(0);
        if let Some(i) = live(fr.this as usize).and_then(|h| game().bank.inst_mut(h)) {
            i.st.volume = x;
        }
    });
    s.method(t, "float GetVolume()", |fr| {
        fr.ret::<f32>(with_inst(fr.this as usize, 0.0, |h| {
            inst_state(h, 0.0, |s| s.volume)
        }));
    });
    s.method(t, "void SetSpeed(float afX)", |fr| {
        let x = fr.arg::<f32>(0);
        if let Some(h) = live(fr.this as usize) {
            set_speed_mul(h, x);
        }
    });
    s.method(t, "float GetSpeed()", |fr| {
        fr.ret::<f32>(with_inst(fr.this as usize, 1.0, |h| {
            inst_state(h, 1.0, |s| s.speed_mul)
        }));
    });
    s.method(t, "float GetVolumeMul()", |fr| {
        fr.ret::<f32>(with_inst(fr.this as usize, 0.0, |h| {
            inst_state(h, 0.0, |s| s.volume_mul)
        }));
    });
    s.method(t, "float GetSpeedMul()", |fr| {
        fr.ret::<f32>(with_inst(fr.this as usize, 1.0, |h| {
            inst_state(h, 1.0, |s| s.speed_mul)
        }));
    });
    s.method(t, "float GetMinDistance()", |fr| {
        fr.ret::<f32>(with_inst(fr.this as usize, 0.0, |h| {
            event_info(h, 0.0, |e| e.min_dist)
        }));
    });
    s.method(t, "float GetMaxDistance()", |fr| {
        fr.ret::<f32>(with_inst(fr.this as usize, 0.0, |h| {
            event_info(h, 0.0, |e| e.max_dist)
        }));
    });
    s.method(t, "bool Is3D()", |fr| {
        fr.ret::<bool>(with_inst(fr.this as usize, false, |h| {
            event_info(h, false, |e| e.is_3d)
        }));
    });
    s.method(t, "bool IsOneShot()", |fr| {
        fr.ret::<bool>(with_inst(fr.this as usize, false, |h| {
            event_info(h, false, |e| e.one_shot)
        }));
    });
    s.method(t, "bool IsVirtual()", |fr| {
        fr.ret::<bool>(with_inst(fr.this as usize, false, |h| {
            inst_state(h, false, |s| s.audibility < 1e-4)
        }));
    });
    s.method(t, "void Stop(bool abPlayEnd)", |fr| {
        if let Some(h) = live(fr.this as usize) {
            game().bank.stop(&mut Engine, h);
        }
    });
    s.method(t, "void SetVolumeMul(float afMul)", |fr| {
        let x = fr.arg::<f32>(0);
        if let Some(h) = live(fr.this as usize) {
            set_volume_mul(h, x);
        }
    });
    s.method(t, "void SetSpeedMul(float afMul)", |fr| {
        let x = fr.arg::<f32>(0);
        if let Some(h) = live(fr.this as usize) {
            set_speed_mul(h, x);
        }
    });
    s.method(
        t,
        "void FadeVolumeMulTo(float afDestMul, float afSpeed)",
        |fr| {
            let (d, sp) = (fr.arg::<f32>(0), fr.arg::<f32>(1));
            if let Some(h) = live(fr.this as usize) {
                game().bank.fade_volume_mul_to(h, d, sp);
            }
        },
    );
    s.method(
        t,
        "void FadeSpeedMulTo(float afDestMul, float afSpeed)",
        |fr| {
            let (d, sp) = (fr.arg::<f32>(0), fr.arg::<f32>(1));
            if let Some(h) = live(fr.this as usize) {
                game().bank.fade_speed_mul_to(h, d, sp);
            }
        },
    );
    s.method(t, "void FadeOut(float afSpeed)", |fr| {
        let sp = fr.arg::<f32>(0);
        if let Some(h) = live(fr.this as usize) {
            game().bank.fade_out(&mut Engine, h, sp);
        }
    });
    s.method(t, "void FadeIn(float afVolumeMul,float afSpeed)", |fr| {
        let (v, sp) = (fr.arg::<f32>(0), fr.arg::<f32>(1));
        if let Some(h) = live(fr.this as usize) {
            fade_in_to(h, v, sp);
        }
    });
    s.method(t, "void SetStopDisabled(bool abX)", |fr| {
        let x = fr.arg::<bool>(0);
        if let Some(i) = live(fr.this as usize).and_then(|h| game().bank.inst_mut(h)) {
            i.st.stop_disabled = x;
        }
    });
    s.method(t, "bool GetStopDisabled()", |fr| {
        fr.ret::<bool>(with_inst(fr.this as usize, false, |h| {
            inst_state(h, false, |s| s.stop_disabled)
        }));
    });
}

unsafe extern "C" fn is_valid(g: *mut c_void) {
    // SAFETY: the engine passes the generic frame of `cSound_IsValid`.
    unsafe {
        let p = *s2_as::ohpl_as2_g_arg_addr(g, 0).cast::<*mut c_void>();
        let id = *s2_as::ohpl_as2_g_arg_addr(g, 1).cast::<i32>();
        let p = if is_dummy(p) { 0 } else { p as usize };
        let ok =
            game().live(p, id) && inst_state(live(p).unwrap_or(usize::MAX), false, |s| !s.stopped);
        s2_as::ohpl_as2_g_ret_loc(g).cast::<bool>().write(ok);
        release_handle_args(g);
    }
}

unsafe extern "C" fn play_gui_data_ex(g: *mut c_void) {
    // SAFETY: the engine passes the generic frame of `cLux_PlayGuiSoundDataEx`.
    unsafe {
        let name = str_bytes(*s2_as::ohpl_as2_g_arg_addr(g, 0).cast::<*const c_void>());
        let ty = *s2_as::ohpl_as2_g_arg_addr(g, 1).cast::<i32>();
        let vol = *s2_as::ohpl_as2_g_arg_addr(g, 2).cast::<f32>();
        let extra = *s2_as::ohpl_as2_g_arg_addr(g, 4).cast::<*mut u8>();
        let extra = if is_dummy(extra.cast()) {
            std::ptr::null_mut()
        } else {
            extra
        };
        let shell = glue::play_gui(name, vol, ty, false, false);
        if !extra.is_null() {
            extra.add(32).cast::<usize>().write_unaligned(shell);
        }
        s2_as::ohpl_as2_g_ret_loc(g)
            .cast::<bool>()
            .write(shell != 0);
        release_handle_args(g);
    }
}

fn entity_ops(s: &Soma) {
    let t = "cSoundEntity";
    s.method(t, "void FadeIn(float afSpeed, float afTargetVol)", |fr| {
        let (o, speed, vol) = (fr.this as usize, fr.arg::<f32>(0), fr.arg::<f32>(1));
        let e = entity_of(o);
        if e.is_null() {
            return;
        }
        let shell = entity_event(o);
        if shell != 0 {
            // SAFETY: `e` is the live record of the sound entity.
            unsafe { ent_fade_in(e, speed) };
            let Some(ih) = live(shell) else { return };
            fade_in_to(ih, 1.0, speed);
            if speed > 0.0 {
                set_volume_mul(ih, 0.0);
                game().bank.fade_volume_mul_to(ih, vol, speed);
            } else {
                set_volume_mul(ih, vol);
            }
            return;
        }
        // SAFETY: `e` is the live record of the sound entity.
        unsafe { fade_main_in(e, vol, speed) };
    });
    s.method(t, "void FadeOut(float afSpeed)", |fr| {
        let (o, speed) = (fr.this as usize, fr.arg::<f32>(0));
        let e = entity_of(o);
        if !e.is_null() {
            // SAFETY: `e` is the live record of the sound entity.
            unsafe { ent_fade_out(e, speed) };
        }
    });
    s.method(t, "bool IsOneShot()", |fr| {
        let o = fr.this as usize;
        let shell = entity_event(o);
        let r = match live(shell) {
            Some(h) => event_info(h, false, |e| e.one_shot),
            // SAFETY: `o` is a live sound entity.
            None => unsafe { entity_of(o).as_ref() }.is_none_or(|e| !e.data().looping),
        };
        fr.ret::<bool>(r);
    });
    s.method(t, "void SetParam(int alIdx, float afValue)", |fr| {
        let (i, v) = (fr.arg::<i32>(0), fr.arg::<f32>(1));
        if let Some(h) = live(entity_event(fr.this as usize)) {
            set_param(h, i, v);
        }
    });
    s.method(
        t,
        "void SetParam(const tString&in asName, float afValue)",
        |fr| {
            let (name, v) = (bytes(fr.ptr_arg(0)).to_vec(), fr.arg::<f32>(1));
            if let Some(h) = live(entity_event(fr.this as usize)) {
                let idx = game().bank.param_index(h, &name);
                set_param(h, idx, v);
            }
        },
    );
    s.method(t, "float GetParam(int alIdx)", |fr| {
        let i = fr.arg::<i32>(0);
        fr.ret::<f32>(live(entity_event(fr.this as usize)).map_or(0.0, |h| param_value(h, i)));
    });
    s.method(t, "float GetParam(const tString&in asName)", |fr| {
        let name = bytes(fr.ptr_arg(0)).to_vec();
        fr.ret::<f32>(live(entity_event(fr.this as usize)).map_or(0.0, |h| {
            let idx = game().bank.param_index(h, &name);
            param_value(h, idx)
        }));
    });
    s.method(
        t,
        "cSoundEntry@ GetSoundEntry(bool abCheckEntryValidity)",
        |fr| {
            fr.ret_ref(entity_event(fr.this as usize) as *mut c_void);
        },
    );
    s.method(t, "void FadeVolumeMul(float afDest, float afSpeed)", |fr| {
        let (d, sp) = (fr.arg::<f32>(0), fr.arg::<f32>(1));
        if let Some(h) = live(entity_event(fr.this as usize)) {
            game().bank.fade_volume_mul_to(h, d, sp);
        }
    });
    s.method(t, "void FadeSpeedMul(float afDest, float afSpeed)", |fr| {
        let (d, sp) = (fr.arg::<f32>(0), fr.arg::<f32>(1));
        if let Some(h) = live(entity_event(fr.this as usize)) {
            game().bank.fade_speed_mul_to(h, d, sp);
        }
    });
    s.method(t, "float GetElapsedTime()", |fr| {
        fr.ret::<f32>(
            live(entity_event(fr.this as usize)).map_or(0.0, |h| inst_state(h, 0.0, |s| s.time)),
        );
    });
    for (decl, op) in [
        ("void SetCustomMinDistance(float afX)", 0u32),
        ("void SetCustomMaxDistance(float afX)", 1),
    ] {
        s.method(t, decl, move |fr| {
            let (o, x) = (fr.this as usize, fr.arg::<f32>(0));
            // SAFETY: `o` is a live sound entity.
            if let Some(e) = unsafe { entity_of(o).as_mut() } {
                if op == 0 {
                    e.min_distance = x;
                } else {
                    e.max_distance = x;
                }
            }
        });
    }
    for (decl, op) in [
        ("float GetCustomMinDistance()", 2u32),
        ("float GetCustomMaxDistance()", 3),
    ] {
        s.method(t, decl, move |fr| {
            let o = fr.this as usize;
            // SAFETY: `o` is a live sound entity.
            fr.ret::<f32>(unsafe { entity_of(o).as_ref() }.map_or(0.0, |e| {
                if op == 2 {
                    e.min_distance
                } else {
                    e.max_distance
                }
            }));
        });
    }
    s.method(t, "void SetUseCustomProperties(bool abX)", |fr| {
        let (o, x) = (fr.this as usize, fr.arg::<bool>(0));
        // SAFETY: `o` is a live sound entity.
        if let (false, Some(e)) = (x, unsafe { entity_of(o).as_mut() }) {
            e.reset_props();
        }
    });
}

fn world_ops(s: &Soma) {
    let create = |fr: &Frame| -> usize {
        let (w, name, file, remove) = (
            fr.this as usize,
            bytes(fr.ptr_arg(0)),
            bytes(fr.ptr_arg(1)),
            fr.arg::<bool>(2),
        );
        if file.is_empty() {
            return 0;
        }
        ops().map_or(0, |h| {
            // SAFETY: `w` is the script's world; the strings are valid for the call.
            unsafe {
                (h.world_create_sound)(
                    w,
                    name.as_ptr(),
                    name.len(),
                    file.as_ptr(),
                    file.len(),
                    remove,
                )
            }
        })
    };
    let id_of = |ent: usize, fr: &Frame| {
        let mut id = [0u8; 12];
        if let Some(h) = ops() {
            // SAFETY: `id` holds a `tID`.
            unsafe { (h.object_id)(ent, id.as_mut_ptr()) };
        }
        fr.ret::<[u8; 12]>(id);
    };
    s.method("cWorld", "cSoundEntity@ CreateSoundEntity(const tString&in asName, const tString&in asSoundEntity, bool abRemoveWhenOver)", move |fr| {
        fr.ret_ref(create(fr) as *mut c_void);
    });
    s.method("cWorld", "cSoundEntity@ CreateSoundEntityEx(const tString &in asName,const tString &in asSoundDataFile, bool abRemoveWhenOver, bool abNonBlockLoad)", move |fr| {
        fr.ret_ref(create(fr) as *mut c_void);
    });
    s.method("cWorld", "tID CreateSoundEntityID(const tString &in asName,const tString &in asSoundDataFile, bool abRemoveWhenOver)", move |fr| {
        id_of(create(fr), fr);
    });
    s.method(
        "cWorld",
        "cSoundEntity@ GetSoundEntityFromCreationID(int alID)",
        |fr| {
            let (w, id) = (fr.this as usize, fr.arg::<i32>(0));
            fr.ret_ref(ops().map_or(0, |h| {
                // SAFETY: `w` is the script's world.
                unsafe { (h.world_sound_by_creation)(w, id) }
            }) as *mut c_void);
        },
    );
    s.method("cWorld", "tID CreateSoundEntityExID(const tString &in asName,const tString &in asSoundDataFile, bool abRemoveWhenOver, bool abNonBlockLoad)", move |fr| {
        id_of(create(fr), fr);
    });
}

fn global_ops(s: &Soma) {
    s.func("bool cLux_PlayGuiSoundData(const tString&in asName, eSoundEntryType aDestType, float afVolMul, bool abSkipPreviousRandom)", |fr| {
        let (n, ty, v) = (bytes(fr.ptr_arg(0)).to_vec(), fr.arg::<i32>(1), fr.arg::<f32>(2));
        fr.ret::<bool>(glue::play_gui(&n, v, ty, false, false) != 0);
    });
    let d = active_native_decl(None, b"bool cLux_PlayGuiSoundDataEx(const tString&in asName, eSoundEntryType aDestType, float afVolMul, bool abSkipPreviousRandom, cLuxSoundExtraData @apExtraData)");
    s.0.reg_func_raw(&d, play_gui_data_ex as *const c_void);
    s.func(
        "void cSound_FadeMusicVolumeMul(float afDest, float afSpeed)",
        |fr| {
            let (d, sp) = (fr.arg::<f32>(0), fr.arg::<f32>(1));
            if let Some(m) = music() {
                m.fade_volume_mul(d, sp);
            }
        },
    );
    s.func("float cSound_GetMusicVolumeMul()", |fr| {
        fr.ret::<f32>(music().map_or(1.0, |m| m.volume_mul()));
    });
}

fn play_ops(s: &Soma) {
    s.func("cSoundEntry@ cSound_PlayGui(const tString&in asName, bool abLoop, float afVolume, const cVector3f&in avPos, eSoundEntryType aEntryType)", |fr| {
        let (n, l, v, ty) = (bytes(fr.ptr_arg(0)).to_vec(), fr.arg::<bool>(1), fr.arg::<f32>(2), fr.arg::<i32>(4));
        fr.ret_ref(glue::play_gui(&n, v, ty, l, false) as *mut c_void);
    });
    s.func("cSoundEntry@ cSound_PlayGuiStream(const tString&in asFileName, bool abLoop, float afVolume, const cVector3f&in avPos, eSoundEntryType aEntryType)", |fr| {
        let (n, l, v, ty) = (bytes(fr.ptr_arg(0)).to_vec(), fr.arg::<bool>(1), fr.arg::<f32>(2), fr.arg::<i32>(4));
        fr.ret_ref(glue::play_gui(&n, v, ty, l, true) as *mut c_void);
    });
    s.func("cSoundEntry@ cSound_PlaySoundEntityGui(const tString&in asName,bool abLoop,float afVolume, eSoundEntryType aEntryType, const cVector3f&in avPos)", |fr| {
        let (n, l, v, ty) = (bytes(fr.ptr_arg(0)).to_vec(), fr.arg::<bool>(1), fr.arg::<f32>(2), fr.arg::<i32>(3));
        fr.ret_ref(glue::play_gui(&n, v, ty, l, false) as *mut c_void);
    });
    s.func("cSoundEntry@ cSound_PlaySoundEvent(const tString&in asInternalPath,float afVolume,const cVector3f&in avPos,const cVector3f&in avOrientation, bool abNonBlockLoad)", |fr| {
        let (n, v, pos) = (bytes(fr.ptr_arg(0)).to_vec(), fr.arg::<f32>(1), pos_of(fr, 2));
        fr.ret_ref(glue::play_3d(&n, v, pos, ENTRY_WORLD) as *mut c_void);
    });
    s.func("cSoundEntry@ cSound_Play3D(const tString&in asName,bool abLoop,float afVolume,const cVector3f&in avPos, float afMinDist,float afMaxDist, eSoundEntryType aEntryType, bool abRelative, int alPriority, bool abOnlyPlayInside, bool abNonBlockLoad)", |fr| {
        let (n, l, v, pos, ty) = (bytes(fr.ptr_arg(0)).to_vec(), fr.arg::<bool>(1), fr.arg::<f32>(2), pos_of(fr, 3), fr.arg::<i32>(6));
        let p = match glue::play_3d(&n, v, pos, ty) {
            0 => glue::play_gui(&n, v, ty, l, false),
            p => p,
        };
        fr.ret_ref(p as *mut c_void);
    });
    s.func(
        "void cSound_PreloadProject(const tString&in asName, bool abNonBlockingLoad)",
        |fr| {
            glue::load_project(bytes(fr.ptr_arg(0)));
        },
    );
    s.func("void cSound_PreloadGroup(const tString&in asInternalPath, bool abNonBlockingLoad, bool abSubGroups)", |_| {});
}

fn volume_ops(s: &Soma) {
    s.func(
        "int cSound_SetGlobalVolume(float afVolume, uint aAffectedTypes, int alId)",
        |fr| {
            let (v, t, id) = (fr.arg::<f32>(0), fr.arg::<u32>(1), fr.arg::<i32>(2));
            fr.ret::<i32>(handler().map_or(0, |h| h.set_global(0, v, t, id)));
        },
    );
    s.func("float cSound_GetGlobalVolumeFromId(int alId)", |fr| {
        let id = fr.arg::<i32>(0);
        fr.ret::<f32>(handler().map_or(1.0, |h| h.setting_val(0, id)));
    });
    s.func("int cSound_FadeGlobalVolume(float afDestVolume, float afSpeed, uint aAffectedTypes, int alId, bool abDestroyIdAtDest)", |fr| {
        let (v, sp, t, id, d) = (fr.arg::<f32>(0), fr.arg::<f32>(1), fr.arg::<u32>(2), fr.arg::<i32>(3), fr.arg::<bool>(4));
        fr.ret::<i32>(handler().map_or(0, |h| h.fade_global(0, v, sp, t, id, d)));
    });
    s.func(
        "int cSound_SetGlobalSpeed(float afSpeed, uint aAffectedTypes, int alId)",
        |fr| {
            let (v, t, id) = (fr.arg::<f32>(0), fr.arg::<u32>(1), fr.arg::<i32>(2));
            fr.ret::<i32>(handler().map_or(0, |h| h.set_global(1, v, t, id)));
        },
    );
    s.func("float cSound_GetGlobalSpeedFromId(int alId)", |fr| {
        let id = fr.arg::<i32>(0);
        fr.ret::<f32>(handler().map_or(1.0, |h| h.setting_val(1, id)));
    });
    s.func("int cSound_FadeGlobalSpeed(float afDestSpeed, float afSpeed, uint aAffectedTypes, int alId, bool abDestroyIdAtDest)", |fr| {
        let (v, sp, t, id, d) = (fr.arg::<f32>(0), fr.arg::<f32>(1), fr.arg::<u32>(2), fr.arg::<i32>(3), fr.arg::<bool>(4));
        fr.ret::<i32>(handler().map_or(0, |h| h.fade_global(1, v, sp, t, id, d)));
    });
    s.func(
        "void cSound_FadeOutAll(uint aTypes, float afFadeSpeed, bool abDisableStop)",
        |fr| {
            let (t, sp) = (fr.arg::<u32>(0), fr.arg::<f32>(1));
            for h in super::game::fade_out_all(t) {
                game().bank.fade_out(&mut Engine, h, sp);
            }
            if let Some(h) = handler() {
                h.fade_out_all(t, sp, false);
            }
        },
    );
}

/// `cSomaSoundEvents::RegisterNatives`, in the original order.
pub fn register(engine: *mut c_void) {
    let s = Soma(s2_as::Engine(engine));
    if let Some(h) = ops() {
        // SAFETY: plain call.
        unsafe { (h.install_resolver)() };
    }
    world_ops(&s);
    register_entries(&s);
    let d = active_native_decl(None, b"bool cSound_IsValid(cSoundEntry @apEntry, int alID)");
    s.0.reg_func_raw(&d, is_valid as *const c_void);
    s.func(
        "cSoundEntry@ cSound_GetEntry(const tString&in asName)",
        |fr| {
            fr.ret_ref(super::game::find_by_name(bytes(fr.ptr_arg(0))) as *mut c_void);
        },
    );
    entity_ops(&s);
    global_ops(&s);
    music::register(&s);
    play_ops(&s);
    volume_ops(&s);
}

unsafe extern "C" {
    fn snprintf(
        buf: *mut std::ffi::c_char,
        n: usize,
        fmt: *const std::ffi::c_char,
        ...
    ) -> std::ffi::c_int;
}

fn ts(v: f32) -> String {
    String::from_utf8_lossy(&crate::string::float_to_string(v, 20, false)).into_owned()
}

/// `cSomaSoundInstance::Describe`.
pub(super) fn describe(h: usize) -> Vec<u8> {
    let g = game();
    let (Some(inst), Some(rec)) = (g.bank.inst(h), g.shells.iter().find(|s| s.h == h)) else {
        return Vec::new();
    };
    let st = &inst.st;
    let mut buf = [0u8; 256];
    let name = std::ffi::CString::new(
        rec.name
            .iter()
            .copied()
            .filter(|&b| b != 0)
            .collect::<Vec<u8>>(),
    )
    .unwrap_or_default();
    // SAFETY: the buffer holds 256 bytes; the arguments match the format.
    let n = unsafe {
        snprintf(
            buf.as_mut_ptr().cast(),
            buf.len(),
            c"%s voices=%d aud=%.3f vol=%.2f mul=%.2f fade=%.2f%s%s".as_ptr(),
            name.as_ptr(),
            inst.voices.len() as i32,
            f64::from(st.audibility),
            f64::from(st.volume),
            f64::from(st.volume_mul),
            f64::from(st.fade),
            if rec.entity != 0 {
                c" entity".as_ptr()
            } else {
                c"".as_ptr()
            },
            if st.stopped {
                c" stopped".as_ptr()
            } else {
                c"".as_ptr()
            },
        )
    };
    let n = usize::try_from(n).unwrap_or(0).min(buf.len() - 1);
    let mut s = String::from_utf8_lossy(buf.get(..n).unwrap_or_default()).into_owned();
    for v in &inst.voices {
        if v.gain_hf < 1.0 || v.gain_lf < 1.0 {
            s += &format!(" hf={} lf={}", ts(v.gain_hf), ts(v.gain_lf));
        }
    }
    for v in &inst.voices {
        if v.send_gain < 1.0 {
            s += &format!(" send={}", ts(v.send_gain));
        }
    }
    if let Some(ev) = g.bank.events.list.get(st.event) {
        for (i, p) in ev.params.iter().enumerate() {
            let v = inst.param_value.get(i).copied().unwrap_or(0.0);
            s += &format!(" {}={}", String::from_utf8_lossy(&p.name), ts(v));
        }
    }
    s.into_bytes()
}
