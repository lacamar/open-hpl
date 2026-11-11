use crate::resmgr::*;
use crate::resources::ResMeta;
use std::collections::HashMap;
use std::ffi::c_void;

fn w(s: &str) -> Vec<u32> {
    s.chars().map(u32::from).collect()
}

fn ptr(n: usize) -> *mut c_void {
    std::ptr::without_provenance_mut(n)
}

#[derive(Default)]
struct Fake {
    files: HashMap<Vec<u8>, (Vec<u32>, i32)>,
    loaded: HashMap<Vec<u32>, usize>,
    log: Vec<String>,
}

impl Env for Fake {
    fn file_path(&mut self, name: &[u8], want_count: bool) -> (Vec<u32>, i32) {
        self.log.push(String::from_utf8_lossy(name).into_owned());
        let (p, c) = self.files.get(name).cloned().unwrap_or_default();
        (p, if want_count { c } else { 0 })
    }

    fn get_resource(&mut self, path: &[u32]) -> *mut c_void {
        self.loaded
            .get(path)
            .map_or(std::ptr::null_mut(), |p| ptr(*p))
    }
}

fn fake(files: &[(&str, &str, i32)]) -> Fake {
    let mut f = Fake::default();
    for (n, p, c) in files {
        f.files.insert(n.as_bytes().to_vec(), (w(p), *c));
    }
    f
}

const EXTS: [&[u8]; 3] = [b"png", b"jpg", b"dds"];

#[test]
fn formats_pick_highest_count_first_on_ties() {
    let mut f = fake(&[
        ("a.png", "/x/a.png", 1),
        ("a.jpg", "/y/a.jpg", 3),
        ("a.dds", "/z/a.dds", 3),
    ]);
    let lk = find_with_formats(&mut f, b"a", &EXTS);
    assert_eq!(lk.path, w("/y/a.jpg"));
    assert!(lk.res.is_null());
}

#[test]
fn formats_loaded_resource_clears_path() {
    let mut f = fake(&[("a.png", "/x/a.png", 0)]);
    f.loaded.insert(w("/x/a.png"), 7);
    let lk = find_with_formats(&mut f, b"a", &EXTS);
    assert_eq!(lk.res, ptr(7));
    assert!(lk.path.is_empty());
}

#[test]
fn formats_with_extension_probe_once() {
    let mut f = fake(&[("a.png", "/x/a.png", 0)]);
    let lk = find_with_formats(&mut f, b"a.png", &EXTS);
    assert_eq!(lk.path, w("/x/a.png"));
    assert_eq!(f.log, ["a.png"]);
}

#[test]
fn sample_keeps_last_probe_path() {
    let mut f = fake(&[("s.ogg", "/s.ogg", 0)]);
    let lk = find_sample(&mut f, b"s", &[b"wav", b"ogg", b"mp3"]);
    assert_eq!(lk.path, w("/s.ogg"));
    assert_eq!(f.log, ["s.wav", "s.ogg"]);
    let lk = find_sample(&mut f, b"s", &[]);
    assert!(lk.path.is_empty());
}

#[test]
fn stream_path_stops_at_first_hit() {
    let mut f = fake(&[("m.ogg", "/m.ogg", 0)]);
    assert_eq!(
        find_stream_path(&mut f, b"m", &[b"ogg", b"wav"]),
        w("/m.ogg")
    );
    assert_eq!(f.log, ["m.ogg"]);
}

#[test]
fn model_search_and_missing() {
    let mut f = fake(&[("m.dae", "/m.dae", 0)]);
    match resolve_model(&mut f, b"m", &[b"msh", b"dae"]) {
        Model::Found(n, lk) => {
            assert_eq!(n, b"m.dae");
            assert_eq!(lk.path, w("/m.dae"));
        }
        Model::NoFormat => panic!(),
    }
    assert!(matches!(
        resolve_model(&mut f, b"q", &[b"msh"]),
        Model::NoFormat
    ));
    assert!(matches!(
        resolve_model(&mut f, b"q.msh", &[]),
        Model::Found(..)
    ));
}

#[test]
fn simple_names() {
    assert_eq!(simple_name(Simple::EntFile, b"Dir/Foo.BAR"), b"dir/foo.ent");
    assert_eq!(simple_name(Simple::Font, b"Arial.TTF"), b"arial.ttf");
    assert_eq!(simple_name(Simple::Script, b"a.hps"), b"a.hps");
    assert_eq!(simple_name(Simple::Script, b"a.chps"), b"a.chps");
    assert_eq!(simple_name(Simple::Script, b"a.CHPS"), b"a.hps");
    assert_eq!(simple_name(Simple::SoundEntity, b"x"), b"x.snt");
    let mut f = Fake::default();
    assert!(resolve_simple(&mut f, Simple::Material, b"").is_none());
    assert!(resolve_simple(&mut f, Simple::PhysicsMaterial, b"").is_some());
}

#[test]
fn font_kinds() {
    assert_eq!(font_kind(b"a.TTF"), FontKind::TrueType);
    assert_eq!(font_kind(b"a.fnt"), FontKind::Bitmap);
    assert_eq!(font_kind(b"a"), FontKind::Unknown);
}

#[test]
fn destroy_decisions() {
    let mut m = ResMeta::new(0, 0);
    m.inc_user(1);
    m.inc_user(1);
    assert!(!destroy_standard(&mut m));
    assert!(destroy_standard(&mut m));
    assert!(destroy_standard(&mut m));
    destroy_particle(&mut m);
    assert_eq!(m.user_count, 0);
    m.inc_user(1);
    assert!(destroy_sound(&mut m, true));
    m.inc_user(1);
    assert!(!destroy_sound(&mut m, false));
    assert_eq!(
        destroy_image_frame(true, false, true),
        ImageAfter {
            delete_frame_bitmap: true,
            delete_frame: true
        }
    );
    assert_eq!(
        destroy_image_frame(false, false, true),
        ImageAfter::default()
    );
    assert!(!destroy_image_frame(true, true, true).delete_frame_bitmap);
}

#[test]
fn frame_size_growth_and_degenerate() {
    assert_eq!(image_frame_size((512, 512), (100, 600)), (512, 1024));
    assert_eq!(image_frame_size((512, 512), (510, 510)), (512, 512));
    assert_eq!(image_frame_size((0, 512), (10, 10)), (0, 512));
    assert_eq!(image_frame_size((512, 512), (i32::MAX, 1)).0, 512);
    assert_eq!(
        image_frame_size((512, 512), (i32::MAX - 3, 1)).0,
        0x4000_0000
    );
}

#[test]
fn anim_plan_collects_frames() {
    let mut f = fake(&[
        ("fire01.png", "/g/fire01.png", 0),
        ("fire02.png", "/g/fire02.png", 0),
        ("fire03.png", "/g/fire03.png", 0),
    ]);
    match anim_plan(&mut f, b"fire01.png") {
        Ok(AnimPlan::Frames {
            base_name,
            fake_path,
            paths,
        }) => {
            assert_eq!(base_name, b"fire.png");
            assert_eq!(fake_path, w("/g/fire.png"));
            assert_eq!(paths.len(), 3);
        }
        _ => panic!(),
    }
    assert_eq!(anim_plan(&mut f, b"fire.png").err(), Some(AnimErr::No01));
    assert_eq!(
        anim_plan(&mut f, b"fire01x.png").err(),
        Some(AnimErr::No01BeforeExt)
    );
    assert_eq!(
        anim_plan(&mut f, b"gone01.png").err(),
        Some(AnimErr::FirstFrameMissing)
    );
    f.loaded.insert(w("/g/fire.png"), 9);
    assert!(matches!(
        anim_plan(&mut f, b"fire01.png"),
        Ok(AnimPlan::Loaded(p)) if p == ptr(9)
    ));
}

#[test]
fn anim_first_frame_probes_formats() {
    let mut f = fake(&[("a01.jpg", "/a01.jpg", 0)]);
    assert_eq!(
        anim_first_frame(&mut f, b"a01", &[b"png", b"jpg"]),
        b"a01.jpg"
    );
    assert_eq!(anim_first_frame(&mut f, b"a01", &[b"gif"]), b"a01");
}

#[test]
fn cube_faces_stale_path_without_formats() {
    let mut f = Fake::default();
    assert_eq!(cube_faces(&mut f, b"sky", &[]), Err(0));
    let mut f = fake(&[("sky_pos_x.png", "/p", 0), ("sky_neg_x.png", "/n", 0)]);
    assert_eq!(cube_faces(&mut f, b"sky", &[b"png"]), Err(2));
    assert!(matches!(cube_pre(&mut f, b"sky.DDS"), CubePre::SingleFile));
    match cube_pre(&mut f, b"env/sky.png") {
        CubePre::Faces {
            name, fake_path, ..
        } => {
            assert_eq!(name, b"env/sky");
            assert_eq!(fake_path, w("env/sky"));
        }
        CubePre::SingleFile => panic!(),
    }
}

#[test]
fn material_tables() {
    use mat::*;
    assert_eq!(texture_type(b"Cube"), TEX_CUBE);
    assert_eq!(texture_type(b"RECT"), TEX_CUBE);
    assert_eq!(texture_type(b"junk"), TEX_2D);
    assert_eq!(wrap(b"ClampToEdge"), 2);
    assert_eq!(blend_mode(b"nope"), (1, true));
    assert_eq!(blend_mode(b"MulX2"), (3, false));
    assert_eq!(uv_anim_type(None), (3, true));
    assert_eq!(uv_anim_type(Some(b"SIN")), (1, false));
    assert_eq!(anim_axis(Some(b"q")), (3, true));
    assert_eq!(texture_string(0), b"Diffuse");
    assert_eq!(texture_string(3), b"Alpha");
    assert_eq!(texture_string(21), b"Diffuse_R");
    assert_eq!(texture_string(24), b"Height_R");
    assert_eq!(texture_string(32), b"Height_B");
    assert_eq!(texture_string(33), b"Liquid");
    assert!(texture_string(34).is_empty());
    assert!(texture_string(-1).is_empty());
    let srgb: Vec<i32> = (0..36).filter(|t| texture_is_srgb(*t)).collect();
    assert_eq!(
        srgb,
        [
            0, 2, 5, 6, 8, 9, 12, 13, 14, 18, 19, 20, 21, 22, 25, 26, 29, 30
        ]
    );
    assert_eq!(create_kind(1, TEX_CUBE), Create::Anim);
    assert_eq!(create_kind(0, 2), Create::None);
    assert_eq!(texture_file(b"a.png", &w("/m/x.mat")), b"/m/a.png");
    assert_eq!(texture_file(b"d/a.png", &w("/m/x.mat")), b"d/a.png");
    assert!(anisotropy_accepted(4.0, 1, 16, 1.0));
    assert!(!anisotropy_accepted(0.5, 1, 16, 1.0));
    assert!(!anisotropy_accepted(4.0, 0, 16, 1.0));
    assert!(!anisotropy_accepted(32.0, 1, 16, 1.0));
    assert!(!anisotropy_accepted(4.0, 1, 16, 4.0));
}

#[test]
fn world_tables() {
    use world::*;
    assert_eq!(billboard_type(b"axis"), 0);
    assert_eq!(billboard_type(b"FixedAxis"), 2);
    assert_eq!(shadow_resolution(b"LOW"), 0);
    assert_eq!(shadow_resolution(b"?"), 2);
    assert_eq!(light_kind(b"SpotLight"), Some(Light::Spot));
    assert_eq!(light_kind(b"spotlight"), None);
    assert_eq!(gobo_kind(false, 2), Gobo::AnimCube);
    assert_eq!(shadow_casters(true, false), 2);
    assert!(!sound_use_default(true, Some(true)));
    assert!(sound_use_default(true, None));
    assert_eq!(band_count(8), 2);
    assert_eq!(band_count(2), 0);
    assert_eq!(light_mask_id(b"4294967295"), u32::MAX);
    assert_eq!(light_mask_id(b"  12x"), 12);
    assert_eq!(light_mask_id(b"-1"), u32::MAX);
    assert_eq!(light_mask_id(b"4294967296"), 0);
    assert_eq!(light_mask_id(b"99999999999999999999999"), u32::MAX);
    assert_eq!(light_mask_id(b""), 0);
    let pos = [1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0];
    let n = [0.0; 8];
    let rows = decal_vertices([&pos, &n, &n, &pos], 2);
    assert_eq!(rows.map(|r| r.len()), Some(8));
    assert!(decal_vertices([&pos, &n, &n, &pos], 3).is_none());
}

#[test]
fn loader_map_semantics() {
    let mut m = LoaderMap::default();
    assert_eq!(m.get(b"a"), (std::ptr::null_mut(), true));
    m.add(b"a", ptr(1), false);
    m.add(b"a", ptr(2), true);
    m.add(b"b", ptr(3), false);
    assert_eq!(m.get(b"a"), (ptr(1), false));
    assert_eq!(m.get(b"zz"), (ptr(2), true));
    assert_eq!(m.values().collect::<Vec<_>>(), [ptr(1), ptr(3)]);
}

#[test]
fn custom_map_is_case_insensitive_and_keeps_null() {
    let mut m = CustomMap::default();
    assert!(m.get(b"X").is_none());
    m.set(b"X", std::ptr::null_mut());
    assert_eq!(m.get(b"x"), Some(std::ptr::null_mut()));
    m.set(b"x", ptr(5));
    assert_eq!(m.get(b"X"), Some(ptr(5)));
}

#[test]
fn sound_entity_resolver_gate() {
    let mut f = fake(&[("a.snt", "/a.snt", 0)]);
    assert!(!sound_entity_needs_resolver(&mut f, b"a", false));
    assert!(!sound_entity_needs_resolver(&mut f, b"a", true));
    assert!(sound_entity_needs_resolver(&mut f, b"b", true));
    assert!(!sound_entity_needs_resolver(&mut f, b"b.snt", true));
}

#[test]
fn particle_lookup_uses_empty_path() {
    let mut f = Fake::default();
    f.loaded.insert(Vec::new(), 4);
    let (file, path, res) = particle_by_file(&mut f, b"fire");
    assert_eq!(file, b"fire.ps");
    assert!(path.is_empty());
    assert_eq!(res, ptr(4));
}

#[test]
fn cache_file_and_dirs() {
    let (p, d) = cache_file(&w("/home/u/.cache/"), &w("//maps/a.hpm"), &w("msh"));
    assert_eq!(p, w("/home/u/.cache/open-hpl/files/maps/a.msh"));
    assert_eq!(d.len(), 6);
    let (p, _) = cache_file(&w("/c/"), &w("///"), &w("x"));
    assert_eq!(p, w("/c/open-hpl/files/.x"));
}

#[test]
fn resource_dirs_alt_and_slash() {
    assert!(resource_dirs(b"", &w("alt/")).is_empty());
    assert_eq!(resource_dirs(b"/core", &[]), [w("core")]);
    assert_eq!(
        resource_dirs(b"\\core", &w("alt/")),
        [w("alt/core"), w("core")]
    );
}
