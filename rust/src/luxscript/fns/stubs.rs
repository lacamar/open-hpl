//! AMFP functions the engine port does not implement: each logs that it was called.

use super::{S, call, ops, s, sf};

/// `%s` of a C string: up to the first NUL.
fn cs(b: &[u8]) -> &[u8] {
    b.split(|&c| c == 0).next().unwrap_or(b)
}

/// `printf("%f", double(x))`.
fn ff(x: f32) -> String {
    let d = f64::from(x);
    if d.is_nan() {
        return if d.is_sign_negative() { "-nan" } else { "nan" }.into();
    }
    if d.is_infinite() {
        return if d < 0.0 { "-inf" } else { "inf" }.into();
    }
    format!("{d:.6}")
}

fn bi(b: bool) -> String {
    i32::from(b).to_string()
}

fn warn(parts: &[&[u8]]) {
    call(ops::WARNING, &[s(&parts.concat())]);
}

sf!(AmfpStub_AddHint(a: S, b: S) {
    warn(&[b"AMFP stub: AddHint('", cs(a.0), b"', '", cs(b.0), b"') not implemented\n"]);
});

sf!(AmfpStub_SetEnemyMoveType(a: S, b: S) {
    warn(&[b"AMFP stub: SetEnemyMoveType('", cs(a.0), b"', '", cs(b.0), b"') not implemented\n"]);
});

sf!(AmfpStub_SetJournalDisabled(a: bool) {
    warn(&[b"AMFP stub: SetJournalDisabled(", bi(a).as_bytes(), b") not implemented\n"]);
});

sf!(AmfpStub_SetLampFlickerActive(a: S, b: bool) {
    warn(&[b"AMFP stub: SetLampFlickerActive('", cs(a.0), b"', ", bi(b).as_bytes(), b") not implemented\n"]);
});

sf!(AmfpStub_SetLanternFlickerActive(a: bool) {
    warn(&[b"AMFP stub: SetLanternFlickerActive(", bi(a).as_bytes(), b") not implemented\n"]);
});

sf!(AmfpStub_SetManPigType(a: S, b: S) {
    warn(&[b"AMFP stub: SetManPigType('", cs(a.0), b"', '", cs(b.0), b"') not implemented\n"]);
});

sf!(AmfpStub_SetParticleSystemActive(a: S, b: bool) {
    warn(&[b"AMFP stub: SetParticleSystemActive('", cs(a.0), b"', ", bi(b).as_bytes(), b") not implemented\n"]);
});

sf!(AmfpStub_SetPhysicsAutoDisable(a: S, b: bool) {
    warn(&[b"AMFP stub: SetPhysicsAutoDisable('", cs(a.0), b"', ", bi(b).as_bytes(), b") not implemented\n"]);
});

sf!(AmfpStub_SetPlayerInfection(a: f32) {
    warn(&[b"AMFP stub: SetPlayerInfection(", ff(a).as_bytes(), b") not implemented\n"]);
});

sf!(AmfpStub_ShowScreenImage(
    a: S, _x: f32, _y: f32, _z: f32, _fade: bool, _f1: f32, _f2: f32, _f3: f32
) {
    warn(&[b"AMFP stub: ShowScreenImage('", cs(a.0), b"', ...) not implemented\n"]);
});

sf!(AmfpStub_StartPhoneRinging(a: S) {
    warn(&[b"AMFP stub: StartPhoneRinging('", cs(a.0), b"') not implemented\n"]);
});

sf!(AmfpStub_StopPhoneRinging(a: S) {
    warn(&[b"AMFP stub: StopPhoneRinging('", cs(a.0), b"') not implemented\n"]);
});

sf!(AmfpStub_AddPlayerInfection(a: f32) {
    warn(&[b"AMFP stub: AddPlayerInfection(", ff(a).as_bytes(), b") not implemented\n"]);
});

sf!(AmfpStub_ChangeEnemyPose(a: S, b: S) {
    warn(&[b"AMFP stub: ChangeEnemyPose('", cs(a.0), b"', '", cs(b.0), b"') not implemented\n"]);
});

sf!(AmfpStub_GiveInfectionDamage(a: f32, b: bool) {
    warn(&[b"AMFP stub: GiveInfectionDamage(", ff(a).as_bytes(), b", ", bi(b).as_bytes(), b") not implemented\n"]);
});

sf!(AmfpStub_HangUpPhone(a: S) {
    warn(&[b"AMFP stub: HangUpPhone('", cs(a.0), b"') not implemented\n"]);
});

sf!(AmfpStub_HideScreenImageImmediately() {
    warn(&[b"AMFP stub: HideScreenImageImmediately() not implemented\n"]);
});

sf!(AmfpStub_HideScreenImageWithFade(a: f32) {
    warn(&[b"AMFP stub: HideScreenImageWithFade(", ff(a).as_bytes(), b") not implemented\n"]);
});

sf!(AmfpStub_ReleasePlayerFromLimbo() {
    warn(&[b"AMFP stub: ReleasePlayerFromLimbo() not implemented\n"]);
});

sf!(AmfpStub_RemoveAllHints() {
    warn(&[b"AMFP stub: RemoveAllHints() not implemented\n"]);
});

sf!(AmfpStub_SetInfectionFauxMode(a: bool) {
    warn(&[b"AMFP stub: SetInfectionFauxMode(", bi(a).as_bytes(), b") not implemented\n"]);
});
