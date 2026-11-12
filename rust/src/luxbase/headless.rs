//! The headless control handlers of `LuxBase.cpp` (`run_script`, `set_debug_gbuffer`,
//! `read_gbuffer_stats`, `state`, `render_stats`, `save_game`, `load_game`, `teleport`,
//! `start_map`) and of the AMFP scaffold (`camera_state`, `set_camera`). Command and reply text
//! is what `scripts/hpl_control.py` and the test tools parse.

pub trait Req {
    fn has(&self, key: &[u8]) -> bool;
    fn string(&self, key: &[u8], def: &[u8]) -> Vec<u8>;
    fn int(&self, key: &[u8], def: i32) -> i32;
    fn float(&self, key: &[u8], def: f32) -> f32;
    fn bool(&self, key: &[u8], def: bool) -> bool;
}

pub trait Resp {
    fn error(&mut self, msg: &[u8]);
    fn set_str(&mut self, key: &[u8], v: &[u8]);
    fn set_int(&mut self, key: &[u8], v: i32);
    fn set_float(&mut self, key: &[u8], v: f32);
    fn set_bool(&mut self, key: &[u8], v: bool);
    fn set_raw(&mut self, key: &[u8], v: &[u8]);
}

#[derive(Clone, Copy, Debug, PartialEq)]
pub struct Body {
    pub pos: [f32; 3],
    pub vel: [f32; 3],
    pub health: f32,
    pub sanity: f32,
}

/// The engine and game calls of the handlers, made at the point the C++ made them.
pub trait Env {
    fn map_loaded(&mut self) -> bool;
    fn run_script(&mut self, line: &[u8]);
    fn set_debug_gbuffer(&mut self, on: bool);
    fn deferred_available(&mut self) -> bool;
    fn gbuffer_texture(&mut self, target: i32) -> Option<(i32, i32)>;
    fn gbuffer_pixels(&mut self, target: i32) -> Option<Vec<f32>>;
    fn map_name(&mut self) -> Vec<u8>;
    fn map_file(&mut self) -> Vec<u8>;
    fn player_body(&mut self) -> Option<Body>;
    fn fps(&mut self) -> f32;
    fn occlusion_culling(&mut self) -> bool;
    fn set_occlusion_culling(&mut self, on: bool);
    fn set_gpu_timing(&mut self, on: bool);
    fn render_stats_json(&mut self) -> Vec<u8>;
    fn save_game(&mut self, file: &[u8]);
    fn file_exists(&mut self, file: &[u8]) -> bool;
    fn load_game(&mut self, file: &[u8]);
    fn has_player(&mut self) -> bool;
    fn teleport(&mut self, pos: [f32; 3]);
    fn have_user_config(&mut self) -> bool;
    fn create_and_set_default_profile(&mut self);
    fn init_user_config(&mut self) -> bool;
    fn enter_game_input(&mut self);
    fn start_game(&mut self, map: &[u8], pos: &[u8]) -> bool;
    fn camera_pose(&mut self) -> Option<([f32; 3], f32, f32)>;
    fn set_camera_pos(&mut self, pos: [f32; 3]);
    fn set_camera_pitch(&mut self, v: f32);
    fn set_camera_yaw(&mut self, v: f32);
}

pub fn run_script(r: &dyn Req, p: &mut dyn Resp, e: &mut dyn Env) {
    if !e.map_loaded() {
        p.error(b"no map loaded");
        return;
    }
    e.run_script(&r.string(b"line", b""));
}

pub fn set_debug_gbuffer(r: &dyn Req, e: &mut dyn Env) {
    e.set_debug_gbuffer(r.bool(b"enabled", false));
}

const CHANNELS: [&[u8]; 4] = [b"r", b"g", b"b", b"a"];

fn key(ch: &[u8], suffix: &[u8]) -> Vec<u8> {
    [ch, suffix].concat()
}

/// Per-channel statistics of `read_gbuffer_stats`. Pixels past the end of `px` are not
/// visited and a sample past the end is not reported (the C++ read out of bounds).
pub fn read_gbuffer_stats(r: &dyn Req, p: &mut dyn Resp, e: &mut dyn Env) {
    if !e.deferred_available() {
        p.error(b"no deferred renderer yet");
        return;
    }
    let target = r.int(b"target", 1);
    let Some((width, height)) = e.gbuffer_texture(target) else {
        p.error(b"no such G-buffer target");
        return;
    };
    let Some(px) = e.gbuffer_pixels(target) else {
        p.error(b"G-buffer target has no GPU data yet");
        return;
    };
    let mut min = [f32::MAX; 4];
    let mut max = [-f32::MAX; 4];
    let mut sum = [0.0f32; 4];
    let mut nan = [0i32; 4];
    let mut zero = [0i32; 4];
    let num = (i64::from(width) as u64).wrapping_mul(i64::from(height) as u64);
    for (n, pix) in px.chunks_exact(4).enumerate() {
        if n as u64 >= num {
            break;
        }
        for (c, &v) in pix.iter().enumerate() {
            let (Some(mn), Some(mx), Some(s), Some(na), Some(ze)) = (
                min.get_mut(c),
                max.get_mut(c),
                sum.get_mut(c),
                nan.get_mut(c),
                zero.get_mut(c),
            ) else {
                continue;
            };
            if v.is_nan() {
                *na += 1;
                continue;
            }
            if v == 0.0 {
                *ze += 1;
            }
            if v < *mn {
                *mn = v;
            }
            if v > *mx {
                *mx = v;
            }
            *s += v;
        }
    }
    p.set_int(b"width", width);
    p.set_int(b"height", height);
    for (c, ch) in CHANNELS.iter().enumerate() {
        let get = |a: &[f32; 4]| a.get(c).copied().unwrap_or(0.0);
        let nans = nan.get(c).copied().unwrap_or(0);
        let finite = num.wrapping_sub(nans as u64);
        p.set_float(&key(ch, b"_min"), get(&min));
        p.set_float(&key(ch, b"_max"), get(&max));
        let mean = if finite > 0 {
            get(&sum) / (finite as f32)
        } else {
            0.0
        };
        p.set_float(&key(ch, b"_mean"), mean);
        p.set_int(&key(ch, b"_nan_count"), nans);
        p.set_int(&key(ch, b"_zero_count"), zero.get(c).copied().unwrap_or(0));
    }
    let sx = r.int(b"x", width.checked_div(2).unwrap_or(0));
    let sy = r.int(b"y", height.checked_div(2).unwrap_or(0));
    if sx >= 0 && sx < width && sy >= 0 && sy < height {
        let idx = ((i64::from(sy) * i64::from(width) + i64::from(sx)) as usize).wrapping_mul(4);
        if let Some(s) = idx.checked_add(4).and_then(|end| px.get(idx..end)) {
            for (name, v) in [b"sample_r", b"sample_g", b"sample_b", b"sample_a"]
                .iter()
                .zip(s)
            {
                p.set_float(*name, *v);
            }
        }
    }
}

pub fn state(r: &dyn Req, p: &mut dyn Resp, e: &mut dyn Env) {
    let loaded = e.map_loaded();
    p.set_bool(b"map_loaded", loaded);
    if e.map_loaded() {
        p.set_str(b"map_name", &e.map_name());
        p.set_str(b"map_file", &e.map_file());
    }
    if let Some(b) = e.player_body() {
        for (k, v) in [b"pos_x", b"pos_y", b"pos_z"].iter().zip(b.pos) {
            p.set_float(*k, v);
        }
        for (k, v) in [b"vel_x", b"vel_y", b"vel_z"].iter().zip(b.vel) {
            p.set_float(*k, v);
        }
        p.set_float(b"health", b.health);
        p.set_float(b"sanity", b.sanity);
    }
    p.set_float(b"fps", e.fps());
    if r.has(b"occlusion_culling") {
        let on = r.bool(b"occlusion_culling", true);
        e.set_occlusion_culling(on);
    }
    p.set_bool(b"occlusion_culling", e.occlusion_culling());
}

pub fn render_stats(r: &dyn Req, p: &mut dyn Resp, e: &mut dyn Env) {
    if r.has(b"gpu") {
        e.set_gpu_timing(r.int(b"gpu", 0) != 0);
    }
    p.set_raw(b"render", &e.render_stats_json());
    p.set_float(b"fps", e.fps());
}

pub fn save_game(r: &dyn Req, p: &mut dyn Resp, e: &mut dyn Env) {
    let file = r.string(b"file", b"");
    if !e.map_loaded() || file.is_empty() {
        p.error(b"need a loaded map and file=");
        return;
    }
    e.save_game(&file);
}

pub fn load_game(r: &dyn Req, p: &mut dyn Resp, e: &mut dyn Env) {
    let file = r.string(b"file", b"");
    if file.is_empty() || !e.file_exists(&file) {
        p.error(b"file= must name an existing save");
        return;
    }
    e.load_game(&file);
}

pub fn teleport(r: &dyn Req, p: &mut dyn Resp, e: &mut dyn Env) {
    if !e.has_player() {
        p.error(b"no player");
        return;
    }
    let pos = [r.float(b"x", 0.0), r.float(b"y", 0.0), r.float(b"z", 0.0)];
    e.teleport(pos);
}

pub fn start_map(r: &dyn Req, p: &mut dyn Resp, e: &mut dyn Env) {
    let map = r.string(b"map", b"");
    if map.is_empty() {
        p.error(b"missing 'map'");
        return;
    }
    if !e.have_user_config() {
        e.create_and_set_default_profile();
        if !e.init_user_config() {
            p.error(b"InitUserConfig() failed");
            return;
        }
    }
    e.enter_game_input();
    if !e.start_game(&map, &r.string(b"start_pos", b"")) {
        p.error(&[b"StartGame('".as_slice(), &map, b"') failed"].concat());
    }
}

pub fn camera_state(p: &mut dyn Resp, e: &mut dyn Env) {
    let Some((pos, pitch, yaw)) = e.camera_pose() else {
        p.error(b"no camera yet");
        return;
    };
    p.set_float(b"pos_x", pos[0]);
    p.set_float(b"pos_y", pos[1]);
    p.set_float(b"pos_z", pos[2]);
    p.set_float(b"pitch", pitch);
    p.set_float(b"yaw", yaw);
    p.set_float(b"fps", e.fps());
}

pub fn set_camera(r: &dyn Req, p: &mut dyn Resp, e: &mut dyn Env) {
    let Some((cur, _, _)) = e.camera_pose() else {
        p.error(b"no camera yet");
        return;
    };
    if r.has(b"x") || r.has(b"y") || r.has(b"z") {
        let pos = [
            r.float(b"x", cur[0]),
            r.float(b"y", cur[1]),
            r.float(b"z", cur[2]),
        ];
        e.set_camera_pos(pos);
    }
    if r.has(b"pitch") {
        e.set_camera_pitch(r.float(b"pitch", 0.0));
    }
    if r.has(b"yaw") {
        e.set_camera_yaw(r.float(b"yaw", 0.0));
    }
}
