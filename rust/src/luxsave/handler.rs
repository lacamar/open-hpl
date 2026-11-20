//! `LuxSaveHandler.cpp`: save/load flow, autosave rotation and the load sequence. Engine calls,
//! file I/O and the serializable save structs stay C++ behind [`Host`].

use super::names::{self, Date, NameState};

#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum HOp {
    Log = 0,
    HardMode = 1,
    MapDisplayName = 2,
    CopyResult = 3,
    NowDate = 4,
    ProfilePath = 5,
    ScanSaves = 6,
    ScanName = 7,
    ScanDate = 8,
    RemoveScanned = 9,
    CreateData = 10,
    ThreadRunning = 11,
    ThreadSave = 12,
    SaveNow = 13,
    Snapshot = 14,
    NewData = 15,
    LoadFile = 16,
    FreeData = 17,
    ThreadProcess = 18,
    ResetProgressLog = 19,
    Step = 20,
}

pub const LOG_BEGIN_SAVE: i64 = 0;
pub const LOG_END_SAVE: i64 = 1;
pub const LOG_BEGIN_LOAD: i64 = 2;
pub const LOG_END_LOAD: i64 = 3;
pub const LOG_LOADING: i64 = 4;
pub const LOG_NO_SAVE_FILE: i64 = 5;
pub const LOG_NO_HARDMODE_FILE: i64 = 6;

#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Step {
    DrawMenuScreen = 0,
    DestroyDataCache = 1,
    HasCurrentMap = 2,
    DestroyWorldEntities = 3,
    ResetModules = 4,
    StopSounds = 5,
    CacheCreate = 6,
    SetMapFolder = 7,
    LoadScripts = 8,
    NeedsNewMap = 9,
    LoadNewMap = 10,
    ClearWorldDynamics = 11,
    DestroyEntities = 12,
    KillDyingParticles = 13,
    CreateWorldEntities = 14,
    ApplySave = 15,
    SavedMaps = 16,
    HardMode = 17,
    Cleanup = 18,
    CacheDestroy = 19,
}

pub trait Host {
    /// `a`: op argument; `buf`: string in, or output for `CopyResult`, `NowDate` and `ScanDate`.
    fn call(&mut self, op: HOp, a: i64, buf: &mut [u32]) -> i64;
}

trait HostExt: Host {
    fn op(&mut self, op: HOp, a: i64) -> i64 {
        self.call(op, a, &mut [])
    }

    fn step(&mut self, s: Step) -> bool {
        self.op(HOp::Step, i64::from(s as u32)) != 0
    }

    fn str_of(&mut self, op: HOp, a: i64) -> Vec<u32> {
        let n = usize::try_from(self.op(op, a)).unwrap_or(0);
        let mut v = vec![0u32; n];
        self.call(HOp::CopyResult, 0, &mut v);
        v
    }

    fn log(&mut self, kind: i64, s: &[u32]) {
        self.call(HOp::Log, kind, &mut s.to_vec());
    }

    fn date(&mut self, op: HOp, a: i64) -> Date {
        let mut b = [0u32; 6];
        self.call(op, a, &mut b);
        let [seconds, minutes, hours, month_day, month, year] = b.map(|x| x as i32);
        Date {
            seconds,
            minutes,
            hours,
            month_day,
            month,
            year,
        }
    }

    fn dates(&mut self, mode: i64) -> Vec<Date> {
        let n = self.op(HOp::ScanSaves, mode);
        (0..n).map(|i| self.date(HOp::ScanDate, i)).collect()
    }

    fn profile_file(&mut self, name: &[u32]) -> Vec<u32> {
        let mut f = self.str_of(HOp::ProfilePath, 0);
        f.extend_from_slice(name);
        f
    }
}

impl<H: Host + ?Sized> HostExt for H {}

const SCAN_CREATED: i64 = 0;
const SCAN_MODIFIED: i64 = 1;

pub struct Handler {
    max_auto: i32,
    name: NameState,
}

fn w(s: &str) -> Vec<u32> {
    s.bytes().map(u32::from).collect()
}

impl Handler {
    pub fn new(max_auto: i32) -> Handler {
        Handler {
            max_auto,
            name: NameState::default(),
        }
    }

    pub fn save_to_file(&mut self, h: &mut dyn Host, file: &[u32], snapshot: bool) {
        h.log(LOG_BEGIN_SAVE, file);
        let data = h.op(HOp::CreateData, 0);
        if h.op(HOp::ThreadRunning, 0) != 0 {
            h.call(HOp::ThreadSave, data, &mut file.to_vec());
        } else {
            h.call(HOp::SaveNow, data, &mut file.to_vec());
        }
        if snapshot {
            h.call(HOp::Snapshot, 0, &mut file.to_vec());
        }
        h.log(LOG_END_SAVE, &[]);
    }

    pub fn load_from_file(&mut self, h: &mut dyn Host, file: &[u32]) {
        h.log(LOG_BEGIN_LOAD, file);
        let data = h.op(HOp::NewData, 0);
        h.call(HOp::LoadFile, data, &mut file.to_vec());
        self.load_save_game_data(h);
        h.op(HOp::FreeData, data);
        h.log(LOG_END_LOAD, &[]);
    }

    fn save_name(&mut self, h: &mut dyn Host, prefix: &str) -> Vec<u32> {
        let map = h.str_of(HOp::MapDisplayName, 0);
        let now = h.date(HOp::NowDate, 0);
        names::save_name(&mut self.name, &w(prefix), &map, &now)
    }

    fn delete_oldest(&mut self, h: &mut dyn Host) {
        let dates = h.dates(SCAN_CREATED);
        for v in names::rotation_victims(&dates, self.max_auto) {
            h.op(HOp::RemoveScanned, v as i64);
        }
    }

    fn newest_file(h: &mut dyn Host) -> Vec<u32> {
        let dates = h.dates(SCAN_MODIFIED);
        names::newest(&dates).map_or_else(Vec::new, |i| h.str_of(HOp::ScanName, i as i64))
    }

    pub fn auto_save(&mut self, h: &mut dyn Host) -> bool {
        if h.op(HOp::HardMode, 0) != 0 {
            return true;
        }
        self.delete_oldest(h);
        let name = self.save_name(h, "AutoSave");
        let file = h.profile_file(&name);
        self.save_to_file(h, &file, false);
        true
    }

    pub fn hardmode_save(&mut self, h: &mut dyn Host) -> bool {
        let name = self.save_name(h, "HardMode");
        let file = h.profile_file(&name);
        self.save_to_file(h, &file, false);
        true
    }

    pub fn auto_load(&mut self, h: &mut dyn Host, reset_logger: bool) -> bool {
        if h.op(HOp::ThreadRunning, 0) != 0 {
            h.op(HOp::ThreadProcess, 0);
        }
        let file = Self::newest_file(h);
        let dir = h.str_of(HOp::ProfilePath, 0);
        if file.is_empty() {
            h.log(LOG_NO_SAVE_FILE, &dir);
            return false;
        }
        if h.op(HOp::HardMode, 0) != 0 && !names::hardmode_file_ok(&file) {
            h.log(LOG_NO_HARDMODE_FILE, &dir);
            return false;
        }
        if reset_logger {
            h.op(HOp::ResetProgressLog, 0);
        }
        let path = h.profile_file(&file);
        self.load_from_file(h, &path);
        true
    }

    pub fn save_file_exists(&mut self, h: &mut dyn Host) -> bool {
        !h.str_of(HOp::ProfilePath, 0).is_empty() && !Self::newest_file(h).is_empty()
    }

    fn load_save_game_data(&mut self, h: &mut dyn Host) {
        use Step::*;
        h.log(LOG_LOADING, &[]);
        h.step(DrawMenuScreen);
        h.step(DestroyDataCache);
        if h.step(HasCurrentMap) {
            h.step(DestroyWorldEntities);
            h.step(ResetModules);
            h.step(StopSounds);
        }
        h.step(CacheCreate);
        h.step(SetMapFolder);
        h.step(LoadScripts);
        if h.step(NeedsNewMap) {
            h.step(LoadNewMap);
            h.step(ClearWorldDynamics);
        } else {
            h.step(ClearWorldDynamics);
            h.step(DestroyEntities);
            h.step(KillDyingParticles);
            h.step(CreateWorldEntities);
        }
        h.step(ApplySave);
        h.step(SavedMaps);
        h.step(HardMode);
        h.step(Cleanup);
        h.step(CacheDestroy);
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[derive(Default)]
    struct Fake {
        log: Vec<(HOp, i64)>,
        files: Vec<(&'static str, Date, Date)>,
        hard: bool,
        result: Vec<u32>,
        scanned: Vec<&'static str>,
    }

    impl Host for Fake {
        fn call(&mut self, op: HOp, a: i64, buf: &mut [u32]) -> i64 {
            self.log.push((op, a));
            match op {
                HOp::HardMode => i64::from(self.hard),
                HOp::ProfilePath => {
                    self.result = w("/p/");
                    3
                }
                HOp::MapDisplayName => {
                    self.result = w("m");
                    1
                }
                HOp::CopyResult => {
                    buf.copy_from_slice(&self.result);
                    0
                }
                HOp::ScanSaves => {
                    self.scanned = self.files.iter().map(|f| f.0).collect();
                    self.files.len() as i64
                }
                HOp::ScanDate => {
                    let f = &self.files[a as usize];
                    let d = if self.log.iter().filter(|x| x.0 == HOp::ScanSaves).count() % 2 == 1 {
                        f.1
                    } else {
                        f.2
                    };
                    buf.copy_from_slice(&[
                        d.seconds as u32,
                        d.minutes as u32,
                        d.hours as u32,
                        d.month_day as u32,
                        d.month as u32,
                        d.year as u32,
                    ]);
                    0
                }
                HOp::ScanName => {
                    self.result = w(self.scanned[a as usize]);
                    self.result.len() as i64
                }
                HOp::Step => i64::from(a == Step::HasCurrentMap as i64),
                _ => 0,
            }
        }
    }

    fn d(s: i32) -> Date {
        Date {
            seconds: s,
            ..Date::default()
        }
    }

    #[test]
    fn autosave_rotates_then_saves() {
        let mut f = Fake {
            files: vec![("a.sav", d(5), d(0)), ("b.sav", d(1), d(0))],
            ..Fake::default()
        };
        assert!(Handler::new(2).auto_save(&mut f));
        let ops: Vec<HOp> = f.log.iter().map(|x| x.0).collect();
        let rm = ops.iter().position(|o| *o == HOp::RemoveScanned).unwrap();
        let save = ops.iter().position(|o| *o == HOp::SaveNow).unwrap();
        assert!(rm < save);
        assert_eq!(f.log[rm].1, 1);
    }

    #[test]
    fn hardmode_autosave_is_a_no_op() {
        let mut f = Fake {
            hard: true,
            ..Fake::default()
        };
        assert!(Handler::new(2).auto_save(&mut f));
        assert_eq!(f.log.len(), 1);
    }

    #[test]
    fn autoload_without_file_fails() {
        let mut f = Fake::default();
        assert!(!Handler::new(2).auto_load(&mut f, true));
        assert!(!f.log.iter().any(|x| x.0 == HOp::NewData));
    }

    #[test]
    fn op_numbers_match_header() {
        let h = include_str!("../../../amnesia/src/game/RustLuxSave.h");
        let rs = include_str!("handler.rs");
        let pick = |s: &str, pre: &str| -> Vec<(String, u32)> {
            s.lines()
                .filter_map(|l| {
                    let l = l.trim().strip_prefix(pre)?.strip_suffix(',')?;
                    let (n, v) = l.split_once(" = ")?;
                    Some((n.to_string(), v.parse().ok()?))
                })
                .collect()
        };
        let (ops, steps) = rs.split_once("pub const LOG_BEGIN_SAVE").unwrap();
        let steps = steps.split_once("pub enum Step").unwrap().1;
        let steps = steps.split_once("pub trait Host").unwrap().0;
        let in_rs = pick(ops, "");
        assert_eq!(in_rs, pick(h, "eOhplGa6H_"));
        assert_eq!(pick(steps, ""), pick(h, "eOhplGa6HStep_"));
    }
}
