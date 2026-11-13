//! Area state under `OHPL_IMPL=rust`: one enum over the area families, each variant a `repr(C)`
//! block the C++ shell binds its members to as references (so save and loader glue keep reading
//! the members by name). Blocks live in the per-map arena, boxed so their addresses are stable.
//! Strings, bodies and engine objects stay C++; the state machines below read and write the blocks.

use super::area::{self, ExamineIn, ExamineOut, StickyAttachIn, StickyAttachOut};
use crate::obj::{Arena, Handle};
use crate::types::{Matrixf, Vector3f};
use std::ffi::c_void;

pub const T_SCRIPT: i32 = 0;
pub const T_EXAMINE: i32 = 1;
pub const T_INSANITY: i32 = 2;
pub const T_FLASHBACK: i32 = 3;
pub const T_SIGN: i32 = 4;
pub const T_LADDER: i32 = 5;
pub const T_LIQUID: i32 = 6;
pub const T_STICKY: i32 = 7;
pub const T_SLIME: i32 = 8;

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Base {
    pub size: Vector3f,
    pub mtx: Matrixf,
    pub rel_offset: Vector3f,
    pub body_id: i32,
    pub area_type: i32,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct ScriptS {
    pub item_interaction: bool,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct ExamineS {
    pub insane_limit: f32,
    pub play_sound_count: f32,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct InsanityS {
    pub check_time_min: f32,
    pub check_time_max: f32,
    pub check_collision_count: f32,
    pub auto_disable: bool,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct FlashbackS {
    pub check_collision_count: f32,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct SignS {
    pub unused: u8,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct LadderS {
    pub forward: Vector3f,
    pub max_y: f32,
    pub min_y: f32,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct LiquidS {
    pub density: f32,
    pub linear_viscosity: f32,
    pub angular_viscosity: f32,
    pub wave_amp: f32,
    pub wave_freq: f32,
    pub player_speed_mul: f32,
    pub max_wave_distance_sqr: f32,
    pub time_count: f32,
    pub surface_plane: [f32; 4],
    pub has_waves: bool,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct StickyS {
    pub pose_time: f32,
    pub attached_body_mass: f32,
    pub set_mtx_time: f32,
    pub attached_start: Matrixf,
    pub move_body: bool,
    pub rotate_body: bool,
    pub check_center_in_area: bool,
    pub can_detach: bool,
    pub attached_body_gravity: bool,
    pub attached_entity_full_game_saved: bool,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct SlimeS {
    pub slime_type: i32,
    pub min_check_attack_time: f32,
    pub max_check_attack_time: f32,
    pub check_collision_count: f32,
    pub min_attack_damage: f32,
    pub max_attack_damage: f32,
    pub screen_shake_amount: f32,
    pub disable_after_attack: bool,
}

#[derive(Clone, Copy, Debug, PartialEq)]
pub enum Pod {
    Script(ScriptS),
    Examine(ExamineS),
    Insanity(InsanityS),
    Flashback(FlashbackS),
    Sign(SignS),
    Ladder(LadderS),
    Liquid(LiquidS),
    Sticky(StickyS),
    Slime(SlimeS),
}

impl Pod {
    pub fn new(kind: i32) -> Option<Self> {
        Some(match kind {
            T_SCRIPT => Self::Script(ScriptS::default()),
            T_EXAMINE => Self::Examine(ExamineS::default()),
            T_INSANITY => Self::Insanity(InsanityS::default()),
            T_FLASHBACK => Self::Flashback(FlashbackS::default()),
            T_SIGN => Self::Sign(SignS::default()),
            T_LADDER => Self::Ladder(LadderS::default()),
            T_LIQUID => Self::Liquid(LiquidS::default()),
            T_STICKY => Self::Sticky(StickyS::default()),
            T_SLIME => Self::Slime(SlimeS::default()),
            _ => return None,
        })
    }

    pub fn size_of(kind: i32) -> usize {
        use std::mem::size_of;
        match kind {
            T_SCRIPT => size_of::<ScriptS>(),
            T_EXAMINE => size_of::<ExamineS>(),
            T_INSANITY => size_of::<InsanityS>(),
            T_FLASHBACK => size_of::<FlashbackS>(),
            T_SIGN => size_of::<SignS>(),
            T_LADDER => size_of::<LadderS>(),
            T_LIQUID => size_of::<LiquidS>(),
            T_STICKY => size_of::<StickyS>(),
            T_SLIME => size_of::<SlimeS>(),
            _ => 0,
        }
    }

    fn ptr(&mut self) -> *mut c_void {
        match self {
            Self::Script(s) => std::ptr::from_mut(s).cast(),
            Self::Examine(s) => std::ptr::from_mut(s).cast(),
            Self::Insanity(s) => std::ptr::from_mut(s).cast(),
            Self::Flashback(s) => std::ptr::from_mut(s).cast(),
            Self::Sign(s) => std::ptr::from_mut(s).cast(),
            Self::Ladder(s) => std::ptr::from_mut(s).cast(),
            Self::Liquid(s) => std::ptr::from_mut(s).cast(),
            Self::Sticky(s) => std::ptr::from_mut(s).cast(),
            Self::Slime(s) => std::ptr::from_mut(s).cast(),
        }
    }
}

pub struct A7Area {
    pub base: Base,
    pub pod: Pod,
    handle: Option<Handle<Box<A7Area>>>,
}

impl A7Area {
    pub fn new(kind: i32) -> Option<Self> {
        Some(Self {
            base: Base {
                area_type: kind,
                ..Base::default()
            },
            pod: Pod::new(kind)?,
            handle: None,
        })
    }

    pub fn base_ptr(&mut self) -> *mut Base {
        &raw mut self.base
    }

    pub fn pod_ptr(&mut self) -> *mut c_void {
        self.pod.ptr()
    }

    // ---- base

    /// `AttachToBody` success: the body's offset from its parent.
    pub fn attach(&mut self, body_pos: Vector3f, parent_pos: Vector3f) {
        self.base.rel_offset = area::attach_offset(body_pos, parent_pos);
    }

    /// `iLuxArea::OnUpdate` with a parent body: how far the area has to move.
    pub fn follow(&self, parent_pos: Vector3f, body_pos: Vector3f) -> Vector3f {
        area::follow_offset(self.base.rel_offset, parent_pos, body_pos)
    }

    // ---- per-family decisions

    pub fn can_interact(&self, has_interact_callback: bool) -> bool {
        match &self.pod {
            Pod::Script(s) => area::script_can_interact(s.item_interaction, has_interact_callback),
            Pod::Examine(_) | Pod::Sign(_) | Pod::Ladder(_) => true,
            _ => false,
        }
    }

    pub fn on_interact_script(&self, has_interact_callback: bool) -> bool {
        matches!(self.pod, Pod::Script(_)) && has_interact_callback
    }

    /// Crosshair of a family; `tbl` is default, grab, ladder, base answer.
    pub fn crosshair(&self, has_callback: bool, custom: i32, tbl: &[i32; 4]) -> i32 {
        let [default, grab, ladder, last] = *tbl;
        match &self.pod {
            Pod::Script(_) => area::script_crosshair(has_callback, custom, default, grab),
            Pod::Examine(_) => grab,
            Pod::Sign(_) => default,
            Pod::Ladder(_) => ladder,
            _ => last,
        }
    }

    /// Per-update countdown: whether the family's collision check runs this step.
    pub fn tick(&mut self, step: f32, engine_step: f32) -> bool {
        match &mut self.pod {
            Pod::Insanity(s) => area::tick_random(
                &mut s.check_collision_count,
                step,
                s.check_time_min,
                s.check_time_max,
            ),
            Pod::Slime(s) => area::tick_random(
                &mut s.check_collision_count,
                step,
                s.min_check_attack_time,
                s.max_check_attack_time,
            ),
            Pod::Flashback(s) => {
                !area::step_too_small(step, engine_step)
                    && area::tick_fixed(&mut s.check_collision_count, step, 0.1)
            }
            Pod::Examine(s) => {
                area::examine_tick(&mut s.play_sound_count, step);
                false
            }
            Pod::Liquid(s) => {
                if area::step_too_small(step, engine_step) {
                    return false;
                }
                s.time_count += step;
                true
            }
            _ => false,
        }
    }

    pub fn examine_interact(&mut self, i: ExamineIn, sanity: f32) -> ExamineOut {
        let Pod::Examine(s) = &mut self.pod else {
            return ExamineOut::default();
        };
        let out = area::examine_interact(i, sanity, s.insane_limit, s.play_sound_count);
        s.play_sound_count = 0.5;
        out
    }

    pub fn ladder_setup(&mut self, world: &Matrixf, pos_y: f32, size_y: f32) {
        if let Pod::Ladder(s) = &mut self.pod {
            let l = area::ladder_setup(world, pos_y, size_y);
            s.forward = l.forward;
            s.max_y = l.max_y;
            s.min_y = l.min_y;
        }
    }

    pub fn ladder_start(&self, area_pos: Vector3f, char_size: Vector3f, char_y: f32) -> Vector3f {
        match &self.pod {
            Pod::Ladder(s) => {
                area::ladder_start_pos(area_pos, s.forward, char_size, char_y, s.max_y, s.min_y)
            }
            _ => area_pos,
        }
    }

    pub fn liquid_setup(&mut self, pos_y: f32, size_y: f32) {
        if let Pod::Liquid(s) = &mut self.pod {
            let p = area::liquid_surface_plane(pos_y, size_y);
            s.surface_plane = [p.a, p.b, p.c, p.d];
        }
    }

    pub fn sticky_can_detach(&self) -> bool {
        matches!(&self.pod, Pod::Sticky(s) if s.can_detach && s.set_mtx_time >= 1.0)
    }

    /// `AttachBody` bookkeeping once the body is known.
    pub fn sticky_attach(
        &mut self,
        start: Matrixf,
        gravity: bool,
        mass: f32,
        entity_saved: Option<bool>,
    ) {
        if let Pod::Sticky(s) = &mut self.pod {
            s.attached_start = start;
            s.set_mtx_time = 0.0;
            s.attached_body_gravity = gravity;
            s.attached_body_mass = mass;
            s.attached_entity_full_game_saved = entity_saved.unwrap_or(false);
        }
    }

    /// `UpdateAttachBody` step: `None` while the pose is already complete.
    pub fn sticky_step(
        &mut self,
        area_world: Matrixf,
        area_pos: Vector3f,
        mass_centre: Vector3f,
        attached_pos: Vector3f,
        step: f32,
    ) -> Option<StickyAttachOut> {
        let Pod::Sticky(s) = &mut self.pod else {
            return None;
        };
        if s.set_mtx_time >= 1.0 {
            return None;
        }
        let out = area::sticky_attach(&StickyAttachIn {
            start: s.attached_start,
            area_world,
            area_pos,
            mass_centre,
            attached_pos,
            pose_time: s.pose_time,
            step,
            set_time: s.set_mtx_time,
            move_body: s.move_body,
            rotate_body: s.rotate_body,
        });
        s.set_mtx_time = out.set_time;
        Some(out)
    }
}

/// Boxed blocks of one map; a block outlives its handle only until `free`.
#[derive(Default)]
pub struct AreaArena {
    slots: Arena<Box<A7Area>>,
}

impl AreaArena {
    pub fn alloc(&mut self, kind: i32) -> Option<*mut A7Area> {
        let h = self.slots.insert(Box::new(A7Area::new(kind)?))?;
        let a = self.slots.get_mut(h)?;
        a.handle = Some(h);
        Some(&raw mut **a)
    }

    /// # Safety
    /// `p` came from `alloc` on this arena and is not used afterwards.
    pub unsafe fn free(&mut self, p: *mut A7Area) {
        // SAFETY: caller contract; the block is still live.
        let h = unsafe { (*p).handle };
        if let Some(h) = h {
            self.slots.remove(h);
        }
    }

    pub fn len(&self) -> usize {
        self.slots.len()
    }

    pub fn is_empty(&self) -> bool {
        self.slots.is_empty()
    }
}
