//! The world-level part of `cHpmWorldBuilder`: global settings, the entity loaders and the light
//! connections.

use super::efl::{
    KIND_BILLBOARD, KIND_LIGHT, KIND_SOUND, host, load_billboard, load_fog_area, load_lens_flare,
    load_light, load_particle_system, load_sound, pods, transform_updated,
};
use super::ls_light;
use super::sl::{BbPod, LightPod};
use super::wd::World;
use crate::types::{Color, Vector3f};
use crate::world_hpm::abi;
use crate::world_hpm::el::El;
use crate::world_hpm::h4_plan::light_mask;
use std::collections::BTreeMap;
use std::ffi::c_void;

type P = *mut c_void;

const REG_LIGHT: usize = 0;
const REG_BILLBOARD: usize = 3;

fn v3(a: [f32; 3]) -> Vector3f {
    Vector3f {
        x: a[0],
        y: a[1],
        z: a[2],
    }
}

fn c4(a: [f32; 4]) -> Color {
    Color {
        r: a[0],
        g: a[1],
        b: a[2],
        a: a[3],
    }
}

pub(crate) struct HpmEnv {
    w: *mut World,
    billboards: Vec<(i32, Vec<u8>)>,
    particles: Vec<(P, Vec<u8>)>,
    masks: BTreeMap<u32, (Vector3f, Vector3f)>,
}

impl HpmEnv {
    /// # Safety
    /// `w` outlives the loader.
    pub(crate) unsafe fn new(w: *mut World) -> Self {
        Self {
            w,
            billboards: Vec::new(),
            particles: Vec::new(),
            masks: BTreeMap::new(),
        }
    }

    pub(crate) fn global_fog(&mut self, f: &abi::Fog) {
        // SAFETY: `new` contract.
        let p = unsafe { &mut (*self.w).pod };
        p.fog_active = f.active;
        p.fog_color = c4(f.color);
        p.fog_falloff = f.falloff_exp;
        p.fog_start = f.fade_start;
        p.fog_end = f.fade_end;
        p.fog_culling = f.culling;
        p.fog_brightness = if f.brightness > 0.0001 {
            f.brightness
        } else {
            0.0001
        };
        p.fog_underwater = f.underwater;
        p.fog_use_sky = f.use_skybox;
        p.fog_after_areas = f.apply_after_fog_areas;
        p.fog_noise_strength = f.noise_strength;
        p.fog_noise_size = f.noise_size;
        p.fog_noise_turbulence = v3(f.noise_turbulence);
        p.fog2_active = f.secondary_active;
        p.fog2_color = c4(f.secondary_color);
        p.fog2_start = f.secondary_fade_start;
        p.fog2_end = f.secondary_fade_end;
        p.fog2_falloff = f.secondary_falloff_exp;
        p.fog_lighten = f.lighten;
        p.fog_height_based = f.height_based;
        p.fog_exponential = f.exponential;
        p.fog_density = f.density;
        p.fog_height_density = f.height_density;
        p.fog_height_horizon = f.height_horizon;
        p.fog2_density = f.secondary_density;
        p.fog2_height_density = f.secondary_height_density;
        p.fog2_height_horizon = f.secondary_height_horizon;
    }

    pub(crate) fn distance_cull(&mut self, c: &abi::DistanceCull) {
        // SAFETY: `new` contract.
        let p = unsafe { &mut (*self.w).pod };
        p.cull_active = c.active;
        p.cull_min = c.min_range;
        p.cull_screen = c.screen_size;
        p.cull_random = c.random_size;
    }

    pub(crate) fn light(&mut self, el: &El) -> bool {
        let Some(h) = host() else { return false };
        // SAFETY: `new` contract.
        unsafe { !load_light(h, el, b"", self.w, true).is_null() }
    }

    pub(crate) fn sound(&mut self, el: &El, active: bool) -> bool {
        let Some(h) = host() else { return false };
        // SAFETY: `new` contract.
        unsafe {
            let p = load_sound(h, el, b"", self.w);
            if !p.is_null() && !active {
                (*pods(h, KIND_SOUND, p).ent).s.active = false;
            }
            !p.is_null()
        }
    }

    pub(crate) fn fog_area(&mut self, el: &El) -> bool {
        let Some(h) = host() else { return false };
        // SAFETY: `new` contract.
        unsafe { !load_fog_area(h, el, b"", self.w, true).is_null() }
    }

    pub(crate) fn lens_flare(&mut self, el: &El) -> bool {
        let Some(h) = host() else { return false };
        // SAFETY: `new` contract.
        unsafe { !load_lens_flare(h, el, b"", self.w, false).is_null() }
    }

    pub(crate) fn billboard(&mut self, el: &El) -> bool {
        let Some(h) = host() else { return false };
        // SAFETY: `new` contract.
        let (p, conn) = unsafe { load_billboard(h, el, b"", self.w, true, true) };
        self.billboards.extend(conn);
        !p.is_null()
    }

    pub(crate) fn particle_system(&mut self, el: &El, light: &[u8]) -> bool {
        let Some(h) = host() else { return false };
        // SAFETY: `new` contract.
        let p = unsafe { load_particle_system(h, el, b"", self.w) };
        if !p.is_null() && !light.is_empty() {
            self.particles.push((p, light.to_vec()));
        }
        !p.is_null()
    }

    pub(crate) fn light_mask_box(&mut self, m: &abi::LightMask) {
        let plan = light_mask(v3(m.rotation), v3(m.size), v3(m.scale), v3(m.world_pos));
        self.masks.insert(m.id, (plan.center, plan.extent));
    }

    fn find_light(&self, name: &[u8]) -> P {
        // SAFETY: `new` contract; the registry thunks only read names.
        unsafe {
            let reg = (*self.w).pod.reg;
            let f = (*reg).foreign(REG_LIGHT);
            (*reg).find_name(REG_LIGHT, name, false, |p| f.name_of(p)) as P
        }
    }

    pub(crate) fn connect_light_masks(&mut self) {
        let Some(h) = host() else { return };
        // SAFETY: `new` contract.
        unsafe {
            let lights = (*(*self.w).pod.reg).snapshot(REG_LIGHT);
            for p in lights {
                let o = pods(h, KIND_LIGHT, p as P);
                let l = &mut *o.pod.cast::<LightPod>();
                if let Some((center, size)) = self.masks.get(&l.mask_id) {
                    l.mask_box = true;
                    l.mask_center = *center;
                    l.mask_size = *size;
                    (*o.ent).s.update_bv = true;
                    transform_updated(h, o.ent);
                }
            }
        }
        self.masks.clear();
    }

    /// Billboards and particle systems left without their light.
    pub(crate) fn connect_lights(&mut self) -> (i32, i32) {
        let Some(h) = host() else { return (0, 0) };
        let (mut missing_bb, mut missing_ps) = (0, 0);
        // SAFETY: `new` contract.
        unsafe {
            let reg = (*self.w).pod.reg;
            let bbf = (*reg).foreign(REG_BILLBOARD);
            for (id, name) in std::mem::take(&mut self.billboards) {
                let bb = (*reg).find_id(REG_BILLBOARD, id, |p| bbf.id_of(p)) as P;
                let light = self.find_light(&name);
                if bb.is_null() || light.is_null() {
                    missing_bb += 1;
                    continue;
                }
                let color = (*pods(h, KIND_BILLBOARD, bb).pod.cast::<BbPod>()).color;
                ls_light::ohpl_ls_light_attach_billboard(
                    pods(h, KIND_LIGHT, light).pod.cast(),
                    bb as usize,
                    &color,
                );
            }
            for (ps, name) in std::mem::take(&mut self.particles) {
                let light = self.find_light(&name);
                if light.is_null() {
                    missing_ps += 1;
                } else {
                    ls_light::ohpl_ls_light_attach_ps(
                        pods(h, KIND_LIGHT, light).pod.cast(),
                        ps as usize,
                    );
                }
            }
        }
        (missing_bb, missing_ps)
    }
}
