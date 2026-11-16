//! `cLuxMap`: load, enter/leave, update, checkpoints, enemy broadcasts, completion and entity destruction.
//! The map state is the A7 [`Ctx`]; the engine (world, scripts, entities, player, music) is reached through [`mhost`].
//! No reference to the context is held across a host op: entity code re-enters through the same context.

use super::a7_ffi::{
    Ctx, ohpl_a7_destroy_all, ohpl_a7_destroy_entities, ohpl_a7_ent_add, ohpl_a7_ent_unlink,
    ohpl_a7_teardown,
};
use super::a7_map::{A7Map, W_DOOMED, W_ENEMIES, W_ENTITIES, W_STICKY};
use super::mhost::{self, A, S, op};
use super::{area, map};
use crate::math::{dot3, fma};
use crate::string;
use crate::types::Vector3f;
use std::ffi::c_void;

#[derive(Default)]
pub struct Props {
    pub cmap: usize,
    pub file: Vec<u8>,
    pub world: usize,
    pub phys: usize,
    pub script: usize,
    pub quests: i32,
    pub total: i32,
    pub current: i32,
    pub cp_name: Vec<u8>,
    pub cp_start: Vec<u8>,
    pub cp_callback: Vec<u8>,
    pub cp_count: i32,
    pub cp_music: Vec<u8>,
    pub cp_prio: i32,
    pub cp_resume: bool,
    pub cp_volume: f32,
    pub frames: i32,
}

pub mod log {
    pub const ERROR: i32 = 0;
    pub const WARNING: i32 = 1;
    pub const FATAL: i32 = 2;
    pub const DEBUG: i32 = 3;
    pub const PROGRESS: i32 = 4;
}

pub mod ent {
    pub const AFTER_WORLD_LOAD: i32 = 0;
    pub const ON_MAP_ENTER: i32 = 1;
    pub const UPDATE_LOGIC: i32 = 2;
    pub const RENDER_SOLID: i32 = 3;
    pub const BEFORE_DESTRUCTION: i32 = 4;
    pub const DESTROY_ME: i32 = 5;
    pub const SET_ACTIVE: i32 = 6;
    pub const PLAYER_ON_DESTROY: i32 = 7;
    pub const IS_ACTIVE: i32 = 8;
    pub const GET_DESTROY_ME: i32 = 9;
    pub const IS_COMMENTARY: i32 = 10;
    pub const HARD_ITEM: i32 = 11;
}

pub const ITEM_TINDERBOX: i32 = 1;
pub const ITEM_LAMP_OIL: i32 = 2;

#[derive(Clone, Copy)]
pub struct Broadcast {
    pub kind: i32,
    pub has_pos: bool,
    pub pos: Vector3f,
    pub radius: f32,
    pub time: f32,
    pub local: bool,
    pub vx: Vector3f,
    pub fx: f32,
    pub lx: i32,
}

#[derive(Clone, Copy)]
pub struct M(*mut Ctx);

fn host(o: i32, f: impl FnOnce(&mut A)) -> A {
    let mut a = A::new();
    f(&mut a);
    mhost::call(o, &mut a);
    a
}

fn note(level: i32, msg: &[u8]) {
    host(op::LOG, |a| {
        a.i[0] = level;
        a.s[0] = S::of(msg);
    });
}

fn vec3(x: f32, y: f32, z: f32) -> Vector3f {
    Vector3f { x, y, z }
}

fn dist3(a: Vector3f, b: Vector3f) -> f32 {
    let (dx, dy, dz) = (b.x - a.x, b.y - a.y, b.z - a.z);
    fma(dz, dz, fma(dx, dx, dy * dy)).sqrt()
}

fn sphere_hit(pa: Vector3f, ra: f32, pb: Vector3f, rb: f32) -> bool {
    let r = ra + rb;
    let sep = Vector3f {
        x: pa.x - pb.x,
        y: pa.y - pb.y,
        z: pa.z - pb.z,
    };
    dot3(sep, sep) <= r * r
}

impl M {
    /// # Safety
    /// `c` is a live context from `ohpl_a7_new` that stays live for every call on the handle.
    pub unsafe fn new(c: *mut Ctx) -> M {
        M(c)
    }

    fn pr<'a>(self) -> &'a mut Props {
        // SAFETY: `new` contract; every borrow is a statement-local temporary.
        unsafe { &mut (*self.0).props }
    }

    fn map<'a>(self) -> &'a mut A7Map {
        // SAFETY: as `pr`.
        unsafe { &mut (*self.0).map }
    }

    fn cmap(&self) -> usize {
        self.pr().cmap
    }

    fn walk(&self, which: u32, mut f: impl FnMut(usize)) {
        let mut cur = self.map().cursor_begin(which);
        loop {
            let p = self.map().cursor_get(which, cur);
            if p == 0 {
                break;
            }
            f(p);
            cur = self.map().cursor_next(which, cur);
        }
    }

    fn ent_call(&self, e: usize, which: i32) -> A {
        host(op::ENT_CALL, |a| {
            a.p[0] = e;
            a.i[0] = which;
        })
    }

    pub fn init(&self, cmap: usize) {
        self.pr().cmap = cmap;
    }

    // ---- loading

    pub fn load_from_file(&self, file: &[u8], load_entities: bool) {
        self.pr().file = file.to_vec();
        host(op::SET_LOADING, |a| a.p[0] = self.cmap());
        let cfg = host(op::LOAD_CFG, |_| {});
        let flags = map::world_load_flags(
            load_entities,
            cfg.i[0] != 0,
            cfg.i[1] != 0,
            cfg.i[2] != 0,
            [
                cfg.p[0] as i32,
                cfg.p[1] as i32,
                cfg.p[2] as i32,
                cfg.p[3] as i32,
            ],
        );

        self.load_script(file, cfg.i[3] != 0);

        let w = host(op::WORLD_LOAD, |a| {
            a.s[0] = S::of(file);
            a.i[0] = flags;
        });
        if w.r == 0 {
            note(
                log::FATAL,
                &[b"Could not load world file '", file, b"'\n"].concat(),
            );
        }
        self.pr().world = w.r;
        self.pr().phys = w.p[1];

        if load_entities {
            self.after_world_load();
        }
        host(op::SET_LOADING, |_| {});

        if host(op::HARD_MODE, |_| {}).i[0] != 0 {
            self.apply_hard_mode();
        }
    }

    fn load_script(&self, file: &[u8], compressed_enabled: bool) {
        let mut script_file = string::set_file_ext(file, b"hps");
        let plain = host(op::FILE_EXISTS, |a| a.s[0] = S::of(&script_file)).i[0] != 0;
        let mut compressed = false;
        if !plain && compressed_enabled {
            script_file = string::set_file_ext(file, b"chps");
            compressed = host(op::FILE_EXISTS, |a| a.s[0] = S::of(&script_file)).i[0] != 0;
        }
        if map::script_choice(plain, compressed_enabled, compressed) == map::SCRIPT_NONE {
            note(
                log::ERROR,
                &[
                    b"Script file ",
                    script_file.as_slice(),
                    b" does not exist!\n",
                ]
                .concat(),
            );
            return;
        }
        let s = host(op::SCRIPT_CREATE, |a| a.s[0] = S::of(&script_file));
        if s.r == 0 {
            let errors = map::compile_errors(&s.ret_bytes(0));
            note(
                log::FATAL,
                &[
                    b"Could not load script file '".as_slice(),
                    &script_file,
                    b"'!\n",
                    &errors,
                ]
                .concat(),
            );
        }
        self.pr().script = s.r;
    }

    fn apply_hard_mode(&self) {
        let every = map::hard_tinderbox_every(host(op::HARD_MODE, |_| {}).i[1]);
        let mut tinder_counter = 0.0f32;
        let oil_every = map::HARD_OIL_EVERY;
        let mut oil_counter = 0.0f32;
        let mut doomed = Vec::new();
        self.walk(W_ENTITIES, |e| {
            match self.ent_call(e, ent::HARD_ITEM).i[0] {
                ITEM_TINDERBOX if map::hard_remove(&mut tinder_counter, every) => doomed.push(e),
                ITEM_LAMP_OIL if map::hard_remove(&mut oil_counter, oil_every) => doomed.push(e),
                _ => {}
            }
        });
        for e in doomed {
            self.destroy_entity(e);
        }
    }

    pub fn after_world_load(&self) {
        self.walk(W_ENTITIES, |e| {
            self.ent_call(e, ent::AFTER_WORLD_LOAD);
        });
    }

    // ---- enter, leave, scripts

    pub fn on_enter(&self, run_script: bool, first_time: bool) {
        self.walk(W_ENTITIES, |e| {
            self.ent_call(e, ent::ON_MAP_ENTER);
        });
        let script = self.pr().script;
        let plan = map::enter_plan(run_script, script != 0, first_time);
        if plan & map::ENTER_START != 0 {
            self.run_on(script, b"OnStart()");
        }
        if plan & map::ENTER_COMPLETION != 0 {
            self.calculate_total_completion();
        }
        if plan & map::ENTER_ENTER != 0 {
            self.run_on(script, b"OnEnter()");
        }
    }

    pub fn on_leave(&self, run_script: bool) {
        let script = self.pr().script;
        if map::run_script_allowed(script != 0, run_script) {
            self.run_on(script, b"OnLeave()");
        }
    }

    fn run_on(&self, script: usize, cmd: &[u8]) {
        host(op::SCRIPT_RUN, |a| {
            a.p[0] = script;
            a.s[0] = S::of(cmd);
        });
    }

    pub fn run_script(&self, cmd: &[u8]) {
        let script = self.pr().script;
        let current = host(op::IS_CURRENT_MAP, |a| a.p[0] = self.cmap()).i[0] != 0;
        if map::run_script_allowed(script != 0, current) {
            self.run_on(script, cmd);
        }
    }

    /// Returns the compile output of the new script and whether it is usable.
    pub fn recompile_script(&self) -> (bool, Vec<u8>) {
        let old = std::mem::take(&mut self.pr().script);
        if old != 0 {
            host(op::SCRIPT_DESTROY, |a| a.p[0] = old);
        }
        let file = string::set_file_ext(&self.pr().file.clone(), b"hps");
        if host(op::FILE_EXISTS, |a| a.s[0] = S::of(&file)).i[0] != 0 {
            let s = host(op::SCRIPT_CREATE, |a| a.s[0] = S::of(&file));
            self.pr().script = s.r;
            (s.r != 0, s.ret_bytes(0))
        } else {
            note(
                log::ERROR,
                &[b"Script file ", file.as_slice(), b" does not exist!\n"].concat(),
            );
            (true, Vec::new())
        }
    }

    // ---- update

    pub fn update(&self, step: f32) {
        let frames = self.pr().frames;
        host(op::UPDATE_HOOKS, |a| {
            a.p[0] = self.cmap();
            a.i[0] = frames;
        });
        self.pr().frames = frames.wrapping_add(1);

        self.update_commentary_icons();
        self.update_dissolve(step);
        self.update_timers(step);
        self.update_doomed(true);
        self.walk(W_ENTITIES, |e| {
            if self.ent_call(e, ent::IS_ACTIVE).i[0] != 0 {
                host(op::ENT_CALL, |a| {
                    a.p[0] = e;
                    a.i[0] = ent::UPDATE_LOGIC;
                    a.f[0] = step;
                });
            }
        });
        self.update_doomed(true);
        self.update_lamps(step);
    }

    pub fn render_solid(&self, functions: usize) {
        if host(op::SHOW_ENTITY_INFO, |_| {}).i[0] == 0 {
            return;
        }
        self.walk(W_ENTITIES, |e| {
            if self.ent_call(e, ent::IS_ACTIVE).i[0] != 0 {
                host(op::ENT_CALL, |a| {
                    a.p[0] = e;
                    a.i[0] = ent::RENDER_SOLID;
                    a.p[1] = functions;
                });
            }
        });
    }

    fn update_commentary_icons(&self) {
        let show = host(op::SHOW_COMMENTARY, |_| {}).i[0] != 0;
        let Some(on) = self.map().commentary_sync(show) else {
            return;
        };
        self.walk(W_ENTITIES, |e| {
            if self.ent_call(e, ent::IS_COMMENTARY).i[0] != 0 {
                host(op::ENT_CALL, |a| {
                    a.p[0] = e;
                    a.i[0] = ent::SET_ACTIVE;
                    a.i[1] = i32::from(on);
                });
            }
        });
    }

    fn update_dissolve(&self, step: f32) {
        let (out, f) = {
            let mm = self.map();
            (
                mm.dissolve_step(step),
                mm.foreign(super::a7_map::K_DISSOLVE),
            )
        };
        for (obj, alpha, remove) in out {
            host(op::DISSOLVE_ALPHA, |a| {
                a.p[0] = obj;
                a.f[0] = alpha;
            });
            if remove {
                // SAFETY: `obj` is a live dissolve object dropped once.
                unsafe { f.destroy(obj) };
            }
        }
    }

    fn update_timers(&self, step: f32) {
        self.map().timers_begin(step);
        let mut cursor = 0usize;
        while let Some((name, func)) = self.map().timer_poll(&mut cursor) {
            self.run_script(&area::call1(&func, &name));
            self.map().timer_finish(cursor);
        }
        self.map().timers_end();
    }

    fn update_lamps(&self, step: f32) {
        for conn in self.map().lamp_snapshot() {
            host(op::LAMP_UPDATE, |a| {
                a.p[0] = conn;
                a.f[0] = step;
            });
        }
    }

    pub fn update_doomed(&self, use_callbacks: bool) {
        let mut cur = self.map().cursor_begin(W_DOOMED);
        loop {
            let e = self.map().cursor_get(W_DOOMED, cur);
            if e == 0 {
                break;
            }
            if use_callbacks {
                self.ent_call(e, ent::PLAYER_ON_DESTROY);
                self.ent_call(e, ent::BEFORE_DESTRUCTION);
            }
            let k = host(op::ENT_KIND, |a| a.p[0] = e);
            // SAFETY: `self.0` is live; `e` and the typed pointer were added through `add_entity`.
            unsafe {
                ohpl_a7_ent_unlink(
                    self.0,
                    e as *mut c_void,
                    k.r as *mut c_void,
                    k.i[1] != 0,
                    k.i[2] != 0,
                    k.i[3] != 0,
                );
            }
            host(op::ENT_DELETE, |a| a.p[0] = e);
            cur = self.map().cursor_next(W_DOOMED, cur);
        }
        self.map().list_clear(W_DOOMED);
    }

    // ---- entities

    pub fn add_entity(&self, e: usize) {
        let k = host(op::ENT_KIND, |a| a.p[0] = e);
        let name = k.ret_bytes(0);
        // SAFETY: `self.0` is live; `name` outlives the call.
        unsafe {
            ohpl_a7_ent_add(
                self.0,
                e as *mut c_void,
                k.r as *mut c_void,
                name.as_ptr(),
                name.len(),
                k.p[2] as i32,
                k.i[1] != 0,
                k.i[2] != 0,
                k.i[3] != 0,
            );
        }
    }

    pub fn destroy_entity(&self, e: usize) {
        if self.map().deleting_all() {
            let name = host(op::ENT_KIND, |a| a.p[0] = e).ret_bytes(0);
            note(
                log::WARNING,
                &[
                    b"Trying to delete entity '".as_slice(),
                    &name,
                    b"' when Map is destroying all entities!\n",
                ]
                .concat(),
            );
        }
        if self.ent_call(e, ent::GET_DESTROY_ME).i[0] != 0 {
            return;
        }
        self.ent_call(e, ent::DESTROY_ME);
        self.map().doom_push(e);
    }

    pub fn destroy_all_entities(&self) {
        // SAFETY: `self.0` is live.
        unsafe { ohpl_a7_destroy_all(self.0) };
    }

    pub fn create_entity(&self, name: &[u8], file: &[u8], mtx: &[f32; 16], scale: &[f32; 3]) {
        let set = host(op::LOADING_MAP, |_| {}).r == 0;
        if set {
            host(op::SET_LOADING, |a| a.p[0] = self.cmap());
        }
        let id = self.map().free_entity_id();
        let world = self.pr().world;
        host(op::WORLD_CREATE_ENTITY, |a| {
            a.p[0] = world;
            a.s = [S::of(name), S::of(file)];
            a.m = *mtx;
            a.f[..3].copy_from_slice(scale);
            a.i[0] = id;
        });
        if set {
            host(op::SET_LOADING, |_| {});
        }
    }

    pub fn teardown(&self) {
        // SAFETY: `self.0` is live.
        unsafe { ohpl_a7_teardown(self.0) };
        let (world, script) = (self.pr().world, std::mem::take(&mut self.pr().script));
        host(op::WORLD_DESTROY, |a| a.p[0] = world);
        if script != 0 {
            host(op::SCRIPT_DESTROY, |a| a.p[0] = script);
        }
    }

    pub fn destroy_entities_raw(&self) {
        // SAFETY: `self.0` is live.
        unsafe { ohpl_a7_destroy_entities(self.0) };
    }

    // ---- enemies

    pub fn broadcast_enemy_message(&self, b: &Broadcast) {
        let Broadcast {
            kind,
            has_pos,
            pos,
            radius,
            time,
            local,
            vx,
            fx,
            lx,
        } = *b;
        self.walk(W_ENEMIES, |en| {
            let info = host(op::ENEMY, |a| {
                a.p[0] = en;
                a.i[0] = 0;
            });
            if info.i[0] == 0 {
                return;
            }
            if has_pos
                && !sphere_hit(
                    pos,
                    radius,
                    vec3(info.f[0], info.f[1], info.f[2]),
                    info.f[3],
                )
            {
                return;
            }
            host(op::ENEMY, |a| {
                a.p[0] = en;
                a.i[0] = 1;
                a.i[1] = kind;
                a.i[2] = i32::from(local);
                a.i[3] = lx;
                a.f[0] = time;
                a.f[1..4].copy_from_slice(&[vx.x, vx.y, vx.z]);
                a.f[4] = fx;
            });
        });
    }

    pub fn broadcast_enemy_sound(&self, pos: Vector3f, volume: f32, min: f32, max: f32) {
        self.walk(W_ENEMIES, |en| {
            let info = host(op::ENEMY, |a| {
                a.p[0] = en;
                a.i[0] = 0;
            });
            if info.i[0] == 0 || info.i[1] != 0 {
                return;
            }
            let centre = vec3(info.f[0], info.f[1], info.f[2]);
            if !sphere_hit(pos, max, centre, info.f[3]) {
                return;
            }
            let Some(heard) = map::sound_volume(dist3(centre, pos), min, max, volume) else {
                return;
            };
            host(op::ENEMY, |a| {
                a.p[0] = en;
                a.i[0] = 2;
                a.f[0] = pos.x;
                a.f[1] = pos.y;
                a.f[2] = pos.z;
                a.f[3] = heard;
            });
        });
    }

    pub fn ai_node_used_as_goal(&self, node: usize) -> bool {
        let mut used = false;
        self.walk(W_ENEMIES, |en| {
            if used {
                return;
            }
            let front = host(op::ENEMY, |a| {
                a.p[0] = en;
                a.i[0] = 3;
            })
            .r;
            used = front != 0 && front == node;
        });
        used
    }

    // ---- doors, sticky areas, bodies

    pub fn door(&self, id: i32) -> (bool, i32) {
        let e = self.map().find_id(id);
        if e == 0 {
            return (false, 0);
        }
        let d = host(op::DOOR, |a| a.p[0] = e);
        if d.i[0] == 0 {
            return (false, 0);
        }
        (d.i[1] != 0, d.i[2])
    }

    fn sticky_with_body(&self, body: usize) -> Option<(usize, bool)> {
        let mut found = None;
        self.walk(W_STICKY, |s| {
            if found.is_some() {
                return;
            }
            let i = host(op::STICKY, |a| {
                a.p[0] = s;
                a.i[0] = 0;
            });
            if i.r == body {
                found = Some((s, i.i[0] != 0));
            }
        });
        found
    }

    pub fn body_in_detachable_sticky(&self, body: usize) -> bool {
        self.sticky_with_body(body).is_some_and(|(_, can)| can)
    }

    pub fn detach_body_from_sticky(&self, body: usize) -> bool {
        match self.sticky_with_body(body) {
            Some((s, true)) => {
                host(op::STICKY, |a| {
                    a.p[0] = s;
                    a.i[0] = 1;
                });
                true
            }
            _ => false,
        }
    }

    pub fn body_from_id_pair(&self, parent: i32, child: i32) -> usize {
        let e = self.map().find_id(parent);
        if e == 0 {
            return 0;
        }
        host(op::PROP_BODY, |a| {
            a.p[0] = e;
            a.i[0] = child;
        })
        .r
    }

    pub fn check_collision(&self, c1: usize, c2: usize) -> bool {
        let phys = self.pr().phys;
        let n = |c| {
            host(op::COLLIDER, |a| {
                a.p[0] = c;
                a.i[0] = 0;
            })
            .i[0]
        };
        let body = |c, i| {
            host(op::COLLIDER, |a| {
                a.p[0] = c;
                a.i[0] = 1;
                a.i[1] = i;
            })
            .r
        };
        for i1 in 0..n(c1) {
            for i2 in 0..n(c2) {
                let hit = host(op::BODY_PAIR, |a| {
                    a.p[0] = phys;
                    a.p[1] = body(c1, i1);
                    a.p[2] = body(c2, i2);
                });
                if hit.i[0] != 0 {
                    return true;
                }
            }
        }
        false
    }

    // ---- player start, checkpoints

    pub fn place_player_at_start_pos(&self, name: &[u8]) {
        let mut node = self.map().start_find(name);
        if node == 0 {
            note(
                log::WARNING,
                &[b"Could not find player start node '", name, b"'\n"].concat(),
            );
            note(
                log::ERROR,
                &[b"StartPos '", name, b"' does not exist!\n"].concat(),
            );
            node = self.map().start_first();
        }
        if node != 0 {
            host(op::PLAYER, |a| {
                a.i[0] = 0;
                a.p[0] = node;
            });
        }
    }

    pub fn set_check_point(&self, name: &[u8], start: &[u8], callback: &[u8]) {
        if self.pr().cp_name == name {
            return;
        }
        {
            let p = self.pr();
            p.cp_name = name.to_vec();
            p.cp_start = start.to_vec();
            p.cp_callback = callback.to_vec();
            p.cp_count = 0;
            p.cp_music.clear();
        }
        let max_prio = host(op::MUSIC, |a| a.i[0] = 0).i[0];
        for i in (0..=max_prio).rev() {
            let m = host(op::MUSIC, |a| {
                a.i[0] = 1;
                a.i[1] = i;
            });
            let file = m.ret_bytes(0);
            if map::checkpoint_music_pick(&file, m.i[0] != 0, m.i[1] != 0) {
                let mul = host(op::MUSIC, |a| a.i[0] = 2).f[0];
                let p = self.pr();
                p.cp_music = file;
                p.cp_prio = i;
                p.cp_resume = m.i[2] != 0;
                p.cp_volume = map::checkpoint_music_volume(m.f[0], mul);
                break;
            }
        }
        let msg = [
            b"Setting check point ".as_slice(),
            name,
            b" Music: '",
            &self.pr().cp_music.clone(),
            b"'",
        ]
        .concat();
        note(log::DEBUG, &msg);
    }

    pub fn load_checkpoint(&self) {
        let (name, start, callback, count) = {
            let p = self.pr();
            (
                p.cp_name.clone(),
                p.cp_start.clone(),
                p.cp_callback.clone(),
                p.cp_count,
            )
        };
        let count_text = string::int_to_string(count, 0);
        note(
            log::DEBUG,
            &[
                b"Loading check point ".as_slice(),
                &name,
                b" Count: ",
                &count_text,
            ]
            .concat(),
        );
        note(
            log::PROGRESS,
            &[
                b"Loading checkpoint ".as_slice(),
                &name,
                b" count: ",
                &count_text,
            ]
            .concat(),
        );

        self.place_player_at_start_pos(&start);
        self.walk(W_ENEMIES, |en| {
            host(op::ENEMY, |a| {
                a.p[0] = en;
                a.i[0] = 4;
            });
        });
        host(op::PLAYER, |a| a.i[0] = 1);
        host(op::FADE_IN, |a| a.f[0] = 0.2);

        host(op::MUSIC, |a| a.i[0] = 3);
        let (music, prio, resume, volume) = {
            let p = self.pr();
            (p.cp_music.clone(), p.cp_prio, p.cp_resume, p.cp_volume)
        };
        if !music.is_empty() {
            host(op::MUSIC, |a| {
                a.i[0] = 4;
                a.s[0] = S::of(&music);
                a.f[0] = volume;
                a.i[1] = prio;
                a.i[2] = i32::from(resume);
            });
        }

        self.run_script(&map::checkpoint_call(&callback, &name, count));
        self.pr().cp_count = count.wrapping_add(1);
    }

    // ---- completion

    pub fn calculate_total_completion(&self) {
        let quest_value = host(op::QUEST_VALUE, |_| {}).i[0];
        let p = self.pr();
        p.total = map::quest_completion(p.total, p.quests, quest_value);
        let total = string::int_to_string(p.total, 0);
        note(
            log::DEBUG,
            &[b"Total completion value: ".as_slice(), &total].concat(),
        );
    }

    pub fn add_completion_amount(&self, amount: i32, delay: f32) {
        let (prev, now) = {
            let p = self.pr();
            let total = p.total;
            map::completion_add(&mut p.current, total, amount)
        };
        host(op::COMPLETION_SHOW, |a| {
            a.f[..3].copy_from_slice(&[prev, now, delay])
        });
    }

    // ---- dissolve and lamps

    pub fn add_dissolve_entity(&self, mesh_entity: usize, time: f32) {
        let obj = host(op::DISSOLVE_NEW, |a| {
            a.p[0] = self.cmap();
            a.p[1] = mesh_entity;
            a.p[2] = self.pr().world;
        })
        .r;
        self.map().dissolve_add(obj, time);
    }

    pub fn add_lamp_light_connection(
        &self,
        lamp: usize,
        light: usize,
        amount: f32,
        use_on_color: bool,
        use_spec: bool,
    ) -> usize {
        let mut conn = self.map().lamp_find(light);
        if conn == 0 {
            conn = host(op::LAMP_NEW, |a| a.p[0] = light).r;
            self.map().lamp_add(conn, light);
        }
        host(op::LAMP_ADD, |a| {
            a.p[0] = conn;
            a.p[1] = lamp;
            a.f[0] = amount;
            a.i[0] = i32::from(use_on_color);
            a.i[1] = i32::from(use_spec);
        });
        host(op::LAMP_UPDATE, |a| {
            a.p[0] = conn;
            a.f[0] = 0.001;
        });
        conn
    }

    // ---- C++ shell state

    pub fn world(&self) -> usize {
        self.pr().world
    }

    pub fn phys(&self) -> usize {
        self.pr().phys
    }

    pub fn props<'a>(self) -> &'a mut Props {
        // SAFETY: as `pr`; the caller keeps the borrow statement-local.
        unsafe { &mut (*self.0).props }
    }
}
