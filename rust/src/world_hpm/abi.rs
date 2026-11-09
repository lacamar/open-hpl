//! `#[repr(C)]` mirrors of `HPL2/core/include/resources/HpmBuilder.h`. Strings are NUL-terminated
//! and arrays (pointer, length); both are valid only for the duration of the builder call.

use std::ffi::c_char;

pub type Cstr = *const c_char;

#[repr(C)]
pub struct Fog {
    pub active: bool,
    pub color: [f32; 4],
    pub falloff_exp: f32,
    pub fade_start: f32,
    pub fade_end: f32,
    pub culling: bool,
    pub brightness: f32,
    pub underwater: bool,
    pub use_skybox: bool,
    pub apply_after_fog_areas: bool,
    pub noise_strength: f32,
    pub noise_size: f32,
    pub noise_turbulence: [f32; 3],
    pub secondary_active: bool,
    pub secondary_color: [f32; 4],
    pub secondary_fade_start: f32,
    pub secondary_fade_end: f32,
    pub secondary_falloff_exp: f32,
    pub lighten: bool,
    pub height_based: bool,
    pub exponential: bool,
    pub density: f32,
    pub height_density: f32,
    pub height_horizon: f32,
    pub secondary_density: f32,
    pub secondary_height_density: f32,
    pub secondary_height_horizon: f32,
}

#[repr(C)]
pub struct DistanceCull {
    pub active: bool,
    pub min_range: f32,
    pub screen_size: f32,
    pub random_size: f32,
}

#[repr(C)]
pub struct DirLight {
    pub active: bool,
    pub diffuse_color: [f32; 4],
    pub brightness: f32,
    pub direction: [f32; 3],
    pub sky_color: [f32; 4],
    pub ground_color: [f32; 4],
    pub cast_shadows: bool,
    pub shadow_caster_dist: f32,
    pub shadow_map_bias_mul: f32,
    pub shadow_map_slope_scale_bias_mul: f32,
    pub shadow_blur_amount: f32,
    pub auto_shadow_slice_settings: bool,
    pub auto_shadow_slice_log_term: f32,
}

#[repr(C)]
pub struct EnvParticles {
    pub texture: Cstr,
    pub sub_div_uv: [f32; 2],
    pub box_size: f32,
    pub num_particles: i32,
    pub particle_size: [f32; 2],
    pub affected_by_light: bool,
    pub color: [f32; 4],
    pub brightness: f32,
    pub box_distance: f32,
    pub gravity_velocity: [f32; 3],
    pub gravity_speed_random_amount: f32,
    pub wind_velocity: [f32; 3],
    pub wind_speed_random_amount: f32,
    pub wind_dir_random_amount: f32,
    pub rotate_velocity: [f32; 3],
    pub rotate_speed_random_amount: f32,
    pub rotate_both_dirs: bool,
    pub num_iterations: f32,
    pub fade_in_start: f32,
    pub fade_in_end: f32,
    pub fade_out_start: f32,
    pub fade_out_end: f32,
    pub active: bool,
}

#[repr(C)]
pub struct PostEffects {
    pub tone_mapping_key: f32,
    pub tone_mapping_exposure: f32,
    pub tone_mapping_white_cut: f32,
    pub color_grading_texture: Cstr,
}

#[repr(C)]
pub struct SkyBox {
    pub active: bool,
    pub color: [f32; 4],
    pub brightness: f32,
    pub texture: Cstr,
}

#[repr(C)]
pub struct StaticObject {
    pub name: Cstr,
    pub filename: Cstr,
    pub position: [f32; 3],
    pub scale: [f32; 3],
    pub rotation: [f32; 3],
    pub casts_shadows: bool,
    pub collides: bool,
    pub id: i32,
    pub culled_by_distance: bool,
    pub culled_by_fog: bool,
    pub color_mul: [f32; 4],
    pub illum_brightness: f32,
    pub illum_color: [f32; 4],
    pub has_pose: bool,
    pub pose: *const f32,
    pub num_pose: usize,
}

#[repr(C)]
pub struct Plane {
    pub name: Cstr,
    pub material: Cstr,
    pub casts_shadows: bool,
    pub collides: bool,
    pub id: i32,
    pub position: [f32; 3],
    pub scale: [f32; 3],
    pub rotation: [f32; 3],
    pub start_corner: [f32; 3],
    pub end_corner: [f32; 3],
    pub uv: [[f32; 2]; 4],
    pub culled_by_distance: bool,
    pub culled_by_fog: bool,
}

#[repr(C)]
pub struct Entity {
    pub name: Cstr,
    pub filename: Cstr,
    pub id: i32,
    pub active: bool,
    pub is_static: bool,
    pub position: [f32; 3],
    pub scale: [f32; 3],
    pub rotation: [f32; 3],
    pub culled_by_distance: bool,
    pub culled_by_fog: bool,
}

#[repr(C)]
pub struct Area {
    pub name: Cstr,
    pub area_type: Cstr,
    pub id: i32,
    pub active: bool,
    pub position: [f32; 3],
    pub scale: [f32; 3],
    pub rotation: [f32; 3],
}

#[repr(C)]
pub struct Decal {
    pub name: Cstr,
    pub material: Cstr,
    pub color: [f32; 4],
    pub id: i32,
}

#[repr(C)]
pub struct LightMask {
    pub id: u32,
    pub world_pos: [f32; 3],
    pub rotation: [f32; 3],
    pub size: [f32; 3],
    pub scale: [f32; 3],
}

#[repr(C)]
pub struct ExposureArea {
    pub name: Cstr,
    pub rotation: [f32; 3],
    pub world_pos: [f32; 3],
    pub scale: [f32; 3],
    pub exposure: f32,
    pub white_cut: f32,
    pub transition_time: f32,
}

#[repr(C)]
pub struct DetailMeshes {
    pub file: Cstr,
    pub num: i32,
    pub first_index: i32,
    pub positions: *const f32,
    pub rotations: *const f32,
    pub radii: *const f32,
    pub num_radii: usize,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct TerrainMat {
    pub file: Cstr,
    pub tile_amount: f32,
    pub start_fade_value: f32,
    pub detail_amount: [f32; 3],
}

#[repr(C)]
pub struct TerrainLayer {
    pub id: Cstr,
    pub num_mats: i32,
    pub mats: [TerrainMat; 4],
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct TerrainDetail {
    pub file: Cstr,
    pub scale: f32,
}

#[repr(C)]
pub struct TerrainDecal {
    pub file: Cstr,
    pub position: [f32; 2],
    pub size: [f32; 2],
    pub angle: f32,
    pub color: [f32; 4],
    pub current_uv_sub_div: i32,
}

#[repr(C)]
pub struct UndergrowthArea {
    pub file: Cstr,
    pub sub: bool,
    pub max_influence: f32,
    pub fade_border: f32,
    pub circle: bool,
    pub center: [f32; 2],
    pub radius: f32,
    pub points: *const f32,
    pub num_points: usize,
}

#[repr(C)]
pub struct Terrain {
    pub height_map_size: i32,
    pub geometry_patch_size: i32,
    pub unit_size: f32,
    pub max_height: f32,
    pub base_material_tile_amount: f32,
    pub base_material_file: Cstr,
    pub base_material_detail_texture_amount: [f32; 3],
    pub has_blend_layers: bool,
    pub layers: *const TerrainLayer,
    pub num_layers: usize,
    pub has_details: bool,
    pub detail_fade_start: f32,
    pub detail_fade_end: f32,
    pub details: [TerrainDetail; 3],
    pub num_details: i32,
    pub has_sections: bool,
    pub decals: *const TerrainDecal,
    pub num_decals: usize,
    pub undergrowth_grid_size: f32,
    pub undergrowth_fade_start: f32,
    pub undergrowth_fade_end: f32,
    pub areas: *const UndergrowthArea,
    pub num_areas: usize,
}

#[repr(C)]
pub struct Skip {
    pub reason: Cstr,
    pub count: i32,
}
