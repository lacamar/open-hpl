//! Sounds, particle systems, lights, sky, fog and global sound settings of the current map.

use super::super::{a1::Arg, tables};
use super::entity::lossy;
use super::{S, TYPE_ANY, call, entity, error, map_exists, ops, s, sf};
use crate::sound::own::handler;

sf!(PreloadParticleSystem(file: S) {
    call(ops::PRELOAD_PS, &[s(file.0)]);
});

sf!(PreloadSound(file: S) {
    call(ops::PRELOAD_SOUND, &[s(file.0)]);
});

fn create_ps_at_entity(
    name: &[u8],
    file: &[u8],
    ent: &[u8],
    save: bool,
    color: [f32; 4],
    fade: bool,
    fades: [f32; 4],
) {
    if !map_exists() {
        return;
    }
    let ps;
    if ent == b"Player" {
        ps = call(ops::MAP_CREATE_PS, &[s(name), s(file)]).p;
        if !ps.is_null() {
            call(ops::PS_AT_PLAYER, &[Arg::ptr(ps)]);
            call(ops::PS_SAVED, &[Arg::ptr(ps), Arg::b(save)]);
        }
    } else {
        let Some(e) = entity(ent, TYPE_ANY, -1) else {
            return;
        };
        ps = call(ops::MAP_CREATE_PS, &[s(name), s(file)]).p;
        if !ps.is_null() {
            let op = if save {
                ops::PS_MATRIX_FROM_ENTITY
            } else {
                ops::PS_ATTACH_TO_ENTITY
            };
            call(op, &[Arg::ptr(ps), e.a()]);
            call(ops::PS_SAVED, &[Arg::ptr(ps), Arg::b(save)]);
        }
    }
    if !ps.is_null() {
        call(
            ops::PS_SETTINGS,
            &[
                Arg::ptr(ps),
                Arg::f(color[0]),
                Arg::f(color[1]),
                Arg::f(color[2]),
                Arg::f(color[3]),
                Arg::b(fade),
                Arg::f(fades[0]),
                Arg::f(fades[1]),
                Arg::f(fades[2]),
                Arg::f(fades[3]),
            ],
        );
    }
}

sf!(CreateParticleSystemAtEntity(name: S, file: S, ent: S, save: bool) {
    create_ps_at_entity(name.0, file.0, ent.0, save, [1.0; 4], false, [1.0, 2.0, 100.0, 110.0]);
});

sf!(CreateParticleSystemAtEntityExt(
    name: S, file: S, ent: S, save: bool, r: f32, g: f32, b: f32, a: f32, fade: bool, min_end: f32, min_start: f32,
    max_start: f32, max_end: f32
) {
    create_ps_at_entity(name.0, file.0, ent.0, save, [r, g, b, a], fade, [min_end, min_start, max_start, max_end]);
});

fn kill_ps(name: &[u8], instantly: bool) {
    if !map_exists() {
        return;
    }
    if !call(ops::PS_KILL, &[s(name), Arg::b(instantly)]).as_b() {
        error(&format!(
            "Could not find particle system '{}'\n",
            lossy(name)
        ));
    }
}

sf!(DestroyParticleSystem(name: S) {
    kill_ps(name.0, false);
});

sf!(DestroyParticleSystemInstantly(name: S) {
    kill_ps(name.0, true);
});

fn fade_in_if(sound: *const std::ffi::c_void, fade: f32) {
    if fade > 0.0 {
        call(
            ops::SOUND_FADE_IN,
            &[Arg::ptr(sound), Arg::f(tables::fade_speed(fade))],
        );
    }
}

sf!(PlaySoundAtPosition(name: S, file: S, x: f32, y: f32, z: f32, fade: f32, save: bool) {
    if !map_exists() {
        return;
    }
    let snd = call(ops::MAP_CREATE_SOUND, &[s(name.0), s(file.0)]).p;
    if !snd.is_null() {
        call(ops::SOUND_SET_POS, &[Arg::ptr(snd), Arg::f(x), Arg::f(y), Arg::f(z)]);
        call(ops::SOUND_SAVED, &[Arg::ptr(snd), Arg::b(save)]);
        fade_in_if(snd, fade);
    }
});

sf!(PlaySoundAtEntity(name: S, file: S, ent: S, fade: f32, save: bool) {
    if ent.0 == b"Player" {
        if !map_exists() {
            return;
        }
        let snd = call(ops::MAP_CREATE_SOUND, &[s(name.0), s(file.0)]).p;
        if !snd.is_null() {
            call(ops::SOUND_FORCE_GUI, &[Arg::ptr(snd)]);
            call(ops::SOUND_SAVED, &[Arg::ptr(snd), Arg::b(save)]);
            fade_in_if(snd, fade);
        }
    } else {
        let Some(e) = entity(ent.0, TYPE_ANY, -1) else { return };
        if !map_exists() {
            return;
        }
        let snd = call(ops::MAP_CREATE_SOUND, &[s(name.0), s(file.0)]).p;
        if !snd.is_null() {
            let op = if save { ops::SOUND_POS_FROM_ENTITY } else { ops::SOUND_ATTACH_TO_ENTITY };
            call(op, &[Arg::ptr(snd), e.a()]);
            call(ops::SOUND_SAVED, &[Arg::ptr(snd), Arg::b(save)]);
            fade_in_if(snd, fade);
        }
    }
});

sf!(FadeInSound(name: S, fade: f32, play_start: bool) {
    if !map_exists() {
        return;
    }
    let speed = tables::fade_speed(fade);
    let snd = call(ops::MAP_SOUND, &[s(name.0)]).p;
    if snd.is_null() {
        error(&format!("Could not find sound entity '{}'\n", lossy(name.0)));
    } else if speed <= 0.0 {
        call(ops::SOUND_PLAY, &[Arg::ptr(snd), Arg::b(play_start)]);
    } else {
        call(ops::SOUND_FADE_IN, &[Arg::ptr(snd), Arg::f(speed)]);
    }
});

sf!(StopSound(name: S, fade: f32) {
    if !map_exists() {
        return;
    }
    let speed = tables::fade_speed(fade);
    if !call(ops::SOUNDS_STOP, &[s(name.0), Arg::f(speed)]).as_b() {
        error(&format!("Could not find sound entity '{}'\n", lossy(name.0)));
    }
});

fn light(name: &[u8]) -> Option<*const std::ffi::c_void> {
    if !map_exists() {
        return None;
    }
    let l = call(ops::MAP_LIGHT, &[s(name)]).p;
    if l.is_null() {
        error(&format!("Could not find light '{}'\n", lossy(name)));
        return None;
    }
    Some(l)
}

sf!(SetLightVisible(name: S, on: bool) {
    if let Some(l) = light(name.0) {
        call(ops::LIGHT_VISIBLE, &[Arg::ptr(l), Arg::b(on)]);
    }
});

sf!(FadeLightTo(name: S, r: f32, g: f32, b: f32, a: f32, radius: f32, time: f32) {
    let Some(l) = light(name.0) else { return };
    call(ops::LIGHT_FLICKER, &[Arg::ptr(l), Arg::b(false)]);
    let diffuse = |i: i32| call(ops::LIGHT_DIFFUSE, &[Arg::ptr(l), Arg::i(i)]).as_f();
    let pick = |v: f32, i: i32| if v >= 0.0 { v } else { diffuse(i) };
    let color = [pick(r, 0), pick(g, 1), pick(b, 2), pick(a, 3)];
    let new_radius = if radius >= 0.0 { radius } else { call(ops::LIGHT_RADIUS, &[Arg::ptr(l)]).as_f() };
    call(ops::LIGHT_VISIBLE, &[Arg::ptr(l), Arg::b(true)]);
    call(ops::LIGHT_FADE, &[
        Arg::ptr(l), Arg::f(color[0]), Arg::f(color[1]), Arg::f(color[2]), Arg::f(color[3]), Arg::f(new_radius),
        Arg::f(time),
    ]);
});

sf!(SetLightFlickerActive(name: S, on: bool) {
    if let Some(l) = light(name.0) {
        call(ops::LIGHT_FLICKER, &[Arg::ptr(l), Arg::b(on)]);
    }
});

/// `eSoundEntryType_World` and `eLuxGlobalVolumeType_Script`.
const WORLD_TYPE: u32 = 1;
const SCRIPT_ID: i32 = 0;

/// The script's slot of the global volume (0) or speed (1) table; created when absent.
fn script_global_value(kind: i32) -> f32 {
    handler().map_or(0.0, |h| {
        // SAFETY: a forced slot is live until the table changes.
        unsafe {
            h.setting_entry(usize::try_from(kind).unwrap_or(0), SCRIPT_ID, true)
                .as_ref()
        }
        .map_or(0.0, |e| e.val)
    })
}

fn fade_global(kind: i32, dest: f32, time: f32) {
    let destroy = dest == 1.0;
    let mut speed = 1000.0f32;
    if time > 0.0 {
        speed = (dest - script_global_value(kind)).abs() / time;
    } else {
        script_global_value(kind);
    }
    if let Some(h) = handler() {
        h.fade_global(
            usize::try_from(kind).unwrap_or(0),
            dest,
            speed,
            WORLD_TYPE,
            SCRIPT_ID,
            destroy,
        );
    }
}

sf!(FadeGlobalSoundVolume(dest: f32, time: f32) {
    fade_global(0, dest, time);
});

sf!(FadeGlobalSoundSpeed(dest: f32, time: f32) {
    fade_global(1, dest, time);
});

sf!(SetSkyBoxActive(on: bool) {
    call(ops::WORLD_SKYBOX_ACTIVE, &[Arg::b(on)]);
});

sf!(SetSkyBoxTexture(texture: S) {
    call(ops::WORLD_SKYBOX_TEXTURE, &[s(texture.0)]);
});

sf!(SetSkyBoxColor(r: f32, g: f32, b: f32, a: f32) {
    call(ops::WORLD_SKYBOX_COLOR, &[Arg::f(r), Arg::f(g), Arg::f(b), Arg::f(a)]);
});

sf!(SetFogActive(on: bool) {
    call(ops::WORLD_FOG_ACTIVE, &[Arg::b(on)]);
});

sf!(SetFogColor(r: f32, g: f32, b: f32, a: f32) {
    call(ops::WORLD_FOG_COLOR, &[Arg::f(r), Arg::f(g), Arg::f(b), Arg::f(a)]);
});

sf!(SetFogProperties(start: f32, end: f32, falloff: f32, culling: bool) {
    call(ops::WORLD_FOG_PROPS, &[Arg::f(start), Arg::f(end), Arg::f(falloff), Arg::b(culling)]);
});
