//! `cScene` and `cViewport`: Rust-owned. Viewports are allocated here and C++ sees them through
//! the `cViewport` layout twin (`Viewport.h`); the scene lists, listener and render loop live here.
//! Engine objects (renderer, post effects, GUI sets, cameras, worlds, sound) go through [`Host`].

use super::cxx_camera::camera_frustum;
use super::scenemgr::{self, ViewportIn};
use super::wd::{self, World};
use crate::types::Vector3f;
use std::ffi::c_void;
use std::sync::atomic::{AtomicPtr, Ordering};

type P = *mut c_void;

#[repr(C)]
pub struct PtrView {
    ptr: *const P,
    len: usize,
}

impl PtrView {
    fn sync(&mut self, v: &[P]) {
        self.ptr = v.as_ptr();
        self.len = v.len();
    }
}

/// `cViewport` in C++ (`Viewport.h`): the leading fields are the C++ layout, the rest is Rust only.
#[repr(C)]
pub struct Viewport {
    pub camera: P,
    pub world: P,
    pub active: bool,
    pub visible: bool,
    pub listener: bool,
    pub renderer: P,
    pub composite: P,
    pub frame_buffer: P,
    pub pos: [i32; 2],
    pub size: [i32; 2],
    pub settings: P,
    pub rcbs: PtrView,
    scene: *mut Scene,
    guis: Vec<P>,
    vcbs: Vec<P>,
    rcb_list: Vec<P>,
}

const _: () = {
    use std::mem::{offset_of, size_of};
    assert!(offset_of!(Viewport, renderer) == 24 && offset_of!(Viewport, frame_buffer) == 40);
    assert!(offset_of!(Viewport, settings) == 64 && size_of::<PtrView>() == 16);
};

#[repr(C)]
pub struct Scene {
    graphics: P,
    resources: P,
    sound: P,
    physics: P,
    system: P,
    ai: P,
    gui: P,
    owner: P,
    listener: *mut Viewport,
    viewports: Vec<*mut Viewport>,
    worlds: Vec<P>,
    cameras: Vec<P>,
    view: PtrView,
}

#[repr(C)]
pub struct Host {
    pub settings_new: unsafe extern "C" fn() -> P,
    pub settings_delete: unsafe extern "C" fn(P),
    pub settings_reset: unsafe extern "C" fn(P),
    pub camera_new: unsafe extern "C" fn(graphics: P) -> P,
    pub camera_delete: unsafe extern "C" fn(P),
    pub world_new: unsafe extern "C" fn(args: *const WorldArgs, name: *const u8, len: usize) -> P,
    pub world_delete: unsafe extern "C" fn(P),
    pub world_load:
        unsafe extern "C" fn(resources: P, file: *const u8, len: usize, flags: u32) -> P,
    pub main_renderer: unsafe extern "C" fn(graphics: P) -> P,
    pub frame_count_inc: unsafe extern "C" fn(),
    pub renderer_render: unsafe extern "C" fn(
        renderer: P,
        frame_time: f32,
        frustum: P,
        world: P,
        settings: P,
        target: P,
        post: bool,
        callbacks: P,
    ),
    pub post_texture: unsafe extern "C" fn(renderer: P) -> P,
    pub composite_active: unsafe extern "C" fn(P) -> bool,
    pub composite_render: unsafe extern "C" fn(P, f32, frustum: P, input: P, target: P),
    pub set_frame_buffer:
        unsafe extern "C" fn(graphics: P, fb: P, pos: *const i32, size: *const i32),
    pub gui_is_3d: unsafe extern "C" fn(P) -> bool,
    pub gui_priority: unsafe extern "C" fn(P) -> i32,
    pub gui_render: unsafe extern "C" fn(P, frustum: P),
    pub viewport_callback: unsafe extern "C" fn(cb: P, message: i32),
    pub set_listener: unsafe extern "C" fn(
        sound: P,
        pos: *const Vector3f,
        fwd: *const Vector3f,
        up: *const Vector3f,
    ),
    pub log: unsafe extern "C" fn(error: bool, msg: *const u8, len: usize),
}

#[repr(C)]
pub struct WorldArgs {
    pub graphics: P,
    pub resources: P,
    pub sound: P,
    pub physics: P,
    pub scene: P,
    pub system: P,
    pub ai: P,
}

static HOST: AtomicPtr<Host> = AtomicPtr::new(std::ptr::null_mut());

/// # Safety
/// `h` outlives the process.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_scn_set_host(h: *const Host) {
    HOST.store(h.cast_mut(), Ordering::Release);
}

fn host() -> Option<&'static Host> {
    // SAFETY: set once at startup to a static table.
    unsafe { HOST.load(Ordering::Acquire).as_ref() }
}

fn log(error: bool, msg: &str) {
    if let Some(h) = host() {
        // SAFETY: table contract.
        unsafe { (h.log)(error, msg.as_ptr(), msg.len()) }
    }
}

fn at<T: Copy>(v: &[T], i: usize) -> Option<T> {
    v.get(i).copied()
}

fn remove_first<T: PartialEq>(v: &mut Vec<T>, x: T) -> bool {
    match v.iter().position(|e| *e == x) {
        Some(i) => {
            v.remove(i);
            true
        }
        None => false,
    }
}

/// `cWorld` keeps its Rust state pointer as first member.
unsafe fn world_state(w: P) -> *mut World {
    // SAFETY: caller passes a live cWorld.
    unsafe { *w.cast::<*mut World>() }
}

unsafe fn set_sound_emitter(w: P, on: bool) {
    // SAFETY: caller passes a live cWorld.
    unsafe { (*world_state(w)).pod.sound_emitter = on }
}

impl Scene {
    fn world_exists(&self, w: P) -> bool {
        self.worlds.contains(&w)
    }

    fn viewport_exists(&self, v: *mut Viewport) -> bool {
        self.viewports.contains(&v)
    }

    fn sync(&mut self) {
        // SAFETY: `*mut Viewport` and `P` are both thin pointers.
        let v = unsafe {
            std::slice::from_raw_parts(self.viewports.as_ptr().cast::<P>(), self.viewports.len())
        };
        self.view.sync(v);
    }

    fn world_args(&self) -> WorldArgs {
        WorldArgs {
            graphics: self.graphics,
            resources: self.resources,
            sound: self.sound,
            physics: self.physics,
            scene: self.owner,
            system: self.system,
            ai: self.ai,
        }
    }
}

impl Viewport {
    unsafe fn set_world(&mut self, w: P) {
        // SAFETY: scene and worlds are live.
        unsafe {
            if !self.world.is_null() && (*self.scene).world_exists(self.world) {
                set_sound_emitter(self.world, false);
            }
            self.world = w;
            if !w.is_null() {
                set_sound_emitter(w, true);
            }
            if let Some(h) = host() {
                (h.settings_reset)(self.settings);
            }
        }
    }
}

/// # Safety
/// Pointers are live engine objects (or null where C++ passes null).
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_scn_new(
    owner: P,
    graphics: P,
    resources: P,
    sound: P,
    physics: P,
    system: P,
    ai: P,
    gui: P,
) -> *mut Scene {
    let mut s = Box::new(Scene {
        graphics,
        resources,
        sound,
        physics,
        system,
        ai,
        gui,
        owner,
        listener: std::ptr::null_mut(),
        viewports: Vec::new(),
        worlds: Vec::new(),
        cameras: Vec::new(),
        view: PtrView {
            ptr: std::ptr::null(),
            len: 0,
        },
    });
    s.sync();
    Box::into_raw(s)
}

/// # Safety
/// `s` came from `ohpl_scn_new`; the host table is set.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_scn_free(s: *mut Scene) {
    log(false, "Exiting Scene Module\n");
    log(
        false,
        "--------------------------------------------------------\n",
    );
    // SAFETY: caller contract; deletes may re-enter through destructors, so take the lists first.
    unsafe {
        let mut s = Box::from_raw(s);
        let (vps, worlds, cams) = (
            std::mem::take(&mut s.viewports),
            std::mem::take(&mut s.worlds),
            std::mem::take(&mut s.cameras),
        );
        s.sync();
        if let Some(h) = host() {
            for vp in vps {
                let vp = Box::from_raw(vp);
                (h.settings_delete)(vp.settings);
            }
            for w in worlds {
                (h.world_delete)(w);
            }
            for c in cams {
                (h.camera_delete)(c);
            }
        }
    }
    log(
        false,
        "--------------------------------------------------------\n\n",
    );
}

/// # Safety
/// `s` live; `cam` and `world` are live or null.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_scn_create_viewport(
    s: *mut Scene,
    cam: P,
    world: P,
    front: bool,
) -> *mut Viewport {
    let Some(h) = host() else {
        return std::ptr::null_mut();
    };
    // SAFETY: caller contract.
    unsafe {
        let mut vp = Box::new(Viewport {
            camera: std::ptr::null_mut(),
            world: std::ptr::null_mut(),
            active: true,
            visible: true,
            listener: false,
            renderer: std::ptr::null_mut(),
            composite: std::ptr::null_mut(),
            frame_buffer: std::ptr::null_mut(),
            pos: [0, 0],
            size: [-1, -1],
            settings: (h.settings_new)(),
            rcbs: PtrView {
                ptr: std::ptr::null(),
                len: 0,
            },
            scene: s,
            guis: Vec::new(),
            vcbs: Vec::new(),
            rcb_list: Vec::new(),
        });
        vp.rcbs.sync(&vp.rcb_list);
        vp.camera = cam;
        vp.set_world(world);
        vp.renderer = (h.main_renderer)((*s).graphics);
        let p = Box::into_raw(vp);
        if front {
            (*s).viewports.insert(0, p);
        } else {
            (*s).viewports.push(p);
        }
        (*s).sync();
        p
    }
}

/// # Safety
/// `s` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_scn_destroy_viewport(s: *mut Scene, vp: *mut Viewport) {
    // SAFETY: caller contract.
    unsafe {
        if remove_first(&mut (*s).viewports, vp) {
            (*s).sync();
            let vp = Box::from_raw(vp);
            if let Some(h) = host() {
                (h.settings_delete)(vp.settings);
            }
        }
    }
}

/// # Safety
/// `s` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_scn_viewport_exists(s: *const Scene, vp: *mut Viewport) -> bool {
    // SAFETY: caller contract.
    unsafe { (*s).viewport_exists(vp) }
}

/// # Safety
/// `s` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_scn_viewports(s: *const Scene) -> *const PtrView {
    // SAFETY: caller contract.
    unsafe { &raw const (*s).view }
}

/// # Safety
/// `s` live; `vp` live or null.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_scn_set_current_listener(s: *mut Scene, vp: *mut Viewport) {
    // SAFETY: caller contract.
    unsafe {
        let prev = (*s).listener;
        if !prev.is_null() && (*s).viewport_exists(prev) {
            (*prev).listener = false;
            let w = (*prev).world;
            if !w.is_null() && (*s).world_exists(w) {
                set_sound_emitter(w, false);
            }
        }
        (*s).listener = vp;
        if !vp.is_null() {
            (*vp).listener = true;
            let w = (*vp).world;
            if !w.is_null() {
                set_sound_emitter(w, true);
            }
        }
    }
}

/// # Safety
/// `s` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_scn_create_camera(s: *mut Scene) -> P {
    let Some(h) = host() else {
        return std::ptr::null_mut();
    };
    // SAFETY: caller contract.
    unsafe {
        let c = (h.camera_new)((*s).graphics);
        (*s).cameras.push(c);
        c
    }
}

/// # Safety
/// `s` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_scn_destroy_camera(s: *mut Scene, c: P) {
    // SAFETY: caller contract.
    unsafe {
        if remove_first(&mut (*s).cameras, c)
            && let Some(h) = host()
        {
            (h.camera_delete)(c);
        }
    }
}

/// # Safety
/// `s` live; `file` valid for `len` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_scn_load_world(
    s: *mut Scene,
    file: *const u8,
    len: usize,
    flags: u32,
) -> P {
    let Some(h) = host() else {
        return std::ptr::null_mut();
    };
    // SAFETY: caller contract.
    unsafe { (h.world_load)((*s).resources, file, len, flags) }
}

/// # Safety
/// `s` live; `name` valid for `len` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_scn_create_world(s: *mut Scene, name: *const u8, len: usize) -> P {
    let Some(h) = host() else {
        return std::ptr::null_mut();
    };
    // SAFETY: caller contract.
    unsafe {
        let args = (*s).world_args();
        let w = (h.world_new)(&args, name, len);
        (*s).worlds.push(w);
        w
    }
}

/// # Safety
/// `s` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_scn_destroy_world(s: *mut Scene, w: P) {
    // SAFETY: caller contract.
    unsafe {
        if remove_first(&mut (*s).worlds, w)
            && let Some(h) = host()
        {
            (h.world_delete)(w);
        }
    }
}

/// # Safety
/// `s` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_scn_world_exists(s: *const Scene, w: P) -> bool {
    // SAFETY: caller contract.
    unsafe { (*s).world_exists(w) }
}

/// # Safety
/// `s` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_scn_post_update(s: *mut Scene, dt: f32) {
    let Some(h) = host() else { return };
    // SAFETY: caller contract; world updates may create or destroy worlds, so index the live list.
    unsafe {
        let mut i = 0;
        while i < (*s).worlds.len() {
            let Some(w) = at(&(*s).worlds, i) else {
                break;
            };
            if (*world_state(w)).pod.active {
                wd::ohpl_wd_update(world_state(w), dt);
            }
            i += 1;
        }
        let vp = (*s).listener;
        if !vp.is_null() && !(*vp).camera.is_null() {
            let cam = (*vp).camera.cast::<u8>();
            let (pos, fwd, up) = super::cxx_camera::listener_basis(cam);
            (h.set_listener)((*s).sound, &pos, &fwd, &up);
        }
    }
}

/// # Safety
/// `s` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_scn_render(s: *mut Scene, frame_time: f32, flags: u32) {
    let Some(h) = host() else { return };
    // SAFETY: caller contract; callbacks may add or remove viewports, so index the live list.
    unsafe {
        (h.frame_count_inc)();
        let mut i = 0;
        while i < (*s).viewports.len() {
            let Some(vp) = at(&(*s).viewports, i) else {
                break;
            };
            i += 1;
            render_viewport(h, s, vp, frame_time, flags);
        }
    }
}

unsafe fn run_callbacks(h: &Host, vp: *mut Viewport, msg: i32) {
    // SAFETY: caller contract.
    unsafe {
        let mut i = 0;
        while i < (*vp).vcbs.len() {
            let Some(cb) = at(&(*vp).vcbs, i) else {
                break;
            };
            (h.viewport_callback)(cb, msg);
            i += 1;
        }
    }
}

unsafe fn render_viewport(h: &Host, s: *mut Scene, vp: *mut Viewport, frame_time: f32, flags: u32) {
    // SAFETY: caller contract.
    unsafe {
        let composite = (*vp).composite;
        let renderer = (*vp).renderer;
        let camera = (*vp).camera;
        let frustum: P = if camera.is_null() {
            std::ptr::null_mut()
        } else {
            camera_frustum(camera.cast()).cast()
        };
        let mut view = ViewportIn {
            visible: (*vp).visible,
            has_composite: !composite.is_null(),
            has_renderer: !renderer.is_null(),
            has_frustum: !frustum.is_null(),
            ..ViewportIn::default()
        };
        let mut plan = scenemgr::plan_viewport(flags, &view);
        if plan & scenemgr::STEP_SKIP != 0 {
            return;
        }
        if plan & scenemgr::STEP_PRE_CALLBACK != 0 {
            run_callbacks(h, vp, 0);
        }
        view.has_world = !(*vp).world.is_null();
        if scenemgr::wants_post_effects(flags, view.has_composite) {
            view.post_effects_active = (h.composite_active)(composite);
        }
        plan = scenemgr::plan_viewport(flags, &view);
        let post = plan & scenemgr::STEP_POST_FLAG != 0;
        let target: P = (&raw mut (*vp).frame_buffer).cast();
        if plan & scenemgr::STEP_POST_CALLBACK != 0 {
            if plan & scenemgr::STEP_RENDER_WORLD != 0 {
                (h.renderer_render)(
                    renderer,
                    frame_time,
                    frustum,
                    (*vp).world,
                    (*vp).settings,
                    target,
                    post,
                    (&raw mut (*vp).rcbs).cast(),
                );
            } else {
                (h.set_frame_buffer)(
                    (*s).graphics,
                    (*vp).frame_buffer,
                    (*vp).pos.as_ptr(),
                    (*vp).size.as_ptr(),
                );
            }
            run_callbacks(h, vp, 1);
            if !(*vp).camera.is_null() {
                let mut j = 0;
                while j < (*vp).guis.len() {
                    let Some(set) = at(&(*vp).guis, j) else {
                        break;
                    };
                    j += 1;
                    if (h.gui_is_3d)(set) {
                        (h.gui_render)(set, frustum);
                    }
                }
            }
        }
        if plan & scenemgr::STEP_POST_EFFECTS != 0 {
            let input = (h.post_texture)(renderer);
            (h.composite_render)(composite, frame_time, frustum, input, target);
        }
        if plan & scenemgr::STEP_GUI_SCREEN != 0 {
            let sets: Vec<P> = (*vp)
                .guis
                .iter()
                .copied()
                .filter(|&g| !(h.gui_is_3d)(g))
                .collect();
            let prios: Vec<i32> = sets.iter().map(|&g| (h.gui_priority)(g)).collect();
            let mut order = vec![0u32; sets.len()];
            let n = scenemgr::gui_order(&prios, &mut order);
            for &o in order.iter().take(n) {
                if let Some(&set) = sets.get(o as usize) {
                    (h.gui_render)(set, std::ptr::null_mut());
                }
            }
        }
    }
}

macro_rules! vp_list {
    ($($add:ident $remove:ident $field:ident;)*) => {$(
        /// # Safety
        /// `vp` live.
        #[unsafe(no_mangle)]
        pub unsafe extern "C" fn $add(vp: *mut Viewport, x: P) {
            // SAFETY: caller contract.
            unsafe {
                (*vp).$field.push(x);
                let v = &mut *vp;
                v.rcbs.sync(&v.rcb_list);
            }
        }

        /// # Safety
        /// `vp` live.
        #[unsafe(no_mangle)]
        pub unsafe extern "C" fn $remove(vp: *mut Viewport, x: P) {
            // SAFETY: caller contract.
            unsafe {
                remove_first(&mut (*vp).$field, x);
                let v = &mut *vp;
                v.rcbs.sync(&v.rcb_list);
            }
        }
    )*};
}

vp_list! {
    ohpl_vp_add_gui ohpl_vp_remove_gui guis;
    ohpl_vp_add_callback ohpl_vp_remove_callback vcbs;
    ohpl_vp_add_renderer_callback ohpl_vp_remove_renderer_callback rcb_list;
}

/// # Safety
/// `vp` live; `w` live or null.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_vp_set_world(vp: *mut Viewport, w: P) {
    // SAFETY: caller contract.
    unsafe { (*vp).set_world(w) }
}

/// # Safety
/// `vp` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_vp_run_callbacks(vp: *mut Viewport, msg: i32) {
    if let Some(h) = host() {
        // SAFETY: caller contract.
        unsafe { run_callbacks(h, vp, msg) }
    }
}
