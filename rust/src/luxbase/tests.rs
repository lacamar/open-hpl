use super::base::{self, w};
use super::cfg::Cfg;
use super::config::{self, MainCfg};
use super::debug::{Debug, Toggle};
use super::headless::{self, Body, Env, Req, Resp};
use super::input::{self, ActionEnv, MouseActivity, MouseEnv, Smooth, SubAction};
use std::collections::BTreeMap;

#[derive(Default)]
struct Fake {
    vals: BTreeMap<(Vec<u8>, Vec<u8>), Vec<u8>>,
    log: Vec<String>,
}

impl Fake {
    fn with(entries: &[(&str, &str, &str)]) -> Self {
        let mut f = Fake::default();
        for (l, k, v) in entries {
            f.vals.insert(
                (l.as_bytes().to_vec(), k.as_bytes().to_vec()),
                v.as_bytes().to_vec(),
            );
        }
        f
    }

    fn get(&mut self, l: &[u8], k: &[u8]) -> Option<Vec<u8>> {
        self.log.push(format!(
            "{}/{}",
            String::from_utf8_lossy(l),
            String::from_utf8_lossy(k)
        ));
        self.vals.get(&(l.to_vec(), k.to_vec())).cloned()
    }
}

impl Cfg for Fake {
    fn bool(&mut self, l: &[u8], k: &[u8], d: bool) -> bool {
        self.get(l, k).map_or(d, |v| crate::string::to_bool(&v))
    }
    fn int(&mut self, l: &[u8], k: &[u8], d: i32) -> i32 {
        self.get(l, k)
            .map_or(d, |v| crate::string::token_to_int(&v))
    }
    fn float(&mut self, l: &[u8], k: &[u8], d: f32) -> f32 {
        self.get(l, k)
            .map_or(d, |v| crate::string::token_to_float(&v))
    }
    fn string(&mut self, l: &[u8], k: &[u8], d: &[u8]) -> Vec<u8> {
        self.get(l, k).unwrap_or_else(|| d.to_vec())
    }
    fn string_w(&mut self, l: &[u8], k: &[u8], d: &[u32]) -> Vec<u32> {
        let d8 = crate::string::wide_to_utf8(d);
        crate::string::utf8_to_wide(&self.string(l, k, &d8))
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
        self.log.push(format!(
            "set {}/{}={}",
            String::from_utf8_lossy(l),
            String::from_utf8_lossy(k),
            String::from_utf8_lossy(v)
        ));
        self.vals.insert((l.to_vec(), k.to_vec()), v.to_vec());
    }
}

fn text(v: &[u8]) -> String {
    String::from_utf8_lossy(v).into_owned()
}

#[test]
fn ptest_strings_decrypt_to_the_original_names() {
    assert_eq!(
        text(&base::decrypt(&base::MAIN_INIT_STR)),
        "config/ptest_main_init.cfg"
    );
    assert_eq!(text(&base::decrypt(&base::START_MAP_STR)), "01_cells.map");
    assert_eq!(text(&base::decrypt(&base::START_FOLDER_STR)), "maps/ptest/");
    assert_eq!(
        text(&base::decrypt(&base::START_POS_STR)),
        "PlayerStartArea_1"
    );
}

#[test]
fn command_line_modes() {
    let d = w("config/main_init.cfg");
    let p = base::parse_command_line(b"ptest");
    assert!(p.ptest && !p.hard_mode);
    assert_eq!(p.init_file, w("config/ptest_main_init.cfg"));
    let h = base::parse_command_line(b"hardmode");
    assert!(h.hard_mode && h.init_file == d);
    assert_eq!(base::parse_command_line(b"").init_file, d);
    assert_eq!(base::parse_command_line(b"x/y.cfg").init_file, w("x/y.cfg"));
}

#[test]
fn main_config_defaults_and_save_order() {
    let mut f = Fake::default();
    let m: MainCfg = config::load_main(&mut f, &mut || true, b"english.lang");
    assert!(!m.first_start && m.lang_file == b"english.lang" && m.screen_width == 800);
    assert_eq!(
        (m.shadow_quality, m.shadow_res, m.texture_filter),
        (1, 2, 1)
    );
    assert_eq!(f.log.len(), 38);

    struct S(Fake);
    impl Cfg for S {
        fn bool(&mut self, l: &[u8], k: &[u8], d: bool) -> bool {
            self.0.bool(l, k, d)
        }
        fn int(&mut self, l: &[u8], k: &[u8], d: i32) -> i32 {
            self.0.int(l, k, d)
        }
        fn float(&mut self, l: &[u8], k: &[u8], d: f32) -> f32 {
            self.0.float(l, k, d)
        }
        fn string(&mut self, l: &[u8], k: &[u8], d: &[u8]) -> Vec<u8> {
            self.0.string(l, k, d)
        }
        fn string_w(&mut self, l: &[u8], k: &[u8], d: &[u32]) -> Vec<u32> {
            self.0.string_w(l, k, d)
        }
        fn set_bool(&mut self, l: &[u8], k: &[u8], v: bool) {
            self.0.set_bool(l, k, v);
        }
        fn set_int(&mut self, l: &[u8], k: &[u8], v: i32) {
            self.0.set_int(l, k, v);
        }
        fn set_float(&mut self, l: &[u8], k: &[u8], v: f32) {
            self.0.set_float(l, k, v);
        }
        fn set_string(&mut self, l: &[u8], k: &[u8], v: &[u8]) {
            self.0.set_string(l, k, v);
        }
    }
    impl config::SaveEnv for S {
        fn raise_first_start_flag(&mut self) {
            self.0.log.push("raise".into());
        }
        fn gamma(&mut self) -> f32 {
            1.5
        }
        fn gbuffer_type(&mut self) -> i32 {
            1
        }
        fn num_gbuffer_textures(&mut self) -> i32 {
            3
        }
        fn volume(&mut self) -> f32 {
            0.5
        }
        fn limit_fps(&mut self) -> bool {
            true
        }
        fn wait_if_out_of_focus(&mut self) -> bool {
            false
        }
    }
    let mut s = S(Fake::default());
    let first = MainCfg {
        first_start: true,
        ..m
    };
    config::save_main(&mut s, &first);
    assert_eq!(
        s.0.log.first().map(String::as_str),
        Some("set Main/LoadDebugMenu=false")
    );
    assert_eq!(s.0.log.get(1).map(String::as_str), Some("raise"));
    assert!(s.0.log.contains(&"set Graphics/Gamma=1.5".to_string()));
    assert_eq!(
        s.0.log.last().map(String::as_str),
        Some("set Engine/SleepWhenOutOfFocus=false")
    );
}

#[test]
fn restart_popup_shows_once_per_request() {
    let mut r = config::Restart::default();
    assert!(!r.show_due());
    r.set_needed();
    assert!(r.show_due() && !r.show_due());
    r.set_needed();
    assert!(r.show_due());
}

#[test]
fn path_plan_orders_the_side_effects() {
    let p = base::plan_paths(
        &w("/d/"),
        &w("/c/"),
        &w("/s/"),
        &w("Main"),
        &w("custom_stories"),
        Some(42),
    );
    assert_eq!(
        p.ops.first(),
        Some(&base::PathOp::Migrate(w("/d/open-hpl/Amnesia/")))
    );
    assert_eq!(p.base_save, w("/d/open-hpl/Amnesia/Main/"));
    assert_eq!(p.base_config, w("/c/open-hpl/Amnesia/Main/"));
    assert_eq!(p.log_file, w("/s/open-hpl/Amnesia/Main/hpl-42.log"));
    assert_eq!(
        p.update_log_file,
        w("/s/open-hpl/Amnesia/Main/hpl_update-42.log")
    );
    let creates: Vec<_> = p
        .ops
        .iter()
        .filter_map(|o| match o {
            base::PathOp::CreateIfMissing(d) => Some(base::PathOp::CreateIfMissing(d.clone())),
            _ => None,
        })
        .collect();
    assert_eq!(creates.len(), 6);
    assert_eq!(p.ops.len(), 1 + 6 + 2 + 2);
}

#[test]
fn start_resolution_prefers_given_then_story_then_user_then_default() {
    let mut u = Fake::with(&[("Map", "File", "u.map")]);
    let def = base::StartDefaults {
        file: b"d.map",
        folder: b"maps/",
        pos: b"P",
    };
    let none = base::StartTarget {
        file: vec![],
        folder: vec![],
        pos: vec![],
    };
    let mut set = vec![];
    let t = base::resolve_start(&none, None, &mut u, &def, false, &mut |f| set = f.to_vec());
    assert_eq!(
        (t.file, t.folder, t.pos),
        (b"u.map".to_vec(), b"maps/".to_vec(), b"P".to_vec())
    );
    assert_eq!(set, b"maps/");
    assert_eq!(u.log, ["Map/File", "Map/Folder", "Map/StartPos"]);
    let story = base::StartStory {
        start_map: b"s.map",
        maps_folder: b"sm/",
        start_pos: b"",
    };
    let t = base::resolve_start(&none, Some(story), &mut u, &def, false, &mut |_| {});
    assert_eq!(
        (t.file, t.folder, t.pos),
        (b"s.map".to_vec(), b"sm/".to_vec(), b"P".to_vec())
    );
    let mut set = vec![];
    let t = base::resolve_start(&none, None, &mut u, &def, true, &mut |f| set = f.to_vec());
    assert_eq!(text(&t.file), "01_cells.map");
    assert_eq!(set, b"maps/");
}

#[derive(Default)]
struct Lang {
    log: Vec<String>,
    found: bool,
}

impl base::LangEnv for Lang {
    fn clear_translations(&mut self) {
        self.log.push("clear".into());
    }
    fn file_found(&mut self, n: &[u8]) -> bool {
        self.log.push(format!("found {}", text(n)));
        self.found
    }
    fn add_language_file(&mut self, n: &[u8], force: bool, user: bool) {
        self.log.push(format!("add {} {force} {user}", text(n)));
    }
}

#[test]
fn language_plan() {
    let f = base::LangFolders {
        game_folder: b"g/",
        base_folder: b"b/",
        default_game: b"english.lang",
        default_base: b"base_english.lang",
    };
    let mut l = Lang::default();
    assert_eq!(
        base::load_language(&mut l, b"French.lang", false, b"english.lang", None, &f),
        Some(b"french.lang".to_vec())
    );
    assert_eq!(
        l.log,
        [
            "clear",
            "add g/French.lang true true",
            "add b/base_French.lang true true",
            "add g/english.lang false true",
            "add b/base_english.lang false true"
        ]
    );
    let mut l = Lang::default();
    assert_eq!(
        base::load_language(&mut l, b"english.lang", false, b"english.lang", None, &f),
        None
    );
    assert!(l.log.is_empty());
}

#[test]
fn profile_paths() {
    let p = base::set_profile(&w("old"), true, &w("dev"), &w("/s/"), &w("/c/"));
    assert!(p.save_previous);
    assert_eq!(p.main_profile_save, w("/s/dev/"));
    assert_eq!(p.main_profile_config, w("/c/dev/"));
    assert_eq!(p.ops.len(), 3);
    let none = base::set_profile(&w(""), true, &w(""), &w("/s/"), &w("/c/"));
    assert!(!none.save_previous && none.main_profile_save.is_empty() && none.ops.is_empty());
    assert_eq!(
        base::profile_folder(&w("/s/"), &w("dev"), false),
        Some(w("/s/dev/"))
    );
    assert_eq!(base::profile_folder(&w("/s/"), &w("dev"), true), None);
}

#[test]
fn story_save_path_of_an_empty_root_has_no_folder() {
    assert_eq!(
        base::custom_story_save_path(&w("/s/dev/"), &w("custom_stories/Tale/")),
        w("/s/dev/custom/Tale/")
    );
    assert_eq!(
        base::custom_story_save_path(&w("/s/dev/"), &[]),
        w("/s/dev/custom/")
    );
}

#[test]
fn gui_offset_keeps_the_ratio() {
    let (size, off) = base::calc_gui_set_offset([800.0, 600.0], [1920.0, 1080.0]);
    assert!(size[0] > 800.0 && (off[0] - (size[0] - 800.0) * 0.5).abs() < 1e-3);
    assert_eq!(
        base::calc_gui_set_offset([800.0, 600.0], [800.0, 800.0]),
        ([800.0, 600.0], [0.0, 0.0])
    );
}

#[test]
fn start_modes_and_features() {
    assert_eq!(base::start_mode(true, true), base::StartMode::PreMenu);
    assert_eq!(base::start_mode(false, true), base::StartMode::Menu);
    assert_eq!(base::start_mode(true, false), base::StartMode::Direct);
    assert_eq!(base::direct_start_input(true, false), None);
    assert_eq!(base::direct_start_input(false, false), Some(true));
    assert!(base::feature_error(1, 4, 1).is_none());
    assert!(base::feature_error(1, 3, 1).is_some_and(|m| m.starts_with("Not enough")));
}

#[test]
fn axis_names() {
    assert_eq!(base::axis_from_name(b"Y"), (1, true));
    assert_eq!(base::axis_from_name(b"w"), (0, false));
}

#[test]
fn tables_are_consistent() {
    for a in &input::ACTIONS {
        assert_eq!(input::action_by_id(a.id), Some(a));
        assert_eq!(input::action_by_name(a.name.as_bytes()), Some(a));
    }
    assert_eq!(
        input::actions_by_category(input::CAT_LAST).len(),
        input::ACTIONS.len()
    );
    assert!(
        input::INPUTS
            .iter()
            .all(|x| input::action_by_id(x.action).is_some())
    );
    assert_eq!(input::default_inputs(input::act::FORWARD).len(), 2);
}

#[test]
fn input_strings_keep_the_quirks() {
    assert_eq!(
        input::sub_action_from_string(b"Keyboard.W"),
        (true, Some(SubAction::Key(66)))
    );
    assert_eq!(
        input::sub_action_from_string(b"keyboard.Nope"),
        (false, None)
    );
    assert_eq!(input::sub_action_from_string(b"Keyboard"), (false, None));
    assert_eq!(
        input::sub_action_from_string(b"MouseButton.LeftMouse"),
        (true, Some(SubAction::Mouse(0)))
    );
    assert_eq!(
        input::sub_action_from_string(b"GamepadAxis.Axis LeftY.Negative"),
        (true, Some(SubAction::PadAxis(1, 2)))
    );
    // unknown hat state keeps the not-found code 16 (compared against the hat enum end, 10)
    assert_eq!(
        input::sub_action_from_string(b"GamepadHat.Hat 0.Nope"),
        (true, Some(SubAction::PadHat(0, 16)))
    );
    // "gamepad" anywhere is valid, but only a prefix creates the action
    assert_eq!(
        input::sub_action_from_string(b"xgamepad.Axis LeftY.Positive"),
        (true, None)
    );
    assert_eq!(
        input::sub_action_from_string(b"GamepadButton.Button A"),
        (true, Some(SubAction::PadButton(0)))
    );
}

#[derive(Default)]
struct Actions {
    subs: BTreeMap<i32, Vec<SubAction>>,
    created: usize,
}

impl ActionEnv for Actions {
    fn create_action(&mut self, _: &[u8], _: i32) {
        self.created += 1;
    }
    fn clear_sub_actions(&mut self, id: i32) {
        self.subs.insert(id, vec![]);
    }
    fn add_sub_action(&mut self, id: i32, s: SubAction) {
        self.subs.entry(id).or_default().push(s);
    }
    fn sub_action_count(&mut self, id: i32) -> usize {
        self.subs.get(&id).map_or(0, Vec::len)
    }
    fn sub_action_input(&mut self, id: i32, i: usize) -> (Vec<u8>, Vec<u8>) {
        (format!("T{id}").into_bytes(), format!("N{i}").into_bytes())
    }
}

#[test]
fn user_keys_replace_defaults_only_when_valid() {
    let mut user = Fake::default();
    let mut keys = Fake::with(&[
        ("Forward", "Primary", "Keyboard.Up"),
        ("Jump", "Primary", "Keyboard.Nope"),
    ]);
    let mut e = Actions::default();
    let s = input::load_user_config(&mut user, &mut keys, &mut e);
    assert!(!s.invert_mouse && s.smooth_mouse && s.gamepad_look_sensitivity == 1.5);
    assert_eq!(e.subs.get(&input::act::FORWARD).map(Vec::len), Some(1));
    assert_eq!(e.subs.get(&input::act::JUMP).map(Vec::len), Some(2));
    let mut out = Fake::default();
    input::save_user_config(&s, &mut user, &mut out, &mut e);
    assert!(out.log.contains(&"set Forward/Primary=T33.N0".to_string()));
    assert!(out.log.contains(&"set Forward/Secondary=".to_string()));
}

#[test]
fn create_actions_binds_defaults() {
    let mut e = Actions::default();
    input::create_actions(&mut e);
    assert_eq!(e.created, input::ACTIONS.len());
    assert_eq!(
        e.subs.values().map(Vec::len).sum::<usize>(),
        input::INPUTS.len()
    );
}

#[test]
fn smoothing_weights_recent_positions() {
    let mut s = Smooth::new(2, 0.5);
    assert_eq!(s.push([1.0, 0.0]), [1.0, 0.0]);
    let r = s.push([0.0, 1.0]);
    assert!((r[0] - 1.0 / 3.0).abs() < 1e-6 && (r[1] - 2.0 / 3.0).abs() < 1e-6);
    s.reset();
    assert!(s.is_empty());
    // a limit of 0 empties the list and divides 0 by 0
    let mut z = Smooth::new(0, 0.0);
    assert!(z.push([1.0, 1.0])[0].is_nan() && z.is_empty());
}

struct Mouse {
    down: bool,
    abs: [i32; 2],
    time: f64,
    pad: bool,
    dir: bool,
    hid: u32,
}

impl MouseEnv for Mouse {
    fn any_mouse_button_down(&mut self) -> bool {
        self.down
    }
    fn abs_position(&mut self) -> [i32; 2] {
        self.abs
    }
    fn game_time(&mut self) -> f64 {
        self.time
    }
    fn gamepad_present(&mut self) -> bool {
        self.pad
    }
    fn ui_direction_triggered(&mut self) -> bool {
        self.dir
    }
    fn screen_size(&mut self) -> [i32; 2] {
        [1280, 720]
    }
    fn hide_mouse_uis(&mut self) {
        self.hid += 1;
    }
}

#[test]
fn mouse_activity_ignores_the_first_movement() {
    let mut a = MouseActivity::default();
    let mut e = Mouse {
        down: false,
        abs: [5, 5],
        time: 10.0,
        pad: true,
        dir: false,
        hid: 0,
    };
    assert!(!a.step(&mut e) && a.active_at == 10.0);
    e.abs = [6, 5];
    e.time = 11.0;
    assert!(a.step(&mut e));
    e.time = 17.0;
    assert!(!a.step(&mut e) && e.hid == 1 && a.active_at == -1.0 && a.last_abs == [640, 360]);
}

#[test]
fn gui_input_states() {
    assert!(input::state_sends_to_gui(input::STATE_MAIN_MENU, false));
    assert!(!input::state_sends_to_gui(input::STATE_INVENTORY, true));
    assert!(input::state_sends_to_gui(input::STATE_INVENTORY, false));
    assert!(!input::state_sends_to_gui(input::STATE_GAME, false));
}

#[test]
fn screenshot_names_skip_existing_files() {
    let mut taken = vec![w("/p/OpenHPL/Amnesia_Screenshot_000.jpg")];
    let n = input::screenshot_name(&w("/p/OpenHPL"), b"jpg", &mut |p| {
        taken.contains(&p.to_vec())
    });
    assert_eq!(n, w("/p/OpenHPL/Amnesia_Screenshot_001.jpg"));
    taken.clear();
}

#[test]
fn debug_messages_dedupe_expire_and_cap() {
    let mut d = Debug::default();
    d.add_message(&w("x"), false);
    assert!(d.messages.is_empty());
    d.f.show_debug_messages = true;
    d.f.show_error_messages = true;
    d.add_message(&w("a"), false);
    d.add_message(&w("a"), true);
    d.add_error_or_warning(&w("a"));
    assert_eq!(d.messages.len(), 1);
    d.add_message(&w("a"), false);
    assert_eq!(d.messages.len(), 2);
    for i in 0..120 {
        d.add_message(&w(&i.to_string()), false);
    }
    assert_eq!(d.messages.len(), 100);
    d.update_messages(3.0);
    assert_eq!(d.messages.len(), 100);
    d.update_messages(1.0);
    assert!(d.messages.is_empty());
}

#[test]
fn debug_batch_and_ticks() {
    let mut d = Debug::default();
    d.load_batch(b"a.map b.map\r\nc.map,");
    d.on_map_enter();
    let mut due = false;
    for _ in 0..31 {
        due = d.update_tick().1;
    }
    assert!(due && d.f.temp_count == 0);
    assert_eq!(d.batch_pop(), b"a.map");
    assert_eq!(d.batch_maps.len(), 2);
    assert_eq!(d.set_fast_forward(true), Some(4.0));
    assert_eq!(d.set_fast_forward(true), None);
    assert_eq!(Debug::window_change(false, true, false), None);
    assert_eq!(Debug::window_change(true, true, false), Some((true, true)));
}

#[test]
fn debug_widget_dispatch() {
    let mut d = Debug::default();
    assert_eq!(d.change_text(0, 1), Toggle::None);
    assert!(d.f.show_fps);
    assert_eq!(d.change_text(6, 1), Toggle::OcclusionCulling(true));
    assert_eq!(d.change_text(14, 50), Toggle::FreeCamSpeed(0.5));
    assert_eq!(d.change_text(14, 0), Toggle::FreeCamSpeed(0.001));
    assert_eq!(d.change_text(17, 0), Toggle::FastForward(false));
    assert_eq!(d.change_text(15, 1), Toggle::None);
}

#[test]
fn debug_flags_round_trip_through_the_config() {
    let mut d = Debug::default();
    let mut c = Fake::with(&[
        ("Debug", "ShowFPS", "false"),
        ("Debug", "AllowQuickSave", "true"),
    ]);
    d.load_user_config(&mut c, false);
    assert!(!d.f.show_fps && d.f.allow_quick_save && d.f.reload_from_current_position);
    d.load_user_config(&mut c, true);
    assert!(!d.f.show_player_info && d.f.allow_quick_save);
    let mut out = Fake::default();
    d.save_user_config(&mut out);
    assert_eq!(out.log.len(), 12);
}

#[derive(Default)]
struct HReq(BTreeMap<String, String>);

impl Req for HReq {
    fn has(&self, k: &[u8]) -> bool {
        self.0.contains_key(&text(k))
    }
    fn string(&self, k: &[u8], d: &[u8]) -> Vec<u8> {
        self.0
            .get(&text(k))
            .map_or_else(|| d.to_vec(), |v| v.clone().into_bytes())
    }
    fn int(&self, k: &[u8], d: i32) -> i32 {
        self.0.get(&text(k)).map_or(d, |v| v.parse().unwrap_or(d))
    }
    fn float(&self, k: &[u8], d: f32) -> f32 {
        self.0.get(&text(k)).map_or(d, |v| v.parse().unwrap_or(d))
    }
    fn bool(&self, k: &[u8], d: bool) -> bool {
        self.0.get(&text(k)).map_or(d, |v| v == "true" || v == "1")
    }
}

#[derive(Default)]
struct HResp(Vec<String>);

impl Resp for HResp {
    fn error(&mut self, m: &[u8]) {
        self.0.push(format!("error {}", text(m)));
    }
    fn set_str(&mut self, k: &[u8], v: &[u8]) {
        self.0.push(format!("{}={}", text(k), text(v)));
    }
    fn set_int(&mut self, k: &[u8], v: i32) {
        self.0.push(format!("{}={v}", text(k)));
    }
    fn set_float(&mut self, k: &[u8], v: f32) {
        self.0.push(format!("{}={v}", text(k)));
    }
    fn set_bool(&mut self, k: &[u8], v: bool) {
        self.0.push(format!("{}={v}", text(k)));
    }
    fn set_raw(&mut self, k: &[u8], v: &[u8]) {
        self.0.push(format!("{}={}", text(k), text(v)));
    }
}

#[derive(Default)]
struct HEnv {
    loaded: bool,
    user_cfg: bool,
    init_ok: bool,
    px: Option<Vec<f32>>,
    log: Vec<String>,
}

impl Env for HEnv {
    fn map_loaded(&mut self) -> bool {
        self.loaded
    }
    fn run_script(&mut self, l: &[u8]) {
        self.log.push(format!("script {}", text(l)));
    }
    fn set_debug_gbuffer(&mut self, _: bool) {}
    fn deferred_available(&mut self) -> bool {
        true
    }
    fn gbuffer_texture(&mut self, t: i32) -> Option<(i32, i32)> {
        (t == 1).then_some((2, 2))
    }
    fn gbuffer_pixels(&mut self, _: i32) -> Option<Vec<f32>> {
        self.px.clone()
    }
    fn map_name(&mut self) -> Vec<u8> {
        b"n".to_vec()
    }
    fn map_file(&mut self) -> Vec<u8> {
        b"f".to_vec()
    }
    fn player_body(&mut self) -> Option<Body> {
        None
    }
    fn fps(&mut self) -> f32 {
        60.0
    }
    fn occlusion_culling(&mut self) -> bool {
        true
    }
    fn set_occlusion_culling(&mut self, _: bool) {}
    fn set_gpu_timing(&mut self, _: bool) {}
    fn render_stats_json(&mut self) -> Vec<u8> {
        b"{}".to_vec()
    }
    fn save_game(&mut self, _: &[u8]) {}
    fn file_exists(&mut self, _: &[u8]) -> bool {
        false
    }
    fn load_game(&mut self, _: &[u8]) {}
    fn has_player(&mut self) -> bool {
        false
    }
    fn teleport(&mut self, _: [f32; 3]) {}
    fn have_user_config(&mut self) -> bool {
        self.user_cfg
    }
    fn create_and_set_default_profile(&mut self) {
        self.log.push("profile".into());
    }
    fn init_user_config(&mut self) -> bool {
        self.init_ok
    }
    fn enter_game_input(&mut self) {
        self.log.push("enter".into());
    }
    fn start_game(&mut self, m: &[u8], p: &[u8]) -> bool {
        self.log.push(format!("start {} {}", text(m), text(p)));
        false
    }
    fn camera_pose(&mut self) -> Option<([f32; 3], f32, f32)> {
        None
    }
    fn set_camera_pos(&mut self, _: [f32; 3]) {}
    fn set_camera_pitch(&mut self, _: f32) {}
    fn set_camera_yaw(&mut self, _: f32) {}
}

#[test]
fn headless_replies_keep_their_text() {
    let mut e = HEnv::default();
    let mut p = HResp::default();
    headless::run_script(&HReq::default(), &mut p, &mut e);
    headless::save_game(&HReq::default(), &mut p, &mut e);
    headless::load_game(&HReq::default(), &mut p, &mut e);
    headless::teleport(&HReq::default(), &mut p, &mut e);
    headless::start_map(&HReq::default(), &mut p, &mut e);
    assert_eq!(
        p.0,
        [
            "error no map loaded",
            "error need a loaded map and file=",
            "error file= must name an existing save",
            "error no player",
            "error missing 'map'"
        ]
    );
    let mut r = HReq::default();
    r.0.insert("map".into(), "m.hpm".into());
    let mut p = HResp::default();
    headless::start_map(&r, &mut p, &mut e);
    assert_eq!(p.0, ["error InitUserConfig() failed"]);
    e.init_ok = true;
    let mut p = HResp::default();
    headless::start_map(&r, &mut p, &mut e);
    assert_eq!(p.0, ["error StartGame('m.hpm') failed"]);
    assert_eq!(e.log, ["profile", "profile", "enter", "start m.hpm "]);
}

#[test]
fn gbuffer_stats() {
    let mut e = HEnv {
        px: Some(vec![
            1.0,
            0.0,
            f32::NAN,
            2.0, //
            -1.0,
            0.0,
            3.0,
            4.0, //
            0.5,
            0.0,
            1.0,
            0.0, //
            9.0,
            0.0,
            1.0,
            1.0,
        ]),
        ..HEnv::default()
    };
    let mut p = HResp::default();
    let mut r = HReq::default();
    r.0.insert("target".into(), "1".into());
    headless::read_gbuffer_stats(&r, &mut p, &mut e);
    assert!(p.0.contains(&"r_min=-1".to_string()) && p.0.contains(&"r_max=9".to_string()));
    assert!(
        p.0.contains(&"b_nan_count=1".to_string()) && p.0.contains(&"g_zero_count=4".to_string())
    );
    assert!(p.0.contains(&"sample_a=1".to_string()));
    r.0.insert("target".into(), "7".into());
    let mut p = HResp::default();
    headless::read_gbuffer_stats(&r, &mut p, &mut e);
    assert_eq!(p.0, ["error no such G-buffer target"]);
    e.px = None;
    r.0.insert("target".into(), "1".into());
    let mut p = HResp::default();
    headless::read_gbuffer_stats(&r, &mut p, &mut e);
    assert_eq!(p.0, ["error G-buffer target has no GPU data yet"]);
}

#[test]
fn state_reports_the_occlusion_flag() {
    let mut e = HEnv {
        loaded: true,
        ..HEnv::default()
    };
    let mut p = HResp::default();
    headless::state(&HReq::default(), &mut p, &mut e);
    assert_eq!(
        p.0,
        [
            "map_loaded=true",
            "map_name=n",
            "map_file=f",
            "fps=60",
            "occlusion_culling=true"
        ]
    );
}

#[test]
fn amfp_vars() {
    let mut c = Fake::default();
    let v = super::amfp::load_vars(true, &w("a.cfg"), &mut c);
    assert!(v.is_ok_and(|v| v.game_name == b"Amnesia - A Machine For Pigs"));
    assert!(
        super::amfp::load_vars(false, &w("a.cfg"), &mut c)
            .is_err_and(|m| m == w("Could not load main init file: a.cfg"))
    );
    assert_eq!(
        super::amfp::init_config_file(b""),
        w("config/main_init.cfg")
    );
}
