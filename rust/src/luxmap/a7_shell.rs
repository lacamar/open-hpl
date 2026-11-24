//! Generic C++ shell of the liquid, sticky, slime, insanity and flashback areas: construction
//! defaults, loader variables and the type-specific save data fields. The shell hands over a
//! table of engine thunks ([`Vt`]); everything else is Rust state in [`A7Area`].

use super::a7_area::{A7Area, Pod, st};
use crate::luxsave::layout_ar as lay;
use crate::math::rand_rectf;
use crate::types::Vector3f;
use std::ffi::c_void;

/// A string the shell lends for the duration of one call.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct Sv {
    pub p: *const u8,
    pub n: usize,
}

type Key = unsafe extern "C" fn(*mut c_void, *const u8, usize);

#[repr(C)]
pub struct Vt {
    pub ctx: *mut c_void,
    pub var_f: unsafe extern "C" fn(*mut c_void, *const u8, usize, f32) -> f32,
    pub var_i: unsafe extern "C" fn(*mut c_void, *const u8, usize, i32) -> i32,
    pub var_b: unsafe extern "C" fn(*mut c_void, *const u8, usize, bool) -> bool,
    pub var_s: unsafe extern "C" fn(*mut c_void, *const u8, usize, *const u8, usize) -> Sv,
    pub cfg_f: unsafe extern "C" fn(*mut c_void, *const u8, usize, *const u8, usize, f32) -> f32,
    pub cfg_s: unsafe extern "C" fn(*mut c_void, *const u8, usize, *const u8, usize) -> Sv,
    pub preload_sound: Key,
    pub preload_ps: Key,
    pub mat_find: unsafe extern "C" fn(*mut c_void, *const u8, usize) -> usize,
    pub mat_name: unsafe extern "C" fn(*mut c_void, usize) -> Sv,
    /// Writes the owning entity id, then the unique body id.
    pub body_ids: unsafe extern "C" fn(*mut c_void, usize, *mut i32),
    pub find_body: unsafe extern "C" fn(*mut c_void, i32, i32) -> usize,
    pub max_focus: unsafe extern "C" fn(*mut c_void, f32),
    /// Look-at (0) or interact (1) callback of the entity: name, auto remove.
    pub callback: unsafe extern "C" fn(*mut c_void, i32, *const u8, usize, bool),
    /// Opens the rope settings file; false (after the C++ error log) when it does not load.
    pub doc_open: unsafe extern "C" fn(*mut c_void, *const u8, usize) -> bool,
    pub doc_f: unsafe extern "C" fn(*mut c_void, *const u8, usize) -> f32,
    pub doc_s: unsafe extern "C" fn(*mut c_void, *const u8, usize) -> Sv,
    pub doc_close: unsafe extern "C" fn(*mut c_void),
}

/// Safe face of a [`Vt`]; the FFI entries state the contract once.
pub struct V<'a>(&'a Vt);

fn own(s: Sv) -> Vec<u8> {
    if s.p.is_null() {
        return Vec::new();
    }
    // SAFETY: the shell lends `n` readable bytes until its next thunk call; copied here.
    unsafe { std::slice::from_raw_parts(s.p, s.n) }.to_vec()
}

impl<'a> V<'a> {
    /// # Safety
    /// Every function of `vt` is callable with `vt.ctx` for the lifetime of the value.
    pub unsafe fn new(vt: &'a Vt) -> Self {
        Self(vt)
    }

    fn f(&self, k: &str, d: f32) -> f32 {
        // SAFETY: contract of `new`.
        unsafe { (self.0.var_f)(self.0.ctx, k.as_ptr(), k.len(), d) }
    }

    fn i(&self, k: &str, d: i32) -> i32 {
        // SAFETY: contract of `new`.
        unsafe { (self.0.var_i)(self.0.ctx, k.as_ptr(), k.len(), d) }
    }

    fn b(&self, k: &str, d: bool) -> bool {
        // SAFETY: contract of `new`.
        unsafe { (self.0.var_b)(self.0.ctx, k.as_ptr(), k.len(), d) }
    }

    fn s(&self, k: &str) -> Vec<u8> {
        self.sd(k, "")
    }

    fn sd(&self, k: &str, d: &str) -> Vec<u8> {
        // SAFETY: contract of `new`.
        own(unsafe { (self.0.var_s)(self.0.ctx, k.as_ptr(), k.len(), d.as_ptr(), d.len()) })
    }

    fn doc_open(&self, name: &[u8]) -> bool {
        // SAFETY: contract of `new`.
        unsafe { (self.0.doc_open)(self.0.ctx, name.as_ptr(), name.len()) }
    }

    fn doc_f(&self, k: &str) -> f32 {
        // SAFETY: contract of `new`.
        unsafe { (self.0.doc_f)(self.0.ctx, k.as_ptr(), k.len()) }
    }

    fn doc_s(&self, k: &str) -> Vec<u8> {
        // SAFETY: contract of `new`.
        own(unsafe { (self.0.doc_s)(self.0.ctx, k.as_ptr(), k.len()) })
    }

    fn doc_close(&self) {
        // SAFETY: contract of `new`.
        unsafe { (self.0.doc_close)(self.0.ctx) }
    }

    fn max_focus(&self, f: f32) {
        // SAFETY: contract of `new`.
        unsafe { (self.0.max_focus)(self.0.ctx, f) }
    }

    fn callback(&self, which: i32, name: &[u8], auto_remove: bool) {
        // SAFETY: contract of `new`.
        unsafe { (self.0.callback)(self.0.ctx, which, name.as_ptr(), name.len(), auto_remove) }
    }

    fn cfg_f(&self, sec: &str, key: &str, d: f32) -> f32 {
        // SAFETY: contract of `new`.
        unsafe {
            (self.0.cfg_f)(
                self.0.ctx,
                sec.as_ptr(),
                sec.len(),
                key.as_ptr(),
                key.len(),
                d,
            )
        }
    }

    fn cfg_s(&self, sec: &str, key: &str) -> Vec<u8> {
        // SAFETY: contract of `new`.
        own(unsafe { (self.0.cfg_s)(self.0.ctx, sec.as_ptr(), sec.len(), key.as_ptr(), key.len()) })
    }

    fn preload(&self, f: Key, name: &[u8]) {
        // SAFETY: contract of `new`.
        unsafe { f(self.0.ctx, name.as_ptr(), name.len()) }
    }

    fn mat_find(&self, name: &[u8]) -> usize {
        // SAFETY: contract of `new`.
        unsafe { (self.0.mat_find)(self.0.ctx, name.as_ptr(), name.len()) }
    }

    fn mat_name(&self, m: usize) -> Vec<u8> {
        // SAFETY: contract of `new`.
        own(unsafe { (self.0.mat_name)(self.0.ctx, m) })
    }

    fn body_ids(&self, body: usize) -> [i32; 2] {
        let mut out = [0; 2];
        // SAFETY: contract of `new`; `out` holds the two ids.
        unsafe { (self.0.body_ids)(self.0.ctx, body, out.as_mut_ptr()) };
        out
    }

    fn find_body(&self, entity: i32, body: i32) -> usize {
        // SAFETY: contract of `new`.
        unsafe { (self.0.find_body)(self.0.ctx, entity, body) }
    }
}

type Str = [u64; 4];

#[cfg(not(test))]
unsafe extern "C" {
    fn ohpl_as2_str_assign(s: *mut c_void, p: *const u8, n: usize);
}

fn write_str(f: &mut Str, b: &[u8]) {
    #[cfg(not(test))]
    // SAFETY: `f` is a live `std::string` of the save data.
    unsafe {
        ohpl_as2_str_assign(std::ptr::from_mut(f).cast(), b.as_ptr(), b.len());
    }
    // Test builds have no libstdc++: a leaked buffer behind the pointer and size words.
    #[cfg(test)]
    {
        let leaked: &'static mut [u8] = Box::leak(b.to_vec().into_boxed_slice());
        f[0] = leaked.as_ptr() as u64;
        f[1] = leaked.len() as u64;
    }
}

fn vec3(v: Vector3f) -> [f32; 3] {
    [v.x, v.y, v.z]
}

fn unvec3(v: [f32; 3]) -> Vector3f {
    Vector3f {
        x: v[0],
        y: v[1],
        z: v[2],
    }
}

fn read_str(f: &Str) -> Vec<u8> {
    // SAFETY: libstdc++ keeps the data pointer and the size in the first two words.
    unsafe { std::slice::from_raw_parts(f[0] as *const u8, f[1] as usize) }.to_vec()
}

impl A7Area {
    /// Constructor defaults; `true` when the area is saved with the full game save.
    pub fn init(&mut self, name: &[u8], v: &V) -> bool {
        self.name = name.to_vec();
        match &mut self.pod {
            Pod::Sticky(s) => {
                s.move_body = true;
                s.rotate_body = true;
                s.check_center_in_area = true;
                s.pose_time = 0.2;
                s.can_detach = true;
                s.set_mtx_time = 1.0;
                s.attached_body_mass = 0.0;
                s.attached_entity_full_game_saved = false;
            }
            Pod::Script(_) => {
                v.max_focus(v.cfg_f("Player_Interaction", "ScriptArea_MaxFocusDist", 0.0))
            }
            Pod::Examine(s) => {
                v.max_focus(v.cfg_f("Player_Interaction", "Examine_MaxFocusDist", 0.0));
                s.insane_limit = v.cfg_f("Player_Interaction", "MaxExamineSanity", 0.0);
                s.play_sound_count = 0.0;
            }
            Pod::Sign(_) => v.max_focus(v.cfg_f("Player_Interaction", "Sign_MaxFocusDist", 0.0)),
            Pod::Ladder(_) => {
                v.max_focus(v.cfg_f("Player_Interaction", "Ladder_MaxFocusDist", 0.0))
            }
            Pod::Insanity(s) => {
                s.check_time_min = v.cfg_f("Insanity", "InsanityArea_CheckTimeMin", 0.0);
                s.check_time_max = v.cfg_f("Insanity", "IsanityArea_CheckTimeMax", 0.0);
                s.check_collision_count = rand_rectf(s.check_time_min, s.check_time_max);
            }
            _ => {}
        }
        matches!(self.pod, Pod::Liquid(_) | Pod::Sticky(_))
    }

    /// `LoadVariables` of the five loaders.
    pub fn load_vars(&mut self, v: &V) {
        match &mut self.pod {
            Pod::Liquid(s) => {
                s.density = v.f("Density", 0.0);
                s.linear_viscosity = v.f("LinearViscosity", 0.0);
                s.angular_viscosity = v.f("AngularViscosity", 0.0);
                self.material = v.mat_find(&v.s("PhysicsMaterial"));
                s.has_waves = v.b("HasWaves", false);
                s.wave_amp = v.f("WaveAmp", 0.0);
                s.wave_freq = v.f("WaveFreq", 0.0);
                s.player_speed_mul = v.f("PlayerSpeedMul", 0.8);
                let max = v.f("MaxWaveDistance", 25.0);
                s.max_wave_distance_sqr = max * max;
            }
            Pod::Sticky(s) => {
                self.strs[st::ATTACH_FUNCTION] = v.s("AttachFunction");
                self.strs[st::DETACH_FUNCTION] = v.s("DetachFunction");
                self.strs[st::ATTACH_SOUND] = v.s("AttachSound");
                self.strs[st::DETACH_SOUND] = v.s("DetachSound");
                self.strs[st::ATTACH_PS] = v.s("AttachPS");
                self.strs[st::DETACH_PS] = v.s("DetachPS");
                self.strs[st::ATTACHABLE_BODY] = v.s("AttachableBodyName");
                s.move_body = v.b("MoveBody", true);
                s.rotate_body = v.b("RotateBody", true);
                s.check_center_in_area = v.b("CheckCenterInArea", true);
                s.can_detach = v.b("CanDetach", true);
                s.pose_time = v.f("PoseTime", 1.0);
                v.preload(v.0.preload_sound, &self.strs[st::ATTACH_SOUND]);
                v.preload(v.0.preload_sound, &self.strs[st::DETACH_SOUND]);
                v.preload(v.0.preload_ps, &self.strs[st::ATTACH_PS]);
                v.preload(v.0.preload_ps, &self.strs[st::DETACH_PS]);
            }
            Pod::Slime(s) => {
                s.slime_type = v.i("SlimeType", 0);
                s.min_check_attack_time = v.f("MinCheckAttackTime", 0.0);
                s.max_check_attack_time = v.f("MaxCheckAttackTime", 0.0);
                s.disable_after_attack = v.b("DisableAfterAttack", false);
                self.strs[st::SLIME_CALLBACK] = v.s("Callback");
                s.check_collision_count =
                    rand_rectf(s.min_check_attack_time, s.max_check_attack_time);
                let p = format!("SlimeType{}", s.slime_type);
                self.strs[st::SLIME_SOUND] = v.cfg_s("Slime", &format!("{p}_AttackSound"));
                self.strs[st::SLIME_PS] = v.cfg_s("Slime", &format!("{p}_AttackPS"));
                s.min_attack_damage = v.cfg_f("Slime", &format!("{p}_MinAttackDamage"), 0.0);
                s.max_attack_damage = v.cfg_f("Slime", &format!("{p}_MaxAttackDamage"), 0.0);
                s.screen_shake_amount = v.cfg_f("Slime", &format!("{p}_ScreenShakeAmount"), 0.0);
            }
            Pod::Script(s) => {
                v.callback(
                    0,
                    &v.s("PlayerLookAtCallback"),
                    v.b("PlayerLookAtCallbackAutoRemove", false),
                );
                v.callback(
                    1,
                    &v.s("PlayerInteractCallback"),
                    v.b("PlayerInteractCallbackAutoRemove", false),
                );
                s.item_interaction = v.b("ItemInteraction", false);
                let max = v.f("MaxFocusDistance", -1.0);
                if max >= 0.0 {
                    v.max_focus(max);
                }
            }
            Pod::Examine(_) => {
                self.strs[st::DESC_CAT] = v.s("DescCat");
                self.strs[st::DESC_ENTRY] = v.s("DescEntry");
                self.strs[st::INSANE_CAT] = v.s("DescInsaneCat");
                self.strs[st::INSANE_ENTRY] = v.s("DescInsaneEntry");
                self.strs[st::EXAMINE_SOUND] = v.s("Sound");
                self.strs[st::INSANE_SOUND] = v.s("InsaneSound");
            }
            Pod::Sign(_) => {
                self.strs[st::TEXT_CAT] = v.s("TextCat");
                self.strs[st::TEXT_ENTRY] = v.s("TextEntry");
            }
            Pod::Ladder(_) => self.strs[st::MATERIAL] = v.sd("Material", "metal"),
            Pod::Rope(_) => {}
            Pod::Insanity(s) => s.auto_disable = v.b("AutoDisable", false),
            Pod::Flashback(_) => {
                self.strs[st::FLASHBACK_FILE] = v.s("FlashbackFile");
                self.strs[st::FLASHBACK_CALLBACK] = v.s("Callback");
            }
        }
    }

    /// Loader of a rope entity: the start position, the loader variables, then the settings
    /// file when `RopeFile` names one that loads, else the same keys from the variables.
    pub fn load_rope(&mut self, start: Vector3f, v: &V) {
        let Pod::Rope(s) = &mut self.pod else {
            return;
        };
        s.start_pos = start;
        self.strs[st::END_POS_NODE] = v.s("EndPosNode");
        self.strs[st::START_BODY] = v.s("StartBody");
        self.strs[st::END_BODY] = v.s("EndBody");
        s.auto_move = v.b("AutoMove", false);
        s.auto_move_acc = v.f("AutoMoveAcc", 0.0);
        s.auto_move_max_speed = v.f("AutoMoveMaxSpeed", 0.0);
        s.min_total_length = v.f("MinTotalLength", 0.0);
        s.max_total_length = v.f("MaxTotalLength", 0.0);
        let file = v.s("RopeFile");
        let doc = !file.is_empty() && v.doc_open(&file);
        let f = |k: &str| if doc { v.doc_f(k) } else { v.f(k, 0.0) };
        let t = |k: &str| if doc { v.doc_s(k) } else { v.s(k) };
        s.segment_length = f("SegmentLength");
        s.damping = f("Damping");
        s.strength = f("Strength");
        s.stiffness = f("Stiffness");
        self.strs[st::ROPE_MATERIAL] = t("Material");
        s.radius = f("Radius");
        s.length_tile_amount = f("LengthTileAmount");
        s.length_tile_size = f("LengthTileSize");
        self.strs[st::ROPE_SOUND] = t("Sound");
        s.sound_start_speed = f("SoundStartSpeed");
        s.sound_stop_speed = f("SoundStopSpeed");
        if doc {
            v.doc_close();
        }
    }

    /// Type-specific half of `SaveToSaveData`.
    ///
    /// # Safety
    /// `data` is a live save data object of this area's type.
    pub unsafe fn save(&mut self, data: *mut u8, v: &V) {
        // SAFETY: caller contract; the mirrors are gcc-checked in `luxsave::layout_ar`.
        unsafe {
            let pod = self.pod;
            match &pod {
                Pod::Liquid(s) => {
                    let d = &mut *data.cast::<lay::cLuxArea_Liquid_SaveData>();
                    if self.material != 0 {
                        write_str(&mut d.msPhysicsMaterial, &v.mat_name(self.material));
                    }
                    d.mfDensity = s.density;
                    d.mfLinearViscosity = s.linear_viscosity;
                    d.mfAngularViscosity = s.angular_viscosity;
                    d.mbHasWaves = u8::from(s.has_waves);
                    d.mfWaveAmp = s.wave_amp;
                    d.mfWaveFreq = s.wave_freq;
                    d.mfPlayerSpeedMul = s.player_speed_mul;
                    d.mfMaxWaveDistanceSqr = s.max_wave_distance_sqr;
                }
                Pod::Sticky(s) => {
                    let d = &mut *data.cast::<lay::cLuxArea_Sticky_SaveData>();
                    let [entity, body] = if s.attached_body != 0 {
                        let [e, b] = v.body_ids(s.attached_body);
                        [e, b]
                    } else {
                        [-1, -1]
                    };
                    d.mlAttachedEntityID = entity;
                    d.mlAttachedBodyID = body;
                    write_str(&mut d.msAttachFunction, &self.strs[st::ATTACH_FUNCTION]);
                    write_str(&mut d.msDetachFunction, &self.strs[st::DETACH_FUNCTION]);
                    write_str(&mut d.msAttachSound, &self.strs[st::ATTACH_SOUND]);
                    write_str(&mut d.msDetachSound, &self.strs[st::DETACH_SOUND]);
                    write_str(&mut d.msAttachPS, &self.strs[st::ATTACH_PS]);
                    write_str(&mut d.msDetachPS, &self.strs[st::DETACH_PS]);
                    d.mbCanDetach = u8::from(s.can_detach);
                    d.mfPoseTime = s.pose_time;
                    d.mbCheckCenterInArea = u8::from(s.check_center_in_area);
                    write_str(&mut d.msAttachableBodyName, &self.strs[st::ATTACHABLE_BODY]);
                    d.mfAttachedBodyMass = s.attached_body_mass;
                    d.mbAttachedBodyGravity = u8::from(s.attached_body_gravity);
                    d.mbAttachedEntityFullGameSaved = u8::from(s.attached_entity_full_game_saved);
                    d.mfSetMtxTime = s.set_mtx_time;
                }
                Pod::Slime(s) => {
                    let d = &mut *data.cast::<lay::cLuxArea_SlimeDamage_SaveData>();
                    d.mlSlimeType = s.slime_type;
                    d.mfMinCheckAttackTime = s.min_check_attack_time;
                    d.mfMaxCheckAttackTime = s.max_check_attack_time;
                    d.mbDisableAfterAttack = u8::from(s.disable_after_attack);
                    // Kept C++ quirk: the callback is copied from the save data into the area.
                    self.strs[st::SLIME_CALLBACK] = read_str(&d.msCallback);
                    d.mfCheckCollisionCount = s.check_collision_count;
                    write_str(&mut d.msAttackSound, &self.strs[st::SLIME_SOUND]);
                    write_str(&mut d.msAttackPS, &self.strs[st::SLIME_PS]);
                    d.mfMinAttackDamage = s.min_attack_damage;
                    d.mfMaxAttackDamage = s.max_attack_damage;
                    d.mfScreenShakeAmount = s.screen_shake_amount;
                }
                Pod::Insanity(s) => {
                    let d = &mut *data.cast::<lay::cLuxArea_Insanity_SaveData>();
                    d.mbAutoDisable = u8::from(s.auto_disable);
                }
                Pod::Script(s) => {
                    let d = &mut *data.cast::<lay::cLuxArea_Script_SaveData>();
                    d.mbItemInteraction = u8::from(s.item_interaction);
                }
                Pod::Examine(_) => {
                    let d = &mut *data.cast::<lay::cLuxArea_Examine_SaveData>();
                    write_str(&mut d.msDescCat, &self.strs[st::DESC_CAT]);
                    write_str(&mut d.msDescEntry, &self.strs[st::DESC_ENTRY]);
                    write_str(&mut d.msDescInsaneCat, &self.strs[st::INSANE_CAT]);
                    write_str(&mut d.msDescInsaneEntry, &self.strs[st::INSANE_ENTRY]);
                    write_str(&mut d.msSound, &self.strs[st::EXAMINE_SOUND]);
                    write_str(&mut d.msInsaneSound, &self.strs[st::INSANE_SOUND]);
                }
                Pod::Sign(_) => {
                    let d = &mut *data.cast::<lay::cLuxArea_Sign_SaveData>();
                    write_str(&mut d.msTextCat, &self.strs[st::TEXT_CAT]);
                    write_str(&mut d.msTextEntry, &self.strs[st::TEXT_ENTRY]);
                }
                Pod::Ladder(_) => {
                    let d = &mut *data.cast::<lay::cLuxArea_Ladder_SaveData>();
                    write_str(&mut d.msMaterial, &self.strs[st::MATERIAL]);
                }
                Pod::Rope(s) => {
                    let d = &mut *data.cast::<lay::cLuxRope_SaveData>();
                    d.mbRopeCreated = u8::from(s.rope_created);
                    d.mvStartPos = vec3(s.start_pos);
                    d.mvEndPos = vec3(s.end_pos);
                    write_str(&mut d.msEndPosNode, &self.strs[st::END_POS_NODE]);
                    write_str(&mut d.msStartBody, &self.strs[st::START_BODY]);
                    write_str(&mut d.msEndBody, &self.strs[st::END_BODY]);
                    d.mfMinTotalLength = s.min_total_length;
                    d.mfMaxTotalLength = s.max_total_length;
                    d.mfSegmentLength = s.segment_length;
                    d.mfDamping = s.damping;
                    d.mfStrength = s.strength;
                    d.mfStiffness = s.stiffness;
                    write_str(&mut d.msMaterial, &self.strs[st::ROPE_MATERIAL]);
                    d.mfRadius = s.radius;
                    d.mfLengthTileAmount = s.length_tile_amount;
                    d.mfLengthTileSize = s.length_tile_size;
                    write_str(&mut d.msSound, &self.strs[st::ROPE_SOUND]);
                    d.mfSoundStartSpeed = s.sound_start_speed;
                    d.mfSoundStopSpeed = s.sound_stop_speed;
                    d.mbAutoMove = u8::from(s.auto_move);
                    d.mfAutoMoveAcc = s.auto_move_acc;
                    d.mfAutoMoveMaxSpeed = s.auto_move_max_speed;
                }
                Pod::Flashback(_) => {
                    let d = &mut *data.cast::<lay::cLuxArea_Flashback_SaveData>();
                    write_str(&mut d.msFlashbackFile, &self.strs[st::FLASHBACK_FILE]);
                    write_str(&mut d.msCallback, &self.strs[st::FLASHBACK_CALLBACK]);
                }
            }
        }
    }

    /// Type-specific half of `LoadFromSaveData`.
    ///
    /// # Safety
    /// `data` is a live save data object of this area's type.
    pub unsafe fn load(&mut self, data: *const u8, v: &V) {
        // SAFETY: caller contract; the mirrors are gcc-checked in `luxsave::layout_ar`.
        unsafe {
            match &mut self.pod {
                Pod::Liquid(s) => {
                    let d = &*data.cast::<lay::cLuxArea_Liquid_SaveData>();
                    let name = read_str(&d.msPhysicsMaterial);
                    if !name.is_empty() {
                        self.material = v.mat_find(&name);
                    }
                    s.density = d.mfDensity;
                    s.linear_viscosity = d.mfLinearViscosity;
                    s.angular_viscosity = d.mfAngularViscosity;
                    s.has_waves = d.mbHasWaves != 0;
                    s.wave_amp = d.mfWaveAmp;
                    s.wave_freq = d.mfWaveFreq;
                    s.player_speed_mul = d.mfPlayerSpeedMul;
                    s.max_wave_distance_sqr = d.mfMaxWaveDistanceSqr;
                }
                Pod::Sticky(s) => {
                    let d = &*data.cast::<lay::cLuxArea_Sticky_SaveData>();
                    if d.mlAttachedEntityID >= 0 {
                        s.attached_body = v.find_body(d.mlAttachedEntityID, d.mlAttachedBodyID);
                        s.last_attached = s.attached_body;
                    } else {
                        s.attached_body = 0;
                        s.last_attached = 0;
                    }
                    self.strs[st::ATTACH_FUNCTION] = read_str(&d.msAttachFunction);
                    self.strs[st::DETACH_FUNCTION] = read_str(&d.msDetachFunction);
                    self.strs[st::ATTACH_SOUND] = read_str(&d.msAttachSound);
                    self.strs[st::DETACH_SOUND] = read_str(&d.msDetachSound);
                    self.strs[st::ATTACH_PS] = read_str(&d.msAttachPS);
                    self.strs[st::DETACH_PS] = read_str(&d.msDetachPS);
                    s.can_detach = d.mbCanDetach != 0;
                    s.pose_time = d.mfPoseTime;
                    s.check_center_in_area = d.mbCheckCenterInArea != 0;
                    self.strs[st::ATTACHABLE_BODY] = read_str(&d.msAttachableBodyName);
                    s.attached_body_mass = d.mfAttachedBodyMass;
                    s.attached_body_gravity = d.mbAttachedBodyGravity != 0;
                    s.attached_entity_full_game_saved = d.mbAttachedEntityFullGameSaved != 0;
                    s.set_mtx_time = d.mfSetMtxTime;
                }
                Pod::Slime(s) => {
                    let d = &*data.cast::<lay::cLuxArea_SlimeDamage_SaveData>();
                    s.slime_type = d.mlSlimeType;
                    s.min_check_attack_time = d.mfMinCheckAttackTime;
                    s.max_check_attack_time = d.mfMaxCheckAttackTime;
                    s.disable_after_attack = d.mbDisableAfterAttack != 0;
                    self.strs[st::SLIME_CALLBACK] = read_str(&d.msCallback);
                    s.check_collision_count = d.mfCheckCollisionCount;
                    self.strs[st::SLIME_SOUND] = read_str(&d.msAttackSound);
                    self.strs[st::SLIME_PS] = read_str(&d.msAttackPS);
                    s.min_attack_damage = d.mfMinAttackDamage;
                    s.max_attack_damage = d.mfMaxAttackDamage;
                    s.screen_shake_amount = d.mfScreenShakeAmount;
                }
                Pod::Insanity(s) => {
                    let d = &*data.cast::<lay::cLuxArea_Insanity_SaveData>();
                    s.auto_disable = d.mbAutoDisable != 0;
                }
                Pod::Script(s) => {
                    let d = &*data.cast::<lay::cLuxArea_Script_SaveData>();
                    s.item_interaction = d.mbItemInteraction != 0;
                }
                Pod::Examine(_) => {
                    let d = &*data.cast::<lay::cLuxArea_Examine_SaveData>();
                    self.strs[st::DESC_CAT] = read_str(&d.msDescCat);
                    self.strs[st::DESC_ENTRY] = read_str(&d.msDescEntry);
                    self.strs[st::INSANE_CAT] = read_str(&d.msDescInsaneCat);
                    self.strs[st::INSANE_ENTRY] = read_str(&d.msDescInsaneEntry);
                    self.strs[st::EXAMINE_SOUND] = read_str(&d.msSound);
                    self.strs[st::INSANE_SOUND] = read_str(&d.msInsaneSound);
                }
                Pod::Sign(_) => {
                    let d = &*data.cast::<lay::cLuxArea_Sign_SaveData>();
                    self.strs[st::TEXT_CAT] = read_str(&d.msTextCat);
                    self.strs[st::TEXT_ENTRY] = read_str(&d.msTextEntry);
                }
                Pod::Ladder(_) => {
                    let d = &*data.cast::<lay::cLuxArea_Ladder_SaveData>();
                    self.strs[st::MATERIAL] = read_str(&d.msMaterial);
                }
                Pod::Rope(s) => {
                    let d = &*data.cast::<lay::cLuxRope_SaveData>();
                    s.rope_created = d.mbRopeCreated != 0;
                    s.start_pos = unvec3(d.mvStartPos);
                    s.end_pos = unvec3(d.mvEndPos);
                    self.strs[st::END_POS_NODE] = read_str(&d.msEndPosNode);
                    self.strs[st::START_BODY] = read_str(&d.msStartBody);
                    self.strs[st::END_BODY] = read_str(&d.msEndBody);
                    s.min_total_length = d.mfMinTotalLength;
                    s.max_total_length = d.mfMaxTotalLength;
                    s.segment_length = d.mfSegmentLength;
                    s.damping = d.mfDamping;
                    s.strength = d.mfStrength;
                    s.stiffness = d.mfStiffness;
                    self.strs[st::ROPE_MATERIAL] = read_str(&d.msMaterial);
                    s.radius = d.mfRadius;
                    s.length_tile_amount = d.mfLengthTileAmount;
                    s.length_tile_size = d.mfLengthTileSize;
                    self.strs[st::ROPE_SOUND] = read_str(&d.msSound);
                    s.sound_start_speed = d.mfSoundStartSpeed;
                    s.sound_stop_speed = d.mfSoundStopSpeed;
                    s.auto_move = d.mbAutoMove != 0;
                    s.auto_move_acc = d.mfAutoMoveAcc;
                    s.auto_move_max_speed = d.mfAutoMoveMaxSpeed;
                }
                Pod::Flashback(_) => {
                    let d = &*data.cast::<lay::cLuxArea_Flashback_SaveData>();
                    self.strs[st::FLASHBACK_FILE] = read_str(&d.msFlashbackFile);
                    self.strs[st::FLASHBACK_CALLBACK] = read_str(&d.msCallback);
                }
            }
        }
    }
}

// ---- C ABI

/// # Safety
/// `a` live; `name` readable for `n` bytes; `vt` valid for the call (see [`V::new`]).
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a7_area_init(
    a: *mut A7Area,
    name: *const u8,
    n: usize,
    vt: *const Vt,
) -> bool {
    // SAFETY: caller contract.
    unsafe {
        let name = std::slice::from_raw_parts(name, n);
        (*a).init(name, &V::new(&*vt))
    }
}

/// # Safety
/// `a` a live rope block; `start` readable; `vt` valid for the call.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a7_rope_load(a: *mut A7Area, start: *const Vector3f, vt: *const Vt) {
    // SAFETY: caller contract.
    unsafe { (*a).load_rope(*start, &V::new(&*vt)) };
}

/// # Safety
/// `a` live; `vt` valid for the call.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a7_area_load_vars(a: *mut A7Area, vt: *const Vt) {
    // SAFETY: caller contract.
    unsafe { (*a).load_vars(&V::new(&*vt)) };
}

/// # Safety
/// `a` live; `data` a save data object of the area's type; `vt` valid for the call.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a7_area_save(a: *mut A7Area, data: *mut u8, vt: *const Vt) {
    // SAFETY: caller contract.
    unsafe { (*a).save(data, &V::new(&*vt)) };
}

/// # Safety
/// `a` live; `data` a save data object of the area's type; `vt` valid for the call.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a7_area_load(a: *mut A7Area, data: *const u8, vt: *const Vt) {
    // SAFETY: caller contract.
    unsafe { (*a).load(data, &V::new(&*vt)) };
}

/// The attached body of a sticky area, or null.
///
/// # Safety
/// `a` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a7_area_sticky_attached(a: *const A7Area) -> usize {
    // SAFETY: caller contract.
    match unsafe { &(*a).pod } {
        Pod::Sticky(s) => s.attached_body,
        _ => 0,
    }
}
