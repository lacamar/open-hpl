//! `WriteWorld` / `ReadWorld` / `WriteEntity` / `ReadEntity` of `SomaSave.cpp`: the save body,
//! field order and version gates. C++ gathers and applies; this drives both in the original order.

use super::codec::{In, Out};
use super::rec::*;
use super::script::{self, Addr, ScriptHost};
use crate::string;

pub trait WriteHost {
    fn map_script(&mut self, o: &mut Out);
    fn entity_head(&mut self, i: usize) -> EntityHead;
    fn entity_script(&mut self, i: usize, o: &mut Out);
    fn entity_tail(&mut self, i: usize) -> EntityTail;
    fn other_key(&mut self, i: usize) -> Str;
    fn other_timers(&mut self, i: usize) -> Timers;
    fn other_script(&mut self, i: usize, o: &mut Out);
}

fn write_timers(o: &mut Out, t: &Timers) {
    o.len(t.timers.get().len());
    for x in t.timers.get() {
        o.u64(x.id);
        o.f32(x.length);
        o.f32(x.time_left);
        o.str(x.func.get());
        o.bool(x.repeat);
        o.bool(x.paused);
    }
    o.len(t.faders.get().len());
    for f in t.faders.get() {
        o.bytes(f);
    }
}

fn write_entity(o: &mut Out, h: &mut dyn WriteHost, i: usize) {
    let e = h.entity_head(i);
    o.str(e.name.get());
    o.bool(e.active);
    o.bool(e.interaction_disabled);
    o.bool(e.interacted_with);
    o.bool(e.effects_active);
    o.i32(e.parent_type);
    o.bytes(&e.parent_id);
    o.str(e.parent_name.get());
    o.str(e.interact_cb.get());
    o.bool(e.interact_auto_remove);
    o.str(e.look_cb.get());
    o.bool(e.look_auto_remove);
    o.bool(e.look_center);
    o.bool(e.look_ray);
    o.f32(e.look_max);
    o.f32(e.look_delay);
    o.len(e.collide.get().len());
    for c in e.collide.get() {
        o.str(c.child.get());
        o.str(c.func.get());
    }
    o.len(e.bodies.get().len());
    for b in e.bodies.get() {
        o.f32s(&b.mtx);
        o.bool(b.active);
        o.f32(b.mass);
        o.bool(b.gravity);
    }
    if e.bodies.get().is_empty() {
        o.f32s(&e.mtx);
    }
    o.bool(e.static_physics);
    o.len(e.dyn_mass.get().len());
    o.f32s(e.dyn_mass.get());
    o.bool(e.has_attach);
    if e.has_attach {
        let a = &e.attach;
        o.str(a.parent.get());
        o.str(a.body.get());
        o.str(a.socket.get());
        o.bool(a.use_rotation);
        o.bool(a.locked);
        o.f32s(&a.parent_prev);
        o.f32s(&a.offset);
    }
    o.len(e.joints.get().len());
    for j in e.joints.get() {
        o.str(j.name.get());
        o.f32s(&j.limits);
    }
    write_timers(o, &e.timers);
    h.entity_script(i, o);
    let t = h.entity_tail(i);
    o.len(t.vars.get().len());
    for v in t.vars.get() {
        o.str(v.key.get());
        o.str(v.value.get());
    }
    o.bool(t.gui_active);
    o.str(t.on_gui.get());
    o.len(t.imgui_timers.get().len());
    for x in t.imgui_timers.get() {
        o.bytes(x);
    }
    o.len(t.imgui_states.get().len());
    for s in t.imgui_states.get() {
        o.u64(s.id);
        o.bytes(&s.state);
    }
    o.len(t.anims.get().len());
    for a in t.anims.get() {
        o.str(a.name.get());
        o.bool(a.active);
        o.bool(a.looping);
        o.bool(a.paused);
        o.f32(a.time);
        o.f32(a.weight);
        o.f32(a.speed);
        o.f32(a.fade);
    }
    o.str(t.conn_cb.get());
    o.len(t.conns.get().len());
    for c in t.conns.get() {
        o.str(c.name.get());
        o.str(c.entity.get());
        o.bool(c.invert);
        o.i32(c.states_used);
    }
}

/// `cSomaSaveState::WriteWorld`.
pub fn write_world(o: &mut Out, w: &World, h: &mut dyn WriteHost) {
    let p = &w.player;
    o.u8(p.present);
    if p.present != 0 {
        o.f32s(&p.feet);
        o.f32(p.yaw);
        o.f32(p.pitch);
        o.i32(p.state);
        o.i32(p.move_state);
        o.i32(p.char_crouching);
        o.u8(p.gravity_active as u8);
        o.f32s(&p.gravity);
    }
    o.len(w.map_timers.get().len());
    for t in w.map_timers.get() {
        o.str(t.name.get());
        o.str(t.func.get());
        o.f32(t.time);
        o.bool(t.paused);
        o.f32(t.user_float);
        o.i32(t.user_int);
        o.str(t.user_str.get());
        o.f32(t.length);
    }
    h.map_script(o);

    o.len(w.spawned.get().len());
    for s in w.spawned.get() {
        o.str(s.name.get());
        o.str(s.file.get());
        o.f32s(&s.mtx);
        o.f32s(&s.scale);
        o.bytes(&s.id);
    }
    o.len(w.entity_count);
    for i in 0..w.entity_count {
        write_entity(o, h, i);
    }
    o.len(w.other_count);
    for i in 0..w.other_count {
        let key = h.other_key(i);
        o.str(key.get());
        let t = h.other_timers(i);
        write_timers(o, &t);
        h.other_script(i, o);
    }

    o.len(w.lights.get().len());
    for l in w.lights.get() {
        o.str(l.name.get());
        o.bool(l.active);
        o.bool(l.visible);
        o.bool(l.flicker);
        o.f32s(&l.color);
        o.f32(l.radius);
        o.f32(l.brightness);
    }

    o.len(w.particles.get().len());
    for s in w.particles.get() {
        o.str(s.name.get());
        o.str(s.data.get());
        o.f32s(&s.size);
        o.f32s(&s.mtx);
        o.f32s(&s.color);
        o.f32(s.brightness);
        o.bool(s.active);
        o.bool(s.visible);
        o.bool(s.fade_at_distance);
        o.f32s(&s.fade);
        o.u32(s.dead);
        o.bool(s.owned);
    }

    o.len(w.sounds.get().len());
    for s in w.sounds.get() {
        o.str(s.name.get());
        o.str(s.data.get());
        o.bool(s.remove);
        o.bool(s.active);
        o.bool(s.stopped);
        o.f32(s.min_dist);
        o.f32(s.max_dist);
        o.f32(s.volume);
        o.f32s(&s.pos);
        o.f32(s.fade);
        o.f32(s.volume_mul);
        o.len(s.params.get().len());
        o.f32s(s.params.get());
    }

    o.len(w.billboards.get().len());
    for b in w.billboards.get() {
        o.str(b.name.get());
        o.bool(b.visible);
        o.f32s(&b.color);
    }
    o.f32(w.health);
    o.len(w.agents.get().len());
    for a in w.agents.get() {
        o.str(a.name.get());
        o.f32s(&a.mtx);
        o.i32(a.state);
        o.f32(a.yaw);
        o.bool(a.senses);
        o.bool(a.detection);
        o.str(a.path.get());
    }
    o.bool(w.underwater);
    for b in w.fog.flags {
        o.bool(b);
    }
    o.f32s(&w.fog.floats);
    for c in &w.fog.colors {
        o.f32s(c);
    }
    o.len(w.cam_textures.get().len());
    for c in w.cam_textures.get() {
        o.str(c.name.get());
        o.str(c.attached.get());
        for s in c.size {
            o.i32(s);
        }
        o.u32(c.fps);
        o.f32(c.fov);
        o.f32(c.near);
        o.f32(c.far);
        o.f32s(&c.rot);
        o.f32s(&c.pos);
    }
    o.len(w.fog_areas.get().len());
    for f in w.fog_areas.get() {
        o.str(f.name.get());
        o.bool(f.visible);
    }
    o.len(w.colliding.get().len());
    for c in w.colliding.get() {
        o.str(c.parent.get());
        o.str(c.child.get());
        o.str(c.func.get());
    }
    o.str(w.display_name.get());
    o.u8(u8::from(w.has_camera));
    if w.has_camera {
        let c = &w.camera;
        o.u8(c.detached);
        o.f32s(&c.pos);
        o.f32s(&c.limits);
        o.bytes(&c.fov_mul);
        o.bytes(&c.aspect_mul);
    }
    o.len(w.owned_lights.get().len());
    for l in w.owned_lights.get() {
        o.str(l.name.get());
        o.bool(l.visible);
        o.f32s(&l.color);
        o.f32(l.radius);
    }
    o.u8(u8::from(w.has_tone));
    if w.has_tone {
        o.f32(w.tone.exposure);
        o.f32(w.tone.white_cut);
    }
}

/// Engine side of `ReadWorld`: lookups, applies and the script objects behind the save.
pub trait ReadHost {
    fn script(&mut self) -> &mut dyn ScriptHost;
    fn has_player(&mut self) -> bool;
    fn has_camera(&mut self) -> bool;
    fn map_timers_begin(&mut self);
    fn map_timer(&mut self, t: &MapTimer);
    fn map_script(&mut self) -> Option<Addr>;
    fn spawned(&mut self, s: &Spawned);
    fn entity_begin(&mut self, name: &[u8]) -> usize;
    fn entity_head(&mut self, h: usize, e: &EntityHead);
    fn entity_script(&mut self, h: usize) -> Option<Addr>;
    fn entity_tail(&mut self, h: usize, e: &EntityHead, t: &EntityTail);
    /// Lower-cased names of the saved entities; called only when the read stayed ok.
    fn entities_done(&mut self, saved: &[Str]);
    fn other_begin(&mut self, key: &[u8]) -> usize;
    fn other_timers(&mut self, h: usize, t: &Timers);
    fn other_reset(&mut self, h: usize);
    fn other_script(&mut self, h: usize) -> Option<Addr>;
    fn light(&mut self, l: &Light);
    fn particles_begin(&mut self);
    fn particle(&mut self, p: &Particle);
    fn particles_end(&mut self, has_list: bool);
    fn sounds_begin(&mut self);
    fn sound(&mut self, s: &Sound);
    fn sounds_end(&mut self, has_list: bool);
    fn billboard(&mut self, b: &Billboard);
    fn player_apply(&mut self, p: &Player);
    fn health(&mut self, h: f32);
    fn agent(&mut self, a: &Agent);
    fn underwater(&mut self, b: bool);
    fn fog(&mut self, f: &Fog);
    fn cam_texture(&mut self, c: &CamTex);
    fn fog_area(&mut self, f: &FogArea);
    fn colliding(&mut self, c: &Colliding);
    fn display_name(&mut self, n: &[u8]);
    fn camera(&mut self, c: &Camera);
    fn owned_light(&mut self, l: &OwnedLight);
    fn tone(&mut self, t: &Tone);
}

fn read_timers<'a>(i: &mut In<'a>, timers: &mut Vec<Timer>, faders: &mut Vec<Fader>) {
    let n = i.u32();
    let mut k = 0;
    while k < n && i.ok {
        let id = i.u64();
        let length = i.f32();
        let time_left = i.f32();
        let func = Str::new(i.str());
        let repeat = i.bool();
        let paused = i.bool();
        timers.push(Timer {
            func,
            id,
            length,
            time_left,
            repeat,
            paused,
        });
        k += 1;
    }
    let n = i.u32();
    let mut k = 0;
    while k < n && i.ok {
        faders.push(i.blob());
        k += 1;
    }
}

fn read_entity(i: &mut In, version: i32, h: &mut dyn ReadHost) -> Vec<u8> {
    let name = i.str();
    let handle = h.entity_begin(name);
    let mut e = EntityHead {
        name: Str::new(name),
        parent_name: Str::EMPTY,
        interact_cb: Str::EMPTY,
        look_cb: Str::EMPTY,
        collide: Slice::EMPTY,
        bodies: Slice::EMPTY,
        dyn_mass: Slice::EMPTY,
        joints: Slice::EMPTY,
        timers: Timers {
            timers: Slice::EMPTY,
            faders: Slice::EMPTY,
        },
        attach: Attach {
            parent: Str::EMPTY,
            body: Str::EMPTY,
            socket: Str::EMPTY,
            parent_prev: [0.0; 16],
            offset: [0.0; 16],
            use_rotation: false,
            locked: false,
        },
        mtx: [0.0; 16],
        parent_id: [0; 12],
        parent_type: 0,
        look_max: 0.0,
        look_delay: 0.0,
        active: i.bool(),
        interaction_disabled: i.bool(),
        interacted_with: i.bool(),
        effects_active: i.bool(),
        interact_auto_remove: false,
        look_auto_remove: false,
        look_center: false,
        look_ray: false,
        static_physics: false,
        has_attach: false,
        has_mtx: false,
    };
    e.parent_type = i.i32();
    e.parent_id = i.blob();
    e.parent_name = Str::new(i.str());
    e.interact_cb = Str::new(i.str());
    e.interact_auto_remove = i.bool();
    e.look_cb = Str::new(i.str());
    e.look_auto_remove = i.bool();
    if version >= 8 {
        e.look_center = i.bool();
        e.look_ray = i.bool();
        e.look_max = i.f32();
        e.look_delay = i.f32();
    }
    let mut collide = Vec::new();
    let n = i.u32();
    let mut k = 0;
    while k < n && i.ok {
        let child = Str::new(i.str());
        let func = Str::new(i.str());
        collide.push(Collide { child, func });
        k += 1;
    }
    e.collide = Slice::new(&collide);
    let mut bodies = Vec::new();
    let n = i.u32();
    let mut k = 0;
    while k < n && i.ok {
        let mtx = i.f32s();
        let active = if version >= 5 { i.bool() } else { true };
        let mass = if version >= 9 { i.f32() } else { -1.0 };
        let gravity = if version >= 9 { i.bool() } else { true };
        bodies.push(Body {
            mtx,
            mass,
            active,
            gravity,
        });
        k += 1;
    }
    e.bodies = Slice::new(&bodies);
    let mut dyn_mass = Vec::new();
    if version >= 6 {
        if n == 0 {
            e.has_mtx = true;
            e.mtx = i.f32s();
        }
        if version >= 17 {
            e.static_physics = i.bool();
            let m = i.u32();
            let mut k = 0;
            while k < m && i.ok {
                dyn_mass.push(i.f32());
                k += 1;
            }
        }
        e.has_attach = i.bool();
        if e.has_attach {
            e.attach.parent = Str::new(i.str());
            e.attach.body = Str::new(i.str());
            e.attach.socket = Str::new(i.str());
            e.attach.use_rotation = i.bool();
            e.attach.locked = i.bool();
            e.attach.parent_prev = i.f32s();
            e.attach.offset = i.f32s();
        }
    }
    e.dyn_mass = Slice::new(&dyn_mass);
    let mut joints = Vec::new();
    let n = if version >= 7 { i.u32() } else { 0 };
    let mut k = 0;
    while k < n && i.ok {
        let jn = if version >= 11 { i.str() } else { &[] };
        let limits = i.f32s();
        joints.push(Joint {
            name: Str::new(jn),
            limits,
        });
        k += 1;
    }
    e.joints = Slice::new(&joints);
    let (mut timers, mut faders) = (Vec::new(), Vec::new());
    read_timers(i, &mut timers, &mut faders);
    e.timers = Timers {
        timers: Slice::new(&timers),
        faders: Slice::new(&faders),
    };
    h.entity_head(handle, &e);

    let obj = h.entity_script(handle);
    script::read_script(h.script(), i, version, obj);

    let mut t = EntityTail {
        vars: Slice::EMPTY,
        on_gui: Str::EMPTY,
        imgui_timers: Slice::EMPTY,
        imgui_states: Slice::EMPTY,
        anims: Slice::EMPTY,
        conn_cb: Str::EMPTY,
        conns: Slice::EMPTY,
        gui_active: false,
    };
    let mut vars = Vec::new();
    if version >= 12 {
        let n = i.u32();
        let mut k = 0;
        while k < n && i.ok {
            let key = Str::new(i.str());
            let value = Str::new(i.str());
            vars.push(VarPair { key, value });
            k += 1;
        }
    }
    t.vars = Slice::new(&vars);
    if version >= 13 {
        t.gui_active = i.bool();
    }
    let mut imt = Vec::new();
    if version >= 23 {
        t.on_gui = Str::new(i.str());
        let n = i.u32();
        let mut k = 0;
        while k < n && i.ok {
            imt.push(i.blob());
            k += 1;
        }
    }
    t.imgui_timers = Slice::new(&imt);
    let mut ims = Vec::new();
    let n = if version >= 14 { i.u32() } else { 0 };
    let mut k = 0;
    while k < n && i.ok {
        let id = i.u64();
        let state = i.blob();
        ims.push(ImStatePair { id, state });
        k += 1;
    }
    t.imgui_states = Slice::new(&ims);
    let mut anims = Vec::new();
    let n = if version >= 18 { i.u32() } else { 0 };
    let mut k = 0;
    while k < n && i.ok {
        let name = Str::new(i.str());
        let active = i.bool();
        let looping = i.bool();
        let paused = i.bool();
        let time = i.f32();
        let weight = i.f32();
        let speed = i.f32();
        let fade = i.f32();
        anims.push(Anim {
            name,
            time,
            weight,
            speed,
            fade,
            active,
            looping,
            paused,
        });
        k += 1;
    }
    t.anims = Slice::new(&anims);
    let mut conns = Vec::new();
    if version >= 20 {
        t.conn_cb = Str::new(i.str());
        let n = i.u32();
        let mut k = 0;
        while k < n && i.ok {
            let name = Str::new(i.str());
            let entity = Str::new(i.str());
            let invert = i.bool();
            let states_used = i.i32();
            conns.push(Conn {
                name,
                entity,
                states_used,
                invert,
            });
            k += 1;
        }
    }
    t.conns = Slice::new(&conns);
    h.entity_tail(handle, &e, &t);
    string::to_lower(name)
}

/// `cSomaSaveState::ReadWorld`.
pub fn read_world(i: &mut In, version: i32, h: &mut dyn ReadHost) {
    let mut pl = Player {
        present: i.u8(),
        char_crouching: 0,
        feet: [0.0; 3],
        yaw: 0.0,
        pitch: 0.0,
        state: -1,
        move_state: -1,
        gravity_active: -1,
        gravity: [0.0; 3],
    };
    if pl.present != 0 {
        pl.feet = i.f32s();
        pl.yaw = i.f32();
        pl.pitch = i.f32();
        pl.state = i.i32();
        pl.move_state = i.i32();
        if version >= 15 {
            pl.char_crouching = i.i32();
        }
        if version >= 28 {
            pl.gravity_active = i32::from(i.u8());
            pl.gravity = i.f32s();
        }
    }

    h.map_timers_begin();
    let n = i.u32();
    let mut k = 0;
    while k < n && i.ok {
        let name = Str::new(i.str());
        let func = Str::new(i.str());
        let time = i.f32();
        let paused = i.bool();
        let user_float = i.f32();
        let user_int = i.i32();
        let user_str = Str::new(i.str());
        let length = i.f32();
        h.map_timer(&MapTimer {
            name,
            func,
            user_str,
            time,
            user_float,
            length,
            user_int,
            paused,
        });
        k += 1;
    }
    let obj = h.map_script();
    script::read_script(h.script(), i, version, obj);

    let n = if version >= 4 { i.u32() } else { 0 };
    let mut k = 0;
    while k < n && i.ok {
        let name = Str::new(i.str());
        let file = Str::new(i.str());
        let mtx = i.f32s();
        let scale = i.f32s();
        let id = i.blob();
        h.spawned(&Spawned {
            name,
            file,
            mtx,
            scale,
            id,
        });
        k += 1;
    }

    let n = i.u32();
    let mut saved: Vec<Vec<u8>> = Vec::new();
    let mut k = 0;
    while k < n && i.ok {
        saved.push(read_entity(i, version, h));
        k += 1;
    }
    if i.ok {
        let names: Vec<Str> = saved.iter().map(|s| Str::new(s)).collect();
        h.entities_done(&names);
    }

    let n = i.u32();
    let mut k = 0;
    while k < n && i.ok {
        let key = i.str();
        let handle = h.other_begin(key);
        let (mut timers, mut faders) = (Vec::new(), Vec::new());
        read_timers(i, &mut timers, &mut faders);
        h.other_timers(
            handle,
            &Timers {
                timers: Slice::new(&timers),
                faders: Slice::new(&faders),
            },
        );
        h.other_reset(handle);
        let obj = h.other_script(handle);
        script::read_script(h.script(), i, version, obj);
        k += 1;
    }

    let n = i.u32();
    let mut k = 0;
    while k < n && i.ok {
        let name = Str::new(i.str());
        let active = i.bool();
        let visible = i.bool();
        let flicker = i.bool();
        let color = i.f32s();
        let radius = i.f32();
        let brightness = i.f32();
        if i.ok {
            h.light(&Light {
                name,
                color,
                radius,
                brightness,
                active,
                visible,
                flicker,
            });
        }
        k += 1;
    }

    h.particles_begin();
    let n = i.u32();
    let has = i.ok;
    let mut k = 0;
    while k < n && i.ok {
        let name = Str::new(i.str());
        let data = Str::new(i.str());
        let size = i.f32s();
        let mtx = i.f32s();
        let color = i.f32s();
        let brightness = i.f32();
        let active = i.bool();
        let visible = i.bool();
        let fade_at_distance = i.bool();
        let fade = i.f32_each();
        let dead = i.u32();
        let owned = version >= 22 && i.bool();
        if !i.ok {
            break;
        }
        h.particle(&Particle {
            name,
            data,
            size,
            mtx,
            color,
            brightness,
            fade,
            dead,
            active,
            visible,
            fade_at_distance,
            owned,
        });
        k += 1;
    }
    h.particles_end(has);

    h.sounds_begin();
    let n = i.u32();
    let has = i.ok;
    let mut k = 0;
    while k < n && i.ok {
        let name = Str::new(i.str());
        let data = Str::new(i.str());
        let remove = i.bool();
        let active = i.bool();
        let stopped = i.bool();
        let min_dist = i.f32();
        let max_dist = i.f32();
        let volume = i.f32();
        let pos = i.f32s();
        let fade = i.f32();
        let volume_mul = i.f32();
        let pn = i.u32().min(256);
        let mut params = Vec::new();
        for _ in 0..pn {
            params.push(i.f32());
        }
        if !i.ok {
            break;
        }
        h.sound(&Sound {
            name,
            data,
            params: Slice::new(&params),
            pos,
            min_dist,
            max_dist,
            volume,
            fade,
            volume_mul,
            remove,
            active,
            stopped,
        });
        k += 1;
    }
    h.sounds_end(has);

    let n = i.u32();
    let mut k = 0;
    while k < n && i.ok {
        let name = Str::new(i.str());
        let visible = i.bool();
        let color = i.f32s();
        if i.ok {
            h.billboard(&Billboard {
                name,
                color,
                visible,
            });
        }
        k += 1;
    }

    if pl.present != 0 {
        h.player_apply(&pl);
    }
    if h.has_player() && i.left() {
        let hp = i.f32();
        h.health(hp);
    }
    let n = if i.left() { i.u32() } else { 0 };
    let mut k = 0;
    while k < n && i.ok {
        let name = Str::new(i.str());
        let mtx = i.f32s();
        let state = i.i32();
        let yaw = if version >= 10 { i.f32() } else { 0.0 };
        let senses = if version >= 10 { i.bool() } else { true };
        let detection = if version >= 10 { i.bool() } else { true };
        let path = Str::new(if version >= 24 { i.str() } else { &[] });
        h.agent(&Agent {
            name,
            path,
            mtx,
            state,
            yaw,
            senses,
            detection,
        });
        k += 1;
    }
    if i.left() {
        let b = i.bool();
        h.underwater(b);
    }
    if i.left() {
        let mut f = Fog {
            colors: [[0.0; 4]; 3],
            floats: [0.0; 9],
            flags: [false; 5],
        };
        for b in &mut f.flags {
            *b = i.bool();
        }
        for x in &mut f.floats {
            *x = i.f32();
        }
        for c in &mut f.colors {
            *c = i.f32s();
        }
        if i.ok {
            h.fog(&f);
        }
    }
    let n = if i.left() { i.u32() } else { 0 };
    let mut k = 0;
    while k < n && i.ok {
        let name = Str::new(i.str());
        let attached = Str::new(i.str());
        let size = i.i32s();
        let fps = i.u32();
        let fov = i.f32();
        let near = i.f32();
        let far = i.f32();
        let rot = i.f32s();
        let pos = i.f32s();
        if i.ok {
            h.cam_texture(&CamTex {
                name,
                attached,
                rot,
                pos,
                size,
                fps,
                fov,
                near,
                far,
            });
        }
        k += 1;
    }
    let n = if i.left() { i.u32() } else { 0 };
    let mut k = 0;
    while k < n && i.ok {
        let name = Str::new(i.str());
        let visible = i.bool();
        if i.ok {
            h.fog_area(&FogArea { name, visible });
        }
        k += 1;
    }
    let n = if i.left() { i.u32() } else { 0 };
    let mut k = 0;
    while k < n && i.ok {
        let parent = Str::new(i.str());
        let child = Str::new(i.str());
        let func = Str::new(i.str());
        h.colliding(&Colliding {
            parent,
            child,
            func,
        });
        k += 1;
    }
    if version >= 25 {
        let n = i.str();
        h.display_name(n);
    }
    let has_cam = h.has_camera();
    if version >= 26 && i.u8() != 0 && has_cam {
        let detached = i.u8();
        let pos = i.f32s();
        let limits = i.f32_each();
        let fov_mul = i.blob();
        let aspect_mul = i.blob();
        h.camera(&Camera {
            pos,
            limits,
            fov_mul,
            aspect_mul,
            detached,
        });
    }
    if version >= 29 {
        let n = i.u32();
        let mut k = 0;
        while k < n && i.ok {
            let name = Str::new(i.str());
            let visible = i.bool();
            let color = i.f32s();
            let radius = i.f32();
            if i.ok {
                h.owned_light(&OwnedLight {
                    name,
                    color,
                    radius,
                    visible,
                });
            }
            k += 1;
        }
    }
    if version >= 30 && i.u8() != 0 {
        let exposure = i.f32();
        let white_cut = i.f32();
        if i.ok {
            h.tone(&Tone {
                exposure,
                white_cut,
            });
        }
    }
}
