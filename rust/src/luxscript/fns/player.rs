//! Player, lantern, death hint and spawn effect functions.

use super::super::a1::Arg;
use super::super::tables;
use super::{S, TYPE_ANY, call, entity, error, ops, s, sf};
use crate::math;

fn f(op: ops::Op, args: &[Arg]) -> f32 {
    call(op, args).as_f()
}

sf!(SetPlayerActive(on: bool) {
    call(ops::P_ACTIVE, &[Arg::b(on)]);
});

sf!(ChangePlayerStateToNormal() {
    call(ops::P_STATE_NORMAL, &[]);
});

sf!(SetPlayerCrouching(on: bool) {
    call(ops::P_MOVE_STATE_NORMAL, &[]);
    call(ops::P_CROUCH, &[Arg::b(on)]);
});

sf!(AddPlayerBodyForce(x: f32, y: f32, z: f32, local: bool) {
    let force = if local {
        let p = call(ops::P_BODY_AXES, &[]).p.cast::<[f32; 9]>();
        // SAFETY: the host returns nine floats (forward, right, up) valid until its next call.
        let [fx, fy, fz, rx, ry, rz, ux, uy, uz] = unsafe { p.read_unaligned() };
        tables::local_force([fx, fy, fz], [rx, ry, rz], [ux, uy, uz], y, z)
    } else {
        [x, y, z]
    };
    call(ops::P_BODY_ADD_FORCE, &[Arg::f(force[0]), Arg::f(force[1]), Arg::f(force[2])]);
});

sf!(ShowPlayerCrossHairIcons(on: bool) {
    call(ops::P_SCRIPT_CROSSHAIR, &[Arg::b(on)]);
});

sf!(SetPlayerPos(x: f32, y: f32, z: f32) {
    call(ops::P_SET_FEET, &[Arg::f(x), Arg::f(y), Arg::f(z)]);
});

sf!(GetPlayerPosX() -> f32 {
    f(ops::P_FEET, &[Arg::i(0)])
});

sf!(GetPlayerPosY() -> f32 {
    f(ops::P_FEET, &[Arg::i(1)])
});

sf!(GetPlayerPosZ() -> f32 {
    f(ops::P_FEET, &[Arg::i(2)])
});

sf!(SetPlayerSanity(v: f32) {
    call(ops::P_SET_SANITY, &[Arg::f(v)]);
});

sf!(AddPlayerSanity(v: f32) {
    call(ops::P_ADD_SANITY, &[Arg::f(v)]);
});

sf!(GetPlayerSanity() -> f32 {
    f(ops::P_SANITY, &[])
});

sf!(SetPlayerHealth(v: f32) {
    call(ops::P_SET_HEALTH, &[Arg::f(v)]);
});

sf!(AddPlayerHealth(v: f32) {
    call(ops::P_ADD_HEALTH, &[Arg::f(v)]);
});

sf!(GetPlayerHealth() -> f32 {
    f(ops::P_HEALTH, &[])
});

sf!(SetPlayerLampOil(v: f32) {
    call(ops::P_SET_LAMP_OIL, &[Arg::f(v)]);
});

sf!(AddPlayerLampOil(v: f32) {
    call(ops::P_ADD_LAMP_OIL, &[Arg::f(v)]);
});

sf!(GetPlayerLampOil() -> f32 {
    f(ops::P_LAMP_OIL, &[])
});

sf!(GetPlayerSpeed() -> f32 {
    f(ops::P_VELOCITY, &[Arg::i(0)])
});

sf!(GetPlayerYSpeed() -> f32 {
    f(ops::P_VELOCITY, &[Arg::i(1)])
});

sf!(MovePlayerForward(v: f32) {
    call(ops::P_MOVE_FORWARD, &[Arg::f(v)]);
});

sf!(SetPlayerPermaDeathSound(sound: S) {
    call(ops::P_PERMA_DEATH_SOUND, &[s(sound.0)]);
});

sf!(SetSanityDrainDisabled(off: bool) {
    call(ops::P_SANITY_DRAIN_DISABLED, &[Arg::b(off)]);
});

sf!(GiveSanityBoost() {
    let cur = f(ops::P_SANITY, &[]);
    call(ops::P_ADD_SANITY, &[Arg::f(tables::sanity_boost(cur))]);
});

sf!(GiveSanityBoostSmall() {
    let cur = f(ops::P_SANITY, &[]);
    call(ops::P_ADD_SANITY, &[Arg::f(tables::sanity_boost_small(cur))]);
});

sf!(GiveSanityDamage(amount: f32, effect: bool) {
    if effect {
        call(ops::P_GIVE_SANITY_DAMAGE, &[Arg::f(amount)]);
    } else {
        call(ops::P_LOWER_SANITY, &[Arg::f(amount), Arg::b(false)]);
    }
});

sf!(GivePlayerDamage(amount: f32, ty: S, spin: bool, lethal: bool) {
    call(ops::P_GIVE_DAMAGE, &[Arg::f(amount), Arg::i(tables::damage_type(ty.0)), Arg::b(spin), Arg::b(lethal)]);
});

sf!(FadePlayerPitchTo(x: f32, mul: f32, max: f32) {
    call(ops::P_FADE_PITCH, &[Arg::f(math::to_rad(x)), Arg::f(mul), Arg::f(math::to_rad(max))]);
});

sf!(FadePlayerFOVMulTo(x: f32, speed: f32) {
    call(ops::P_FADE_FOV, &[Arg::f(x), Arg::f(speed)]);
});

sf!(FadePlayerAspectMulTo(x: f32, speed: f32) {
    call(ops::P_FADE_ASPECT, &[Arg::f(x), Arg::f(speed)]);
});

sf!(FadePlayerRollTo(x: f32, mul: f32, max: f32) {
    call(ops::P_FADE_ROLL, &[Arg::f(math::to_rad(x)), Arg::f(mul), Arg::f(math::to_rad(max))]);
});

sf!(MovePlayerHeadPos(x: f32, y: f32, z: f32, speed: f32, slow: f32) {
    call(ops::P_HEAD_POS_ADD, &[Arg::f(x), Arg::f(y), Arg::f(z), Arg::f(speed), Arg::f(slow)]);
});

sf!(StartPlayerLookAt(name: S, mul: f32, max: f32, callback: S) {
    let Some(e) = entity(name.0, TYPE_ANY, -1) else { return };
    let mut pos = [0.0f32; 3];
    if call(ops::ENT_BODY_NUM, &[e.a()]).as_i() > 0 {
        for (i, v) in pos.iter_mut().enumerate() {
            *v = f(ops::ENT_BODY_POS, &[e.a(), Arg::i(i as i32)]);
        }
    } else if call(ops::ENT_HAS_MESH, &[e.a()]).as_b() {
        for (i, v) in pos.iter_mut().enumerate() {
            *v = f(ops::ENT_MESH_CENTER, &[e.a(), Arg::i(i as i32)]);
        }
    } else {
        error(&format!(
            "Entity {} does not have body or mesh entity! Cannot make player look at it!",
            String::from_utf8_lossy(name.0)
        ));
        return;
    }
    call(ops::P_LOOK_AT_TARGET, &[Arg::f(pos[0]), Arg::f(pos[1]), Arg::f(pos[2]), Arg::f(mul), Arg::f(max), s(callback.0)]);
    call(ops::P_LOOK_AT_ACTIVE, &[Arg::b(true)]);
});

sf!(StopPlayerLookAt() {
    call(ops::P_LOOK_AT_ACTIVE, &[Arg::b(false)]);
});

sf!(SetPlayerMoveSpeedMul(v: f32) {
    call(ops::P_MOVE_SPEED_MUL, &[Arg::f(v)]);
});

sf!(SetPlayerRunSpeedMul(v: f32) {
    call(ops::P_RUN_SPEED_MUL, &[Arg::f(v)]);
});

sf!(SetPlayerLookSpeedMul(v: f32) {
    call(ops::P_LOOK_SPEED_MUL, &[Arg::f(v)]);
});

sf!(SetPlayerJumpForceMul(v: f32) {
    call(ops::P_JUMP_FORCE_MUL, &[Arg::f(v)]);
});

sf!(SetPlayerJumpDisabled(v: bool) {
    call(ops::P_JUMP_DISABLED, &[Arg::b(v)]);
});

sf!(SetPlayerCrouchDisabled(v: bool) {
    call(ops::P_CROUCH_DISABLED, &[Arg::b(v)]);
});

sf!(SetPlayerFallDamageDisabled(v: bool) {
    call(ops::P_NO_FALL_DAMAGE, &[Arg::b(v)]);
});

sf!(SetPlayerUsesDragFootsteps(v: bool) {
    call(ops::P_DRAG_FOOTSTEPS, &[Arg::b(v)]);
});

sf!(TeleportPlayer(start: S) {
    let node = call(ops::MAP_PLAYER_START, &[s(start.0)]).p;
    if node.is_null() {
        error(&format!("Start pos '{}' could not be found!\n", String::from_utf8_lossy(start.0)));
        return;
    }
    call(ops::P_PLACE_AT_START, &[Arg::ptr(node)]);
});

sf!(SetLanternActive(on: bool, effects: bool) {
    call(ops::LANTERN_ACTIVE_SET, &[Arg::b(on), Arg::b(effects)]);
});

sf!(GetLanternActive() -> bool {
    call(ops::LANTERN_ACTIVE, &[]).as_b()
});

sf!(SetLanternDisabled(v: bool) {
    call(ops::LANTERN_DISABLED, &[Arg::b(v)]);
});

sf!(SetLanternLitCallback(cb: S) {
    if call(ops::MAP_EXISTS, &[]).as_b() {
        call(ops::MAP_LANTERN_CALLBACK, &[s(cb.0)]);
    }
});

sf!(SetDeathHint(cat: S, entry: S) {
    call(ops::DEATH_HINT, &[s(cat.0), s(entry.0)]);
});

sf!(DisableDeathStartSound() {
    call(ops::DEATH_DISABLE_START_SOUND, &[]);
});

sf!(SetInDarknessEffectsActive(v: bool) {
    call(ops::P_IN_DARKNESS_FX, &[Arg::b(v)]);
});

sf!(StartPlayerSpawnPS(file: S) {
    call(ops::P_SPAWN_PS, &[s(file.0)]);
});

sf!(GetFlashbackIsActive() -> bool {
    call(ops::P_FLASHBACK_ACTIVE, &[]).as_b()
});
