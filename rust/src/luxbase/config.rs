//! `LuxConfigHandler`: the main settings it reads on start and writes on exit.

use super::cfg::Cfg;

#[derive(Clone, Debug, PartialEq)]
pub struct MainCfg {
    pub load_debug_menu: bool,
    pub first_start: bool,
    pub lang_file: Vec<u8>,
    pub screenshot_ext: Vec<u8>,
    pub force_cache_loading_and_skip_saving: bool,
    pub screen_width: i32,
    pub screen_height: i32,
    pub display: i32,
    pub fullscreen: bool,
    pub vsync: bool,
    pub adaptive_vsync: bool,
    pub fast_physics_load: bool,
    pub fast_static_load: bool,
    pub fast_entity_load: bool,
    pub occlusion_test_lights: bool,
    pub shadows_active: bool,
    pub shadow_quality: i32,
    pub shadow_res: i32,
    pub gui_scale: i32,
    pub renderer: Vec<u8>,
    pub color_output: Vec<u8>,
    pub paper_white: f32,
    pub peak_nits: f32,
    pub world_reflection: bool,
    pub refraction: bool,
    pub edge_smooth: bool,
    pub ssao_active: bool,
    pub ssao_samples: i32,
    pub ssao_resolution: i32,
    pub parallax_enabled: bool,
    pub parallax_quality: i32,
    pub texture_quality: i32,
    pub texture_filter: i32,
    pub texture_anisotropy: f32,
    pub force_shader_model_3_and_4_off: bool,
    pub sound_dev_id: i32,
    pub max_sound_channels: i32,
    pub sound_stream_buffers: i32,
    pub sound_stream_buffer_size: i32,
}

/// `LoadMainConfig`; `first_start_flag_exists` is `CheckFirstStartFlag()`, asked after
/// `LoadDebugMenu`; `default_lang` the game's default language file.
pub fn load_main(
    c: &mut dyn Cfg,
    first_start_flag_exists: &mut dyn FnMut() -> bool,
    default_lang: &[u8],
) -> MainCfg {
    let load_debug_menu = c.bool(b"Main", b"LoadDebugMenu", false);
    MainCfg {
        load_debug_menu,
        first_start: !first_start_flag_exists(),
        lang_file: c.string(b"Main", b"StartLanguage", default_lang),
        screenshot_ext: c.string(b"Main", b"ScreenShotExt", b"jpg"),
        force_cache_loading_and_skip_saving: c.bool(
            b"Main",
            b"ForceCacheLoadingAndSkipSaving",
            true,
        ),
        screen_width: c.int(b"Screen", b"Width", 800),
        screen_height: c.int(b"Screen", b"Height", 600),
        display: c.int(b"Screen", b"Display", 0),
        fullscreen: c.bool(b"Screen", b"FullScreen", false),
        vsync: c.bool(b"Screen", b"Vsync", false),
        adaptive_vsync: c.bool(b"Screen", b"AdaptiveVsync", false),
        fast_physics_load: c.bool(b"MapLoad", b"FastPhysicsLoad", false),
        fast_static_load: c.bool(b"MapLoad", b"FastStaticLoad", false),
        fast_entity_load: c.bool(b"MapLoad", b"FastEntityLoad", false),
        occlusion_test_lights: c.bool(b"Graphics", b"OcclusionTestLights", true),
        shadows_active: c.bool(b"Graphics", b"ShadowsActive", true),
        shadow_quality: c.int(b"Graphics", b"ShadowQuality", 1),
        shadow_res: c.int(b"Graphics", b"ShadowResolution", 2),
        gui_scale: c.int(b"Graphics", b"GuiScale", 1),
        renderer: c.string(b"Graphics", b"Renderer", b"OpenGL"),
        color_output: c.string(b"Graphics", b"ColorOutput", b"SDR"),
        paper_white: c.float(b"Graphics", b"HdrPaperWhite", 203.0),
        peak_nits: c.float(b"Graphics", b"HdrPeak", 1000.0),
        world_reflection: c.bool(b"Graphics", b"WorldReflection", true),
        refraction: c.bool(b"Graphics", b"Refraction", true),
        edge_smooth: c.bool(b"Graphics", b"EdgeSmooth", false),
        ssao_active: c.bool(b"Graphics", b"SSAOActive", true),
        ssao_samples: c.int(b"Graphics", b"SSAOSamples", 8),
        ssao_resolution: c.int(b"Graphics", b"SSAOResolution", 0),
        parallax_enabled: c.bool(b"Graphics", b"ParallaxEnabled", true),
        parallax_quality: c.int(b"Graphics", b"ParallaxQuality", 0),
        texture_quality: c.int(b"Graphics", b"TextureQuality", 0),
        texture_filter: c.int(b"Graphics", b"TextureFilter", 1),
        texture_anisotropy: c.float(b"Graphics", b"TextureAnisotropy", 1.0),
        force_shader_model_3_and_4_off: c.bool(b"Graphics", b"ForceShaderModel3And4Off", false),
        sound_dev_id: c.int(b"Sound", b"Device", -1),
        max_sound_channels: c.int(b"Sound", b"MaxChannels", 32),
        sound_stream_buffers: c.int(b"Sound", b"StreamBuffers", 4),
        sound_stream_buffer_size: c.int(b"Sound", b"StreamBufferSize", 262_144),
    }
}

/// What `SaveMainConfig` asks the engine at the point it needs it.
pub trait SaveEnv: Cfg {
    fn raise_first_start_flag(&mut self);
    fn gamma(&mut self) -> f32;
    fn gbuffer_type(&mut self) -> i32;
    fn num_gbuffer_textures(&mut self) -> i32;
    fn volume(&mut self) -> f32;
    fn limit_fps(&mut self) -> bool;
    fn wait_if_out_of_focus(&mut self) -> bool;
}

pub fn save_main(e: &mut dyn SaveEnv, m: &MainCfg) {
    e.set_bool(b"Main", b"LoadDebugMenu", m.load_debug_menu);
    if m.first_start {
        e.raise_first_start_flag();
    }
    e.set_string(b"Main", b"StartLanguage", &m.lang_file);
    e.set_string(b"Main", b"ScreenShotExt", &m.screenshot_ext);
    e.set_bool(
        b"Main",
        b"ForceCacheLoadingAndSkipSaving",
        m.force_cache_loading_and_skip_saving,
    );
    e.set_int(b"Screen", b"Width", m.screen_width);
    e.set_int(b"Screen", b"Height", m.screen_height);
    e.set_bool(b"Screen", b"FullScreen", m.fullscreen);
    e.set_bool(b"Screen", b"Vsync", m.vsync);
    e.set_bool(b"MapLoad", b"FastPhysicsLoad", m.fast_physics_load);
    e.set_bool(b"MapLoad", b"FastStaticLoad", m.fast_static_load);
    e.set_bool(b"MapLoad", b"FastEntityLoad", m.fast_entity_load);
    e.set_string(b"Graphics", b"Renderer", &m.renderer);
    e.set_string(b"Graphics", b"ColorOutput", &m.color_output);
    e.set_float(b"Graphics", b"HdrPaperWhite", m.paper_white);
    e.set_float(b"Graphics", b"HdrPeak", m.peak_nits);
    let g = e.gamma();
    e.set_float(b"Graphics", b"Gamma", g);
    let g = e.gbuffer_type();
    e.set_int(b"Graphics", b"GBufferType", g);
    let g = e.num_gbuffer_textures();
    e.set_int(b"Graphics", b"NumOfGBufferTextures", g);
    e.set_bool(b"Graphics", b"OcclusionTestLights", m.occlusion_test_lights);
    e.set_int(b"Graphics", b"TextureQuality", m.texture_quality);
    e.set_int(b"Graphics", b"TextureFilter", m.texture_filter);
    e.set_float(b"Graphics", b"TextureAnisotropy", m.texture_anisotropy);
    e.set_bool(b"Graphics", b"SSAOActive", m.ssao_active);
    e.set_int(b"Graphics", b"SSAOResolution", m.ssao_resolution);
    e.set_int(b"Graphics", b"SSAOSamples", m.ssao_samples);
    e.set_bool(b"Graphics", b"WorldReflection", m.world_reflection);
    e.set_bool(b"Graphics", b"Refraction", m.refraction);
    e.set_bool(b"Graphics", b"ShadowsActive", m.shadows_active);
    e.set_int(b"Graphics", b"ShadowQuality", m.shadow_quality);
    e.set_int(b"Graphics", b"ShadowResolution", m.shadow_res);
    e.set_int(b"Graphics", b"GuiScale", m.gui_scale);
    e.set_int(b"Graphics", b"ParallaxQuality", m.parallax_quality);
    e.set_bool(b"Graphics", b"ParallaxEnabled", m.parallax_enabled);
    e.set_bool(b"Graphics", b"EdgeSmooth", m.edge_smooth);
    e.set_bool(
        b"Graphics",
        b"ForceShaderModel3And4Off",
        m.force_shader_model_3_and_4_off,
    );
    e.set_int(b"Sound", b"Device", m.sound_dev_id);
    let v = e.volume();
    e.set_float(b"Sound", b"Volume", v);
    e.set_int(b"Sound", b"MaxChannels", m.max_sound_channels);
    e.set_int(b"Sound", b"StreamBuffers", m.sound_stream_buffers);
    e.set_int(b"Sound", b"StreamBufferSize", m.sound_stream_buffer_size);
    let l = e.limit_fps();
    e.set_bool(b"Engine", b"LimitFPS", l);
    let w = e.wait_if_out_of_focus();
    e.set_bool(b"Engine", b"SleepWhenOutOfFocus", w);
}

/// `ShowRestartWarning`'s gate: the popup shows once per `SetGameNeedsRestart`.
#[derive(Clone, Copy, Default, Debug, PartialEq)]
pub struct Restart {
    pub needed: bool,
    pub dialog_shown: bool,
}

impl Restart {
    pub fn set_needed(&mut self) {
        self.needed = true;
        self.dialog_shown = false;
    }

    pub fn show_due(&mut self) -> bool {
        if self.needed && !self.dialog_shown {
            self.dialog_shown = true;
            true
        } else {
            false
        }
    }
}
