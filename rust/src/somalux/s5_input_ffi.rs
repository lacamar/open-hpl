//! S5: C ABI of the input handler (`soma/src/game/RustSomaLuxS5.h`) and its script natives.

use super::s5_input::{self as input, Action, InHost, Preset};
use crate::somascript::s2_as::{Engine, Frame, Soma, str_bytes};
use std::ffi::c_void;
use std::sync::OnceLock;

#[repr(C)]
pub struct Io {
    pub(super) p: [*mut c_void; 2],
    pub(super) i: [i32; 4],
    pub(super) f: [f32; 4],
    pub(super) s: *const u8,
    pub(super) n: usize,
}

impl Io {
    pub(super) fn new() -> Self {
        Self {
            p: [std::ptr::null_mut(); 2],
            i: [0; 4],
            f: [0.0; 4],
            s: std::ptr::null(),
            n: 0,
        }
    }

    pub(super) fn text(&self) -> Vec<u8> {
        if self.s.is_null() {
            return Vec::new();
        }
        // SAFETY: the host points `s` at `n` bytes valid until its next call.
        unsafe { std::slice::from_raw_parts(self.s, self.n) }.to_vec()
    }
}

#[repr(C)]
pub struct Host {
    pub(super) ctx: *mut c_void,
    pub(super) call: unsafe extern "C" fn(*mut c_void, u32, *mut Io) -> i32,
}

// SAFETY: the table is installed once, lives for the process and is only used on the main thread.
unsafe impl Sync for Host {}
// SAFETY: as above.
unsafe impl Send for Host {}

static HOST: OnceLock<&'static Host> = OnceLock::new();

mod op {
    pub const EXISTS_ID: u32 = 1;
    pub const EXISTS_NAME: u32 = 2;
    pub const DESTROY_ID: u32 = 3;
    pub const DESTROY_NAME: u32 = 4;
    pub const CREATE: u32 = 5;
    pub const CREATE_MOUSE: u32 = 6;
    pub const ID_OF: u32 = 7;
    pub const TRIGGERED: u32 = 8;
    pub const ADD_KEY: u32 = 9;
    pub const ADD_BUTTON: u32 = 10;
    pub const KEY_DOWN: u32 = 11;
    pub const KEY_NAME: u32 = 12;
    pub const BUTTON_DOWN: u32 = 13;
    pub const BUTTON_NAME: u32 = 14;
    pub const WARN: u32 = 15;
    pub const LOG_ACTIONS: u32 = 16;
    pub const CALL: u32 = 17;
    pub const CALL_KEY_CONFIG: u32 = 18;
    pub const BROADCAST_ACTION: u32 = 19;
    pub const BROADCAST_ANALOG: u32 = 20;
    pub const MOUSE_REL: u32 = 21;
    pub const SCREEN_HEIGHT: u32 = 22;
    pub const USER_CONFIG: u32 = 23;
    pub const ARR_SIZE: u32 = 24;
    pub const ARR_AT: u32 = 25;
    pub const STR_REF: u32 = 26;
    pub const ENGINE_ACTION_NAME: u32 = 27;
    pub const ENGINE_ACTION_BINDS: u32 = 28;
    pub const DUMP: u32 = 29;
}

pub(super) struct CHost(pub(super) &'static Host);

impl CHost {
    pub(super) fn go(&self, op: u32, mut io: Io) -> (i32, Io) {
        // SAFETY: the table was installed by the host and `io` outlives the call.
        let r = unsafe { (self.0.call)(self.0.ctx, op, &mut io) };
        (r, io)
    }

    fn named(&self, op: u32, name: &[u8], i0: i32) -> i32 {
        let mut io = Io::new();
        io.s = name.as_ptr();
        io.n = name.len();
        io.i[0] = i0;
        self.go(op, io).0
    }

    fn id(&self, op: u32, id: i32) -> i32 {
        let mut io = Io::new();
        io.i[0] = id;
        self.go(op, io).0
    }
}

impl InHost for CHost {
    fn action_exists_id(&self, id: i32) -> bool {
        self.id(op::EXISTS_ID, id) != 0
    }
    fn action_exists_name(&self, name: &[u8]) -> bool {
        self.named(op::EXISTS_NAME, name, 0) != 0
    }
    fn action_destroy_id(&self, id: i32) {
        self.id(op::DESTROY_ID, id);
    }
    fn action_destroy_name(&self, name: &[u8]) {
        self.named(op::DESTROY_NAME, name, 0);
    }
    fn action_create(&self, name: &[u8], id: i32) {
        self.named(op::CREATE, name, id);
    }
    fn action_create_mouse(&self, name: &[u8], id: i32, button: i32) {
        let mut io = Io::new();
        io.s = name.as_ptr();
        io.n = name.len();
        io.i[0] = id;
        io.i[1] = button;
        self.go(op::CREATE_MOUSE, io);
    }
    fn action_id_of(&self, name: &[u8]) -> Option<i32> {
        let mut io = Io::new();
        io.s = name.as_ptr();
        io.n = name.len();
        let (r, io) = self.go(op::ID_OF, io);
        (r != 0).then_some(io.i[0])
    }
    fn action_triggered(&self, id: i32) -> bool {
        self.id(op::TRIGGERED, id) != 0
    }
    fn add_key(&self, id: i32, key: &[u8]) {
        self.named(op::ADD_KEY, key, id);
    }
    fn add_button(&self, id: i32, button: &[u8]) {
        self.named(op::ADD_BUTTON, button, id);
    }
    fn key_down(&self, i: i32) -> bool {
        self.id(op::KEY_DOWN, i) != 0
    }
    fn key_name(&self, i: i32) -> Vec<u8> {
        let mut io = Io::new();
        io.i[0] = i;
        self.go(op::KEY_NAME, io).1.text()
    }
    fn button_down(&self, i: i32) -> bool {
        self.id(op::BUTTON_DOWN, i) != 0
    }
    fn button_name(&self, i: i32) -> Vec<u8> {
        let mut io = Io::new();
        io.i[0] = i;
        self.go(op::BUTTON_NAME, io).1.text()
    }
    fn warn(&self, text: &[u8]) {
        self.named(op::WARN, text, 0);
    }
    fn log_actions(&self, count: usize) {
        self.id(op::LOG_ACTIONS, count as i32);
    }
    fn call(&self, decl: &[u8]) {
        self.named(op::CALL, decl, 0);
    }
    fn call_key_config(&self) {
        self.go(op::CALL_KEY_CONFIG, Io::new());
    }
    fn broadcast_action(&self, id: i32, pressed: bool) {
        let mut io = Io::new();
        io.i[0] = id;
        io.i[1] = i32::from(pressed);
        self.go(op::BROADCAST_ACTION, io);
    }
    fn broadcast_analog(&self, id: i32, v: [f32; 3]) {
        let mut io = Io::new();
        io.i[0] = id;
        io.f[..3].copy_from_slice(&v);
        self.go(op::BROADCAST_ANALOG, io);
    }
    fn mouse_rel(&self) -> (i32, i32) {
        let io = self.go(op::MOUSE_REL, Io::new()).1;
        (io.i[0], io.i[1])
    }
    fn screen_height(&self) -> f32 {
        self.go(op::SCREEN_HEIGHT, Io::new()).1.f[0]
    }
    fn user_config(&self) -> Option<(bool, bool, f32, f32)> {
        let (r, io) = self.go(op::USER_CONFIG, Io::new());
        (r != 0).then_some((io.i[0] != 0, io.i[1] != 0, io.f[0], io.f[1]))
    }
    fn arr_size(&self, a: *const c_void) -> usize {
        let mut io = Io::new();
        io.p[0] = a.cast_mut();
        usize::try_from(self.go(op::ARR_SIZE, io).0).unwrap_or(0)
    }
    fn arr_at(&self, a: *const c_void, i: usize) -> *const c_void {
        let mut io = Io::new();
        io.p[0] = a.cast_mut();
        io.i[0] = i as i32;
        self.go(op::ARR_AT, io).1.p[1].cast_const()
    }
    fn str_ref(&self, text: &[u8]) -> *mut c_void {
        let mut io = Io::new();
        io.s = text.as_ptr();
        io.n = text.len();
        self.go(op::STR_REF, io).1.p[1]
    }
    fn engine_action_name(&self, id: i32) -> Option<Vec<u8>> {
        let mut io = Io::new();
        io.i[0] = id;
        let (r, io) = self.go(op::ENGINE_ACTION_NAME, io);
        (r != 0).then(|| io.text())
    }
    fn engine_action_binds(&self, id: i32) -> Vec<u8> {
        let mut io = Io::new();
        io.i[0] = id;
        self.go(op::ENGINE_ACTION_BINDS, io).1.text()
    }
    fn dump(&self, line: &[u8]) {
        self.named(op::DUMP, line, 0);
    }
}

#[cfg(test)]
thread_local! {
    pub(super) static TEST_HOST: std::cell::RefCell<Option<std::rc::Rc<dyn InHost>>> =
        const { std::cell::RefCell::new(None) };
}

fn with_host<R>(f: impl FnOnce(&dyn InHost) -> R) -> Option<R> {
    #[cfg(test)]
    if let Some(h) = TEST_HOST.with(|t| t.borrow().clone()) {
        return Some(f(&*h));
    }
    HOST.get().map(|h| f(&CHost(h)))
}

/// # Safety
/// `host` points at a table that lives for the whole process.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_s5_in_install_host(host: *const Host) {
    // SAFETY: caller contract.
    if let Some(h) = unsafe { host.as_ref() } {
        let _ = HOST.set(h);
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s5_in_new(max_smooth: i32, prev_mul: f32, keys: usize, buttons: usize) {
    with_host(|h| input::init(h, max_smooth, prev_mul, keys, buttons));
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s5_in_load_user_config() {
    with_host(input::load_user_config);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s5_in_load_script() {
    with_host(input::load_script);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s5_in_load_key_config() {
    with_host(input::load_key_config);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s5_in_latch() {
    with_host(input::latch);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s5_in_dump() {
    with_host(input::dump_state);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s5_in_clear_gui() {
    input::clear_gui_input();
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s5_in_update(game_input: bool) {
    with_host(|h| input::update(h, game_input));
}

/// # Safety
/// `engine` is a live `asIScriptEngine`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_s5_register_input_natives(engine: *mut c_void) {
    register(Engine(engine));
}

fn elements<T>(f: &Frame, i: usize, read: impl Fn(*const c_void) -> T) -> Vec<T> {
    let a = f.ptr_arg(i).cast_const();
    with_host(|h| (0..h.arr_size(a)).map(|k| read(h.arr_at(a, k))).collect()).unwrap_or_default()
}

fn strings(f: &Frame, i: usize) -> Vec<Vec<u8>> {
    // SAFETY: elements of `array<tString>` are `std::string` objects.
    elements(f, i, |p| unsafe { str_bytes(p) }.to_vec())
}

fn ints(f: &Frame, i: usize) -> Vec<i32> {
    // SAFETY: elements of `array<int>` are 4-byte integers.
    elements(f, i, |p| unsafe { p.cast::<i32>().read_unaligned() })
}

fn uints(f: &Frame, i: usize) -> Vec<u32> {
    // SAFETY: elements of `array<uint>` are 4-byte integers.
    elements(f, i, |p| unsafe { p.cast::<u32>().read_unaligned() })
}

fn bools(f: &Frame, i: usize) -> Vec<bool> {
    // SAFETY: elements of `array<bool>` are single bytes.
    elements(f, i, |p| unsafe { p.cast::<u8>().read() } != 0)
}

fn create(a: Action) {
    with_host(|h| input::create_action(h, a));
}

const T: &str = "cLuxInputHandler";

pub fn register(e: Engine) {
    let s = Soma(e);
    s.func("bool cInput_BecameTriggered(int alId)", |f| {
        f.ret(input::became_down(f.arg(0)))
    });
    s.func("bool cInput_WasTriggered(int alId)", |f| {
        f.ret(input::was_triggered(f.arg(0)))
    });
    s.func(
        "bool cInput_BecameTriggered(const tString&in asName)",
        |f| {
            let id = with_host(|h| h.action_id_of(f.str_arg(0))).flatten();
            f.ret(id.is_some_and(input::became_down));
        },
    );
    s.func("bool cInput_WasTriggered(const tString&in asName)", |f| {
        let id = with_host(|h| h.action_id_of(f.str_arg(0))).flatten();
        f.ret(id.is_some_and(input::was_triggered));
    });
    s.method(T, "void CreateAction(const tString&in asName, int alId, bool abConfigurable, const tString&in asCat)", |f| {
        create(Action::plain(f.str_arg(0), f.arg(1)));
    });
    s.method(
        T,
        "void CreateDebugAction(const tString&in asName, int alId)",
        |f| {
            create(Action::plain(f.str_arg(0), f.arg(1)));
        },
    );
    s.method(
        T,
        "void CreateAnalogAction(const tString&in asName, int alId, bool abConfigurable, const tString&in asCat, int alAxis, float afMul, int alAnalogId)",
        |f| {
            let a = Action {
                axis: f.arg(4),
                mul: f.arg(5),
                analog_id: f.arg(6),
                ..Action::plain(f.str_arg(0), f.arg(1))
            };
            create(a);
        },
    );
    s.method(
        T,
        "void CreateAnalogGamepadAction(const tString&in asName, int alId, const tString&in asCat, int alAnalogId, float afSmoothness, int alDirectionLimit)",
        |f| {
            let a = Action {
                analog_id: f.arg(3),
                gamepad: true,
                ..Action::plain(f.str_arg(0), f.arg(1))
            };
            create(a);
        },
    );
    s.method(
        T,
        "void CreateActionInput(const tString&in asInputType, int alActionId)",
        |f| {
            with_host(|h| input::create_action_input(h, f.str_arg(0), f.arg(1)));
        },
    );
    s.method(
        T,
        "void CreateAnalogGamepadActionInput(const tString&in asInputType, int alActionId)",
        |_| {},
    );
    s.method(T, "bool IsGamepadConnected()", |f| f.ret(false));
    s.method(T, "void LoadKeyConfig()", |_| {
        with_host(input::load_key_config);
    });
    s.method(T, "float GetTimeSinceGamepadWasUsed(int alID)", |f| {
        f.ret(100_000.0f32)
    });
    s.method(
        T,
        "int GetLastUsedGamepadIndex(float afTimeLimit=-1.0f)",
        |f| f.ret(-1i32),
    );
    s.method(T, "bool GetGamepadWasLastDeviceUsed()", |f| f.ret(false));
    s.method(T, "const tString& GetLatestKeyPressed()", |f| {
        let p = with_host(|h| h.str_ref(&input::latest_key())).unwrap_or(std::ptr::null_mut());
        f.ret_ref(p);
    });
    s.method(T, "bool IsYAxisInverted()", |f| {
        f.ret(input::settings().invert)
    });
    s.method(T, "bool WasAnalogueInputFromPad()", |f| f.ret(false));
    s.method(T, "bool GetSmoothMouse()", |f| {
        f.ret(input::settings().smooth)
    });
    s.method(T, "void SetSmoothMouse(bool abX)", |f| {
        input::tune(|s| s.smooth_mouse = f.arg(0))
    });
    s.method(T, "float GetMouseSensitivity()", |f| {
        f.ret(input::settings().mouse)
    });
    s.method(T, "void SetMouseSensitivity(float afX)", |f| {
        input::tune(|s| s.mouse_sensitivity = f.arg(0))
    });
    s.method(T, "float GetGamepadSensitivity()", |f| {
        f.ret(input::settings().gamepad)
    });
    s.method(T, "void SetGamepadSensitivity(float afX)", |f| {
        input::tune(|s| s.gamepad_sensitivity = f.arg(0))
    });
    s.method(
        T,
        "void SetRumble(int alDevice, float afStrength, float afDuration)",
        |_| {},
    );
    s.method(T, "void ResetSmoothMousePos()", |_| input::reset_smooth());
    s.method(T, "void SetMaxSmoothMousePos(int alX)", |f| {
        input::tune(|s| s.max_smooth = f.arg(0))
    });
    s.method(T, "void SetPrevSmoothMousePosMul(float afX)", |f| {
        input::tune(|s| s.prev_smooth_mul = f.arg(0))
    });
    s.method(T, "tString GetActionName(int alId, bool abAnalog)", |f| {
        f.ret_string(&input::action_name(f.arg(0), f.arg(1)));
    });
    s.method(
        T,
        "void CreateGamepadProfile(const tString&in asName, const tString&in asPrefix, array<tString> &in avButtons, array<tString> &in avAxes, array<uint> &in avDPad)",
        |f| input::create_profile(f.str_arg(0), f.str_arg(1), strings(f, 2), strings(f, 3), uints(f, 4)),
    );
    s.method(
        T,
        "void AddPresetToProfile(const tString&in asProfile, const tString&in asPreset, array<int> &in avActions, array<tString> &in avBindings, array<bool> &in avAnalog)",
        |f| {
            let preset = Preset {
                name: f.str_arg(1).to_vec(),
                actions: ints(f, 2),
                bindings: strings(f, 3),
                analog: bools(f, 4),
            };
            input::add_preset(f.str_arg(0), preset);
        },
    );
    s.method(
        T,
        "void SetGamepadMapping(const tString&in asProfile, const tString&in asPreset)",
        |f| {
            input::set_mapping(f.str_arg(0), f.str_arg(1));
        },
    );
    s.method(T, "int GetGamepadMappingActionNum()", |f| {
        f.ret(input::mapping_action_num())
    });
    s.method(
        T,
        "bool GetGamepadMappingAction(int alId, int&out alAction, tString&out asPrimary, bool&out abAnalog)",
        |f| {
            let m = input::mapping_action(f.arg(0));
            if let Some(a) = m.action {
                *f.rm::<i32>(1) = a;
            }
            if let Some((primary, analog)) = m.primary {
                f.put_str(2, &primary);
                *f.rm::<bool>(3) = analog;
            }
            f.ret(m.found);
        },
    );
    s.method(
        T,
        "void GetActionsAssociatedToGamepadControl(const tString &in asProfile, const tString &in asPreset, const tString &in asControl, tString &out asActions)",
        |f| {
            // SAFETY: slot 3 holds the `tString &out`.
            let mut out = unsafe { str_bytes(f.ptr_arg(3)) }.to_vec();
            input::actions_for_control(f.str_arg(0), f.str_arg(1), f.str_arg(2), &mut out);
            f.put_str(3, &out);
        },
    );
    s.method(
        T,
        "void FetchGamepadInputLayoutString(const tString &in asInputName, tString &out asPrefixName, tString &out asLayoutString)",
        |f| {
            let (prefix, layout) = input::layout(f.str_arg(0));
            f.put_str(1, &prefix);
            f.put_str(2, &layout);
        },
    );
}
