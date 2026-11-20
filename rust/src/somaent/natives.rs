//! Script natives of `cSomaLuxEntity` that only read or write its `Core`.

use super::ent_core::Core;
use crate::somascript::s2_as::{Engine, Frame, Soma};
use std::ffi::{c_char, c_void};
use std::sync::atomic::{AtomicUsize, Ordering};

static CORE_OFFSET: AtomicUsize = AtomicUsize::new(0);

#[allow(clippy::mut_from_ref)]
fn core(f: &Frame) -> &mut Core {
    let slot = f
        .this
        .cast::<u8>()
        .wrapping_add(CORE_OFFSET.load(Ordering::Relaxed));
    // SAFETY: `this` is a live `cSomaLuxEntity` whose `mpCore` sits at the registered offset.
    unsafe { &mut **slot.cast::<*mut Core>() }
}

macro_rules! getters {
    ($s:expr, $t:expr; $($decl:literal => $field:ident),* $(,)?) => {
        $($s.method_new($t, $decl, |f| f.ret(core(f).$field));)*
    };
}

macro_rules! setters {
    ($s:expr, $t:expr; $($decl:literal => $field:ident: $ty:ty),* $(,)?) => {
        $($s.method_new($t, $decl, |f| core(f).$field = f.arg::<$ty>(0));)*
    };
}

fn register_entity(e: Engine, t: &str) {
    let s = Soma(e);
    s.method_new(t, "const tID& GetID()", |f| f.ret_ref(&raw mut core(f).mID));
    s.method_new(t, "const cMatrixf& GetOnLoadTransform()", |f| {
        f.ret_ref(&raw mut core(f).m_mtxOnLoad)
    });
    s.method_new(t, "const cVector3f& GetOnLoadScale()", |f| {
        f.ret_ref(&raw mut core(f).mvScale)
    });
    getters!(s, t;
        "eLuxEntityType GetEntityType()" => meType,
        "bool IsActive()" => mbActive,
        "cVector3f GetMeshScaleMul()" => mvMeshScaleMul,
        "int GetParentType()" => mlParentType,
        "tID GetParentId()" => mParentID,
        "bool IsSleeping()" => mbSleeping,
        "bool GetAutoSleep()" => mbAutoSleep,
        "bool GetIsBreakable()" => mbBreakable,
        "int GetCurrentAnimationIndex()" => mlCurrentAnim,
        "bool GetGuiSetUseInput()" => mbGuiSetUseInput,
        "bool IsGuiActive()" => mbGuiActive,
        "bool IsInteractedWith()" => mbInteractedWith,
        "float GetVariableUpdateRate()" => mfVariableUpdateRate,
        "bool GetInteractionDisabled()" => mbInteractionDisabled,
        "bool IsLookedAtByPlayer()" => mbLookedAt,
        "bool GetEffectsActive()" => mbEffectsActive,
        "float GetEffectsAlpha()" => mfEffectsAlpha,
    );
    setters!(s, t;
        "void SetIsBreakable(bool abX)" => mbBreakable: bool,
        "void SetGuiVariableFPS(float afX)" => mfGuiFPS: f32,
        "void SetGuiSetUseInput(bool)" => mbGuiSetUseInput: bool,
        "void SetGuiUpdateWhenOutOfView(bool abX)" => mbGuiUpdateWhenOutOfView: bool,
        "void SetGuiCacheAtScreenSize(float afX)" => mfGuiCacheAtScreenSize: f32,
        "void SetIsInteractedWith(bool abX)" => mbInteractedWith: bool,
        "void SetMaxInteractDistance(float afX)" => mfMaxInteractDistance: f32,
        "void SetVariableUpdateRate(float afX)" => mfVariableUpdateRate: f32,
        "void SetInteractionDisabled(bool abX)" => mbInteractionDisabled: bool,
        "void SetForceLookAtCheck(bool abX)" => mbForceLookAtCheck: bool,
        "void SetLookAtCheckCenterOfScreen(bool abX)" => mbLookAtCheckCenter: bool,
        "void SetLookAtCheckRayIntersection(bool abX)" => mbLookAtCheckRay: bool,
        "void SetLookAtMaxDistance(float afX)" => mfLookAtMaxDistance: f32,
        "void SetCustomEffectsOffAlpha(float afAlpha)" => mfEffectsOffAlpha: f32,
    );
    s.method_new(t, "void Sleep()", |f| core(f).mbSleeping = true);
    s.method_new(t, "void SetDisableBreakable(bool abX)", |f| {
        if f.arg::<bool>(0) {
            core(f).mbBreakable = false;
        }
    });
    s.method_new(t, "void ForceGuiCacheUpdate()", |f| {
        let c = core(f);
        c.mbGuiDirty = true;
        c.mbGuiCacheForced = true;
    });
    s.method_new(t, "bool IsEffectsFading()", |f| {
        f.ret(core(f).mfEffectsFadeSpeed != 0.0)
    });
}

fn has_type(e: Engine, t: &str) -> bool {
    cfg!(test) || e.type_by_name(t.as_bytes()).is_some()
}

fn register_props(e: Engine) {
    let s = Soma(e);
    for t in ["cLuxProp", "cLuxArea"]
        .into_iter()
        .filter(|t| has_type(e, t))
    {
        s.method(t, "void SetCheckCollision(bool abX)", |f| {
            core(f).mbCheckCollision = f.arg::<bool>(0)
        });
        s.method(t, "bool GetCheckCollision()", |f| {
            f.ret(core(f).mbCheckCollision)
        });
    }
    if has_type(e, "cLuxProp") {
        register_prop_only(&s);
    }
    if has_type(e, "cLuxArea") {
        s.method("cLuxArea", "const cVector3f& GetSize()", |f| {
            f.ret_ref(&raw mut core(f).mvSize)
        });
    }
}

fn register_prop_only(s: &Soma) {
    s.method("cLuxProp", "bool IsMoving()", |f| {
        f.ret(core(f).mMove.moving)
    });
    s.method("cLuxProp", "float GetHealth()", |f| f.ret(core(f).mfHealth));
    s.method("cLuxProp", "float GetLastInteractConnectionState()", |f| {
        f.ret(core(f).mfInteractConnectionState)
    });
    s.method("cLuxProp", "void SetAllowMapTransfer(bool abX)", |f| {
        core(f).mbAllowMapTransfer = f.arg::<bool>(0)
    });
    s.method("cLuxProp", "bool GetAllowMapTransfer()", |f| {
        f.ret(core(f).mbAllowMapTransfer)
    });
    s.method("cLuxProp", "bool GetStaticPhysics()", |f| {
        f.ret(core(f).mbStaticPhysics)
    });
    s.method("cLuxProp", "void EnableCharCollisionUntilStopped()", |f| {
        let c = core(f);
        c.mbCharCollisionUntilStopped = true;
        c.mbCharCollisionUntilStoppedStarted = false;
    });
}

/// # Safety
/// `engine` is a live `asIScriptEngine`, `ty` a NUL-terminated type name and `core_offset` the
/// offset of `cSomaLuxEntity::mpCore`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_se_register_entity(
    engine: *mut c_void,
    ty: *const c_char,
    core_offset: usize,
) {
    CORE_OFFSET.store(core_offset, Ordering::Relaxed);
    // SAFETY: caller contract.
    let t = unsafe { std::ffi::CStr::from_ptr(ty) }.to_string_lossy();
    register_entity(Engine(engine), &t);
}

/// # Safety
/// `engine` is a live `asIScriptEngine`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_se_register_props(engine: *mut c_void, core_offset: usize) {
    CORE_OFFSET.store(core_offset, Ordering::Relaxed);
    register_props(Engine(engine));
}
