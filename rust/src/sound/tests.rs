use super::channel::{Channel, EntryHost};
use super::entity_data::{self, LoadError, Names};
use super::entry::{EntryState, Step};
use super::music::{Music, MusicHost, Song};
use super::play::{PlayParams, distance_priority, setup_channel};
use crate::math::{IDENTITY, v3};
use crate::types::{Matrixf, Vector3f};
use crate::xml::Doc;
use std::cell::RefCell;
use std::ffi::c_void;
use std::rc::Rc;

fn h(bits: u32) -> f32 {
    f32::from_bits(bits)
}

#[derive(Default)]
struct Chan {
    log: Vec<String>,
    playing: bool,
    paused: bool,
    stop_used: bool,
    looping: bool,
    speed: f32,
    volume: f32,
    priority: i32,
    pos: Vector3f,
    min: f32,
    max: f32,
    block_mul: f32,
    relative: bool,
    rel: Vector3f,
    elapsed: f64,
    total: f64,
}

impl Channel for Chan {
    fn play(&mut self) {
        self.log.push("play".into());
        self.playing = true;
        self.paused = false;
    }
    fn stop(&mut self) {
        self.log.push("stop".into());
        self.playing = false;
        self.stop_used = true;
    }
    fn set_paused(&mut self, paused: bool) {
        self.log.push(format!("paused {paused}"));
        self.paused = paused;
    }
    fn paused(&self) -> bool {
        self.paused
    }
    fn is_playing(&mut self) -> bool {
        self.playing
    }
    fn stop_used(&self) -> bool {
        self.stop_used
    }
    fn speed(&self) -> f32 {
        self.speed
    }
    fn set_speed(&mut self, speed: f32) {
        self.speed = speed;
    }
    fn volume(&self) -> f32 {
        self.volume
    }
    fn set_volume(&mut self, volume: f32) {
        self.log.push(format!("volume {volume}"));
        self.volume = volume;
    }
    fn looping(&self) -> bool {
        self.looping
    }
    fn set_looping(&mut self, looping: bool) {
        self.log.push(format!("looping {looping}"));
        self.looping = looping;
    }
    fn set_3d(&mut self, is_3d: bool) {
        self.log.push(format!("3d {is_3d}"));
    }
    fn set_priority(&mut self, priority: i32) {
        self.log.push(format!("priority {priority}"));
        self.priority = priority;
    }
    fn set_priority_modifier(&mut self, modifier: i32) {
        self.log.push(format!("modifier {modifier}"));
    }
    fn set_affected_by_env(&mut self, affected: bool) {
        self.log.push(format!("env {affected}"));
    }
    fn position_is_relative(&self) -> bool {
        self.relative
    }
    fn set_position_is_relative(&mut self, relative: bool) {
        self.log.push(format!("relative {relative}"));
        self.relative = relative;
    }
    fn position(&self) -> Vector3f {
        self.pos
    }
    fn set_position(&mut self, pos: Vector3f) {
        self.log
            .push(format!("position {} {} {}", pos.x, pos.y, pos.z));
        self.pos = pos;
    }
    fn rel_position(&self) -> Vector3f {
        self.rel
    }
    fn set_rel_position(&mut self, pos: Vector3f) {
        self.log.push(format!("rel {} {} {}", pos.x, pos.y, pos.z));
        self.rel = pos;
    }
    fn min_distance(&self) -> f32 {
        self.min
    }
    fn set_min_distance(&mut self, dist: f32) {
        self.min = dist;
    }
    fn max_distance(&self) -> f32 {
        self.max
    }
    fn set_max_distance(&mut self, dist: f32) {
        self.max = dist;
    }
    fn block_volume_mul(&self) -> f32 {
        self.block_mul
    }
    fn elapsed_time(&self) -> f64 {
        self.elapsed
    }
    fn total_time(&self) -> f64 {
        self.total
    }
    fn set_elapsed_time(&mut self, time: f64) {
        self.log.push(format!("elapsed {time}"));
        self.elapsed = time;
    }
}

struct Host {
    blocked: bool,
    silent: bool,
    listener: Vector3f,
    matrix: Matrixf,
}

impl Default for Host {
    fn default() -> Self {
        Self {
            blocked: false,
            silent: false,
            listener: v3(0.0, 0.0, 0.0),
            matrix: IDENTITY,
        }
    }
}

impl EntryHost for Host {
    fn global_speed(&self, _: u32) -> f32 {
        1.0
    }
    fn global_volume(&self, _: u32) -> f32 {
        1.0
    }
    fn silent(&self) -> bool {
        self.silent
    }
    fn is_blocked(&mut self, _: Vector3f) -> bool {
        self.blocked
    }
    fn listener_position(&self) -> Vector3f {
        self.listener
    }
    fn listener_matrix(&self) -> Matrixf {
        self.matrix
    }
}

fn channel_3d(min: f32, max: f32, pos: Vector3f) -> Chan {
    Chan {
        playing: true,
        speed: 1.0,
        min,
        max,
        pos,
        block_mul: 1.0,
        ..Chan::default()
    }
}

#[test]
fn priority_boundary_matches_gcc_fusion() {
    let pos = v3(h(0x3ff52554), h(0x3f97b0b8), h(0x40dafdcd));
    let lp = v3(h(0x40b7a80d), h(0xc0aca290), h(0x40264c2a));
    let min = h(0x410b6de8);
    let max = h(0x41c7ad8c);
    assert_eq!(distance_priority(pos, lp, min, max, true, false), 100);
}

#[test]
fn priority_bands() {
    let at = |x| distance_priority(v3(x, 0.0, 0.0), v3(0.0, 0.0, 0.0), 2.0, 10.0, true, false);
    assert_eq!(
        [at(1.0), at(2.0), at(9.0), at(10.0), at(50.0)],
        [100, 10, 10, 0, 0]
    );
    let p = v3(1.0, 0.0, 0.0);
    assert_eq!(
        distance_priority(p, v3(0.0, 0.0, 0.0), 2.0, 10.0, true, true),
        0
    );
    assert_eq!(
        distance_priority(p, v3(0.0, 0.0, 0.0), 2.0, 10.0, false, false),
        0
    );
    assert_eq!(
        distance_priority(p, v3(f32::NAN, 0.0, 0.0), 2.0, 10.0, true, false),
        100
    );
}

#[test]
fn setup_orders_channel_calls() {
    let mut ch = Chan::default();
    let p = PlayParams {
        pos: v3(1.0, 2.0, 3.0),
        min_dist: 1.0,
        max_dist: 5.0,
        priority_modifier: 7,
        entry_type: 1,
        looping: true,
        relative: false,
        is_3d: true,
        silent: true,
    };
    setup_channel(&mut ch, &p, 100, &IDENTITY);
    assert_eq!(
        ch.log,
        [
            "looping true",
            "3d true",
            "priority 100",
            "modifier 7",
            "env true",
            "relative false",
            "position 1 2 3",
            "volume 0",
            "looping false",
            "stop",
        ]
    );
}

#[test]
fn entry_3d_volume_follows_distance_and_priority() {
    let mut host = Host::default();
    let mut e = EntryState::new(0.5, 2, true, false);
    e.first_time = false;
    let mut near = channel_3d(2.0, 10.0, v3(1.0, 0.0, 0.0));
    assert_eq!(e.update(0.016, &mut near, &mut host), Step::Alive);
    assert_eq!((near.volume, near.priority), (0.5, 100));

    let mut mid = channel_3d(2.0, 10.0, v3(6.0, 0.0, 0.0));
    e.update(0.016, &mut mid, &mut host);
    assert_eq!((mid.volume, mid.priority), (0.25, 10));

    let mut far = channel_3d(2.0, 10.0, v3(10.0, 0.0, 0.0));
    far.volume = 0.9;
    e.update(0.016, &mut far, &mut host);
    assert_eq!((far.volume, far.priority), (0.0, 0));
}

#[test]
fn entry_block_fades_and_first_update_starts_playing() {
    let mut host = Host {
        blocked: true,
        ..Host::default()
    };
    let mut e = EntryState::new(1.0, 2, true, false);
    let mut ch = channel_3d(1.0, 10.0, v3(0.5, 0.0, 0.0));
    ch.block_mul = 0.0;
    ch.playing = false;
    ch.paused = true;
    assert_eq!(e.update(0.016, &mut ch, &mut host), Step::Alive);
    assert_eq!((e.block_mul, ch.volume, e.first_time), (0.0, 0.0, false));
    assert!(ch.log.contains(&"play".to_string()));

    host.blocked = false;
    for _ in 0..16 {
        e.update(0.016, &mut ch, &mut host);
    }
    assert!(e.block_mul > 0.99);
}

#[test]
fn entry_silent_never_starts() {
    let mut host = Host {
        silent: true,
        ..Host::default()
    };
    let mut e = EntryState::new(1.0, 2, false, false);
    let mut ch = Chan {
        paused: true,
        ..Chan::default()
    };
    ch.speed = 1.0;
    assert_eq!(e.update(0.016, &mut ch, &mut host), Step::Alive);
    assert!(e.first_time && !ch.log.contains(&"play".to_string()));
}

#[test]
fn entry_fade_out_stops_the_channel_and_ends_the_entry() {
    let mut host = Host::default();
    let mut e = EntryState::new(1.0, 2, false, false);
    e.first_time = false;
    let mut ch = Chan {
        playing: true,
        speed: 1.0,
        ..Chan::default()
    };
    e.fade_out(2.0);
    assert_eq!(e.volume_fade_speed, -2.0);
    let mut steps = 0;
    while e.update(0.1, &mut ch, &mut host) == Step::Alive {
        steps += 1;
        assert!(steps < 20);
    }
    assert_eq!(
        steps, 4,
        "the fifth update reaches 0, stops the channel and ends the entry"
    );
    assert!(ch.stop_used && !e.stop_after_fade_out);
}

#[test]
fn entry_priority_release_only_for_looping_unstopped_channels() {
    let run = |looping: bool, stop_used: bool, fading_out: bool| {
        let mut e = EntryState::new(1.0, 2, false, false);
        e.first_time = false;
        if fading_out {
            e.volume_fade_dest = 0.0;
            e.volume_mul = 0.5;
        }
        let mut ch = Chan {
            speed: 1.0,
            looping,
            stop_used,
            ..Chan::default()
        };
        e.update(0.0, &mut ch, &mut Host::default())
    };
    assert_eq!(run(true, false, false), Step::Finished { release: true });
    assert_eq!(run(false, false, false), Step::Finished { release: false });
    assert_eq!(run(true, true, false), Step::Finished { release: false });
    assert_eq!(run(true, false, true), Step::Finished { release: false });
}

#[test]
fn entry_stop_respects_disable_and_starts_unstarted_sounds_first() {
    let mut e = EntryState::new(1.0, 2, false, false);
    let mut ch = Chan::default();
    e.stop_disabled = true;
    e.stop(&mut ch);
    assert!(ch.log.is_empty());
    e.stop_disabled = false;
    e.stop(&mut ch);
    assert_eq!(ch.log, ["play", "paused false", "stop"]);
}

#[test]
fn fade_speed_sign_follows_direction() {
    let mut e = EntryState::new(1.0, 2, false, false);
    e.volume_mul = 0.5;
    e.fade_volume_mul_to(1.0, -3.0);
    assert_eq!(e.volume_fade_speed, 3.0);
    e.fade_volume_mul_to(0.0, 3.0);
    assert_eq!(e.volume_fade_speed, -3.0);
    e.fade_in(0.8, -1.0);
    assert_eq!(
        (e.volume_mul, e.volume_fade_dest, e.volume_fade_speed),
        (0.0, 0.8, 1.0)
    );
}

fn parse(xml: &str) -> (Result<entity_data::Props, LoadError>, Names) {
    let mut doc = Doc::new();
    doc.parse(xml.as_bytes());
    let mut names = Names::default();
    let r = entity_data::parse(&doc, &mut names);
    (r, names)
}

#[test]
fn snt_defaults_and_names() {
    let (r, names) = parse(
        r#"<E><SOUNDS><Main><File File="a"/><File/></Main><Stop><File File="z"/></Stop></SOUNDS><PROPERTIES/></E>"#,
    );
    let p = r.unwrap();
    assert_eq!(names[0], [b"a".to_vec(), Vec::new()]);
    assert!(names[1].is_empty());
    assert_eq!(names[2], [b"z".to_vec()]);
    assert_eq!(
        (
            p.volume,
            p.max_distance,
            p.block_volume_mul,
            p.random,
            p.priority
        ),
        (1.0, 1.0, 0.6, 1.0, 0)
    );
    assert!(p.use_3d && p.looping && p.stream && p.fade_start && p.fade_stop && !p.blockable);
}

#[test]
fn snt_attribute_values() {
    let (r, _) = parse(
        r#"<E><SOUNDS/><PROPERTIES Volume="0.25" Use3D="FALSE" Loop="True" Priority="-3" Volume="0.75" Interval="abc"/></E>"#,
    );
    let p = r.unwrap();
    assert_eq!(
        (p.volume, p.interval, p.priority),
        (0.25, 0.0, -3),
        "the parser keeps the first duplicate"
    );
    assert!(!p.use_3d && p.looping);
}

#[test]
fn snt_errors_and_partial_names() {
    assert_eq!(parse("").0, Err(LoadError::NoRoot));
    assert_eq!(parse("<E><PROPERTIES/></E>").0, Err(LoadError::NoSounds));
    let (r, names) = parse(r#"<E><SOUNDS><Main><File File="kept"/></Main></SOUNDS></E>"#);
    assert_eq!(r, Err(LoadError::NoProperties));
    assert_eq!(names[0], [b"kept".to_vec()]);
    assert_eq!(
        parse("<e><sounds/><properties/></e>").0,
        Err(LoadError::NoSounds)
    );
}

#[test]
fn random_index_never_repeats_when_skipping() {
    assert_eq!(entity_data::random_index(0, -1, true), None);
    assert_eq!(entity_data::random_index(1, 0, true), Some((0, false)));
    for size in 3..8 {
        let mut prev = -1;
        for _ in 0..200 {
            let (idx, remember) = entity_data::random_index(size, prev, true).unwrap();
            assert!(remember && (0..size).contains(&idx));
            if prev > 0 {
                assert_ne!(idx, prev, "size {size}");
            }
            prev = idx;
        }
    }
}

#[test]
fn name_index_rejects_out_of_range() {
    let idx = |i| entity_data::name_index(3, i);
    assert_eq!(
        [idx(-1), idx(0), idx(2), idx(3)],
        [None, Some(0), Some(2), None]
    );
}

struct SongHost {
    songs: Rc<RefCell<Vec<*mut Song>>>,
    #[expect(
        clippy::vec_box,
        reason = "stream pointers must survive the Vec growing"
    )]
    chans: Vec<Box<RefCell<Chan>>>,
    fail: bool,
    deleted: usize,
    lock: Vec<String>,
}

struct Stream(*const RefCell<Chan>);

macro_rules! forward {
    ($($name:ident($($a:ident: $t:ty),*) $(-> $r:ty)?;)*) => {
        $(fn $name(&mut self $(, $a: $t)*) $(-> $r)? {
            // SAFETY: the box outlives every `Stream` in these tests.
            unsafe { (*self.0).borrow_mut().$name($($a),*) }
        })*
    };
}

impl Channel for Stream {
    forward! {
        play();
        stop();
        set_paused(p: bool);
        is_playing() -> bool;
        set_speed(s: f32);
        set_volume(v: f32);
        set_looping(l: bool);
        set_3d(b: bool);
        set_priority(p: i32);
        set_priority_modifier(p: i32);
        set_affected_by_env(b: bool);
        set_position_is_relative(b: bool);
        set_position(p: Vector3f);
        set_rel_position(p: Vector3f);
        set_min_distance(d: f32);
        set_max_distance(d: f32);
        set_elapsed_time(t: f64);
    }
    fn paused(&self) -> bool {
        // SAFETY: as `forward!`.
        unsafe { (*self.0).borrow().paused() }
    }
    fn stop_used(&self) -> bool {
        // SAFETY: as `forward!`.
        unsafe { (*self.0).borrow().stop_used() }
    }
    fn speed(&self) -> f32 {
        // SAFETY: as `forward!`.
        unsafe { (*self.0).borrow().speed() }
    }
    fn volume(&self) -> f32 {
        // SAFETY: as `forward!`.
        unsafe { (*self.0).borrow().volume() }
    }
    fn looping(&self) -> bool {
        // SAFETY: as `forward!`.
        unsafe { (*self.0).borrow().looping() }
    }
    fn position_is_relative(&self) -> bool {
        false
    }
    fn position(&self) -> Vector3f {
        v3(0.0, 0.0, 0.0)
    }
    fn rel_position(&self) -> Vector3f {
        v3(0.0, 0.0, 0.0)
    }
    fn min_distance(&self) -> f32 {
        0.0
    }
    fn max_distance(&self) -> f32 {
        0.0
    }
    fn block_volume_mul(&self) -> f32 {
        1.0
    }
    fn elapsed_time(&self) -> f64 {
        // SAFETY: as `forward!`.
        unsafe { (*self.0).borrow().elapsed }
    }
    fn total_time(&self) -> f64 {
        // SAFETY: as `forward!`.
        unsafe { (*self.0).borrow().total }
    }
}

impl SongHost {
    fn new() -> Self {
        Self {
            songs: Rc::default(),
            chans: Vec::new(),
            fail: false,
            deleted: 0,
            lock: Vec::new(),
        }
    }
}

impl MusicHost for SongHost {
    type Stream = Stream;

    fn new_song(&mut self) -> *mut Song {
        let song = Box::into_raw(Box::new(Song {
            stream: std::ptr::null_mut(),
            max_volume: 1.0,
            volume: 0.0,
            volume_add: 0.01,
            looping: false,
        }));
        self.songs.borrow_mut().push(song);
        song
    }

    fn delete_song(&mut self, song: *mut Song) {
        self.deleted += 1;
        self.songs.borrow_mut().retain(|&s| s != song);
        // SAFETY: `song` came from `new_song` and is deleted once.
        drop(unsafe { Box::from_raw(song) });
    }

    fn create_stream(&mut self, _: &[u8], _: bool) -> Option<*mut c_void> {
        if self.fail {
            return None;
        }
        self.chans.push(Box::new(RefCell::new(Chan {
            playing: true,
            total: 100.0,
            ..Chan::default()
        })));
        self.chans
            .last()
            .map(|c| std::ptr::from_ref(&**c).cast_mut().cast())
    }

    fn set_song_name(&mut self, _: *mut Song, _: &[u8]) {}

    fn set_lock(&mut self, _: *mut c_void, name: &[u8], volume: f32, looping: bool) {
        self.lock.push(format!(
            "{} {volume} {looping}",
            String::from_utf8_lossy(name)
        ));
    }

    unsafe fn stream(&self, stream: *mut c_void) -> Stream {
        Stream(stream.cast_const().cast())
    }
}

fn song_volume(m: &Music) -> f32 {
    // SAFETY: the main song is live in these tests.
    unsafe { (*m.main_song()).volume }
}

#[test]
fn music_crossfade_retires_the_old_song() {
    let mut host = SongHost::new();
    let mut m = Music::new();
    assert!(m.play(&mut host, b"a", 1.0, 0.5, true, false));
    let first = m.main_song();
    assert!(m.play(&mut host, b"b", 0.8, 0.5, true, false));
    assert_ne!(m.main_song(), first);
    assert_eq!(host.songs.borrow().len(), 2);
    for _ in 0..4 {
        m.update(&mut host, 0.25);
    }
    assert_eq!(host.songs.borrow().len(), 1, "faded-out song is gone");
    assert_eq!(host.deleted, 1);
    assert_eq!(song_volume(&m), 0.5);
    for _ in 0..4 {
        m.update(&mut host, 0.25);
    }
    assert_eq!(song_volume(&m), 0.8, "main song stops at its max volume");
}

#[test]
fn music_same_song_only_changes_volume_target() {
    let mut host = SongHost::new();
    let mut m = Music::new();
    m.play(&mut host, b"a", 1.0, 0.5, true, false);
    let song = m.main_song();
    assert!(m.play(&mut host, b"a", 1.0, 0.1, true, false));
    // SAFETY: live song.
    assert_eq!(
        unsafe { (*song).volume_add },
        0.5,
        "equal max volume returns early"
    );
    assert!(m.play(&mut host, b"a", 0.0, 0.1, true, false));
    // SAFETY: live song.
    assert_eq!(
        unsafe { ((*song).max_volume, (*song).volume_add) },
        (0.0, -0.1)
    );
    assert_eq!(host.songs.borrow().len(), 1);
}

#[test]
fn music_lock_records_instead_of_playing() {
    let mut host = SongHost::new();
    let mut m = Music::new();
    let mut token = 0u8;
    m.lock(std::ptr::from_mut(&mut token).cast());
    assert!(m.play(&mut host, b"a", 0.7, 0.5, true, false));
    assert!(m.main_song().is_null());
    assert_eq!(host.lock, ["a 0.7 true"]);
    m.lock(std::ptr::null_mut());
    assert!(m.play(&mut host, b"a", 0.7, 0.5, true, false));
    assert!(!m.main_song().is_null());
}

#[test]
fn music_load_failure_drops_the_main_song() {
    let mut host = SongHost::new();
    let mut m = Music::new();
    m.play(&mut host, b"a", 1.0, 0.5, true, false);
    host.fail = true;
    assert!(!m.play(&mut host, b"b", 1.0, 0.5, true, false));
    assert!(m.main_song().is_null());
    assert_eq!(host.songs.borrow().len(), 1, "the old song keeps fading");
}

#[test]
fn music_resume_continues_from_the_stop_position() {
    let mut host = SongHost::new();
    let mut m = Music::new();
    m.play(&mut host, b"a", 1.0, 0.5, true, true);
    {
        let c = host.chans.first().unwrap();
        let mut c = c.borrow_mut();
        c.elapsed = 30.0;
        c.volume = 0.5;
    }
    m.stop(&host, 0.25);
    m.play(&mut host, b"a", 1.0, 0.5, true, true);
    let c = host.chans.last().unwrap().borrow();
    assert_eq!(
        c.log
            .iter()
            .filter(|l| l.starts_with("elapsed"))
            .collect::<Vec<_>>(),
        ["elapsed 32"]
    );
}

#[test]
fn music_pause_holds_updates() {
    let mut host = SongHost::new();
    let mut m = Music::new();
    m.play(&mut host, b"a", 1.0, 1.0, true, false);
    m.set_paused(&host, true);
    m.update(&mut host, 0.5);
    assert_eq!(song_volume(&m), 0.0);
    m.set_paused(&host, false);
    m.update(&mut host, 0.5);
    assert_eq!(song_volume(&m), 0.5);
}

#[test]
fn music_volume_mul_fades_both_ways() {
    let mut host = SongHost::new();
    let mut m = Music::new();
    m.fade_volume_mul(0.0, 2.0);
    m.update(&mut host, 0.25);
    assert_eq!(m.volume_mul(), 0.5);
    m.update(&mut host, 1.0);
    assert_eq!(m.volume_mul(), 0.0);
    m.fade_volume_mul(1.0, 4.0);
    m.update(&mut host, 0.5);
    assert_eq!(m.volume_mul(), 1.0);
}

#[test]
fn music_destroy_deletes_every_song() {
    let mut host = SongHost::new();
    let mut m = Music::new();
    m.play(&mut host, b"a", 1.0, 0.5, true, false);
    m.play(&mut host, b"b", 1.0, 0.5, true, false);
    m.stop(&host, 1.0);
    m.destroy(&mut host);
    assert!(host.songs.borrow().is_empty());
    assert_eq!(host.deleted, 2);
}
