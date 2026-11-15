//! `cLuxInsanityHandler` and its four event kinds: the owner of the event list, the disabled sets
//! and the timers. XML, GUI, sound, particles and the player stay C++ behind [`Host`].

use super::host12::{Call, Host, code, log};
use super::insanity::{
    self, Bug, BugsParams, Steps, Stream, Timing, bugs_start, bugs_update, steps_start,
    steps_update, stream_a, stream_b, stream_load, timer_expired,
};
use super::voice;
use std::collections::BTreeSet;

#[derive(Clone, Copy, Debug, Default)]
pub struct Entry {
    pub ptr: usize,
    pub id: i32,
}

impl Entry {
    fn valid(&self, h: &dyn Host) -> bool {
        self.ptr != 0
            && Call::new(h, code::S_IS_VALID)
                .p(0, self.ptr)
                .i(0, self.id)
                .run()
                .flag()
    }

    fn start(h: &dyn Host, file: &[u8], looped: bool, volume: f32) -> Entry {
        let r = Call::new(h, code::S_STREAM)
            .s(0, file)
            .i(0, i32::from(looped))
            .f(0, volume)
            .run();
        Entry {
            ptr: r.ret,
            id: r.out_i[0],
        }
    }

    fn fade_in(&self, h: &dyn Host, a: f32, b: f32) {
        Call::new(h, code::S_FADE_IN)
            .p(0, self.ptr)
            .f(0, a)
            .f(1, b)
            .run();
    }

    fn fade_out(&self, h: &dyn Host, a: f32) {
        Call::new(h, code::S_FADE_OUT).p(0, self.ptr).f(0, a).run();
    }

    fn stop(&self, h: &dyn Host) {
        Call::new(h, code::S_STOP).p(0, self.ptr).run();
    }
}

struct Xml<'a> {
    h: &'a dyn Host,
    idx: i32,
}

impl Xml<'_> {
    fn string(&self, key: &str, def: &str) -> Vec<u8> {
        Call::new(self.h, code::X_STR)
            .i(0, self.idx)
            .s(0, key.as_bytes())
            .s(1, def.as_bytes())
            .run()
            .string()
    }

    fn f32(&self, key: &str, def: f32) -> f32 {
        Call::new(self.h, code::X_F32)
            .i(0, self.idx)
            .s(0, key.as_bytes())
            .f(0, def)
            .run()
            .out[0]
    }

    fn i32(&self, key: &str, def: i32) -> i32 {
        Call::new(self.h, code::X_I32)
            .i(0, self.idx)
            .s(0, key.as_bytes())
            .i(1, def)
            .run()
            .out_i[0]
    }

    fn bool(&self, key: &str, def: bool) -> bool {
        Call::new(self.h, code::X_BOOL)
            .i(0, self.idx)
            .s(0, key.as_bytes())
            .i(1, i32::from(def))
            .run()
            .flag()
    }

    fn color(&self, key: &str) -> [f32; 4] {
        let r = Call::new(self.h, code::X_COLOR)
            .i(0, self.idx)
            .s(0, key.as_bytes())
            .run();
        [r.out[0], r.out[1], r.out[2], r.out[3]]
    }
}

#[derive(Debug, Default)]
struct Bugs {
    num: i32,
    loop_sound: Vec<u8>,
    sound_volume: f32,
    duration: f32,
    params: BugsParams,
    image: usize,
    bugs: Vec<Bug>,
    entry: Entry,
    time: f32,
}

#[derive(Debug, Default)]
struct Particles {
    sound_file: Vec<u8>,
    loop_sound: bool,
    sound_volume: f32,
    fade_in: f32,
    fade_out: f32,
    system: Vec<u8>,
    duration: f32,
    time: f32,
    entry: Entry,
    ps: usize,
}

#[derive(Debug, Default)]
struct SoundStream {
    file: Vec<u8>,
    volume: f32,
    delay: f32,
    fade_screen: bool,
    white: usize,
    color: [f32; 4],
    fade_in_speed: f32,
    fade_out_speed: f32,
    disable_player: bool,
    fade_image_file: Vec<u8>,
    sub_cat: Vec<u8>,
    sub_entry: Vec<u8>,
    rows: Vec<Vec<u8>>,
    sound_count: f32,
    fade_alpha: f32,
    fade_alpha_speed: f32,
    entry: Entry,
    fade_image: usize,
}

#[derive(Debug, Default)]
struct StepsEv {
    sound: Vec<u8>,
    step_time: f32,
    step_num: i32,
    distance: f32,
    s: Steps,
}

#[derive(Debug)]
enum Kind {
    Bugs(Bugs),
    Particles(Particles),
    Stream(SoundStream),
    Steps(StepsEv),
}

#[derive(Debug)]
struct Event {
    name: Vec<u8>,
    set: Vec<u8>,
    max_sanity: f32,
    used: bool,
    over: bool,
    kind: Kind,
}

fn has_map(h: &dyn Host) -> bool {
    Call::new(h, code::M_HAS_MAP).run().flag()
}

fn ps_exists(h: &dyn Host, ps: usize) -> bool {
    ps != 0 && Call::new(h, code::P_EXISTS).p(0, ps).run().flag()
}

impl Event {
    fn load(h: &dyn Host, kind: Kind, idx: i32) -> Event {
        let x = Xml { h, idx };
        let mut e = Event {
            name: x.string("Name", "Unknown"),
            set: x.string("Set", ""),
            max_sanity: x.f32("MaxSanity", 100.0),
            used: false,
            over: true,
            kind,
        };
        match &mut e.kind {
            Kind::Bugs(b) => {
                let image = x.string("BugImage", "");
                if !image.is_empty() {
                    b.image = Call::new(h, code::G_IMAGE).s(0, &image).run().ret;
                }
                b.num = x.i32("NumOfBugs", 10);
                b.params = BugsParams {
                    min_size_mul: x.f32("MinSizeMul", 1.0),
                    max_size_mul: x.f32("MaxSizeMul", 1.0),
                    wander_dist: x.f32("WanderCircleDist", 1.0),
                    wander_radius: x.f32("WanderCircleRadius", 1.0),
                    swarm_mul: x.f32("SwarmPointMul", 1.0),
                    max_speed: x.f32("MaxSpeed", 1.0),
                };
                b.loop_sound = x.string("LoopSound", "");
                b.sound_volume = x.f32("SoundVolume", 1.0);
                b.duration = x.f32("Duration", 5.0);
            }
            Kind::Particles(p) => {
                p.sound_file = x.string("SoundFile", "");
                p.loop_sound = x.bool("LoopSound", false);
                p.sound_volume = x.f32("SoundVolume", 0.0);
                p.fade_in = x.f32("SoundFadeInTime", 1.0);
                p.fade_out = x.f32("SoundFadeOutTime", 1.0);
                p.system = x.string("ParticleSystem", "");
                p.duration = x.f32("Duration", 1.0);
            }
            Kind::Stream(s) => {
                s.file = x.string("File", "");
                s.volume = x.f32("Volume", 1.0);
                let delay = x.f32("SoundDelayTime", 0.0);
                s.fade_screen = x.bool("FadeScreen", false);
                s.color = x.color("FadeColor");
                let fade_in = x.f32("FadeInTime", 0.0);
                let fade_out = x.f32("FadeOutTime", 0.0);
                (s.delay, s.fade_in_speed, s.fade_out_speed) =
                    stream_load(delay, fade_in, fade_out);
                s.sub_cat = x.string("SubtitleCat", "");
                s.sub_entry = x.string("SubtitleEntry", "");
                s.fade_image_file = x.string("FadeImageFile", "");
                s.disable_player = x.bool("DisablePlayer", false);
            }
            Kind::Steps(s) => {
                s.sound = x.string("Sound", "");
                s.step_time = x.f32("StepTime", 1.0);
                s.step_num = x.i32("StepNum", 0);
                s.distance = x.f32("Distance", 0.0);
                s.s.time_mul = x.f32("TimeMulPerStep", 0.0);
                s.s.dist_mul = x.f32("DistanceMulPerStep", 0.0);
            }
        }
        e
    }

    fn start(&mut self, h: &dyn Host) {
        self.used = true;
        self.over = false;
        Call::new(h, code::LOG_START)
            .s(0, &self.name)
            .s(1, &self.set)
            .run();
        match &mut self.kind {
            Kind::Bugs(b) => {
                let r = Call::new(h, code::G_BOUNDS).p(0, b.image).run();
                b.bugs = vec![Bug::default(); usize::try_from(b.num).unwrap_or(0)];
                if !b.bugs.is_empty() {
                    bugs_start(
                        &mut b.bugs,
                        &b.params,
                        [r.out[0], r.out[1]],
                        [r.out[2], r.out[3]],
                    );
                }
                if !b.loop_sound.is_empty() {
                    b.entry = Entry::start(h, &b.loop_sound, true, b.sound_volume);
                    if b.entry.ptr != 0 {
                        b.entry.fade_in(h, 1.0, 1.0 / 2.0);
                    }
                }
                b.time = b.duration;
            }
            Kind::Particles(p) => {
                if !p.sound_file.is_empty() {
                    p.entry = Entry::start(h, &p.sound_file, p.loop_sound, p.sound_volume);
                    if p.entry.ptr != 0 && p.fade_in > 0.0 {
                        p.entry.fade_in(h, 1.0, 1.0 / p.fade_in);
                    }
                }
                if !p.system.is_empty() {
                    let mut name = self.name.clone();
                    name.extend_from_slice(b"_PS");
                    p.ps = Call::new(h, code::P_CREATE)
                        .s(0, &name)
                        .s(1, &p.system)
                        .run()
                        .ret;
                    if p.ps != 0 {
                        Call::new(h, code::P_SET_CAMERA).p(0, p.ps).run();
                    }
                }
                p.time = p.duration;
            }
            Kind::Stream(s) => {
                s.fade_image = 0;
                s.entry = Entry::default();
                s.sound_count = s.delay;
                s.fade_alpha = 0.0;
                s.fade_alpha_speed = s.fade_in_speed;
                if s.disable_player {
                    Call::new(h, code::PL_ACTIVE).i(0, 0).run();
                }
                if !s.sub_cat.is_empty() && !s.sub_entry.is_empty() {
                    s.rows = Call::new(h, code::SUB_ROWS)
                        .s(0, &s.sub_cat)
                        .s(1, &s.sub_entry)
                        .run()
                        .sunk
                        .rows;
                    log(
                        h,
                        &format!(
                            "i wrap {} {} rows {}",
                            String::from_utf8_lossy(&s.sub_cat),
                            String::from_utf8_lossy(&s.sub_entry),
                            s.rows.len()
                        ),
                    );
                }
                if !s.fade_image_file.is_empty() {
                    s.fade_image = Call::new(h, code::G_TEXTURE)
                        .s(0, &s.fade_image_file)
                        .run()
                        .ret;
                }
            }
            Kind::Steps(s) => {
                steps_start(&mut s.s, s.step_time, s.step_num, s.distance);
            }
        }
    }

    fn on_exit(&mut self, h: &dyn Host) {
        match &mut self.kind {
            Kind::Bugs(b) => {
                b.bugs.clear();
                let valid = b.entry.valid(h);
                log(h, &format!("i bugs exit valid {}", i32::from(valid)));
                if valid {
                    b.entry.fade_out(h, 1.0 / 2.0);
                    b.entry = Entry::default();
                }
            }
            Kind::Particles(p) => {
                if has_map(h) {
                    let exists = ps_exists(h, p.ps);
                    log(h, &format!("i ps exit exists {}", i32::from(exists)));
                    if exists {
                        Call::new(h, code::P_KILL).p(0, p.ps).run();
                        p.ps = 0;
                    }
                }
                let valid = p.entry.valid(h);
                log(h, &format!("i particles exit valid {}", i32::from(valid)));
                if valid {
                    if p.fade_out > 0.0 {
                        p.entry.fade_out(h, 1.0 / p.fade_out);
                    } else {
                        p.entry.stop(h);
                    }
                    p.entry = Entry::default();
                }
            }
            Kind::Stream(s) => {
                if s.fade_image != 0 {
                    Call::new(h, code::G_DESTROY).p(0, s.fade_image).run();
                }
                if s.disable_player {
                    Call::new(h, code::PL_ACTIVE).i(0, 1).run();
                }
                s.rows.clear();
            }
            Kind::Steps(_) => {}
        }
    }

    fn update(&mut self, h: &dyn Host, dt: f32) {
        let done = match &mut self.kind {
            Kind::Bugs(b) => bugs_update(
                &mut b.bugs,
                usize::try_from(b.num).unwrap_or(0),
                &b.params,
                dt,
                &mut b.time,
            ),
            Kind::Particles(p) => {
                let exists = ps_exists(h, p.ps);
                log(h, &format!("i ps exists {}", i32::from(exists)));
                if exists {
                    Call::new(h, code::P_SET_CAMERA).p(0, p.ps).run();
                }
                timer_expired(&mut p.time, dt)
            }
            Kind::Stream(s) => {
                let mut st = Stream {
                    sound_count: s.sound_count,
                    fade_alpha: s.fade_alpha,
                    fade_alpha_speed: s.fade_alpha_speed,
                    fade_out_speed: s.fade_out_speed,
                    fade_screen: u32::from(s.fade_screen),
                };
                if stream_a(&mut st, s.entry.ptr == 0, dt) {
                    s.entry = Entry::start(h, &s.file, false, s.volume);
                }
                let gone = s.entry.ptr == 0 || !s.entry.valid(h);
                log(h, &format!("i gone {}", i32::from(gone)));
                let done = stream_b(&mut st, gone, dt);
                s.sound_count = st.sound_count;
                s.fade_alpha = st.fade_alpha;
                s.fade_alpha_speed = st.fade_alpha_speed;
                done
            }
            Kind::Steps(s) => {
                let old = s.s.pos;
                let (create, done) = steps_update(&mut s.s, dt);
                if create {
                    Call::new(h, code::E_STEP_SOUND)
                        .s(0, &s.sound)
                        .f(0, old[0])
                        .f(1, old[1])
                        .f(2, old[2])
                        .run();
                }
                done
            }
        };
        if done {
            self.over = true;
        }
    }

    fn on_draw(&mut self, h: &dyn Host) {
        match &self.kind {
            Kind::Bugs(b) => {
                for (i, bug) in b
                    .bugs
                    .iter()
                    .enumerate()
                    .take(usize::try_from(b.num).unwrap_or(0))
                {
                    if b.image == 0 {
                        break;
                    }
                    Call::new(h, code::D_BUG)
                        .p(0, b.image)
                        .i(0, i as i32)
                        .f(0, bug.pos[0])
                        .f(1, bug.pos[1])
                        .f(2, bug.size_mul)
                        .f(3, bug.alpha)
                        .f(4, bug.angle)
                        .run();
                }
            }
            Kind::Stream(s) => {
                let subs = Call::new(h, code::SUBS_STATE).run().ret;
                let (show, voice_active) = (subs & 1 != 0, subs & 2 != 0);
                log(
                    h,
                    &format!("i subs {} {}", i32::from(show), i32::from(voice_active)),
                );
                if show && !s.rows.is_empty() && !voice_active {
                    let rows = voice::stream_rows(s.rows.len());
                    for row in &rows {
                        if let Some(text) = s.rows.get(row.row as usize) {
                            Call::new(h, code::D_FONT)
                                .s(0, text)
                                .i(0, row.row as i32)
                                .f(0, row.x)
                                .f(1, row.y)
                                .f(2, row.z)
                                .run();
                        }
                    }
                }
                if s.fade_screen {
                    Call::new(h, code::D_FADE)
                        .p(0, s.fade_image)
                        .p(1, s.white)
                        .f(0, s.color[0])
                        .f(1, s.color[1])
                        .f(2, s.color[2])
                        .f(3, s.color[3])
                        .f(4, s.fade_alpha)
                        .run();
                }
            }
            Kind::Particles(_) | Kind::Steps(_) => {}
        }
    }
}

#[derive(Debug, Default)]
pub struct InsanityHandler {
    timing: Timing,
    events: Vec<Event>,
    new_event_count: f32,
    current: i32,
    disabled: BTreeSet<Vec<u8>>,
}

fn event_file(h: &dyn Host) -> Vec<u8> {
    Call::new(h, code::CFG_STR)
        .s(0, b"Insanity")
        .s(1, b"EventsFile")
        .s(2, b"")
        .run()
        .string()
}

fn cfg_f32(h: &dyn Host, key: &str) -> f32 {
    Call::new(h, code::CFG_F32)
        .s(0, b"Insanity")
        .s(1, key.as_bytes())
        .f(0, 0.0)
        .run()
        .out[0]
}

impl InsanityHandler {
    pub fn new(h: &dyn Host) -> Self {
        let timing = Timing {
            max_sanity_long: cfg_f32(h, "MaxSanity_LongWait"),
            max_sanity_med: cfg_f32(h, "MaxSanity_MedWait"),
            max_sanity_short: cfg_f32(h, "MaxSanity_ShortWait"),
            time_long: cfg_f32(h, "TimeBetween_LongWait"),
            time_med: cfg_f32(h, "TimeBetween_MedWait"),
            time_short: cfg_f32(h, "TimeBetween_ShortWait"),
        };
        let mut me = InsanityHandler {
            timing,
            current: -1,
            ..InsanityHandler::default()
        };
        me.load_events(h, &event_file(h));
        log(h, &format!("i load events {}", me.events.len()));
        me
    }

    fn load_events(&mut self, h: &dyn Host, file: &[u8]) {
        if file.is_empty() {
            Call::new(h, code::WARN)
                .s(0, b"No sanity events file specified!")
                .run();
            return;
        }
        let n = Call::new(h, code::X_OPEN).s(0, file).run().ret as i64;
        if n < 0 {
            let mut m = b"Could not load sanity events file: '".to_vec();
            m.extend_from_slice(file);
            m.extend_from_slice(b"'\n");
            Call::new(h, code::ERR).s(0, &m).run();
            return;
        }
        for idx in 0..n as i32 {
            let ty = Call::new(h, code::X_TYPE).i(0, idx).run().string();
            let kind = match ty.to_ascii_lowercase().as_slice() {
                b"soundstream" => Kind::Stream(SoundStream {
                    white: Call::new(h, code::G_RECT_WHITE).run().ret,
                    ..SoundStream::default()
                }),
                b"steps" => Kind::Steps(StepsEv::default()),
                b"particles" => Kind::Particles(Particles::default()),
                b"bugs" => Kind::Bugs(Bugs::default()),
                _ => {
                    let mut m = b"Insanity event type '".to_vec();
                    m.extend_from_slice(&ty);
                    m.extend_from_slice(b"' does not exist!\n");
                    Call::new(h, code::ERR).s(0, &m).run();
                    continue;
                }
            };
            self.events.push(Event::load(h, kind, idx));
        }
        Call::new(h, code::X_CLOSE).run();
    }

    fn exit_current(&mut self, h: &dyn Host) {
        if let Some(e) = usize::try_from(self.current)
            .ok()
            .and_then(|i| self.events.get_mut(i))
        {
            e.on_exit(h);
        }
    }

    pub fn reset(&mut self, h: &dyn Host) {
        self.exit_current(h);
        self.current = -1;
        self.new_event_count = 0.0;
    }

    pub fn update(&mut self, h: &dyn Host, dt: f32) {
        self.new_event_count = insanity::count_down(self.new_event_count, self.current, dt);
        let Some(idx) = usize::try_from(self.current).ok() else {
            return;
        };
        let Some(e) = self.events.get_mut(idx) else {
            return;
        };
        e.update(h, dt);
        if e.over {
            e.on_exit(h);
            self.current = -1;
            let sanity = sanity(h);
            self.new_event_count = insanity::wait_after(&self.timing, sanity);
        }
    }

    pub fn on_draw(&mut self, h: &dyn Host) {
        if let Some(e) = usize::try_from(self.current)
            .ok()
            .and_then(|i| self.events.get_mut(i))
        {
            e.on_draw(h);
        }
    }

    pub fn start_random(&mut self, h: &dyn Host) {
        if self.events.is_empty() {
            return;
        }
        let sanity = sanity(h);
        self.exit_current(h);
        self.current = -1;
        let set_disabled: Vec<u32> = self
            .events
            .iter()
            .map(|e| u32::from(self.disabled.contains(&e.set)))
            .collect();
        let mut used: Vec<u32> = self.events.iter().map(|e| u32::from(e.used)).collect();
        let max: Vec<f32> = self.events.iter().map(|e| e.max_sanity).collect();
        let pick = insanity::pick(sanity, &set_disabled, &mut used, &max);
        if pick.reset_used != 0 {
            for e in &mut self.events {
                e.used = false;
            }
        }
        if pick.index >= 0 {
            self.current = pick.index;
        }
        let Some(e) = usize::try_from(self.current)
            .ok()
            .and_then(|i| self.events.get_mut(i))
        else {
            return;
        };
        e.start(h);
        let mut msg = b"Starting insanity event ".to_vec();
        msg.extend_from_slice(&e.name);
        Call::new(h, code::LOG_PROG).s(0, &msg).run();
    }

    pub fn start_index(&mut self, h: &dyn Host, idx: i32) {
        let Some(i) = usize::try_from(idx).ok().filter(|i| *i < self.events.len()) else {
            return;
        };
        self.exit_current(h);
        self.current = idx;
        if let Some(e) = self.events.get_mut(i) {
            e.start(h);
        }
    }

    pub fn start_named(&mut self, h: &dyn Host, name: &[u8]) {
        if let Some(i) = self.events.iter().position(|e| e.name == name) {
            self.start_index(h, i as i32);
        }
    }

    pub fn stop_current(&mut self, h: &dyn Host) {
        self.exit_current(h);
        self.current = -1;
    }

    pub fn possible(&self, h: &dyn Host) -> bool {
        insanity::new_event_possible(&self.timing, self.new_event_count, sanity(h), self.current)
    }

    pub fn reload(&mut self, h: &dyn Host) {
        self.reset(h);
        self.events.clear();
        self.load_events(h, &event_file(h));
    }

    pub fn count(&self) -> i32 {
        self.events.len() as i32
    }

    pub fn current(&self) -> i32 {
        self.current
    }

    pub fn new_event_count(&self) -> f32 {
        self.new_event_count
    }

    pub fn set_new_event_count(&mut self, v: f32) {
        self.new_event_count = v;
    }

    pub fn event_name(&self, i: i32) -> Vec<u8> {
        self.event(i).map(|e| e.name.clone()).unwrap_or_default()
    }

    pub fn event_set(&self, i: i32) -> Vec<u8> {
        self.event(i).map(|e| e.set.clone()).unwrap_or_default()
    }

    pub fn event_max_sanity(&self, i: i32) -> f32 {
        self.event(i).map_or(0.0, |e| e.max_sanity)
    }

    pub fn event_used(&self, i: i32) -> bool {
        self.event(i).is_some_and(|e| e.used)
    }

    pub fn event_over(&self, i: i32) -> bool {
        self.event(i).is_some_and(|e| e.over)
    }

    pub fn set_event_used(&mut self, i: i32, v: bool) {
        if let Some(e) = usize::try_from(i).ok().and_then(|i| self.events.get_mut(i)) {
            e.used = v;
        }
    }

    fn event(&self, i: i32) -> Option<&Event> {
        usize::try_from(i).ok().and_then(|i| self.events.get(i))
    }

    pub fn enable_set(&mut self, set: &[u8]) {
        self.disabled.remove(set);
    }

    pub fn disable_set(&mut self, set: &[u8]) {
        self.disabled.insert(set.to_vec());
    }

    pub fn clear_sets(&mut self) {
        self.disabled.clear();
    }

    pub fn sets(&self) -> Vec<Vec<u8>> {
        self.disabled.iter().cloned().collect()
    }
}

fn sanity(h: &dyn Host) -> f32 {
    Call::new(h, code::PL_SANITY).run().out[0]
}
