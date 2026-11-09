//! `cWorldLoaderHpm`'s parse half: reads the `.hpm` file and its per-type sidecars and drives a
//! [`Builder`]. Every engine object, log line and file read stays behind the builder.

use super::abi::{self, Cstr};
use super::el::El;
use crate::string;
use crate::types::{Color, Vector2f, Vector3f};
use crate::xml::Doc;
use std::collections::BTreeMap;
use std::ffi::{CString, c_char, c_int};

unsafe extern "C" {
    fn snprintf(buf: *mut c_char, n: usize, fmt: *const c_char, ...) -> c_int;
    fn strtoul(s: *const c_char, end: *mut *mut c_char, base: c_int) -> std::ffi::c_ulong;
}

pub enum FileRead {
    NotOpened,
    Read(Vec<u8>),
    Short(Vec<u8>),
}

/// Engine side of the loader. `bool` results say whether the object was made; elements handed
/// over are the ones the engine's own loaders still read.
pub trait Builder {
    fn now_ms(&mut self) -> u64;
    fn object_done(&mut self);
    fn warn(&mut self, msg: &[u8]);
    fn log(&mut self, msg: &[u8]);
    fn read_file(&mut self, path: &[u32]) -> FileRead;

    fn global_fog(&mut self, v: &abi::Fog);
    fn distance_cull(&mut self, v: &abi::DistanceCull);
    fn dir_light(&mut self, v: &abi::DirLight);
    fn env_particles(&mut self, v: &abi::EnvParticles);
    fn post_effects(&mut self, v: &abi::PostEffects);
    fn sky_box(&mut self, v: &abi::SkyBox);

    fn static_object(&mut self, v: &abi::StaticObject) -> bool;
    fn plane(&mut self, v: &abi::Plane) -> bool;
    fn entity(&mut self, v: &abi::Entity, el: &El) -> bool;
    fn light(&mut self, el: &El) -> bool;
    fn area(&mut self, v: &abi::Area, el: &El) -> bool;
    fn sound(&mut self, el: &El, active: bool) -> bool;
    fn decal(&mut self, v: &abi::Decal, mesh: Option<&El>) -> bool;
    fn billboard(&mut self, el: &El) -> bool;
    fn particle_system(&mut self, el: &El, connect_light: Cstr) -> bool;
    fn fog_area(&mut self, el: &El) -> bool;
    fn lens_flare(&mut self, el: &El) -> bool;
    fn light_mask_box(&mut self, v: &abi::LightMask);

    /// Billboards and particle systems left without their light.
    fn connect_lights(&mut self) -> (i32, i32);
    fn connect_light_masks(&mut self);

    /// Instances made; stops at the first mesh that fails to load.
    fn detail_meshes(&mut self, v: &abi::DetailMeshes) -> i32;
    fn exposure_area(&mut self, v: &abi::ExposureArea);
    fn terrain(&mut self, v: &abi::Terrain);

    fn track_report(
        &mut self,
        track: Cstr,
        in_xml: i32,
        created: i32,
        time_ms: i32,
        file_missing: bool,
        skipped: &[abi::Skip],
    );
}

#[derive(Default)]
struct Stats {
    in_xml: i32,
    created: i32,
    time_ms: i32,
    file_missing: bool,
    skipped: BTreeMap<Vec<u8>, i32>,
}

fn cs(b: &[u8]) -> CString {
    CString::new(crate::xml::c_str(b)).unwrap_or_default()
}

fn splat3(x: f32) -> Vector3f {
    Vector3f { x, y: x, z: x }
}

fn splat2(x: f32) -> Vector2f {
    Vector2f { x, y: x }
}

fn gray(r: f32, a: f32) -> Color {
    Color { r, g: r, b: r, a }
}

fn arr3(v: Vector3f) -> [f32; 3] {
    [v.x, v.y, v.z]
}

fn arr2(v: Vector2f) -> [f32; 2] {
    [v.x, v.y]
}

fn arr4(c: Color) -> [f32; 4] {
    [c.r, c.g, c.b, c.a]
}

fn cat(parts: &[&[u8]]) -> Vec<u8> {
    parts.concat()
}

fn num(x: i32) -> Vec<u8> {
    x.to_string().into_bytes()
}

fn vec3_to_string(v: Vector3f) -> Vec<u8> {
    let mut buf = [0u8; 512];
    // SAFETY: the buffer holds 512 bytes and the format takes three doubles.
    let n = unsafe {
        snprintf(
            buf.as_mut_ptr().cast(),
            buf.len(),
            c"%f : %f : %f".as_ptr(),
            f64::from(v.x),
            f64::from(v.y),
            f64::from(v.z),
        )
    };
    let len = usize::try_from(n).unwrap_or(0).min(buf.len() - 1);
    buf.get(..len).unwrap_or_default().to_vec()
}

fn strtoul_u32(s: &[u8]) -> u32 {
    let s = cs(s);
    // SAFETY: `s` is NUL-terminated.
    unsafe { strtoul(s.as_ptr(), std::ptr::null_mut(), 10) as u32 }
}

#[cfg(test)]
pub(super) fn widen(s: &str) -> Vec<u32> {
    s.bytes().map(u32::from).collect()
}

fn float_list(s: &[u8], seps: Option<&[u8]>) -> Vec<f32> {
    string::split(s, seps, true)
        .map(string::token_to_float)
        .collect()
}

pub fn parse_map<B: Builder>(b: &mut B, base: &[u32]) {
    let mut p = Parser {
        b,
        stats: BTreeMap::new(),
    };
    p.run(base);
}

struct Parser<'a, B: Builder> {
    b: &'a mut B,
    stats: BTreeMap<Vec<u8>, Stats>,
}

impl<B: Builder> Parser<'_, B> {
    fn run(&mut self, base: &[u32]) {
        self.global_settings(base);

        self.track(base, b"StaticObject", b"FileIndex_StaticObjects");
        self.track(base, b"Primitive", b"");
        self.track(base, b"Entity", b"FileIndex_Entities");
        self.track(base, b"Light", b"");
        self.track(base, b"Area", b"");
        self.track(base, b"Sound", b"");
        self.track(base, b"Decal", b"FileIndex_Decals");
        self.track(base, b"Billboard", b"");
        self.track(base, b"ParticleSystem", b"");
        self.track(base, b"FogArea", b"");
        self.connect_lights();
        self.track(base, b"LightMask", b"");
        self.b.connect_light_masks();

        self.track(base, b"Compound", b"");
        self.track(base, b"LensFlare", b"");
        self.track(base, b"StaticComboArea", b"");
        self.count_unsupported_flat_tracks(base);
        self.detail_meshes_track(base);

        self.exposure_area_track(base);
        self.terrain(base);

        for (name, s) in &self.stats {
            let reasons: Vec<CString> = s.skipped.keys().map(|r| cs(r)).collect();
            let skipped: Vec<abi::Skip> = reasons
                .iter()
                .zip(s.skipped.values())
                .map(|(r, &count)| abi::Skip {
                    reason: r.as_ptr(),
                    count,
                })
                .collect();
            self.b.track_report(
                cs(name).as_ptr(),
                s.in_xml,
                s.created,
                s.time_ms,
                s.file_missing,
                &skipped,
            );
        }
    }

    fn warn(&mut self, parts: &[&[u8]]) {
        let msg = cat(parts);
        self.b.warn(&msg);
    }

    fn stat(&mut self, track: &[u8]) -> &mut Stats {
        self.stats.entry(track.to_vec()).or_default()
    }

    /// `iXmlDocument::CreateFromFile` over the Rust document: failures are logged here.
    fn open_xml(&mut self, path: &[u32]) -> Option<El> {
        let mut doc = Doc::new();
        let loaded = match self.b.read_file(path) {
            FileRead::NotOpened => false,
            FileRead::Read(data) => doc.load(&data, true),
            FileRead::Short(data) => doc.load(&data, false),
        };
        let failure = if loaded {
            match El::from_doc(&doc) {
                Some(root) => return Some(root),
                None => (b"No root element".to_vec(), 0, 0),
            }
        } else {
            let e = doc.error_info();
            (e.desc.clone().into_bytes(), e.row, e.col)
        };
        let (desc, row, col) = failure;
        let path8 = string::wide_to_utf8(path);
        let msg = cat(&[
            b"Failed parsing of XML document ",
            &path8,
            b" in line ",
            &num(row),
            b", column ",
            &num(col),
            b": ",
            &desc,
            b"\n",
        ]);
        self.b.log(&msg);
        None
    }

    fn open_sidecar(&mut self, base: &[u32], suffix: &[u8], warn_if_missing: bool) -> Option<El> {
        let mut path = base.to_vec();
        path.extend(suffix.iter().copied().map(u32::from));
        let doc = self.open_xml(&path);
        if doc.is_none() && warn_if_missing {
            let path8 = string::wide_to_utf8(&path);
            self.warn(&[b"SOMA hpm: could not open/parse '", &path8, b"'\n"]);
        }
        doc
    }

    fn track_root<'e>(&mut self, doc: &'e El, expected: &[u8]) -> Option<&'e El> {
        if doc.name != expected {
            self.warn(&[
                b"SOMA hpm: sidecar file has unexpected root element '",
                &doc.name,
                b"' (expected '",
                expected,
                b"')\n",
            ]);
            return None;
        }
        Some(doc)
    }

    fn local_file_index(section: &El, name: &[u8]) -> Vec<Vec<u8>> {
        let Some(index) = section.first(name) else {
            return Vec::new();
        };
        // std::vector::resize with a negative count is length_error, uncaught
        let Ok(count) = usize::try_from(index.int(b"NumOfFiles", 0)) else {
            std::process::abort();
        };
        let mut out = vec![Vec::new(); count];
        for file in &index.children {
            let id = file.int(b"Id", 0);
            if let Some(slot) = usize::try_from(id).ok().and_then(|i| out.get_mut(i)) {
                *slot = file.string(b"Path");
            }
        }
        out
    }

    fn transform_valid(&mut self, name: &[u8], pos: Vector3f) -> bool {
        if pos.x.abs() > 10000.0 || pos.y.abs() > 10000.0 || pos.z.abs() > 10000.0 {
            let pos = vec3_to_string(pos);
            self.warn(&[
                b"SOMA hpm: object '",
                name,
                b"' has an invalid position: (",
                &pos,
                b")!\n",
            ]);
            return false;
        }
        true
    }

    fn global_settings(&mut self, base: &[u32]) {
        let Some(doc) = self.open_sidecar(base, b"", true) else {
            return;
        };
        let Some(root) = self.track_root(&doc, b"HPLMap") else {
            let base8 = string::wide_to_utf8(base);
            self.warn(&[
                b"SOMA hpm: root file '",
                &base8,
                b"' has no HPLMap element!\n",
            ]);
            return;
        };
        let Some(global) = root.first(b"GlobalSettings") else {
            return;
        };
        let white = gray(1.0, 1.0);
        let clear = gray(0.0, 0.0);

        if let Some(f) = global.first(b"Fog") {
            self.b.global_fog(&abi::Fog {
                active: f.boolean(b"Active", false),
                color: arr4(f.color(b"Color", white)),
                falloff_exp: f.float(b"FalloffExp", 1.0),
                fade_start: f.float(b"FadeStart", 0.0),
                fade_end: f.float(b"FadeEnd", 0.0),
                culling: f.boolean(b"Culling", true),
                brightness: f.float(b"Brightness", 1.0),
                underwater: f.boolean(b"Underwater", false),
                use_skybox: f.boolean(b"UseSkybox", false),
                apply_after_fog_areas: f.boolean(b"ApplyAfterFogAreas", true),
                noise_strength: f.float(b"NoiseStrength", 0.0),
                noise_size: f.float(b"NoiseSize", 1.0),
                noise_turbulence: arr3(f.vec3(b"NoiseTurbulence", splat3(0.0))),
                secondary_active: f.boolean(b"SecondaryActive", false),
                secondary_color: arr4(f.color(b"SecondaryColor", clear)),
                secondary_fade_start: f.float(b"SecondaryFadeStart", 0.0),
                secondary_fade_end: f.float(b"SecondaryFadeEnd", 0.0),
                secondary_falloff_exp: f.float(b"SecondaryFalloffExp", 1.0),
                lighten: f.boolean(b"Lighten", false),
                height_based: f.boolean(b"HeightBased", false),
                exponential: f.boolean(b"Exponential", false),
                density: f.float(b"Density", 0.25),
                height_density: f.float(b"HeightDensity", 0.25),
                height_horizon: f.float(b"HeightHorizon", 1.0),
                secondary_density: f.float(b"SecondaryDensity", 0.25),
                secondary_height_density: f.float(b"SecondaryHeightDensity", 0.25),
                secondary_height_horizon: f.float(b"SecondaryHeightHorizon", 1.0),
            });
        }

        if let Some(c) = global.first(b"DistanceCulling") {
            self.b.distance_cull(&abi::DistanceCull {
                active: c.boolean(b"Active", false),
                min_range: c.float(b"MinRange", 0.0),
                screen_size: c.float(b"ScreenSize", 0.05),
                random_size: c.float(b"RandomSize", 0.25),
            });
        }

        if let Some(d) = global.first(b"DirLight") {
            self.b.dir_light(&abi::DirLight {
                active: d.boolean(b"Active", false),
                diffuse_color: arr4(d.color(b"DiffuseColor", white)),
                brightness: d.float(b"Brightness", 1.0),
                direction: arr3(d.vec3(
                    b"Direction",
                    Vector3f {
                        x: 0.0,
                        y: -1.0,
                        z: 0.0,
                    },
                )),
                sky_color: arr4(d.color(b"SkyCol", clear)),
                ground_color: arr4(d.color(b"GroundCol", clear)),
                cast_shadows: d.boolean(b"CastShadows", false),
                shadow_caster_dist: d.float(b"ShadowCasterDist", 40.0),
                shadow_map_bias_mul: d.float(b"ShadowMapBiasMul", 1.0),
                shadow_map_slope_scale_bias_mul: d.float(b"ShadowMapSlopeScaleBiasMul", 1.0),
                shadow_blur_amount: d.float(b"ShadowBlurAmount", 6.0),
                auto_shadow_slice_settings: d.boolean(b"AutoShadowSliceSettings", true),
                auto_shadow_slice_log_term: d.float(b"AutoShadowSliceLogTerm", 0.9),
            });
        }

        if let Some(e) = global.first(b"EnvParticles") {
            let texture = cs(&e.string(b"Texture"));
            self.b.env_particles(&abi::EnvParticles {
                texture: texture.as_ptr(),
                sub_div_uv: arr2(e.vec2(b"SubDivUV", splat2(1.0))),
                box_size: e.float(b"BoxSize", 1.0),
                num_particles: e.int(b"NumParticles", 100),
                particle_size: arr2(e.vec2(b"ParticleSize", splat2(1.0))),
                affected_by_light: e.boolean(b"AffectedByLight", false),
                color: arr4(e.color(b"Color", white)),
                brightness: e.float(b"Brightness", 1.0),
                box_distance: e.float(b"BoxDistance", 2.0),
                gravity_velocity: arr3(e.vec3(
                    b"GravityVelocity",
                    Vector3f {
                        x: 0.0,
                        y: -0.1,
                        z: 0.0,
                    },
                )),
                gravity_speed_random_amount: e.float(b"GravitySpeedRandomAmount", 0.0),
                wind_velocity: arr3(e.vec3(b"WindVelocity", splat3(0.0))),
                wind_speed_random_amount: e.float(b"WindSpeedRandomAmount", 0.0),
                wind_dir_random_amount: e.float(b"WindDirRandomAmount", 0.0),
                rotate_velocity: arr3(e.vec3(b"RotateVelocity", splat3(0.0))),
                rotate_speed_random_amount: e.float(b"RotateSpeedRandomAmount", 0.0),
                rotate_both_dirs: e.boolean(b"RotateBothDirs", false),
                num_iterations: e.float(b"NumIterations", 1.0),
                fade_in_start: e.float(b"FadeInStart", 0.2),
                fade_in_end: e.float(b"FadeInEnd", 1.0),
                fade_out_start: e.float(b"FadeOutStart", 10.0),
                fade_out_end: e.float(b"FadeOutEnd", 20.0),
                active: e.boolean(b"Active", false),
            });
        }

        if let Some(p) = global.first(b"PostEffects") {
            let grading = cs(&p.string(b"ColorGradingTexture"));
            self.b.post_effects(&abi::PostEffects {
                tone_mapping_key: p.float(b"ToneMappingKey", 0.5),
                tone_mapping_exposure: p.float(b"ToneMappingExposure", 0.0),
                tone_mapping_white_cut: p.float(b"ToneMappingWhiteCut", 3.5),
                color_grading_texture: grading.as_ptr(),
            });
        }

        if let Some(s) = global.first(b"SkyBox") {
            let texture = cs(&s.string(b"Texture"));
            self.b.sky_box(&abi::SkyBox {
                active: s.boolean(b"Active", false),
                color: arr4(s.color(b"Color", white)),
                brightness: s.float(b"Brightness", 1.0),
                texture: texture.as_ptr(),
            });
        }
    }

    fn track(&mut self, base: &[u32], track: &[u8], file_index_element: &[u8]) {
        self.stat(track);
        let start = self.b.now_ms();

        let mut suffix = b"_".to_vec();
        suffix.extend_from_slice(track);
        let Some(doc) = self.open_sidecar(base, &suffix, true) else {
            self.stat(track).file_missing = true;
            return;
        };

        if let Some(root) = self.track_root(&doc, &cat(&[b"HPLMapTrack_", track])) {
            for section in root.children.iter().filter(|c| c.name == b"Section") {
                let file_index = if file_index_element.is_empty() {
                    Vec::new()
                } else {
                    Self::local_file_index(section, file_index_element)
                };
                let Some(objects) = section.first(b"Objects") else {
                    continue;
                };
                for obj in &objects.children {
                    let s = self.stat(track);
                    s.in_xml = s.in_xml.wrapping_add(1);

                    let reason = self.track_object(track, obj, &file_index);
                    let s = self.stat(track);
                    if reason.is_empty() {
                        s.created = s.created.wrapping_add(1);
                    } else {
                        let n = s.skipped.entry(reason).or_insert(0);
                        *n = n.wrapping_add(1);
                    }
                    self.b.object_done();
                }
            }
        }

        let elapsed = self.b.now_ms().wrapping_sub(start) as i32;
        self.stat(track).time_ms = elapsed;
    }

    fn track_object(&mut self, track: &[u8], el: &El, file_index: &[Vec<u8>]) -> Vec<u8> {
        let tag = el.name.as_slice();
        let unsupported = || cat(&[b"unsupported_element:", tag]);
        let when = |ok: bool, reason: Vec<u8>| if ok { Vec::new() } else { reason };

        match track {
            b"StaticObject" if tag != b"StaticObject" => unsupported(),
            b"StaticObject" => self.static_object(el, file_index),
            b"Primitive" => self.plane_primitive(el),
            b"Entity" if tag != b"Entity" => unsupported(),
            b"Entity" => self.map_entity(el, file_index),
            b"Light" => {
                let ok = self.b.light(el);
                when(ok, cat(&[b"load_failed:", tag]))
            }
            b"Area" if tag != b"Area" => unsupported(),
            b"Area" => self.map_area(el),
            b"Sound" if tag != b"Sound" => unsupported(),
            b"Sound" => {
                let ok = self.b.sound(el, el.boolean(b"Active", true));
                let reason: &[u8] = if el.string(b"SoundEntityFile").is_empty() {
                    b"empty_file"
                } else {
                    b"load_failed"
                };
                when(ok, reason.to_vec())
            }
            b"Decal" if tag != b"Decal" => unsupported(),
            b"Decal" => self.decal(el, file_index),
            b"Billboard" if tag != b"Billboard" => unsupported(),
            b"Billboard" => {
                let ok = self.b.billboard(el);
                when(ok, b"load_failed".to_vec())
            }
            b"LensFlare" if tag != b"LensFlare" => unsupported(),
            b"LensFlare" => {
                let ok = self.b.lens_flare(el);
                when(ok, b"load_failed".to_vec())
            }
            b"ParticleSystem" if tag != b"ParticleSystem" => unsupported(),
            b"ParticleSystem" => {
                let light = cs(&el.string(b"ConnectLight"));
                let ok = self.b.particle_system(el, light.as_ptr());
                when(ok, b"load_failed".to_vec())
            }
            b"FogArea" if tag != b"FogArea" => unsupported(),
            b"FogArea" => {
                let ok = self.b.fog_area(el);
                when(ok, b"load_failed".to_vec())
            }
            b"LightMask" if tag != b"LightMaskBox" => unsupported(),
            b"LightMask" => {
                self.b.light_mask_box(&abi::LightMask {
                    rotation: arr3(el.vec3(b"Rotation", splat3(0.0))),
                    size: arr3(el.vec3(b"Size", splat3(1.0))),
                    scale: arr3(el.vec3(b"Scale", splat3(1.0))),
                    id: strtoul_u32(&el.string_or(b"ID", b"0")),
                    world_pos: arr3(el.vec3(b"WorldPos", splat3(0.0))),
                });
                Vec::new()
            }
            _ => b"unsupported_track".to_vec(),
        }
    }

    fn connect_lights(&mut self) {
        let (billboards, particles) = self.b.connect_lights();
        for (track, missing) in [
            (&b"Billboard"[..], billboards),
            (b"ParticleSystem", particles),
        ] {
            if missing > 0 {
                let n = self
                    .stat(track)
                    .skipped
                    .entry(b"connect_light_missing".to_vec())
                    .or_insert(0);
                *n = n.wrapping_add(missing);
            }
        }
    }

    fn count_unsupported_flat_tracks(&mut self, base: &[u32]) {
        let Some(doc) = self.open_sidecar(base, b"_StaticObjectBatches", false) else {
            return;
        };
        self.stat(b"StaticObjectBatches");
        let batches = self
            .track_root(&doc, b"HPLMapTrack_StaticObjectBatches")
            .and_then(|r| r.first(b"StaticObjectBatches"));
        let count = batches.map_or(0, |b| b.children.len() as i32);
        let s = self.stat(b"StaticObjectBatches");
        s.in_xml = s.in_xml.wrapping_add(count);
        if s.in_xml > 0 {
            s.skipped.insert(b"unsupported_track".to_vec(), s.in_xml);
        }
    }

    fn detail_meshes_track(&mut self, base: &[u32]) {
        let Some(doc) = self.open_sidecar(base, b"_DetailMeshes", false) else {
            return;
        };
        self.stat(b"DetailMeshes");
        let start = self.b.now_ms();

        let sections = self
            .track_root(&doc, b"HPLMapTrack_DetailMeshes")
            .and_then(|r| r.first(b"DetailMeshes"))
            .and_then(|d| d.first(b"Sections"));
        for section in sections.iter().flat_map(|s| &s.children) {
            for mesh in &section.children {
                let num = mesh.int(b"NumOfInstances", 0);
                let file = mesh.string(b"File");
                let s = self.stat(b"DetailMeshes");
                s.in_xml = s.in_xml.wrapping_add(num);

                let list = |name: &[u8]| {
                    mesh.first(name)
                        .map(|e| float_list(&e.string(b"_Text"), Some(b" ")))
                        .unwrap_or_default()
                };
                let positions = list(b"DetailMeshEntityPositions");
                let rotations = list(b"DetailMeshEntityRotations");
                let radii = list(b"DetailMeshEntityRadii");
                // Wide arithmetic: the C++ int overflow let huge counts past this check and read past the arrays.
                if (positions.len() as i64) < i64::from(num) * 3
                    || (rotations.len() as i64) < i64::from(num) * 4
                {
                    let n = self
                        .stat(b"DetailMeshes")
                        .skipped
                        .entry(cat(&[b"instance_data_short:", &file]))
                        .or_insert(0);
                    *n = n.wrapping_add(num);
                    continue;
                }

                if num <= 0 {
                    continue;
                }
                let name = cs(&file);
                let first_index = self.stat(b"DetailMeshes").created;
                let made = self.b.detail_meshes(&abi::DetailMeshes {
                    file: name.as_ptr(),
                    num,
                    first_index,
                    positions: positions.as_ptr(),
                    rotations: rotations.as_ptr(),
                    radii: radii.as_ptr(),
                    num_radii: radii.len(),
                });
                let s = self.stat(b"DetailMeshes");
                s.created = s.created.wrapping_add(made);
                if made < num {
                    let n = s
                        .skipped
                        .entry(cat(&[b"mesh_missing:", &file]))
                        .or_insert(0);
                    *n = n.wrapping_add(num - made);
                }
            }
        }

        let elapsed = self.b.now_ms().wrapping_sub(start) as i32;
        self.stat(b"DetailMeshes").time_ms = elapsed;
    }

    fn exposure_area_track(&mut self, base: &[u32]) {
        let Some(doc) = self.open_sidecar(base, b"_ExposureArea", false) else {
            return;
        };
        let Some(root) = self.track_root(&doc, b"HPLMapTrack_ExposureArea") else {
            return;
        };
        for section in root.children.iter().filter(|c| c.name == b"Section") {
            let Some(objects) = section.first(b"Objects") else {
                continue;
            };
            for obj in objects
                .children
                .iter()
                .filter(|c| c.name == b"ExposureArea")
            {
                let s = self.stat(b"ExposureArea");
                s.in_xml = s.in_xml.wrapping_add(1);
                s.created = s.created.wrapping_add(1);

                let name = cs(&obj.string(b"Name"));
                self.b.exposure_area(&abi::ExposureArea {
                    name: name.as_ptr(),
                    rotation: arr3(obj.vec3(b"Rotation", splat3(0.0))),
                    world_pos: arr3(obj.vec3(b"WorldPos", splat3(0.0))),
                    scale: arr3(obj.vec3(b"Scale", splat3(1.0))),
                    exposure: obj.float(b"Exposure", 0.0),
                    white_cut: obj.float(b"WhiteCut", 3.5),
                    transition_time: obj.float(b"TransitionTime", 1.0),
                });
            }
        }
    }

    fn terrain(&mut self, base: &[u32]) {
        let Some(doc) = self.open_sidecar(base, b"_Terrain", false) else {
            return;
        };
        let terrain = self
            .track_root(&doc, b"HPLMapTrack_Terrain")
            .and_then(|r| r.first(b"Terrain"));
        if let Some(t) = terrain.filter(|t| t.boolean(b"Active", false)) {
            self.create_terrain(t);
        }
    }

    fn create_terrain(&mut self, t: &El) {
        let mut strings: Vec<CString> = Vec::new();
        let mut keep = |s: &[u8]| -> Cstr {
            strings.push(cs(s));
            strings.last().map_or(std::ptr::null(), |c| c.as_ptr())
        };

        let base_material_file = keep(&t.string(b"BaseMaterialFile"));
        let mut layers = Vec::new();
        let blend_layers = t.first(b"BlendLayers");
        for layer in blend_layers.iter().flat_map(|l| &l.children) {
            let mut out = abi::TerrainLayer {
                id: keep(&layer.string_or(b"ID", b"0")),
                num_mats: 0,
                mats: [abi::TerrainMat {
                    file: std::ptr::null(),
                    tile_amount: 0.0,
                    start_fade_value: 0.0,
                    detail_amount: [0.0; 3],
                }; 4],
            };
            for (slot, m) in out.mats.iter_mut().zip(&layer.children) {
                *slot = abi::TerrainMat {
                    file: keep(&m.string(b"File")),
                    tile_amount: m.float(b"TileAmount", 1.0),
                    start_fade_value: m.float(b"StartFadeValue", 0.0),
                    detail_amount: arr3(m.vec3(b"DetailTextureAmount", splat3(0.0))),
                };
                out.num_mats += 1;
            }
            layers.push(out);
        }

        let details_el = t.first(b"DetailTextures");
        let mut details = [abi::TerrainDetail {
            file: std::ptr::null(),
            scale: 0.0,
        }; 3];
        let mut num_details = 0;
        for (slot, d) in details
            .iter_mut()
            .zip(details_el.iter().flat_map(|d| &d.children))
        {
            *slot = abi::TerrainDetail {
                file: keep(&d.string(b"File")),
                scale: d.float(b"Scale", 1.0),
            };
            num_details += 1;
        }

        let sections = t.first(b"Sections");
        let mut decals = Vec::new();
        let mut areas = Vec::new();
        let mut area_points: Vec<Vec<f32>> = Vec::new();
        for section in sections.iter().flat_map(|s| &s.children) {
            let files: Vec<Vec<u8>> = section
                .first(b"FileIndex_TerrainDecals")
                .map(|i| i.children.iter().map(|f| f.string(b"Path")).collect())
                .unwrap_or_default();
            for d in section.first(b"Decals").iter().flat_map(|d| &d.children) {
                let Some(file) = usize::try_from(d.int(b"FileIndex", -1))
                    .ok()
                    .and_then(|i| files.get(i))
                else {
                    continue;
                };
                decals.push(abi::TerrainDecal {
                    file: keep(file),
                    position: arr2(d.vec2(b"Position", splat2(0.0))),
                    size: arr2(d.vec2(b"Size", splat2(1.0))),
                    angle: d.float(b"Angle", 0.0),
                    color: arr4(d.color(b"Color", gray(1.0, 1.0))),
                    current_uv_sub_div: d.int(b"CurrentUVSubDiv", 0),
                });
            }

            let Some(undergrowth) = section.first(b"Undergrowth") else {
                continue;
            };
            let files = Self::local_file_index(section, b"FileIndex_UndergrowthMaterials");
            for a in &undergrowth.children {
                let Some(file) = usize::try_from(a.int(b"FileIndex", -1))
                    .ok()
                    .and_then(|i| files.get(i))
                else {
                    continue;
                };
                let circle = a.name == b"Circle";
                let points = if circle {
                    Vec::new()
                } else {
                    a.children
                        .iter()
                        .flat_map(|p| {
                            let v = p.vec2(b"Coords", splat2(0.0));
                            [v.x, v.y]
                        })
                        .collect()
                };
                area_points.push(points);
                areas.push((file.clone(), a, circle));
            }
        }
        let areas: Vec<abi::UndergrowthArea> = areas
            .iter()
            .zip(&area_points)
            .map(|((file, a, circle), points)| abi::UndergrowthArea {
                file: keep(file),
                sub: string::to_lower(&a.string(b"BlendType")) == b"sub",
                max_influence: a.float(b"MaxInfluence", 1.0),
                fade_border: a.float(b"FadeBorder", 0.0),
                circle: *circle,
                center: arr2(a.vec2(b"Center", splat2(0.0))),
                radius: a.float(b"Radius", 0.5),
                points: points.as_ptr(),
                num_points: points.len() >> 1,
            })
            .collect();

        let details_fade = details_el.map_or((0.0, 0.0), |d| {
            (
                d.float(b"DetailFadeStart", 10.0),
                d.float(b"DetailFadeEnd", 20.0),
            )
        });
        self.b.terrain(&abi::Terrain {
            height_map_size: t.int(b"HeightMapSize", 1024),
            geometry_patch_size: t.int(b"GeometryPatchSize", 32),
            unit_size: t.float(b"UnitSize", 1.0),
            max_height: t.float(b"MaxHeight", 1.0),
            base_material_tile_amount: t.float(b"BaseMaterialTileAmount", 1.0),
            base_material_file,
            base_material_detail_texture_amount: arr3(
                t.vec3(b"BaseMaterialDetailTextureAmount", splat3(0.0)),
            ),
            has_blend_layers: blend_layers.is_some(),
            layers: layers.as_ptr(),
            num_layers: layers.len(),
            has_details: details_el.is_some(),
            detail_fade_start: details_fade.0,
            detail_fade_end: details_fade.1,
            details,
            num_details,
            has_sections: sections.is_some(),
            decals: decals.as_ptr(),
            num_decals: decals.len(),
            undergrowth_grid_size: t.float(b"UndergrowthGridSize", 10.0),
            undergrowth_fade_start: t.float(b"UndergrowthFadeStart", 25.0),
            undergrowth_fade_end: t.float(b"UndergrowthFadeEnd", 35.0),
            areas: areas.as_ptr(),
            num_areas: areas.len(),
        });
    }

    fn resolve_file(
        &mut self,
        el: &El,
        kind: &[u8],
        name: &[u8],
        file_index: &[Vec<u8>],
    ) -> Option<Vec<u8>> {
        let idx = el.int(b"FileIndex", -1);
        if idx < 0 {
            Some(el.string(b"Filename"))
        } else if let Some(f) = file_index.get(idx as usize) {
            Some(f.clone())
        } else {
            self.warn(&[
                b"SOMA hpm: ",
                kind,
                b" '",
                name,
                b"' has out-of-bounds FileIndex ",
                &num(idx),
                b"\n",
            ]);
            None
        }
    }

    fn static_object(&mut self, el: &El, file_index: &[Vec<u8>]) -> Vec<u8> {
        let name = el.string(b"Name");
        let Some(file) = self.resolve_file(el, b"static object", &name, file_index) else {
            return b"file_index_out_of_bounds".to_vec();
        };

        let position = el.vec3(b"WorldPos", splat3(0.0));
        if !self.transform_valid(&name, position) {
            return b"bad_transform".to_vec();
        }

        let pose = el
            .first(b"Pose")
            .map(|p| float_list(&p.string(b"_Text"), None));
        let name_c = cs(&name);
        let file_c = cs(&file);
        let made = self.b.static_object(&abi::StaticObject {
            name: name_c.as_ptr(),
            filename: file_c.as_ptr(),
            position: arr3(position),
            scale: arr3(el.vec3(b"Scale", splat3(1.0))),
            rotation: arr3(el.vec3(b"Rotation", splat3(0.0))),
            casts_shadows: el.boolean(b"CastShadows", true),
            collides: el.boolean(b"Collides", true),
            id: el.int(b"ID", -1),
            culled_by_distance: el.boolean(b"CulledByDistance", true),
            culled_by_fog: el.boolean(b"CulledByFog", true),
            color_mul: arr4(el.color(b"ColorMul", gray(1.0, 1.0))),
            illum_brightness: el.float(b"IllumBrightness", 1.0),
            illum_color: arr4(el.color(b"IllumColor", gray(1.0, 1.0))),
            has_pose: pose.is_some(),
            pose: pose.as_ref().map_or(std::ptr::null(), |p| p.as_ptr()),
            num_pose: pose.as_ref().map_or(0, Vec::len),
        });
        if made {
            Vec::new()
        } else {
            cat(&[b"mesh_missing:", &file])
        }
    }

    fn plane_primitive(&mut self, el: &El) -> Vec<u8> {
        if el.name != b"Plane" {
            self.warn(&[
                b"SOMA hpm: skipping unsupported primitive type '",
                &el.name,
                b"'\n",
            ]);
            return cat(&[b"unsupported_primitive:", &el.name]);
        }

        let name = el.string(b"Name");
        let material = el.string(b"Material");
        let position = el.vec3(b"WorldPos", splat3(0.0));
        if !self.transform_valid(&name, position) {
            return b"bad_transform".to_vec();
        }

        let name_c = cs(&name);
        let material_c = cs(&material);
        let uv = |n: &[u8]| arr2(el.vec2(n, splat2(0.0)));
        let made = self.b.plane(&abi::Plane {
            name: name_c.as_ptr(),
            material: material_c.as_ptr(),
            casts_shadows: el.boolean(b"CastShadows", true),
            collides: el.boolean(b"Collides", true),
            id: el.int(b"ID", -1),
            position: arr3(position),
            scale: arr3(el.vec3(b"Scale", splat3(1.0))),
            rotation: arr3(el.vec3(b"Rotation", splat3(0.0))),
            start_corner: arr3(el.vec3(b"StartCorner", splat3(0.0))),
            end_corner: arr3(el.vec3(b"EndCorner", splat3(0.0))),
            uv: [
                uv(b"Corner1UV"),
                uv(b"Corner2UV"),
                uv(b"Corner3UV"),
                uv(b"Corner4UV"),
            ],
            culled_by_distance: el.boolean(b"CulledByDistance", true),
            culled_by_fog: el.boolean(b"CulledByFog", true),
        });
        if made {
            Vec::new()
        } else {
            b"create_failed".to_vec()
        }
    }

    fn decal(&mut self, el: &El, file_index: &[Vec<u8>]) -> Vec<u8> {
        let name = el.string(b"Name");
        let idx = el.int(b"MaterialIndex", -1);
        let material = if idx < 0 {
            el.string(b"Material")
        } else if let Some(m) = file_index.get(idx as usize) {
            m.clone()
        } else {
            return b"file_index_out_of_bounds".to_vec();
        };

        let name_c = cs(&name);
        let material_c = cs(&material);
        let made = self.b.decal(
            &abi::Decal {
                name: name_c.as_ptr(),
                material: material_c.as_ptr(),
                color: arr4(el.color(b"Color", gray(1.0, 1.0))),
                id: el.int(b"ID", -1),
            },
            el.first(b"DecalMesh"),
        );
        if made {
            Vec::new()
        } else {
            b"decal_mesh_failed".to_vec()
        }
    }

    fn map_entity(&mut self, el: &El, file_index: &[Vec<u8>]) -> Vec<u8> {
        let name = el.string(b"Name");
        let position = el.vec3(b"WorldPos", splat3(0.0));
        if !self.transform_valid(&name, position) {
            return b"bad_transform".to_vec();
        }
        let Some(file) = self.resolve_file(el, b"entity", &name, file_index) else {
            return b"file_index_out_of_bounds".to_vec();
        };

        let name_c = cs(&name);
        let file_c = cs(&file);
        let made = self.b.entity(
            &abi::Entity {
                name: name_c.as_ptr(),
                filename: file_c.as_ptr(),
                id: el.int(b"ID", 0),
                active: el.boolean(b"Active", true),
                is_static: el.boolean(b"Static", false),
                position: arr3(position),
                scale: arr3(el.vec3(b"Scale", splat3(1.0))),
                rotation: arr3(el.vec3(b"Rotation", splat3(0.0))),
                culled_by_distance: el.boolean(b"CulledByDistance", true),
                culled_by_fog: el.boolean(b"CulledByFog", true),
            },
            el,
        );
        if made {
            Vec::new()
        } else {
            cat(&[b"entity_failed:", &file])
        }
    }

    fn map_area(&mut self, el: &El) -> Vec<u8> {
        let name = el.string(b"Name");
        let position = el.vec3(b"WorldPos", splat3(0.0));
        if !self.transform_valid(&name, position) {
            return b"bad_transform".to_vec();
        }

        let area_type = el.string(b"AreaType");
        let name_c = cs(&name);
        let type_c = cs(&area_type);
        let made = self.b.area(
            &abi::Area {
                name: name_c.as_ptr(),
                area_type: type_c.as_ptr(),
                id: el.int(b"ID", 0),
                active: el.boolean(b"Active", true),
                position: arr3(position),
                scale: arr3(el.vec3(b"Scale", splat3(1.0))),
                rotation: arr3(el.vec3(b"Rotation", splat3(0.0))),
            },
            el,
        );
        if made {
            Vec::new()
        } else {
            cat(&[b"no_area_loader:", &area_type])
        }
    }
}
