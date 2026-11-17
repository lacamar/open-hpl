//! `cGui`, owned here: sets, skins, gfx elements, materials and input dispatch. The C++ `cGui`
//! is an `iUpdateable` shell holding this object's pointer.

use std::collections::BTreeMap;
use std::ffi::c_void;
use std::ptr;
use std::sync::atomic::{AtomicPtr, Ordering};

use super::g1_gfx::{self, Element};
use super::g1_host::{self, ERROR, LOG, WARNING, host};
use super::g1_set::{self, Set};
use super::g1_skin::{self, Skin};
use super::guicore::{self, SkinKind};
use super::set::{self, Msg};
use crate::string;
use crate::types::{Color, Vector2f};

const MATERIAL_COUNT: usize = guicore::MATERIAL_LAST as usize;
const TEXTURE_TYPE_2D: i32 = 1;
const TEXTURE_TYPE_RECT: i32 = 2;

/// `cGui::mpGfxRect`: the shared filled rect, created by the first `Init`.
static GFX_RECT: AtomicPtr<Element> = AtomicPtr::new(ptr::null_mut());

/// The engine's gui (there is one), for Rust callers that have no gui handle.
static CURRENT: AtomicPtr<Gui> = AtomicPtr::new(ptr::null_mut());

pub(crate) fn current() -> *mut Gui {
    CURRENT.load(Ordering::Acquire)
}

pub struct Gui {
    shell: *mut c_void,
    resources: *mut c_void,
    graphics: *mut c_void,
    sound: *mut c_void,
    scene: *mut c_void,
    input: *mut c_void,
    set_in_focus: usize,
    /// `std::multimap`: ordered by name, equal names in insertion order.
    sets: Vec<(Vec<u8>, usize)>,
    skins: BTreeMap<Vec<u8>, *mut Skin>,
    materials: [usize; MATERIAL_COUNT],
    gfx: Vec<*mut Element>,
    to_destroy: Vec<*mut Element>,
}

/// # Safety
/// `g` is null or a live gui. Callers keep the borrow short: nothing here re-enters the gui.
unsafe fn gu<'a>(g: *mut Gui) -> Option<&'a mut Gui> {
    // SAFETY: caller contract.
    unsafe { g.as_mut() }
}

pub(crate) fn shell(g: *mut Gui) -> *mut c_void {
    // SAFETY: `g` is live.
    unsafe { gu(g) }.map_or(ptr::null_mut(), |g| g.shell)
}

pub(crate) fn resources(g: *mut Gui) -> *mut c_void {
    // SAFETY: `g` is live.
    unsafe { gu(g) }.map_or(ptr::null_mut(), |g| g.resources)
}

/// `cGui::GetSkin*FromString`: the index, or `None` (with the original's warning) for an unknown name.
pub(crate) fn skin_index(_g: *mut Gui, kind: SkinKind, name: &[u8]) -> Option<usize> {
    let i = kind.from_name(name);
    if i == kind.from_name(b"") {
        let what = match kind {
            SkinKind::Gfx => "gfx",
            SkinKind::Font => "Font",
            SkinKind::Attribute => "Attribute",
        };
        g1_host::log(
            WARNING,
            &format!(
                "Skin {what} type '{}' does not exist!\n",
                String::from_utf8_lossy(name)
            ),
        );
        return None;
    }
    usize::try_from(i).ok()
}

fn material(g: &Gui, ty: i32) -> *const c_void {
    let slot = guicore::material_slot(u32::try_from(ty).unwrap_or(u32::MAX));
    usize::try_from(slot)
        .ok()
        .and_then(|s| g.materials.get(s))
        .map_or(ptr::null(), |&m| m as *const c_void)
}

fn track(g: &mut Gui, e: *mut Element, add: bool) -> *mut Element {
    if add {
        g.gfx.push(e);
    }
    e
}

fn new_element(g: &Gui) -> Option<&'static mut Element> {
    // SAFETY: fresh allocation from `ohpl_g1_gfx_new`, owned by the gui's lists until freed.
    unsafe { g1_gfx::ohpl_g1_gfx_new(g.resources).as_mut() }
}

pub(crate) fn create_gfx_filled_rect(
    g: *mut Gui,
    color: Color,
    material_type: i32,
    add: bool,
) -> *mut Element {
    // SAFETY: `g` is live.
    let Some(g) = (unsafe { gu(g) }) else {
        return ptr::null_mut();
    };
    let Some(e) = new_element(g) else {
        return ptr::null_mut();
    };
    e.set_color(color);
    e.material = material(g, material_type);
    track(g, ptr::from_mut(e), add)
}

pub(crate) fn create_gfx_image(
    g: *mut Gui,
    file: &[u8],
    material_type: i32,
    color: Color,
    add: bool,
) -> *mut Element {
    // SAFETY: `g` is live.
    let Some(g) = (unsafe { gu(g) }) else {
        return ptr::null_mut();
    };
    let Some(h) = host() else {
        return ptr::null_mut();
    };
    // SAFETY: host contract.
    let image = unsafe { (h.image_create)(g.resources, file.as_ptr(), file.len()) };
    if image == 0 {
        g1_host::log(
            ERROR,
            &format!(
                "Could not load image '{}'!\n",
                String::from_utf8_lossy(file)
            ),
        );
        return ptr::null_mut();
    }
    let Some(e) = new_element(g) else {
        return ptr::null_mut();
    };
    e.set_color(color);
    e.material = material(g, material_type);
    e.add_image(image);
    track(g, ptr::from_mut(e), add)
}

pub(crate) fn create_gfx_image_buffer(
    g: *mut Gui,
    file: &[u8],
    material_type: i32,
    animation: bool,
    color: Color,
    add: bool,
) -> *mut Element {
    // SAFETY: `g` is live.
    let Some(g) = (unsafe { gu(g) }) else {
        return ptr::null_mut();
    };
    let Some(h) = host() else {
        return ptr::null_mut();
    };
    let name = string::set_file_ext(file, b"");
    let ext = string::file_ext(file);
    let mut images = Vec::new();
    for n in 0.. {
        let num = if n <= 9 {
            format!("0{n}")
        } else {
            n.to_string()
        };
        let mut path = name.clone();
        path.extend_from_slice(num.as_bytes());
        path.push(b'.');
        path.extend_from_slice(ext);
        // SAFETY: host contract.
        if !unsafe { (h.file_found)(g.resources, path.as_ptr(), path.len()) } {
            break;
        }
        // SAFETY: host contract.
        images.push(unsafe { (h.image_create)(g.resources, path.as_ptr(), path.len()) });
    }
    if images.is_empty() {
        g1_host::log(
            ERROR,
            &format!(
                "Could not load any images with '{}' as base!\n",
                String::from_utf8_lossy(file)
            ),
        );
        return ptr::null_mut();
    }
    let Some(e) = new_element(g) else {
        return ptr::null_mut();
    };
    e.set_color(color);
    e.material = material(g, material_type);
    for (i, &image) in images.iter().enumerate() {
        if i == 0 {
            e.add_image(image);
        }
        e.add_image_to_buffer(image);
    }
    let p = track(g, ptr::from_mut(e), add);
    if animation {
        // SAFETY: just created, owned by the lists.
        let a = unsafe { g1_gfx::ohpl_g1_gfx_create_animation(p) };
        for i in 0..images.len() {
            g1_gfx::ohpl_g1_anim_add_frame(a, i32::try_from(i).unwrap_or(0));
        }
    }
    p
}

pub(crate) fn create_gfx_texture_file(
    g: *mut Gui,
    file: &[u8],
    material_type: i32,
    texture_type: i32,
    color: Color,
    mip_maps: bool,
    add: bool,
) -> *mut Element {
    if texture_type != TEXTURE_TYPE_2D && texture_type != TEXTURE_TYPE_RECT {
        g1_host::log(
            ERROR,
            &format!(
                "Texture {} could not be loaded because texture type param was not valid!\n",
                String::from_utf8_lossy(file)
            ),
        );
        return ptr::null_mut();
    }
    // SAFETY: `g` is live.
    let Some(res) = (unsafe { gu(g) }).map(|g| g.resources) else {
        return ptr::null_mut();
    };
    let Some(h) = host() else {
        return ptr::null_mut();
    };
    // SAFETY: host contract.
    let tex =
        unsafe { (h.texture_create_2d)(res, file.as_ptr(), file.len(), mip_maps, texture_type) };
    if tex == 0 {
        g1_host::log(
            ERROR,
            &format!(
                "Could not load texture '{}'!\n",
                String::from_utf8_lossy(file)
            ),
        );
        return ptr::null_mut();
    }
    create_gfx_texture(
        g,
        tex,
        true,
        material_type,
        color,
        add,
        [0.0, 0.0],
        [1.0, 1.0],
    )
}

#[allow(clippy::too_many_arguments)]
pub(crate) fn create_gfx_texture(
    g: *mut Gui,
    texture: usize,
    auto_destroy: bool,
    material_type: i32,
    color: Color,
    add: bool,
    start: [f32; 2],
    end: [f32; 2],
) -> *mut Element {
    // SAFETY: `g` is live.
    let Some(g) = (unsafe { gu(g) }) else {
        return ptr::null_mut();
    };
    let Some(e) = new_element(g) else {
        return ptr::null_mut();
    };
    // SAFETY: `texture` is a live iTexture.
    if host().is_some_and(|h| unsafe { (h.texture_render_target)(texture) }) {
        e.set_flip(true);
    }
    e.set_color(color);
    e.material = material(g, material_type);
    e.add_texture(texture, start, end);
    // SAFETY: `e` is the live element created above.
    unsafe { g1_gfx::ohpl_g1_gfx_set_destroy_texture(ptr::from_mut(e), auto_destroy) };
    track(g, ptr::from_mut(e), add)
}

pub(crate) fn destroy_gfx(g: *mut Gui, e: *mut Element) {
    // SAFETY: `g` is live.
    let Some(g) = (unsafe { gu(g) }) else { return };
    if let Some(i) = g.gfx.iter().position(|&x| x == e) {
        g.gfx.remove(i);
        g.to_destroy.push(e);
    }
}

fn free_all(list: &mut Vec<*mut Element>) {
    for e in list.drain(..) {
        // SAFETY: the gui owns every listed element, freed once here.
        unsafe { g1_gfx::ohpl_g1_gfx_free(e) };
    }
}

fn set_alive(g: *mut Gui, s: usize) -> bool {
    // SAFETY: `g` is live.
    unsafe { gu(g) }.is_some_and(|g| g.sets.iter().any(|(_, x)| *x == s))
}

fn snapshot_sets(g: *mut Gui) -> Vec<usize> {
    // SAFETY: `g` is live.
    unsafe { gu(g) }.map_or_else(Vec::new, |g| g.sets.iter().map(|(_, s)| *s).collect())
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_g1_gui_new(shell: *mut c_void) -> *mut Gui {
    Box::into_raw(Box::new(Gui {
        shell,
        resources: ptr::null_mut(),
        graphics: ptr::null_mut(),
        sound: ptr::null_mut(),
        scene: ptr::null_mut(),
        input: ptr::null_mut(),
        set_in_focus: 0,
        sets: Vec::new(),
        skins: BTreeMap::new(),
        materials: [0; MATERIAL_COUNT],
        gfx: Vec::new(),
        to_destroy: Vec::new(),
    }))
}

/// `cGui::~cGui`.
///
/// # Safety
/// `g` is from `ohpl_g1_gui_new`, freed once.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g1_gui_free(g: *mut Gui) {
    if g.is_null() {
        return;
    }
    g1_host::log(LOG, "Exiting Gui Module\n");
    g1_host::log(
        LOG,
        "--------------------------------------------------------\n",
    );
    g1_host::log(LOG, " Deleting all sets\n");
    // SAFETY: caller contract; borrows below are short and the host calls do not reach the gui.
    let sets = unsafe { gu(g) }.map_or_else(Vec::new, |g| std::mem::take(&mut g.sets));
    for (_, s) in sets {
        // SAFETY: the gui owned the set.
        unsafe { g1_set::free(s as *mut Set) };
    }
    g1_host::log(LOG, " Deleting all skins\n");
    let skins = unsafe { gu(g) }.map_or_else(BTreeMap::new, |g| std::mem::take(&mut g.skins));
    for (_, s) in skins {
        // SAFETY: the gui owned the skin; its drop reaches back through `destroy_gfx`.
        drop(unsafe { Box::from_raw(s) });
    }
    g1_host::log(LOG, " Deleting all gfx elements\n");
    // SAFETY: caller contract.
    let mut gui = unsafe { Box::from_raw(g) };
    free_all(&mut gui.gfx);
    free_all(&mut gui.to_destroy);
    g1_host::log(LOG, " Deleting all materials\n");
    if let Some(h) = host() {
        for &m in &gui.materials {
            if m != 0 {
                // SAFETY: created by `material_new`.
                unsafe { (h.material_delete)(m) };
            }
        }
    }
    g1_host::log(
        LOG,
        "--------------------------------------------------------\n\n",
    );
}

/// # Safety
/// `g` is live; the engine pointers outlive it.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g1_gui_init(
    g: *mut Gui,
    resources: *mut c_void,
    graphics: *mut c_void,
    sound: *mut c_void,
    scene: *mut c_void,
    input: *mut c_void,
) {
    // SAFETY: caller contract.
    let Some(gui) = (unsafe { gu(g) }) else {
        return;
    };
    gui.resources = resources;
    gui.graphics = graphics;
    gui.sound = sound;
    gui.scene = scene;
    gui.input = input;
    CURRENT.store(g, Ordering::Release);
    if let Some(h) = host() {
        for (i, m) in gui.materials.iter_mut().enumerate() {
            // SAFETY: host contract.
            *m = unsafe { (h.material_new)(i32::try_from(i).unwrap_or(0), graphics) };
        }
    }
    if GFX_RECT.load(Ordering::Acquire).is_null() {
        let one = Color {
            r: 1.0,
            g: 1.0,
            b: 1.0,
            a: 1.0,
        };
        GFX_RECT.store(create_gfx_filled_rect(g, one, 1, true), Ordering::Release);
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_g1_gfx_rect() -> *mut Element {
    GFX_RECT.load(Ordering::Acquire)
}

/// `cGui::Update`.
///
/// # Safety
/// `g` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g1_gui_update(g: *mut Gui, dt: f32) {
    for s in snapshot_sets(g) {
        // A set destroyed by an earlier update is skipped.
        if set_alive(g, s) && g1_set::is_active(s as *mut Set) {
            g1_set::update(s as *mut Set, dt);
        }
    }
    // SAFETY: caller contract.
    let gfx = unsafe { gu(g) }.map_or_else(Vec::new, |g| g.gfx.clone());
    for e in gfx {
        // SAFETY: owned by the gui until `OnPostBufferSwap`.
        unsafe { g1_gfx::ohpl_g1_gfx_update(e, dt) };
    }
}

/// # Safety
/// `g` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g1_gui_on_draw(g: *mut Gui, dt: f32) {
    for s in snapshot_sets(g) {
        if set_alive(g, s) {
            g1_set::draw_all(s as *mut Set, dt);
        }
    }
}

/// # Safety
/// `g` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g1_gui_on_post_buffer_swap(g: *mut Gui) {
    for s in snapshot_sets(g) {
        if set_alive(g, s) {
            // SAFETY: the set is live.
            unsafe { g1_set::ohpl_g1_set_clear_render_objects(s as *mut Set) };
        }
    }
    // SAFETY: caller contract.
    if let Some(g) = unsafe { gu(g) } {
        free_all(&mut g.to_destroy);
    }
}

/// # Safety
/// `g` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g1_gui_material(g: *mut Gui, ty: i32) -> *const c_void {
    // SAFETY: caller contract.
    unsafe { gu(g) }.map_or(ptr::null(), |g| material(g, ty))
}

/// `cGui::CreateSkin`; `file` is the path as written, `wide_path` its `cFileSearcher` result is
/// looked up here.
///
/// # Safety
/// `g` is live; `file` readable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g1_gui_create_skin(
    g: *mut Gui,
    file: *const u8,
    len: usize,
) -> *mut Skin {
    // SAFETY: caller contract.
    let file = unsafe { std::slice::from_raw_parts(file, len) };
    let name = string::set_file_ext(string::file_name(file), b"");
    // SAFETY: caller contract.
    let Some(gui) = (unsafe { gu(g) }) else {
        return ptr::null_mut();
    };
    if let Some(&s) = gui.skins.get(&name) {
        return s;
    }
    let res = gui.resources;
    let Some(h) = host() else {
        return ptr::null_mut();
    };
    let mut path: Vec<u32> = Vec::new();
    unsafe extern "C" fn sink(ctx: *mut c_void, p: *const u32, n: usize) {
        // SAFETY: `ctx` is the Vec below; `p` holds `n` units.
        if let Some(v) = unsafe { ctx.cast::<Vec<u32>>().as_mut() } {
            v.extend_from_slice(unsafe { std::slice::from_raw_parts(p, n) });
        }
    }
    // SAFETY: host contract.
    unsafe {
        (h.find_file)(
            res,
            file.as_ptr(),
            file.len(),
            sink,
            ptr::from_mut(&mut path).cast(),
        );
    }
    let skin = g1_skin::create(g, &name, &path);
    if skin.is_null() {
        return skin;
    }
    // SAFETY: caller contract.
    if let Some(gui) = unsafe { gu(g) } {
        gui.skins.entry(name).or_insert(skin);
    }
    skin
}

/// `cGui::GetSkin*FromString`.
///
/// # Safety
/// `g` is live; `name` readable for `len` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g1_gui_skin_from_name(
    g: *mut Gui,
    kind: i32,
    name: *const u8,
    len: usize,
) -> u32 {
    let k = match kind {
        0 => SkinKind::Font,
        1 => SkinKind::Attribute,
        _ => SkinKind::Gfx,
    };
    // SAFETY: caller contract.
    let name = unsafe { std::slice::from_raw_parts(name, len) };
    skin_index(g, k, name).map_or_else(|| k.from_name(b""), |i| u32::try_from(i).unwrap_or(0))
}

/// `cGui::CreateSet`.
///
/// # Safety
/// `g` is live; `name` readable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g1_gui_create_set(
    g: *mut Gui,
    name: *const u8,
    len: usize,
    skin: *mut Skin,
) -> usize {
    // SAFETY: caller contract.
    let name = unsafe { std::slice::from_raw_parts(name, len) };
    // SAFETY: caller contract.
    let Some(gui) = (unsafe { gu(g) }) else {
        return 0;
    };
    let (shell, res, gfx, snd, scene) =
        (gui.shell, gui.resources, gui.graphics, gui.sound, gui.scene);
    // SAFETY: the set constructor may call back into this gui, so no borrow is held.
    let set = unsafe { g1_set::create(g, shell, name, skin, res, gfx, snd, scene) } as usize;
    // SAFETY: caller contract.
    if let Some(gui) = unsafe { gu(g) } {
        let at = gui.sets.partition_point(|(k, _)| k.as_slice() <= name);
        gui.sets.insert(at, (name.to_vec(), set));
    }
    set
}

/// # Safety
/// `g` is live; `name` readable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g1_gui_set_from_name(
    g: *mut Gui,
    name: *const u8,
    len: usize,
) -> usize {
    // SAFETY: caller contract.
    let name = unsafe { std::slice::from_raw_parts(name, len) };
    unsafe { gu(g) }
        .and_then(|g| {
            g.sets
                .iter()
                .find(|(k, _)| k.as_slice() == name)
                .map(|(_, s)| *s)
        })
        .unwrap_or(0)
}

/// # Safety
/// `g` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g1_gui_set_focus(g: *mut Gui, set: usize) {
    if let Some(g) = unsafe { gu(g) } {
        g.set_in_focus = set;
    }
}

/// # Safety
/// `g` is live; `name` readable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g1_gui_set_focus_by_name(g: *mut Gui, name: *const u8, len: usize) {
    // SAFETY: caller contract.
    let set = unsafe { ohpl_g1_gui_set_from_name(g, name, len) };
    if set != 0 {
        // SAFETY: caller contract.
        unsafe { ohpl_g1_gui_set_focus(g, set) };
    }
}

/// # Safety
/// `g` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g1_gui_focused_set(g: *mut Gui) -> usize {
    unsafe { gu(g) }.map_or(0, |g| g.set_in_focus)
}

/// # Safety
/// `g` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g1_gui_destroy_set(g: *mut Gui, set: usize) {
    if set == 0 {
        return;
    }
    // SAFETY: caller contract.
    let found = unsafe { gu(g) }.is_some_and(|g| {
        g.sets
            .iter()
            .position(|(_, s)| *s == set)
            .is_some_and(|i| g.sets.remove(i).1 == set)
    });
    if found {
        // SAFETY: the gui owned the set and no borrow is held.
        unsafe { g1_set::free(set as *mut Set) };
    }
}

/// # Safety
/// `g` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g1_gui_destroy_gfx(g: *mut Gui, e: *mut Element) {
    destroy_gfx(g, e);
}

/// # Safety
/// `g` is live; `color` readable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g1_gui_create_gfx_filled_rect(
    g: *mut Gui,
    color: *const Color,
    material_type: i32,
    add: bool,
) -> *mut Element {
    // SAFETY: caller contract.
    let c = unsafe { color.as_ref() }.copied().unwrap_or_default();
    create_gfx_filled_rect(g, c, material_type, add)
}

/// # Safety
/// As above; `file` readable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g1_gui_create_gfx_image(
    g: *mut Gui,
    file: *const u8,
    len: usize,
    material_type: i32,
    color: *const Color,
    add: bool,
) -> *mut Element {
    // SAFETY: caller contract.
    let (f, c) = unsafe {
        (
            std::slice::from_raw_parts(file, len),
            color.as_ref().copied().unwrap_or_default(),
        )
    };
    create_gfx_image(g, f, material_type, c, add)
}

/// # Safety
/// As above.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g1_gui_create_gfx_image_buffer(
    g: *mut Gui,
    file: *const u8,
    len: usize,
    material_type: i32,
    animation: bool,
    color: *const Color,
    add: bool,
) -> *mut Element {
    // SAFETY: caller contract.
    let (f, c) = unsafe {
        (
            std::slice::from_raw_parts(file, len),
            color.as_ref().copied().unwrap_or_default(),
        )
    };
    create_gfx_image_buffer(g, f, material_type, animation, c, add)
}

/// # Safety
/// As above.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g1_gui_create_gfx_texture_file(
    g: *mut Gui,
    file: *const u8,
    len: usize,
    material_type: i32,
    texture_type: i32,
    color: *const Color,
    mip_maps: bool,
    add: bool,
) -> *mut Element {
    // SAFETY: caller contract.
    let (f, c) = unsafe {
        (
            std::slice::from_raw_parts(file, len),
            color.as_ref().copied().unwrap_or_default(),
        )
    };
    create_gfx_texture_file(g, f, material_type, texture_type, c, mip_maps, add)
}

/// # Safety
/// As above; `start` and `end` readable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g1_gui_create_gfx_texture(
    g: *mut Gui,
    texture: usize,
    auto_destroy: bool,
    material_type: i32,
    color: *const Color,
    add: bool,
    start: *const Vector2f,
    end: *const Vector2f,
) -> *mut Element {
    // SAFETY: caller contract.
    let (c, s, e) = unsafe {
        (
            color.as_ref().copied().unwrap_or_default(),
            start.as_ref().copied().unwrap_or_default(),
            end.as_ref().copied().unwrap_or_default(),
        )
    };
    create_gfx_texture(
        g,
        texture,
        auto_destroy,
        material_type,
        c,
        add,
        [s.x, s.y],
        [e.x, e.y],
    )
}

fn send(g: *mut Gui, msg: i32, data: &Msg) -> bool {
    // SAFETY: caller contract.
    let Some(set) = (unsafe { gu(g) }).map(|g| g.set_in_focus) else {
        return false;
    };
    if set == 0 {
        return false;
    }
    g1_set::send_message(set as *mut Set, msg, data)
}

/// `cGui::SendMousePos`.
///
/// # Safety
/// `g` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g1_gui_send_mouse_pos(
    g: *mut Gui,
    px: i32,
    py: i32,
    rx: i32,
    ry: i32,
) -> bool {
    // SAFETY: caller contract.
    let Some((set, graphics)) = (unsafe { gu(g) }).map(|g| (g.set_in_focus, g.graphics)) else {
        return false;
    };
    let Some(h) = host() else { return false };
    if set == 0 {
        return false;
    }
    let mut screen = [0.0f32; 2];
    // SAFETY: host contract.
    unsafe { (h.screen_size)(graphics, &mut screen) };
    let vs = g1_set::virtual_rect(set as *mut Set);
    let pos = [
        px as f32 / screen[0] * vs[0] - vs[2],
        py as f32 / screen[1] * vs[1] - vs[3],
    ];
    let rel = [rx as f32 / screen[0] * vs[0], ry as f32 / screen[1] * vs[1]];
    let mut m = Msg::at(pos);
    m.set_rel(rel);
    send(g, set::MSG_MOUSE_MOVE, &m)
}

/// `cGui::SendMouse{ClickDown,ClickUp,DoubleClick}`: `msg` is the `eGuiMessage`.
///
/// # Safety
/// `g` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g1_gui_send_mouse_button(
    g: *mut Gui,
    msg: i32,
    button: i32,
    modifiers: i32,
) -> bool {
    let mut m = Msg::with_val(button);
    m.set_modifier(modifiers);
    send(g, msg, &m)
}

/// Key press and release: `key_press` is a `cKeyPress` (12 bytes).
///
/// # Safety
/// `g` is live; `key_press` readable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g1_gui_send_key(g: *mut Gui, msg: i32, key_press: *const u8) -> bool {
    let mut m = Msg::zero();
    // SAFETY: caller contract.
    m.set_key_press(unsafe { std::slice::from_raw_parts(key_press, 12) });
    send(g, msg, &m)
}

/// # Safety
/// `g` is live; `input` is a `cGamepadInputData` (16 bytes).
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g1_gui_send_gamepad(g: *mut Gui, input: *const u8) -> bool {
    let mut m = Msg::zero();
    // SAFETY: caller contract.
    m.set_gamepad(unsafe { std::slice::from_raw_parts(input, 16) });
    send(g, set::MSG_GAMEPAD_INPUT, &m)
}

/// UI arrow and button messages: `value` goes in `mlVal`.
///
/// # Safety
/// `g` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g1_gui_send_value(g: *mut Gui, msg: i32, value: i32) -> bool {
    send(g, msg, &Msg::with_val(value))
}

/// # Safety
/// `g` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g1_gui_resources(g: *mut Gui) -> *mut c_void {
    resources(g)
}

/// # Safety
/// `g` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g1_gui_input(g: *mut Gui) -> *mut c_void {
    // SAFETY: caller contract.
    unsafe { gu(g) }.map_or(ptr::null_mut(), |g| g.input)
}

pub(crate) fn focus_set(g: *mut Gui, set: usize) {
    // SAFETY: `g` is live.
    if let Some(g) = unsafe { gu(g) } {
        g.set_in_focus = set;
    }
}

pub(crate) fn focused_set(g: *mut Gui) -> usize {
    // SAFETY: `g` is live.
    unsafe { gu(g) }.map_or(0, |g| g.set_in_focus)
}
