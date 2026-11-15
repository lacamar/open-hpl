//! The sound layer's process state: the event bank, the instance shells and the music slots.

use super::host::{self, Engine, ops};
use super::instance::{Bank, State};
use super::music::Music;
use std::cell::UnsafeCell;

pub struct Shell {
    pub shell: usize,
    pub h: usize,
    pub id: i32,
    pub name: Vec<u8>,
    pub entity: usize,
}

pub struct Game {
    pub bank: Bank,
    pub music: Music,
    pub shells: Vec<Shell>,
    pub soundscape: super::soundscape::Soundscape,
    pub dialogs: super::dialog::Dialogs,
    updater_added: bool,
    pub factory: bool,
}

struct Cell(UnsafeCell<Option<Box<Game>>>);

// SAFETY: the sound layer runs on the game thread only.
unsafe impl Sync for Cell {}

static GAME: Cell = Cell(UnsafeCell::new(None));

/// The singleton. Hold the reference only between host calls: a thunk can re-enter the layer
/// (a sound entity forwards fades to its event shell).
#[allow(clippy::mut_from_ref)]
pub fn game() -> &'static mut Game {
    // SAFETY: game thread only; callers keep the reference short as documented above.
    let slot = unsafe { &mut *GAME.0.get() };
    slot.get_or_insert_with(|| {
        Box::new(Game {
            bank: Bank::default(),
            music: Music::new(),
            shells: Vec::new(),
            soundscape: Default::default(),
            dialogs: Default::default(),
            updater_added: false,
            factory: false,
        })
    })
}

impl Game {
    pub fn shell_of(&self, shell: usize) -> Option<&Shell> {
        self.shells.iter().find(|s| s.shell == shell)
    }

    pub fn live(&self, shell: usize, id: i32) -> bool {
        self.shell_of(shell).is_some_and(|s| id < 0 || s.id == id)
    }

    pub fn state(&self, h: usize) -> Option<&State> {
        self.bank.inst(h).map(|i| &i.st)
    }
}

/// The constructor of `cSomaSoundInstance`.
pub fn inst_new(
    shell: usize,
    event: usize,
    name: &[u8],
    entity: usize,
    has_entity: bool,
    ty: i32,
) -> usize {
    let g = game();
    let h = g.bank.create(event, entity, has_entity, ty);
    let id = g.state(h).map_or(0, |s| s.id);
    g.shells.push(Shell {
        shell,
        h,
        id,
        name: name.to_vec(),
        entity,
    });
    if !std::mem::replace(&mut g.updater_added, true)
        && let Some(o) = ops()
    {
        // SAFETY: plain call.
        unsafe { (o.install_updater)() };
    }
    h
}

pub fn inst_destroy(h: usize) {
    let g = game();
    g.bank.destroy(&mut Engine, h);
    g.shells.retain(|s| s.h != h);
}

pub fn is_stopped(h: usize) -> bool {
    game()
        .bank
        .inst(h)
        .is_some_and(|i| i.st.stopped && i.voices.is_empty())
}

pub fn total_time(h: usize) -> f32 {
    game().bank.total_time(&mut Engine, h)
}

pub fn update_events(dt: f32) {
    let list: Vec<(usize, usize)> = game()
        .shells
        .iter()
        .filter(|s| s.entity == 0)
        .map(|s| (s.shell, s.h))
        .collect();
    for (shell, h) in list {
        game().bank.update(&mut Engine, h, dt);
        if is_stopped(h)
            && let Some(o) = ops()
        {
            // SAFETY: `shell` is a live shell the host created.
            unsafe { (o.shell_delete)(shell) };
        }
    }
}

pub fn fade_out_all(types: u32) -> Vec<usize> {
    let g: &Game = game();
    g.shells
        .iter()
        .filter(|s| {
            s.entity == 0
                && g.state(s.h)
                    .is_some_and(|st| st.entry_type as u32 & types != 0)
        })
        .map(|s| s.h)
        .collect()
}

pub fn find_by_name(name: &[u8]) -> usize {
    let want = crate::string::to_lower(name);
    let g: &Game = game();
    g.shells
        .iter()
        .find(|s| {
            s.entity == 0
                && g.state(s.h).is_some_and(|st| !st.stopped)
                && crate::string::to_lower(&s.name) == want
        })
        .map_or(0, |s| s.shell)
}

pub fn warn(msg: &str) {
    host::warning(msg.as_bytes());
}
