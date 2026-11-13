//! `cHeadlessControlServer`: unix-socket command server. A socket thread splits lines, parses
//! them and queues requests; the engine thread dispatches them from `update`. Engine access goes
//! through `HcHost`; no lock is held across a host call (host calls re-enter through the log hook).

use crate::string;
use crate::system::headless::{LineSplitter, json_escape, parse_flat_json, response_json};
use std::collections::{BTreeMap, VecDeque};
use std::ffi::c_int;
use std::io::Read;
use std::os::fd::AsRawFd;
use std::os::unix::ffi::OsStrExt;
use std::os::unix::net::{UnixListener, UnixStream};
use std::sync::atomic::{AtomicBool, AtomicI32, AtomicU64, Ordering};
use std::sync::{Arc, Mutex, MutexGuard};
use std::thread::JoinHandle;

unsafe extern "C" {
    fn send(fd: c_int, buf: *const u8, n: usize, flags: c_int) -> isize;
    fn shutdown(fd: c_int, how: c_int) -> c_int;
}

const MSG_NOSIGNAL: c_int = 0x4000;
const SHUT_RDWR: c_int = 2;
const SUN_PATH_MAX: usize = 107;
const LOG_LINES: usize = 500;
pub const MSG_APP_GOT_INPUT_FOCUS: i32 = 11;
pub const MSG_APP_LOST_INPUT_FOCUS: i32 = 14;

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum LogLevel {
    Log,
    Warning,
    Error,
}

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum Shot {
    Saved,
    CopyFailed,
    SaveFailed,
}

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum MouseButton {
    Left,
    Middle,
    Right,
}

/// Input the server injects; the host turns it into SDL events.
#[derive(Clone, PartialEq, Eq, Debug)]
pub enum Ev {
    Key {
        down: bool,
        sym: i32,
        scancode: i32,
    },
    Motion {
        x: i32,
        y: i32,
        xrel: i32,
        yrel: i32,
    },
    Button {
        button: MouseButton,
        down: bool,
        x: i32,
        y: i32,
    },
    /// Goes to the input layer's injected list, not the SDL queue.
    Text(Vec<u8>),
}

pub trait HcHost {
    fn now_ms(&self) -> u64;
    fn rendered_frames(&self) -> u32;
    fn log(&self, level: LogLevel, msg: &[u8]);
    fn exit(&self);
    fn set_wait_if_out_of_focus(&self, on: bool);
    fn dev_hud_active(&self) -> bool;
    fn set_dev_hud_active(&self, on: bool);
    fn screen_float_stats(&self) -> Option<(f32, f32, f32)>;
    fn force_window_size(&self, w: i32, h: i32) -> bool;
    fn shader_fail_count(&self) -> i32;
    fn shader_report_json(&self, failed_only: bool) -> Vec<u8>;
    /// Frame JSON and GL-error JSON, `None` when the frame buffer copy fails.
    fn frame_stats(&self) -> Option<(Vec<u8>, Vec<u8>)>;
    fn xdg_cache_home(&self) -> Vec<u8>;
    fn ensure_folder(&self, dir: &[u8]);
    fn screenshot(&self, path: &[u8]) -> Shot;
    /// 0 for an unknown name (`SDLK_UNKNOWN`).
    fn key_from_name(&self, name: &[u8]) -> i32;
    fn scancode_from_key(&self, key: i32) -> i32;
    fn push_event(&self, ev: &Ev);
    fn run_message(&self, msg: i32);
    /// Reply JSON of the game handler registered in `slot` (see `HcServer::register`).
    fn call_handler(&self, slot: usize, req: &Request) -> Vec<u8>;
}

/// `cHeadlessRequest`.
#[derive(Default, Clone, Debug)]
pub struct Request {
    pub fields: BTreeMap<Vec<u8>, Vec<u8>>,
}

fn cstr(v: &[u8]) -> &[u8] {
    v.split(|&c| c == 0).next().unwrap_or_default()
}

impl Request {
    pub fn cmd(&self) -> Vec<u8> {
        self.get_string(b"cmd", b"")
    }

    pub fn get_string(&self, key: &[u8], default: &[u8]) -> Vec<u8> {
        self.fields
            .get(key)
            .map_or_else(|| default.to_vec(), Clone::clone)
    }

    pub fn get_float(&self, key: &[u8], default: f32) -> f32 {
        self.fields
            .get(key)
            .map_or(default, |v| string::token_to_float(cstr(v)))
    }

    pub fn get_int(&self, key: &[u8], default: i32) -> i32 {
        self.fields
            .get(key)
            .map_or(default, |v| string::token_to_int(cstr(v)))
    }

    pub fn get_bool(&self, key: &[u8], default: bool) -> bool {
        self.fields
            .get(key)
            .map_or(default, |v| v == b"1" || string::to_bool(cstr(v)))
    }

    pub fn has_key(&self, key: &[u8]) -> bool {
        self.fields.contains_key(key)
    }
}

/// `cHeadlessResponse`.
#[derive(Debug)]
pub struct Response {
    ok: bool,
    error: Vec<u8>,
    fields: Vec<(Vec<u8>, Vec<u8>)>,
}

impl Default for Response {
    fn default() -> Self {
        Self {
            ok: true,
            error: Vec::new(),
            fields: Vec::new(),
        }
    }
}

impl Response {
    pub fn set_error(&mut self, msg: &[u8]) {
        self.ok = false;
        self.error = msg.to_vec();
    }

    pub fn set_str(&mut self, key: &[u8], val: &[u8]) {
        let mut v = b"\"".to_vec();
        v.extend(json_escape(val));
        v.push(b'"');
        self.fields.push((key.to_vec(), v));
    }

    pub fn set_float(&mut self, key: &[u8], val: f32) {
        self.fields.push((key.to_vec(), crate::system::num(val)));
    }

    pub fn set_int(&mut self, key: &[u8], val: i32) {
        self.fields
            .push((key.to_vec(), val.to_string().into_bytes()));
    }

    pub fn set_bool(&mut self, key: &[u8], val: bool) {
        let v: &[u8] = if val { b"true" } else { b"false" };
        self.fields.push((key.to_vec(), v.to_vec()));
    }

    pub fn set_raw(&mut self, key: &[u8], json: &[u8]) {
        self.fields.push((key.to_vec(), json.to_vec()));
    }

    pub fn to_json(&self) -> Vec<u8> {
        let f: Vec<(&[u8], &[u8])> = self
            .fields
            .iter()
            .map(|(k, v)| (k.as_slice(), v.as_slice()))
            .collect();
        response_json(self.ok, &self.error, &f)
    }
}

fn lock<T>(m: &Mutex<T>) -> MutexGuard<'_, T> {
    m.lock().unwrap_or_else(std::sync::PoisonError::into_inner)
}

struct Client {
    stream: UnixStream,
}

struct Pending {
    client: Arc<Client>,
    request: Request,
}

#[derive(Clone)]
struct Waiter {
    client: Arc<Client>,
    target: u32,
    start: u32,
    deadline: u64,
}

#[derive(Default)]
struct EngineState {
    waiters: Vec<Waiter>,
    drag_frames: i32,
    drag_x: i32,
    drag_y: i32,
    drag_path: VecDeque<(i32, i32)>,
}

struct Shared {
    queue: Mutex<VecDeque<Pending>>,
    logs: Mutex<VecDeque<Vec<u8>>>,
    active: AtomicBool,
    listen_fd: AtomicI32,
    client_fd: AtomicI32,
    enqueued: AtomicU64,
}

pub struct HcServer {
    shared: Arc<Shared>,
    state: Mutex<EngineState>,
    handlers: Mutex<Vec<Vec<u8>>>,
    path: Vec<u8>,
    listening: bool,
    thread: Option<JoinHandle<()>>,
}

fn send_line(client: &Client, json: &[u8]) {
    let mut line = json.to_vec();
    line.push(b'\n');
    // SAFETY: the fd is kept open by `client`; MSG_NOSIGNAL keeps a closed peer from raising SIGPIPE.
    unsafe {
        send(
            client.stream.as_raw_fd(),
            line.as_ptr(),
            line.len(),
            MSG_NOSIGNAL,
        );
    }
}

fn serve(shared: &Shared, stream: UnixStream) {
    let mut reader = match stream.try_clone() {
        Ok(r) => r,
        Err(_) => return,
    };
    let client = Arc::new(Client { stream });
    shared
        .client_fd
        .store(client.stream.as_raw_fd(), Ordering::SeqCst);
    let mut splitter = LineSplitter::default();
    let mut buf = [0u8; 4096];
    loop {
        let n = match reader.read(&mut buf) {
            Ok(0) | Err(_) => break,
            Ok(n) => n,
        };
        for line in splitter.push(buf.get(..n).unwrap_or_default()) {
            let request = Request {
                fields: parse_flat_json(&line),
            };
            lock(&shared.queue).push_back(Pending {
                client: Arc::clone(&client),
                request,
            });
            shared.enqueued.fetch_add(1, Ordering::SeqCst);
        }
    }
    shared.client_fd.store(-1, Ordering::SeqCst);
}

fn accept_loop(shared: Arc<Shared>, listener: UnixListener) {
    while shared.active.load(Ordering::SeqCst) {
        match listener.accept() {
            Ok((stream, _)) => serve(&shared, stream),
            Err(_) => std::thread::sleep(std::time::Duration::from_millis(1)),
        }
    }
}

impl HcServer {
    /// `cHeadlessControlServer` constructor up to `RegisterBuiltins`.
    pub fn new(host: &dyn HcHost, socket_path: &[u8]) -> Self {
        let shared = Arc::new(Shared {
            queue: Mutex::new(VecDeque::new()),
            logs: Mutex::new(VecDeque::new()),
            active: AtomicBool::new(false),
            listen_fd: AtomicI32::new(-1),
            client_fd: AtomicI32::new(-1),
            enqueued: AtomicU64::new(0),
        });
        let mut server = Self {
            shared: Arc::clone(&shared),
            state: Mutex::new(EngineState::default()),
            handlers: Mutex::new(Vec::new()),
            path: socket_path.to_vec(),
            listening: false,
            thread: None,
        };
        let _ = std::fs::remove_file(std::ffi::OsStr::from_bytes(socket_path));
        // sockaddr_un truncation: C++ copies at most 107 bytes.
        let bound = socket_path.get(..SUN_PATH_MAX).unwrap_or(socket_path);
        match UnixListener::bind(std::ffi::OsStr::from_bytes(bound)) {
            Err(e) => {
                let mut msg = b"HeadlessControl: bind('".to_vec();
                msg.extend_from_slice(socket_path);
                msg.extend_from_slice(b"') failed: ");
                msg.extend_from_slice(os_error_text(&e).as_bytes());
                msg.push(b'\n');
                host.log(LogLevel::Error, &msg);
            }
            Ok(listener) => {
                shared
                    .listen_fd
                    .store(listener.as_raw_fd(), Ordering::SeqCst);
                shared.active.store(true, Ordering::SeqCst);
                let s = Arc::clone(&shared);
                server.thread = std::thread::Builder::new()
                    .spawn(move || accept_loop(s, listener))
                    .ok();
                server.listening = true;
                let mut msg = b"HeadlessControl: listening on '".to_vec();
                msg.extend_from_slice(socket_path);
                msg.extend_from_slice(b"'\n");
                host.log(LogLevel::Log, &msg);
            }
        }
        server
    }

    /// `RegisterHandler`: the slot the host stores the callback in; a repeated name reuses its slot.
    pub fn register(&self, name: &[u8]) -> usize {
        let mut h = lock(&self.handlers);
        h.iter().position(|n| n == name).unwrap_or_else(|| {
            h.push(name.to_vec());
            h.len() - 1
        })
    }

    /// Requests the socket thread has queued so far.
    pub fn enqueued(&self) -> u64 {
        self.shared.enqueued.load(Ordering::SeqCst)
    }

    pub fn is_listening(&self) -> bool {
        self.listening
    }

    pub fn push_log_line(&self, line: &[u8]) {
        let mut logs = lock(&self.shared.logs);
        logs.push_back(line.to_vec());
        while logs.len() > LOG_LINES {
            logs.pop_front();
        }
    }

    pub fn logic_step(&self, host: &dyn HcHost) {
        let (x, y) = {
            let mut st = lock(&self.state);
            if let Some((x, y)) = st.drag_path.pop_front() {
                st.drag_x = x;
                st.drag_y = y;
            } else if st.drag_frames > 0 {
                st.drag_frames -= 1;
            } else {
                return;
            }
            (st.drag_x, st.drag_y)
        };
        host.push_event(&Ev::Motion {
            x: 0,
            y: 0,
            xrel: x,
            yrel: y,
        });
    }

    pub fn update(&self, host: &dyn HcHost, loading: bool) {
        let mut i = 0;
        loop {
            let Some(w) = lock(&self.state).waiters.get(i).cloned() else {
                break;
            };
            let timed_out = host.now_ms() >= w.deadline;
            let frame = host.rendered_frames();
            if frame < w.target && !timed_out {
                i += 1;
                continue;
            }
            let mut resp = Response::default();
            resp.set_int(b"frames", frame.wrapping_sub(w.start) as i32);
            resp.set_bool(b"timed_out", timed_out);
            send_line(&w.client, &resp.to_json());
            let mut st = lock(&self.state);
            if i < st.waiters.len() {
                st.waiters.remove(i);
            }
        }

        loop {
            let pending = {
                let mut q = lock(&self.shared.queue);
                let pos = q.iter().position(|p| {
                    if !loading {
                        return true;
                    }
                    let c = p.request.cmd();
                    c == b"screenshot" || c == b"ping"
                });
                pos.and_then(|i| q.remove(i))
            };
            let Some(p) = pending else { break };
            self.dispatch(host, &p);
        }
    }

    fn dispatch(&self, host: &dyn HcHost, p: &Pending) {
        let req = &p.request;
        let cmd = req.cmd();
        if cmd == b"wait_frames" {
            let total = req.get_int(b"n", 1);
            let start = host.rendered_frames();
            let w = Waiter {
                client: Arc::clone(&p.client),
                target: start.wrapping_add(total as u32),
                start,
                deadline: host
                    .now_ms()
                    .wrapping_add(i64::from(req.get_int(b"max_ms", 600_000)) as u64),
            };
            lock(&self.state).waiters.push(w);
            return;
        }
        let slot = lock(&self.handlers).iter().position(|n| *n == cmd);
        if let Some(slot) = slot {
            send_line(&p.client, &host.call_handler(slot, req));
            return;
        }
        let mut resp = Response::default();
        match cmd.as_slice() {
            b"ping" => resp.set_bool(b"pong", true),
            b"quit" => host.exit(),
            b"screenshot" => cmd_screenshot(host, req, &mut resp),
            b"log_tail" => self.cmd_log_tail(req, &mut resp),
            b"set_focus_wait" => host.set_wait_if_out_of_focus(req.get_bool(b"enabled", false)),
            b"input" => self.cmd_input(host, req, &mut resp),
            b"resize" => cmd_resize(host, req, &mut resp),
            b"shader_report" => {
                resp.set_int(b"failures", host.shader_fail_count());
                resp.set_raw(
                    b"shaders",
                    &host.shader_report_json(req.get_bool(b"failed_only", true)),
                );
            }
            b"frame_stats" => match host.frame_stats() {
                None => resp.set_error(b"CopyFrameBufferToBitmap() failed"),
                Some((frame, gl)) => {
                    resp.set_raw(b"frame", &frame);
                    resp.set_raw(b"gl_errors", &gl);
                }
            },
            b"screen_stats" => match host.screen_float_stats() {
                None => resp.set_error(b"no float screen target"),
                Some((max, mean, over)) => {
                    resp.set_float(b"max", max);
                    resp.set_float(b"mean", mean);
                    resp.set_float(b"over1", over);
                }
            },
            b"dev_hud" => {
                resp.set_bool(b"prev", host.dev_hud_active());
                if req.has_key(b"value") {
                    host.set_dev_hud_active(req.get_bool(b"value", true));
                }
            }
            _ => {
                let mut m = b"unknown command: '".to_vec();
                m.extend_from_slice(&cmd);
                m.push(b'\'');
                resp.set_error(&m);
            }
        }
        send_line(&p.client, &resp.to_json());
    }

    fn cmd_log_tail(&self, req: &Request, resp: &mut Response) {
        let want = req.get_int(b"lines", 50);
        let mut joined = Vec::new();
        {
            let logs = lock(&self.shared.logs);
            let start = (logs.len() as i32).wrapping_sub(want).max(0) as usize;
            for line in logs.iter().skip(start) {
                joined.extend_from_slice(line);
                joined.push(b'\n');
            }
        }
        resp.set_str(b"log", &joined);
    }

    fn cmd_input(&self, host: &dyn HcHost, req: &Request, resp: &mut Response) {
        let ty = req.get_string(b"type", b"");
        match ty.as_slice() {
            b"key" => {
                let name = req.get_string(b"key", b"");
                let action = req.get_string(b"action", b"down");
                let sym = host.key_from_name(&name);
                if sym == 0 {
                    let mut m = b"unknown key name: '".to_vec();
                    m.extend_from_slice(&name);
                    m.push(b'\'');
                    resp.set_error(&m);
                    return;
                }
                let scancode = host.scancode_from_key(sym);
                host.push_event(&Ev::Key {
                    down: action != b"up",
                    sym,
                    scancode,
                });
                if action == b"tap" {
                    host.push_event(&Ev::Key {
                        down: false,
                        sym,
                        scancode,
                    });
                }
            }
            b"mouse_move" => {
                let xrel = req.get_int(b"xrel", 0);
                let yrel = req.get_int(b"yrel", 0);
                let ev = Ev::Motion {
                    x: req.get_int(b"x", 0),
                    y: req.get_int(b"y", 0),
                    xrel,
                    yrel,
                };
                let path = parse_drag_path(&req.get_string(b"path", b""));
                let empty = path.is_empty();
                {
                    let mut st = lock(&self.state);
                    st.drag_path = path;
                    st.drag_frames = req.get_int(b"frames", 1).wrapping_sub(1);
                    st.drag_x = xrel;
                    st.drag_y = yrel;
                }
                if empty {
                    host.push_event(&ev);
                }
            }
            b"text" => {
                for chunk in text_chunks(&req.get_string(b"text", b"")) {
                    host.push_event(&Ev::Text(chunk));
                }
            }
            b"mouse_button" => {
                let button = req.get_string(b"button", b"left");
                let action = req.get_string(b"action", b"down");
                host.push_event(&Ev::Button {
                    button: match button.as_slice() {
                        b"right" => MouseButton::Right,
                        b"middle" => MouseButton::Middle,
                        _ => MouseButton::Left,
                    },
                    down: action != b"up",
                    x: req.get_int(b"x", 0),
                    y: req.get_int(b"y", 0),
                });
            }
            b"focus" => match req.get_string(b"state", b"").as_slice() {
                b"lost" => host.run_message(MSG_APP_LOST_INPUT_FOCUS),
                b"gained" => host.run_message(MSG_APP_GOT_INPUT_FOCUS),
                _ => resp.set_error(b"focus requires 'state' of 'lost' or 'gained'"),
            },
            _ => {
                let mut m = b"unknown input type: '".to_vec();
                m.extend_from_slice(&ty);
                m.push(b'\'');
                resp.set_error(&m);
            }
        }
    }
}

fn os_error_text(e: &std::io::Error) -> String {
    let s = e.to_string();
    // "No such file or directory (os error 2)" -> strerror text
    s.split(" (os error").next().unwrap_or_default().to_string()
}

fn cmd_screenshot(host: &dyn HcHost, req: &Request, resp: &mut Response) {
    let has_path = req.has_key(b"path");
    let mut default_path = Vec::new();
    if !has_path {
        default_path = host.xdg_cache_home();
        default_path.extend_from_slice(b"open-hpl/headless_screenshot.bmp");
        let mut dir = host.xdg_cache_home();
        dir.extend_from_slice(b"open-hpl/");
        host.ensure_folder(&dir);
    }
    let path = req.get_string(b"path", &default_path);
    match host.screenshot(&path) {
        Shot::CopyFailed => resp.set_error(b"CopyFrameBufferToBitmap() failed"),
        Shot::SaveFailed => {
            let mut m = b"SaveBitmap('".to_vec();
            m.extend_from_slice(&path);
            m.extend_from_slice(b"') failed");
            resp.set_error(&m);
        }
        Shot::Saved => resp.set_str(b"path", &path),
    }
}

fn cmd_resize(host: &dyn HcHost, req: &Request, resp: &mut Response) {
    let w = req.get_int(b"width", 0);
    let h = req.get_int(b"height", 0);
    if w <= 0 || h <= 0 {
        resp.set_error(b"resize requires positive 'width' and 'height'");
        return;
    }
    if !host.force_window_size(w, h) {
        resp.set_error(b"could not resize window");
    }
}

/// `sscanf("%d")`: skips whitespace, optional sign, decimal digits; returns the value (long
/// truncated to int, as glibc stores it) and the end index.
fn scan_int(s: &[u8], mut i: usize) -> Option<(i32, usize)> {
    while s.get(i).is_some_and(|&c| matches!(c, b' ' | 9..=13)) {
        i += 1;
    }
    let neg = match s.get(i) {
        Some(b'-') => {
            i += 1;
            true
        }
        Some(b'+') => {
            i += 1;
            false
        }
        _ => false,
    };
    let first = i;
    let mut v: i64 = 0;
    while let Some(&c) = s.get(i).filter(|c| c.is_ascii_digit()) {
        v = v.saturating_mul(10).saturating_add(i64::from(c - b'0'));
        i += 1;
    }
    if i == first {
        return None;
    }
    Some(((if neg { -v } else { v }) as i32, i))
}

/// `"dx,dy;dx,dy;..."`: the loop of `sscanf(p, "%d,%d%n")`, one entry per logic update.
pub fn parse_drag_path(path: &[u8]) -> VecDeque<(i32, i32)> {
    let path = cstr(path);
    let mut out = VecDeque::new();
    let mut p = 0;
    while let Some((x, e)) = scan_int(path, p) {
        if path.get(e) != Some(&b',') {
            break;
        }
        let Some((y, e)) = scan_int(path, e + 1) else {
            break;
        };
        out.push_back((x, y));
        p = e + usize::from(path.get(e) == Some(&b';'));
    }
    out
}

/// Text injection split into at most 31-byte pieces on UTF-8 boundaries.
pub fn text_chunks(text: &[u8]) -> Vec<Vec<u8>> {
    let mut out = Vec::new();
    let mut pos = 0;
    while pos < text.len() {
        let mut len = (text.len() - pos).min(31);
        while pos + len < text.len() && text.get(pos + len).is_some_and(|c| c & 0xc0 == 0x80) {
            len -= 1;
            // ponytail: C++ underflows len here on 32 continuation bytes; stop instead
            if len == 0 {
                break;
            }
        }
        if len == 0 {
            break;
        }
        out.push(text.get(pos..pos + len).unwrap_or_default().to_vec());
        pos += len;
    }
    out
}

impl Drop for HcServer {
    fn drop(&mut self) {
        self.shared.active.store(false, Ordering::SeqCst);
        let lfd = self.shared.listen_fd.swap(-1, Ordering::SeqCst);
        if lfd >= 0 {
            // SAFETY: the accept thread still owns the listener, so the fd is open; shutdown wakes accept.
            unsafe { shutdown(lfd, SHUT_RDWR) };
        }
        let cfd = self.shared.client_fd.load(Ordering::SeqCst);
        if cfd >= 0 {
            // SAFETY: serve() resets the fd to -1 before its stream drops; a stale fd only fails.
            unsafe { shutdown(cfd, SHUT_RDWR) };
        }
        if let Some(t) = self.thread.take() {
            let _ = t.join();
        }
        let _ = std::fs::remove_file(std::ffi::OsStr::from_bytes(&self.path));
    }
}

/// `AcquireHeadlessSingleInstanceLock`.
pub fn acquire_single_instance_lock(
    env: &dyn Fn(&str) -> Option<Vec<u8>>,
    log: &dyn Fn(LogLevel, &str),
) {
    use std::fs::{File, OpenOptions, TryLockError};
    use std::os::unix::fs::OpenOptionsExt;

    if env("OPENHPL_HEADLESS_SOCKET").is_none() {
        return;
    }
    let dir = match env("XDG_RUNTIME_DIR") {
        Some(d) if !d.is_empty() => String::from_utf8_lossy(&d).into_owned(),
        _ => "/tmp".to_string(),
    };
    let lock_path = format!("{dir}/open-hpl-headless.lock");
    let open = |p: &str| {
        OpenOptions::new()
            .read(true)
            .write(true)
            .create(true)
            .truncate(false)
            .mode(0o600)
            .open(p)
    };
    let main: File = match open(&lock_path) {
        Ok(f) => f,
        Err(e) => {
            log(
                LogLevel::Warning,
                &format!(
                    "AcquireHeadlessSingleInstanceLock: couldn't open '{lock_path}' ({}) - proceeding unserialized\n",
                    os_error_text(&e)
                ),
            );
            return;
        }
    };
    if main.try_lock().is_ok() {
        std::mem::forget(main);
        return;
    }
    let slots = env("OPENHPL_HEADLESS_SLOTS")
        .map(|s| string::token_to_int(&s))
        .unwrap_or(0);
    // The held lock file descriptors stay open for the process lifetime.
    for i in 1..slots {
        if let Ok(f) = open(&format!("{lock_path}.{i}")) {
            match f.try_lock() {
                Ok(()) => {
                    std::mem::forget(f);
                    return;
                }
                Err(TryLockError::WouldBlock | TryLockError::Error(_)) => {}
            }
        }
    }
    log(
        LogLevel::Log,
        &format!(
            "Another headless instance already holds '{lock_path}' - waiting for it to exit (this is expected, not a hang - see AcquireHeadlessSingleInstanceLock())\n"
        ),
    );
    match main.lock() {
        Ok(()) => std::mem::forget(main),
        Err(e) => log(
            LogLevel::Warning,
            &format!(
                "AcquireHeadlessSingleInstanceLock: blocking flock() on '{lock_path}' failed ({}) - proceeding unserialized\n",
                os_error_text(&e)
            ),
        ),
    }
}
