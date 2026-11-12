use super::flow::{self, PathOp};
use super::modes::{self, Custom, Device, Label, Mode};
use super::preset::{self, Detect};
use super::quality::{Chooser, UNKNOWN};
use super::stdsort;
use crate::luxbase::cfg::Cfg;
use std::collections::HashMap;

#[derive(Default)]
struct Map(HashMap<(Vec<u8>, Vec<u8>), Vec<u8>>);

impl Map {
    fn with(entries: &[(&str, &str, &str)]) -> Self {
        Self(
            entries
                .iter()
                .map(|(l, k, v)| {
                    (
                        (l.as_bytes().to_vec(), k.as_bytes().to_vec()),
                        v.as_bytes().to_vec(),
                    )
                })
                .collect(),
        )
    }
    fn get(&self, l: &[u8], k: &[u8]) -> Option<&Vec<u8>> {
        self.0.get(&(l.to_vec(), k.to_vec()))
    }
}

impl Cfg for Map {
    fn bool(&mut self, l: &[u8], k: &[u8], d: bool) -> bool {
        self.get(l, k).map_or(d, |v| crate::string::to_bool(v))
    }
    fn int(&mut self, l: &[u8], k: &[u8], d: i32) -> i32 {
        self.get(l, k).map_or(d, |v| crate::string::token_to_int(v))
    }
    fn float(&mut self, l: &[u8], k: &[u8], d: f32) -> f32 {
        self.get(l, k)
            .map_or(d, |v| crate::string::token_to_float(v))
    }
    fn string(&mut self, l: &[u8], k: &[u8], d: &[u8]) -> Vec<u8> {
        self.get(l, k).cloned().unwrap_or_else(|| d.to_vec())
    }
    fn string_w(&mut self, _: &[u8], _: &[u8], d: &[u32]) -> Vec<u32> {
        d.to_vec()
    }
    fn set_bool(&mut self, l: &[u8], k: &[u8], v: bool) {
        self.set_string(l, k, if v { b"true" } else { b"false" });
    }
    fn set_int(&mut self, l: &[u8], k: &[u8], v: i32) {
        self.set_string(l, k, v.to_string().as_bytes());
    }
    fn set_float(&mut self, l: &[u8], k: &[u8], v: f32) {
        self.set_string(l, k, v.to_string().as_bytes());
    }
    fn set_string(&mut self, l: &[u8], k: &[u8], v: &[u8]) {
        self.0.insert((l.to_vec(), k.to_vec()), v.to_vec());
    }
}

const DB: &str = r#"<CardDB><Models>
<Model Name="Geforce GTX"><Series Code="9???" Rating="2"/><Series Code="2??" Rating="1"/></Model>
<Model Name="Radeon HD"><Series Code="4???" Rating="0"/></Model>
<Model Name="Geforce FX"><Series Code="5???" Rating="-1"/></Model>
<Model Name="Intel"><Series Code="*" Rating="0"/></Model>
</Models></CardDB>"#;

fn chooser() -> Chooser {
    Chooser::from_bytes(Some(DB.as_bytes())).unwrap()
}

#[test]
fn rates_exact_and_closest() {
    let c = chooser();
    assert_eq!(c.rate(b"GeForce GTX 980/PCIe"), (2, vec![]));
    assert_eq!(c.rate(b"NVIDIA GeForce GTX 970"), (2, vec![]));
    assert_eq!(c.rate(b"geforce gtx 285"), (1, vec![]));
    assert_eq!(c.rate(b"Radeon HD 4850"), (0, vec![]));
    assert_eq!(c.rate(b"Geforce FX 5200"), (-1, vec![]));
    assert_eq!(c.rate(b"Mali-G78"), (UNKNOWN, vec![]));
    let (r, msg) = c.rate(b"Geforce GTX 1000");
    assert_eq!((r, msg.as_slice()), (2, b"geforce gtx 9???".as_slice()));
}

#[test]
fn bad_database_is_an_error() {
    assert!(Chooser::from_bytes(None).is_err());
    assert!(Chooser::from_bytes(Some(b"<a/>")).is_err());
    assert!(Chooser::from_bytes(Some(b"<a><Models><!-- c --></Models></a>")).is_err());
    assert!(Chooser::from_bytes(Some(b"<a><Models><Model/></Models></a>")).is_err());
}

#[test]
fn sort_matches_stable_small_and_orders_large() {
    let mut v: Vec<(u32, u32)> = (0..10).map(|i| (i % 3, i)).collect();
    stdsort::sort(&mut v, &|a, b| b.0 < a.0);
    assert_eq!(
        v.iter().map(|x| x.0).collect::<Vec<_>>(),
        [2, 2, 2, 1, 1, 1, 0, 0, 0, 0]
    );
    assert_eq!(
        v.iter()
            .filter(|x| x.0 == 2)
            .map(|x| x.1)
            .collect::<Vec<_>>(),
        [2, 5, 8]
    );
    let mut big: Vec<u32> = (0..500).map(|i| (i * 7919) % 101).collect();
    stdsort::sort(&mut big, &|a, b| a < b);
    assert!(big.windows(2).all(|w| w[0] <= w[1]));
    let mut adversarial: Vec<u32> = (0..2000)
        .map(|i| if i % 2 == 0 { i } else { 2000 - i })
        .collect();
    stdsort::sort(&mut adversarial, &|a, b| a < b);
    assert!(adversarial.windows(2).all(|w| w[0] <= w[1]));
}

#[test]
fn preset_compare_and_apply() {
    let mut full = Map::with(&[
        ("Graphics", "ShadowQuality", "0"),
        ("Graphics", "ShadowResolution", "0"),
        ("Graphics", "TextureQuality", "0"),
        ("Graphics", "TextureFilter", "0"),
        ("Graphics", "SSAOResolution", "0"),
        ("Graphics", "SSAOSamples", "4"),
        ("Graphics", "TextureAnisotropy", "2"),
        ("Graphics", "ParallaxEnabled", "false"),
        ("Graphics", "WorldReflection", "false"),
        ("Graphics", "SSAOActive", "false"),
        ("Graphics", "EdgeSmooth", "false"),
    ]);
    let mut cfg = Map::default();
    assert!(!preset::equals_preset(&mut cfg, &mut full));
    preset::apply(&mut cfg, &mut full);
    assert_eq!(cfg.int(b"Graphics", b"SSAOSamples", 9), 4);
    assert!(!cfg.bool(b"Graphics", b"ParallaxEnabled", true));
    assert!(preset::equals_preset(&mut cfg, &mut full));
    let mut sparse = Map::default();
    preset::apply(&mut sparse, &mut Map::default());
    assert_eq!(sparse.int(b"Graphics", b"SSAOSamples", 9), 8);
    assert!(!preset::equals_preset(&mut sparse, &mut Map::default()));
    let mut other = Map::default();
    let mut presets: Vec<Option<&mut dyn Cfg>> = vec![Some(&mut other), None, Some(&mut full)];
    assert_eq!(preset::equal_index(Some(&mut cfg), &mut presets), 2);
}

#[test]
fn detect_rules() {
    assert_eq!(
        preset::detect(UNKNOWN, false),
        Detect {
            show_unknown: true,
            preset: Some(1),
            ..Detect::default()
        }
    );
    assert_eq!(
        preset::detect(-1, true),
        Detect {
            show_closest: true,
            show_unsupported: true,
            ..Detect::default()
        }
    );
    assert_eq!(preset::detect(2, false).preset, Some(2));
    assert_eq!(preset::detect(7, false).preset, None);
}

#[test]
fn paths_and_init() {
    assert_eq!(flow::init_cfg_name(b""), b"config/main_init.cfg");
    assert_eq!(flow::init_cfg_name(b"ptest"), b"config/ptest_main_init.cfg");
    let p = flow::plan_paths(b"/d/", b"/c/", b"/s/", b"TDD", b"cs");
    assert_eq!(p.base_save, b"/d/open-hpl/Amnesia/TDD/");
    assert_eq!(p.crash_flag, b"/s/open-hpl/Amnesia/TDD/crash_flag");
    assert_eq!(
        p.ops.first(),
        Some(&PathOp::CreateIfMissing(b"/d/open-hpl".to_vec()))
    );
    assert_eq!(p.ops.len(), 9);
    assert!(flow::use_wanted_config(false, true));
    assert!(!flow::use_wanted_config(true, true));
    let mut init = Map::with(&[("Directories", "MainSaveFolder", "X")]);
    let r = flow::read_init(
        &mut init,
        b"config/ptest_main_init.cfg",
        flow::DEFAULT_INIT,
        &mut || None,
    );
    assert!(r.failed);
    assert_eq!(r.messages.len(), 2);
    let r = flow::read_init(&mut init, b"a", b"a", &mut || None);
    assert!(!r.failed && r.vars.save_folder == b"X");
}

#[test]
fn language_and_run() {
    let ops = flow::language_plan(
        b"german.lang",
        b"lang/",
        b"glang/",
        b"base_english.lang",
        b"",
    );
    assert_eq!(ops.len(), 3);
    assert_eq!(ops[0].file, b"lang/base_german.lang");
    assert!(ops[0].forced && !ops[1].forced);
    assert_eq!(flow::run_plan(2, b""), (true, b"-cwd ptest".to_vec()));
    assert_eq!(flow::run_plan(0, b""), (false, b"-cwd ".to_vec()));
    assert_eq!(
        flow::game_exe(b"/x"),
        [b"/x/".as_slice(), flow::GAME_BIN].concat()
    );
}

#[test]
fn resolutions() {
    assert_eq!(modes::aspect_ratio(1920, 1200), (16, 10));
    assert_eq!(modes::aspect_ratio(1920, 1080), (16, 9));
    assert_eq!(modes::aspect_ratio(0, 5), (0, 0));
    let m = [
        Mode {
            display: 0,
            width: 800,
            height: 600,
        },
        Mode {
            display: 1,
            width: 800,
            height: 600,
        },
    ];
    assert_eq!(modes::custom_resolution(&m, 800, 600), Custom::Select(1));
    assert_eq!(modes::custom_resolution(&m, 639, 600), Custom::Reject);
    assert_eq!(modes::custom_resolution(&m, 1024, 768), Custom::Add);
    assert!(modes::multi_display(&m));
    let label = modes::label(&Label {
        fullscreen_desktop: false,
        fullscreen_desktop_text: b"",
        display_name: b"A/B",
        width: 1280,
        height: 800,
        custom: true,
        custom_text: b"Custom",
        multi_display: true,
    });
    assert_eq!(label, b"1280x800  [16:10] (Custom)  (A-B)");
    let mut cfg = Map::default();
    assert!(!modes::fix_config_size(&mut cfg, &m));
    assert_eq!(cfg.int(b"Screen", b"Width", 0), -1);
}

#[test]
fn languages_and_sound() {
    assert_eq!(modes::language_name(b"English.lang"), b"english");
    let files: [&[u8]; 3] = [b"a.lang", b"english.lang", b"b.lang"];
    assert_eq!(modes::language_index(&files, b"english.lang", b"b.lang"), 2);
    assert_eq!(modes::language_index(&files, b"english.lang", b"zz"), 1);
    assert_eq!(modes::language_index(&files, b"nope", b"zz"), -1);
    let d = [
        Device {
            is_default: true,
            is_current: false,
        },
        Device {
            is_default: false,
            is_current: false,
        },
    ];
    assert_eq!(modes::sound_selection(-1, &d), (0, false));
    assert_eq!(modes::sound_selection(3, &d), (2, true));
    assert_eq!(modes::escape_device(b"a/b"), b"a\\/b");
}

#[test]
fn real_card_database_if_installed() {
    let Some(home) = std::env::var_os("HOME") else {
        return;
    };
    let p = std::path::Path::new(&home)
        .join(".local/share/Steam/steamapps/common/Amnesia The Dark Descent/launcher/launcher_card_database.cfg");
    let Ok(data) = std::fs::read(p) else {
        return;
    };
    let c = Chooser::from_bytes(Some(&data)).unwrap();
    assert_eq!(c.rate(b"Mali-G78").0, UNKNOWN);
}

#[test]
fn tied_models_keep_file_order() {
    let db = br#"<d><Models><Model Name="a"><Series Code="1" Rating="0"/></Model><Model Name="b"><Series Code="1" Rating="1"/></Model><Model Name="c"><Series Code="1" Rating="2"/></Model></Models></d>"#;
    let c = Chooser::from_bytes(Some(db)).unwrap();
    assert_eq!(c.rate(b"a b c 1").0, 0);
    assert_eq!(c.rate(b"c b 1").0, 1);
}

#[test]
fn repeated_model_word_erases_nothing_extra() {
    let db = br#"<d><Models><Model Name="a a"><Series Code="12?" Rating="1"/><Series Code="5" Rating="2"/></Model></Models></d>"#;
    let c = Chooser::from_bytes(Some(db)).unwrap();
    assert_eq!(c.rate(b"a 5"), (2, vec![]));
    assert_eq!(c.rate(b"a 9 5"), (2, vec![]));
    assert_eq!(c.rate(b"a"), (1, vec![]));
}
