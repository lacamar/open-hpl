//! `cLuxMapHandler`: the map list, the current map, map changes, pause bookkeeping and the enemy-hearable
//! sound callback. Viewport, post effects, save collection and the other modules are reached through [`mhost`].

use super::a7_ffi::{Ctx, ohpl_a7_destroy_entities, ohpl_a7_list_add, ohpl_a7_list_destroy};
use super::mhost::{self, A, S};
use super::mmap::{M, log};
use super::{handler, map};
use crate::types::Vector3f;
use std::ffi::c_void;

pub mod hop {
    pub const MAP_NEW: i32 = 57;
    pub const MAP_NAME: i32 = 58;
    pub const MAP_CTX: i32 = 59;
    pub const SET_CURRENT: i32 = 60;
    pub const MODULE_MSG: i32 = 61;
    pub const VIEW: i32 = 62;
    pub const PLAY_GUI_SOUND: i32 = 63;
    pub const FADE: i32 = 64;
    pub const PLAYER_ACTIVE: i32 = 65;
    pub const SAVE_PENDING: i32 = 66;
    pub const CLEANUP_DATA: i32 = 67;
    pub const LOCK: i32 = 68;
    pub const LOADSCREEN: i32 = 69;
    pub const SAVED: i32 = 70;
    pub const WORLD_SOUNDS_FADE: i32 = 71;
    pub const ACHIEVEMENT: i32 = 72;
    pub const PLAYER_SANITY: i32 = 73;
    pub const TIME: i32 = 74;
    pub const SOUND_ALL: i32 = 75;
    pub const WORLD_ACTIVE: i32 = 76;
    pub const VIEWPORT_ACTIVE: i32 = 77;
    pub const STOP_COMMENTARY: i32 = 78;
    pub const FOLDER: i32 = 79;
    pub const SAVED_RESET: i32 = 80;
    pub const DATACACHE_DESTROY: i32 = 81;
    pub const QUIT: i32 = 82;
    pub const CLEAR_RENDER_LISTS: i32 = 83;
    pub const MAP_LOAD: i32 = 84;
}

pub mod module {
    pub const DESTROY_WORLD_ENTITIES: i32 = 0;
    pub const ON_MAP_LEAVE: i32 = 1;
    pub const CREATE_WORLD_ENTITIES: i32 = 2;
    pub const ON_MAP_ENTER: i32 = 3;
}

#[derive(Default)]
struct Change {
    active: bool,
    file: Vec<u8>,
    start_pos: Vec<u8>,
    sound: Vec<u8>,
}

#[derive(Default)]
pub struct Handler {
    maps: Box<Ctx>,
    cpp: usize,
    current: usize,
    paused: bool,
    update_active: bool,
    show_commentary: bool,
    change: Change,
    hearable: Vec<Vec<u8>>,
}

fn host(o: i32, f: impl FnOnce(&mut A)) -> A {
    let mut a = A::new();
    f(&mut a);
    mhost::call(o, &mut a);
    a
}

#[derive(Clone, Copy)]
pub struct H(*mut Handler);

impl H {
    /// # Safety
    /// `h` is a live handler from [`Handler::create`].
    pub unsafe fn new(h: *mut Handler) -> H {
        H(h)
    }

    /// A handler op: the shell address rides in `p[5]` (the engine's own pointer is not set yet while the shell is constructed).
    fn h(self, o: i32, f: impl FnOnce(&mut A)) -> A {
        let cpp = self.st().cpp;
        host(o, |a| {
            f(a);
            a.p[5] = cpp;
        })
    }

    fn st<'a>(self) -> &'a mut Handler {
        // SAFETY: `new` contract; every borrow is statement-local.
        unsafe { &mut *self.0 }
    }

    pub fn maps_ctx(self) -> *mut Ctx {
        std::ptr::from_mut(&mut *self.st().maps)
    }

    fn map_of(&self, cmap: usize) -> Option<M> {
        if cmap == 0 {
            return None;
        }
        let ctx = self.h(hop::MAP_CTX, |a| a.p[0] = cmap).r as *mut Ctx;
        // SAFETY: the host returns the live context of that map.
        Some(unsafe { M::new(ctx) })
    }

    fn current_map(self) -> Option<M> {
        self.map_of(self.st().current)
    }

    fn name_of(&self, cmap: usize) -> Vec<u8> {
        self.h(hop::MAP_NAME, |a| a.p[0] = cmap).ret_bytes(0)
    }

    pub fn current(self) -> usize {
        self.st().current
    }

    pub fn init(self, cpp: usize) {
        let s = self.st();
        s.cpp = cpp;
        s.update_active = true;
    }

    pub fn add_hearable(self, name: &[u8]) {
        self.st().hearable.push(name.to_vec());
    }

    // ---- frame

    pub fn update(self, step: f32) {
        self.h(hop::CLEAR_RENDER_LISTS, |_| {});
        self.check_map_change();
        if let (false, Some(m)) = (self.st().change.active, self.current_map()) {
            m.update(step);
        }
    }

    pub fn render_solid(self, functions: usize) {
        if let Some(m) = self.current_map() {
            m.render_solid(functions);
        }
    }

    pub fn reset(self) {
        self.h(hop::SOUND_ALL, |a| a.i[0] = 2);
        // SAFETY: the list context is live for the handler's lifetime.
        unsafe { ohpl_a7_destroy_entities(self.maps_ctx()) };
        self.assign(0);
        self.resume_sounds();
        self.h(hop::FOLDER, |a| a.i[0] = 1);
        {
            let s = self.st();
            s.update_active = true;
            s.change.active = false;
        }
        self.h(hop::SAVED_RESET, |_| {});
        self.h(hop::CLEANUP_DATA, |_| {});
        self.h(hop::DATACACHE_DESTROY, |_| {});
    }

    pub fn teardown(self) {
        // SAFETY: as in `reset`.
        unsafe { ohpl_a7_destroy_entities(self.maps_ctx()) };
    }

    pub fn on_quit(self) {
        self.h(hop::QUIT, |a| a.i[0] = 0);
        let cur = self.st().current;
        if cur != 0 {
            self.h(hop::QUIT, |a| a.i[0] = 1);
            self.destroy_map(cur, false);
            self.h(hop::QUIT, |a| a.i[0] = 2);
        }
        self.h(hop::QUIT, |a| a.i[0] = 3);
    }

    pub fn set_update_active(self, active: bool) {
        self.st().update_active = active;
        if let Some(m) = self.current_map() {
            self.h(hop::WORLD_ACTIVE, |a| {
                a.p[0] = m.world();
                a.i[0] = i32::from(active);
            });
        }
    }

    pub fn on_enter_container(self) {
        self.h(hop::VIEWPORT_ACTIVE, |a| a.i[0] = 1);
        if let Some(m) = self.current_map() {
            self.h(hop::WORLD_ACTIVE, |a| {
                a.p[0] = m.world();
                a.i[0] = 1;
            });
        }
        self.resume_sounds();
    }

    pub fn on_leave_container(self) {
        self.h(hop::VIEWPORT_ACTIVE, |a| a.i[0] = 0);
        if let Some(m) = self.current_map() {
            self.h(hop::WORLD_ACTIVE, |a| {
                a.p[0] = m.world();
                a.i[0] = 0;
            });
        }
    }

    pub fn show_commentary(self) -> bool {
        self.st().show_commentary
    }

    pub fn set_show_commentary(self, show: bool, stop: bool) {
        self.st().show_commentary = show;
        if stop && handler::commentary_stops(show) {
            self.h(hop::STOP_COMMENTARY, |_| {});
        }
    }

    pub fn set_show_raw(self, show: bool) {
        self.st().show_commentary = show;
    }

    // ---- pause

    pub fn pause_sounds(self) {
        if handler::pause_sounds(&mut self.st().paused) {
            self.h(hop::SOUND_ALL, |a| a.i[0] = 0);
        }
    }

    pub fn resume_sounds(self) {
        if handler::resume_sounds(&mut self.st().paused) {
            self.h(hop::SOUND_ALL, |a| a.i[0] = 1);
        }
    }

    // ---- maps

    pub fn load_map(self, file: &[u8], load_entities: bool) -> usize {
        let name = handler::map_name(file);
        let folder = self.h(hop::FOLDER, |a| a.i[0] = 0).ret_bytes(0);
        let path = [folder.as_slice(), file].concat();
        let cmap = self.h(hop::MAP_NEW, |a| a.s[0] = S::of(&name)).r;
        self.h(hop::MAP_LOAD, |a| {
            a.p[0] = cmap;
            a.s[0] = S::of(&path);
            a.i[0] = i32::from(load_entities);
        });
        // SAFETY: the list context is live; `cmap` is a live map.
        unsafe { ohpl_a7_list_add(self.maps_ctx(), cmap as *mut c_void) };
        cmap
    }

    pub fn destroy_map(self, cmap: usize, loading_save: bool) {
        if self.st().current == cmap {
            self.set_current_map(0, loading_save, false, b"");
        }
        // SAFETY: as in `load_map`; the host drop thunk deletes the map.
        unsafe { ohpl_a7_list_destroy(self.maps_ctx(), cmap as *mut c_void) };
    }

    fn assign(self, cmap: usize) {
        self.st().current = cmap;
        self.h(hop::SET_CURRENT, |a| a.p[0] = cmap);
    }

    pub fn set_current_map(
        self,
        new: usize,
        run_script: bool,
        first_time: bool,
        player_pos: &[u8],
    ) {
        let old = self.st().current;
        let plan = handler::set_current_plan(old != 0, new != 0, old == new);
        for op in plan {
            let cur = self.current_map();
            match op {
                handler::SET_LEAVE => {
                    if let Some(m) = cur {
                        m.on_leave(run_script);
                    }
                }
                handler::SET_DESTROY_WORLD_ENTITIES => self.module(module::DESTROY_WORLD_ENTITIES),
                handler::SET_MAP_LEAVE => self.module(module::ON_MAP_LEAVE),
                handler::SET_ASSIGN => self.assign(new),
                handler::SET_CREATE_WORLD_ENTITIES => self.module(module::CREATE_WORLD_ENTITIES),
                handler::SET_MAP_ENTER => self.module(module::ON_MAP_ENTER),
                handler::SET_PLACE_PLAYER => {
                    if let Some(m) = cur {
                        m.place_player_at_start_pos(player_pos);
                    }
                }
                handler::SET_AUTO_CHECKPOINT => {
                    if let Some(m) = cur {
                        m.set_check_point(b"_auto", player_pos, b"");
                    }
                }
                handler::SET_ENTER => {
                    if let Some(m) = cur {
                        m.on_enter(run_script, first_time);
                    }
                }
                handler::SET_VIEW_WORLD => {
                    if let Some(m) = cur {
                        self.h(hop::VIEW, |a| {
                            a.i[0] = 0;
                            a.p[0] = m.world();
                            a.p[1] = m.phys();
                        });
                    }
                }
                handler::SET_VIEW_NO_WORLD => {
                    self.h(hop::VIEW, |a| a.i[0] = 1);
                }
                _ => {}
            }
        }
    }

    fn module(self, kind: i32) {
        let cur = self.st().current;
        self.h(hop::MODULE_MSG, |a| {
            a.i[0] = kind;
            a.p[0] = cur;
        });
    }

    // ---- map change

    pub fn change_map(
        self,
        map_name: &[u8],
        start_pos: &[u8],
        start_sound: &[u8],
        end_sound: &[u8],
    ) {
        {
            let c = &mut self.st().change;
            c.active = true;
            c.file = handler::change_file(map_name);
            c.start_pos = start_pos.to_vec();
            c.sound = end_sound.to_vec();
        }
        self.h(hop::PLAY_GUI_SOUND, |a| a.s[0] = S::of(start_sound));
        self.h(hop::FADE, |a| {
            a.i[0] = 0;
            a.f[0] = 1.5;
        });
        self.h(hop::PLAYER_ACTIVE, |a| a.i[0] = 0);
    }

    fn check_map_change(self) {
        let active = self.st().change.active;
        let fading = active && self.h(hop::FADE, |a| a.i[0] = 2).i[0] != 0;
        if !handler::change_proceeds(active, fading) {
            return;
        }
        let mut time_taken = 0.0f32;

        self.st().change.active = false;
        self.h(hop::FADE, |a| {
            a.i[0] = 1;
            a.f[0] = 2.0;
        });
        self.h(hop::PLAYER_ACTIVE, |a| a.i[0] = 1);
        self.h(hop::SAVE_PENDING, |_| {});
        self.h(hop::CLEANUP_DATA, |_| {});

        let (file, start_pos, sound) = {
            let c = &self.st().change;
            (c.file.clone(), c.start_pos.clone(), c.sound.clone())
        };
        let new_name = handler::map_name(&file);
        let cur = self.st().current;
        let cur_name = self.name_of(cur);
        if handler::is_new_map(&cur_name, &file) {
            self.h(hop::LOCK, |a| a.i[0] = 1);
            if let Some(m) = self.current_map() {
                m.run_script(b"OnLeave()");
            }
            let start = self.h(hop::TIME, |_| {}).r as u64;
            self.h(hop::LOADSCREEN, |a| a.i[0] = 0);
            self.h(hop::SAVED, |a| {
                a.i[0] = 0;
                a.p[0] = cur;
            });
            self.h(hop::WORLD_SOUNDS_FADE, |_| {});
            let last = self.st().current;
            let new = self.load_map(&file, true);
            if new == 0 {
                note_error(&file);
                return;
            }
            if last != 0 {
                let hard = host(super::mhost::op::HARD_MODE, |_| {}).i[0] != 0;
                let events = handler::pair_events(&self.name_of(last), &self.name_of(new), hard);
                if events & handler::EVENT_ESCAPE_ARTIST != 0 {
                    self.h(hop::ACHIEVEMENT, |a| a.i[0] = 0);
                }
                if events & handler::EVENT_DESCENDANT != 0 {
                    self.h(hop::ACHIEVEMENT, |a| a.i[0] = 1);
                }
                if events & handler::EVENT_SANITY != 0 {
                    self.h(hop::PLAYER_SANITY, |a| a.f[0] = 100.0);
                }
            }
            let exists = self
                .h(hop::SAVED, |a| {
                    a.i[0] = 2;
                    a.s[0] = S::of(&new_name);
                })
                .i[0]
                != 0;
            let first_time = handler::first_time(exists);
            self.set_current_map(new, false, first_time, &start_pos);
            self.destroy_map(last, false);
            self.h(hop::SAVED, |a| {
                a.i[0] = 1;
                a.p[0] = new;
            });
            if let Some(m) = self.current_map() {
                if first_time {
                    m.run_script(b"OnStart()");
                }
                m.run_script(b"OnEnter()");
            }
            self.h(hop::LOCK, |a| a.i[0] = 0);

            let now = self.h(hop::TIME, |_| {}).r as u64;
            time_taken = handler::load_seconds(start, now);
            let name = self.name_of(self.st().current);
            let msg = [b"Entering map ".as_slice(), &name].concat();
            host(super::mhost::op::LOG, |a| {
                a.i[0] = log::PROGRESS;
                a.s[0] = S::of(&msg);
            });
        } else if let Some(m) = self.current_map() {
            m.place_player_at_start_pos(&start_pos);
        }

        if time_taken > 0.0 {
            self.h(hop::LOADSCREEN, |a| {
                a.i[0] = 1;
                a.s[0] = S::of(&sound);
                a.f[0] = time_taken;
            });
        } else {
            self.h(hop::PLAY_GUI_SOUND, |a| a.s[0] = S::of(&sound));
        }
    }

    // ---- enemy-hearable sounds

    /// A sound entity started: tell the enemies in range when its type is one they hear.
    pub fn sound_started(self, type_name: &[u8], max_dist: f32, volume: f32, pos: Vector3f) {
        let Some(m) = self.current_map() else {
            return;
        };
        let used = self
            .st()
            .hearable
            .iter()
            .any(|n| map::sound_prefix_matches(type_name, n));
        if used {
            m.broadcast_enemy_sound(pos, volume, max_dist, max_dist);
        }
    }
}

fn note_error(file: &[u8]) {
    let msg = [b"Could not load map '".as_slice(), file, b"'!\n"].concat();
    host(super::mhost::op::LOG, |a| {
        a.i[0] = log::ERROR;
        a.s[0] = S::of(&msg);
    });
}

impl Handler {
    pub fn create() -> *mut Handler {
        Box::into_raw(Box::default())
    }
}
