//! `cProgramComboManager` (stage MT): generated programs and shaders by feature flags plus the
//! hand-made extras, over the combo cache and settings; GPU objects through `MtHost`.

use core::ffi::c_void;
use std::collections::BTreeMap;

use super::mt_host::{KV, MtHost, P};
use crate::graphics::combo::{ComboSettings, FRAGMENT_BIT, VERTEX_BIT};
use crate::render::combo::{ComboCache, Release};

const SHADER_VERTEX: i32 = 0;
const SHADER_FRAGMENT: i32 = 1;

pub struct Pcm {
    host: &'static MtHost,
    graphics: P,
    resources: P,
    name: Vec<u8>,
    modes: usize,
    combos: Vec<i32>,
    cache: ComboCache,
    settings: ComboSettings,
    vars: Vec<Vec<(Vec<u8>, i32)>>,
    extra_shaders: Vec<P>,
    extra_programs: Vec<P>,
}

fn kv_list(map: &BTreeMap<Vec<u8>, Vec<u8>>) -> Vec<KV> {
    map.iter()
        .map(|(k, v)| KV {
            name: k.as_ptr(),
            name_len: k.len(),
            value: v.as_ptr(),
            value_len: v.len(),
        })
        .collect()
}

impl Pcm {
    pub fn new(
        host: &'static MtHost,
        name: &[u8],
        graphics: P,
        resources: P,
        modes: usize,
    ) -> Self {
        Self {
            host,
            graphics,
            resources,
            name: name.to_vec(),
            modes,
            combos: vec![0; modes],
            cache: ComboCache::new(modes),
            settings: ComboSettings::new(name, modes),
            vars: vec![Vec::new(); modes],
            extra_shaders: Vec::new(),
            extra_programs: Vec::new(),
        }
    }

    pub fn set_name(&mut self, name: &[u8]) {
        self.name = name.to_vec();
        self.settings.name = name.to_vec();
    }

    pub fn combination_num(&self, mode: i32) -> i32 {
        usize::try_from(mode)
            .ok()
            .and_then(|m| self.combos.get(m))
            .copied()
            .unwrap_or(0)
    }

    fn error(&self, msg: &str) {
        // SAFETY: the host table outlives the process.
        unsafe { (self.host.error)(msg.as_ptr(), msg.len()) };
    }

    fn program_name(&self, mode: i32, flags: i32) -> Vec<u8> {
        self.settings.program_name(mode, flags)
    }

    fn create_shader_raw(
        &self,
        name: &[u8],
        ty: i32,
        vars: Option<&BTreeMap<Vec<u8>, Vec<u8>>>,
    ) -> P {
        let list = vars.map(kv_list);
        let (ptr, n) = list
            .as_ref()
            .map_or((core::ptr::null(), 0), |l| (l.as_ptr(), l.len()));
        // SAFETY: `list` outlives the call.
        unsafe {
            (self.host.shader_create)(
                self.resources,
                name.as_ptr(),
                name.len(),
                ty,
                ptr,
                n,
                list.is_some(),
            )
        }
    }

    fn destroy_shader_raw(&self, s: P) {
        // SAFETY: `s` is a live shader of the shader manager.
        unsafe { (self.host.shader_destroy)(self.resources, s) };
    }

    /// `CreateShader`
    pub fn create_shader(
        &mut self,
        name: &[u8],
        ty: i32,
        vars: Option<&BTreeMap<Vec<u8>, Vec<u8>>>,
        add_to_list: bool,
    ) -> P {
        let s = self.create_shader_raw(name, ty, vars);
        if s.is_null() {
            self.error(&format!(
                "Could not load material '{}' shader '{}'\n",
                String::from_utf8_lossy(&self.name),
                String::from_utf8_lossy(name)
            ));
            return core::ptr::null_mut();
        }
        if add_to_list {
            self.extra_shaders.push(s);
        }
        s
    }

    /// `CreateProgram`
    pub fn create_program(&mut self, name: &[u8], add_to_list: bool) -> P {
        // SAFETY: the host creates a program of the graphics' low level.
        let p = unsafe { (self.host.prog_create)(self.graphics, name.as_ptr(), name.len()) };
        if add_to_list {
            self.extra_programs.push(p);
        }
        p
    }

    pub fn destroy_shader(&mut self, s: P) {
        if s.is_null() {
            return;
        }
        if let Some(i) = self.extra_shaders.iter().position(|x| *x == s) {
            self.extra_shaders.remove(i);
        }
        self.destroy_shader_raw(s);
    }

    pub fn destroy_program(&mut self, p: P) {
        if let Some(i) = self.extra_programs.iter().position(|x| *x == p) {
            self.extra_programs.remove(i);
        }
        // SAFETY: `p` is a live program; STLFindAndDelete deletes it found or not.
        unsafe { (self.host.prog_delete)(p) };
    }

    fn link(&self, p: P) -> bool {
        // SAFETY: `p` is a live program.
        unsafe { (self.host.prog_link)(p) }
    }

    /// `CreateProgramFromShaders` (names)
    pub fn program_from_names(
        &mut self,
        name: &[u8],
        vtx: &[u8],
        frag: &[u8],
        vars: Option<&BTreeMap<Vec<u8>, Vec<u8>>>,
        add_to_list: bool,
    ) -> P {
        let v = self.create_shader(vtx, SHADER_VERTEX, vars, add_to_list);
        if v.is_null() {
            return core::ptr::null_mut();
        }
        let f = self.create_shader(frag, SHADER_FRAGMENT, vars, add_to_list);
        if f.is_null() {
            self.destroy_shader(v);
            return core::ptr::null_mut();
        }
        let p = self.create_program(name, add_to_list);
        // SAFETY: the program and shaders are live.
        unsafe { (self.host.prog_set_shaders)(p, v, f) };
        if !self.link(p) {
            self.error(&format!(
                "Could not link material '{}' program {}!\n",
                String::from_utf8_lossy(&self.name),
                String::from_utf8_lossy(name)
            ));
            if add_to_list {
                self.destroy_shader(v);
                self.destroy_shader(f);
                self.destroy_program(p);
            } else {
                self.destroy_shader_raw(v);
                self.destroy_shader_raw(f);
                // SAFETY: `p` is live and unlisted.
                unsafe { (self.host.prog_delete)(p) };
            }
            return core::ptr::null_mut();
        }
        if !add_to_list {
            // SAFETY: `p` is live.
            unsafe { (self.host.prog_set_owned)(p, self.resources) };
        }
        p
    }

    /// `CreateProgramFromShaders` (shaders)
    pub fn program_from_shaders(&mut self, name: &[u8], vtx: P, frag: P, add_to_list: bool) -> P {
        let p = self.create_program(name, add_to_list);
        // SAFETY: the program and shaders are live.
        unsafe { (self.host.prog_set_shaders)(p, vtx, frag) };
        if !self.link(p) {
            self.error(&format!(
                "Could not link program '{}' in ProgramComboManager!\n",
                String::from_utf8_lossy(name)
            ));
            if add_to_list {
                self.destroy_program(p);
            } else {
                // SAFETY: `p` is live and unlisted.
                unsafe { (self.host.prog_delete)(p) };
            }
            return core::ptr::null_mut();
        }
        p
    }

    fn shader_for_combo(&mut self, mode: i32, flags: i32, file: &[u8], stage: u32) -> P {
        let Ok(m) = usize::try_from(mode) else {
            return core::ptr::null_mut();
        };
        let bits = self.cache.valid_bits(m, flags as u32, stage);
        if let Some(h) = self.cache.shader_acquire(m, stage, bits) {
            return h as P;
        }
        let vars: BTreeMap<Vec<u8>, Vec<u8>> = self
            .settings
            .shader_vars(mode, flags, stage)
            .into_iter()
            .map(|(k, v)| (k.to_vec(), v.to_vec()))
            .collect();
        let ty = if stage == VERTEX_BIT {
            SHADER_VERTEX
        } else {
            SHADER_FRAGMENT
        };
        let s = self.create_shader(file, ty, Some(&vars), false);
        self.cache.shader_insert(m, stage, bits, s as usize);
        if !s.is_null() {
            // SAFETY: `s` is a live shader.
            unsafe { (self.host.shader_set_user_id)(s, bits) };
        }
        s
    }

    /// `GenerateProgram`
    pub fn generate_program(&mut self, mode: i32, flags: i32) -> P {
        let Ok(m) = usize::try_from(mode) else {
            return core::ptr::null_mut();
        };
        if let Some(h) = self.cache.program_acquire(m, flags as u32) {
            return h as P;
        }
        let name = self.program_name(mode, flags);
        let (vtx_file, frag_file) = self.settings.mode(mode).map_or_else(
            || (Vec::new(), Vec::new()),
            |x| (x.vtx_shader.clone(), x.frag_shader.clone()),
        );
        let v = self.shader_for_combo(mode, flags, &vtx_file, VERTEX_BIT);
        let f = self.shader_for_combo(mode, flags, &frag_file, FRAGMENT_BIT);
        let mut p = core::ptr::null_mut();
        if !v.is_null() && !f.is_null() {
            p = self.program_from_shaders(&name, v, f, false);
        }
        self.cache.program_insert(m, flags as u32, p as usize);
        if !p.is_null() {
            // SAFETY: `p` is a live program.
            unsafe { (self.host.prog_set_user_id)(p, flags as u32) };
            if let Some(list) = self.vars.get(m) {
                for (n, id) in list {
                    // SAFETY: `p` is live; the name outlives the call.
                    unsafe { (self.host.prog_var_id)(p, n.as_ptr(), n.len(), *id) };
                }
            }
        }
        p
    }

    /// `SetupGenerateProgramData`; `features` are (stage mask, variable).
    pub fn setup_mode(
        &mut self,
        mode: i32,
        name: &[u8],
        vtx: &[u8],
        frag: &[u8],
        features: &[(u32, Vec<u8>)],
        defaults: &BTreeMap<Vec<u8>, Vec<u8>>,
    ) {
        let Ok(m) = usize::try_from(mode) else { return };
        let stages: Vec<u32> = features.iter().map(|f| f.0).collect();
        let n = self.cache.setup(m, &stages);
        if let Some(c) = self.combos.get_mut(m) {
            *c = n;
        }
        if let Some(x) = self.settings.mode_mut(mode) {
            x.name = name.to_vec();
            x.vtx_shader = vtx.to_vec();
            x.frag_shader = frag.to_vec();
            x.features = features.to_vec();
            x.defaults = defaults
                .iter()
                .map(|(k, v)| (k.clone(), v.clone()))
                .collect();
        }
    }

    pub fn add_variable_id(&mut self, name: &[u8], id: i32, mode: i32) {
        if let Some(l) = usize::try_from(mode)
            .ok()
            .and_then(|m| self.vars.get_mut(m))
        {
            l.push((name.to_vec(), id));
        }
    }

    fn destroy_generated_shader(&mut self, mode: i32, s: P, vertex: bool) {
        // SAFETY: `s` is a live shader.
        let id = unsafe { (self.host.shader_user_id)(s) };
        let stage = if vertex { VERTEX_BIT } else { FRAGMENT_BIT };
        let Ok(m) = usize::try_from(mode) else { return };
        match self.cache.shader_release(m, stage, id) {
            Release::NotFound => {
                let mut name = (core::ptr::null::<u8>(), 0usize);
                // SAFETY: the thunk writes a pointer and length of the shader's name.
                unsafe { (self.host.shader_name)(s, &mut name.0, &mut name.1) };
                let n = if name.0.is_null() {
                    String::new()
                } else {
                    // SAFETY: the thunk returned `len` readable bytes.
                    String::from_utf8_lossy(unsafe { core::slice::from_raw_parts(name.0, name.1) })
                        .into_owned()
                };
                self.error(&format!(
                    "Could not find and destroy generated shader: '{}' with id {} in combo manager '{}'\n",
                    n,
                    id,
                    String::from_utf8_lossy(&self.name)
                ));
            }
            Release::Kept => {}
            Release::Destroyed(h) => {
                if h != 0 {
                    self.destroy_shader_raw(h as P);
                }
            }
        }
    }

    /// `DestroyGeneratedProgram`
    pub fn destroy_generated_program(&mut self, mode: i32, p: P) {
        // SAFETY: `p` is a live program.
        let flags = unsafe { (self.host.prog_user_id)(p) };
        let Ok(m) = usize::try_from(mode) else { return };
        match self.cache.program_release(m, flags) {
            Release::NotFound => {
                let mut name = (core::ptr::null::<u8>(), 0usize);
                // SAFETY: as above.
                unsafe { (self.host.prog_name)(p, &mut name.0, &mut name.1) };
                let n = if name.0.is_null() {
                    String::new()
                } else {
                    // SAFETY: as above.
                    String::from_utf8_lossy(unsafe { core::slice::from_raw_parts(name.0, name.1) })
                        .into_owned()
                };
                self.error(&format!(
                    "Could not find and destroy generated program: {n}\n"
                ));
            }
            Release::Kept => {}
            Release::Destroyed(h) => {
                let prog = h as P;
                // SAFETY: the cache stored a live program handle.
                let (v, f) = unsafe {
                    (
                        (self.host.prog_shader)(prog, SHADER_VERTEX),
                        (self.host.prog_shader)(prog, SHADER_FRAGMENT),
                    )
                };
                if !prog.is_null() {
                    // SAFETY: the program is destroyed once.
                    unsafe { (self.host.prog_delete)(prog) };
                }
                if !v.is_null() {
                    self.destroy_generated_shader(mode, v, true);
                }
                if !f.is_null() {
                    self.destroy_generated_shader(mode, f, false);
                }
            }
        }
    }

    pub fn destroy_generated_shader_pub(&mut self, mode: i32, s: P, ty: i32) {
        self.destroy_generated_shader(mode, s, ty == SHADER_VERTEX);
    }

    /// `DestroyShadersAndPrograms`; the extras go inside the mode loop as in the C++ (none
    /// are destroyed when the manager has no modes).
    pub fn destroy_shaders_and_programs(&mut self) {
        for m in 0..self.modes {
            while let Some(h) = self.cache.pop_program(m) {
                if h != 0 {
                    // SAFETY: the cache stored live programs.
                    unsafe { (self.host.prog_delete)(h as P) };
                }
            }
            for stage in [VERTEX_BIT, FRAGMENT_BIT] {
                while let Some(h) = self.cache.pop_shader(m, stage) {
                    if h != 0 {
                        self.destroy_shader_raw(h as P);
                    }
                }
            }
            if let Some(l) = self.vars.get_mut(m) {
                l.clear();
            }
            for p in std::mem::take(&mut self.extra_programs) {
                // SAFETY: the extras are live programs.
                unsafe { (self.host.prog_delete)(p) };
            }
            for s in std::mem::take(&mut self.extra_shaders) {
                self.destroy_shader_raw(s);
            }
        }
    }
}

impl Drop for Pcm {
    fn drop(&mut self) {
        self.destroy_shaders_and_programs();
    }
}

type Name<'a> = (*const u8, usize);

/// # Safety
/// `p` has `n` readable bytes (or `n` is 0).
unsafe fn bytes<'a>(p: Name<'a>) -> &'a [u8] {
    if p.0.is_null() || p.1 == 0 {
        &[]
    } else {
        // SAFETY: caller contract.
        unsafe { core::slice::from_raw_parts(p.0, p.1) }
    }
}

/// # Safety
/// `kv` has `n` entries with readable strings (or `n` is 0).
unsafe fn kv_map(kv: *const KV, n: usize) -> BTreeMap<Vec<u8>, Vec<u8>> {
    let mut m = BTreeMap::new();
    if kv.is_null() {
        return m;
    }
    for i in 0..n {
        // SAFETY: caller contract.
        let e = unsafe { &*kv.add(i) };
        // SAFETY: caller contract.
        let (k, v) = unsafe { (bytes((e.name, e.name_len)), bytes((e.value, e.value_len))) };
        m.insert(k.to_vec(), v.to_vec());
    }
    m
}

/// # Safety
/// `h` is a live `Pcm` from `ohpl_pcm_new`.
unsafe fn pcm<'a>(h: *mut c_void) -> Option<&'a mut Pcm> {
    // SAFETY: caller contract.
    unsafe { h.cast::<Pcm>().as_mut() }
}

/// # Safety
/// `host` is a valid table that lives for the process; the strings are readable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_pcm_new(
    host: *const MtHost,
    name: *const u8,
    n: usize,
    graphics: P,
    resources: P,
    modes: usize,
) -> *mut c_void {
    // SAFETY: caller contract.
    let Some(host) = (unsafe { host.as_ref() }) else {
        return core::ptr::null_mut();
    };
    // SAFETY: caller contract.
    let name = unsafe { bytes((name, n)) };
    Box::into_raw(Box::new(Pcm::new(host, name, graphics, resources, modes))).cast()
}

/// # Safety
/// `h` is from `ohpl_pcm_new`, freed once.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_pcm_free(h: *mut c_void) {
    if !h.is_null() {
        // SAFETY: caller contract.
        drop(unsafe { Box::from_raw(h.cast::<Pcm>()) });
    }
}

/// # Safety
/// `h` is live; `name` is readable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_pcm_set_name(h: *mut c_void, name: *const u8, n: usize) {
    // SAFETY: caller contract.
    if let Some(p) = unsafe { pcm(h) } {
        // SAFETY: caller contract.
        p.set_name(unsafe { bytes((name, n)) });
    }
}

/// # Safety
/// `h` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_pcm_combination_num(h: *mut c_void, mode: i32) -> i32 {
    // SAFETY: caller contract.
    unsafe { pcm(h) }.map_or(0, |p| p.combination_num(mode))
}

/// # Safety
/// `h` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_pcm_generate_program(h: *mut c_void, mode: i32, flags: i32) -> P {
    // SAFETY: caller contract.
    unsafe { pcm(h) }.map_or(core::ptr::null_mut(), |p| p.generate_program(mode, flags))
}

/// Feature i: `(stages[i], vars[i])` where `vars` holds one `KV` name each.
///
/// # Safety
/// `h` is live; every pointer is readable for its count.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_pcm_setup_mode(
    h: *mut c_void,
    mode: i32,
    name: *const u8,
    name_n: usize,
    vtx: *const u8,
    vtx_n: usize,
    frag: *const u8,
    frag_n: usize,
    features: *const KV,
    stages: *const u32,
    feature_n: usize,
    defaults: *const KV,
    default_n: usize,
) {
    // SAFETY: caller contract.
    let Some(p) = (unsafe { pcm(h) }) else { return };
    // SAFETY: caller contract.
    unsafe {
        let mut feats = Vec::new();
        if !features.is_null() && !stages.is_null() {
            for i in 0..feature_n {
                let f = &*features.add(i);
                feats.push((*stages.add(i), bytes((f.name, f.name_len)).to_vec()));
            }
        }
        let defs = kv_map(defaults, default_n);
        p.setup_mode(
            mode,
            bytes((name, name_n)),
            bytes((vtx, vtx_n)),
            bytes((frag, frag_n)),
            &feats,
            &defs,
        );
    }
}

/// # Safety
/// `h` is live; `name` is readable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_pcm_add_variable_id(
    h: *mut c_void,
    name: *const u8,
    n: usize,
    id: i32,
    mode: i32,
) {
    // SAFETY: caller contract.
    if let Some(p) = unsafe { pcm(h) } {
        // SAFETY: caller contract.
        p.add_variable_id(unsafe { bytes((name, n)) }, id, mode);
    }
}

/// # Safety
/// `h` and `prog` are live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_pcm_destroy_generated_program(h: *mut c_void, mode: i32, prog: P) {
    // SAFETY: caller contract.
    if let Some(p) = unsafe { pcm(h) } {
        p.destroy_generated_program(mode, prog);
    }
}

/// # Safety
/// `h` and `shader` are live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_pcm_destroy_generated_shader(
    h: *mut c_void,
    mode: i32,
    shader: P,
    ty: i32,
) {
    // SAFETY: caller contract.
    if let Some(p) = unsafe { pcm(h) } {
        p.destroy_generated_shader_pub(mode, shader, ty);
    }
}

/// # Safety
/// `h` is live; `vars` has `n` entries (`has_vars` false: no container).
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_pcm_create_shader(
    h: *mut c_void,
    name: *const u8,
    name_n: usize,
    ty: i32,
    vars: *const KV,
    n: usize,
    has_vars: bool,
    add_to_list: bool,
) -> P {
    // SAFETY: caller contract.
    let Some(p) = (unsafe { pcm(h) }) else {
        return core::ptr::null_mut();
    };
    // SAFETY: caller contract.
    let m = has_vars.then(|| unsafe { kv_map(vars, n) });
    // SAFETY: caller contract.
    p.create_shader(
        unsafe { bytes((name, name_n)) },
        ty,
        m.as_ref(),
        add_to_list,
    )
}

/// # Safety
/// `h` is live; `name` is readable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_pcm_create_program(
    h: *mut c_void,
    name: *const u8,
    n: usize,
    add_to_list: bool,
) -> P {
    // SAFETY: caller contract.
    unsafe { pcm(h) }.map_or(core::ptr::null_mut(), |p| {
        // SAFETY: caller contract.
        p.create_program(unsafe { bytes((name, n)) }, add_to_list)
    })
}

/// # Safety
/// `h` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_pcm_destroy_shader(h: *mut c_void, s: P) {
    // SAFETY: caller contract.
    if let Some(p) = unsafe { pcm(h) } {
        p.destroy_shader(s);
    }
}

/// # Safety
/// `h` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_pcm_destroy_program(h: *mut c_void, prog: P) {
    // SAFETY: caller contract.
    if let Some(p) = unsafe { pcm(h) } {
        p.destroy_program(prog);
    }
}

/// # Safety
/// `h` is live; strings readable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_pcm_program_from_names(
    h: *mut c_void,
    name: *const u8,
    name_n: usize,
    vtx: *const u8,
    vtx_n: usize,
    frag: *const u8,
    frag_n: usize,
    vars: *const KV,
    n: usize,
    has_vars: bool,
    add_to_list: bool,
) -> P {
    // SAFETY: caller contract.
    let Some(p) = (unsafe { pcm(h) }) else {
        return core::ptr::null_mut();
    };
    // SAFETY: caller contract.
    unsafe {
        let m = has_vars.then(|| kv_map(vars, n));
        p.program_from_names(
            bytes((name, name_n)),
            bytes((vtx, vtx_n)),
            bytes((frag, frag_n)),
            m.as_ref(),
            add_to_list,
        )
    }
}

/// # Safety
/// `h` is live; the shaders are live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_pcm_program_from_shaders(
    h: *mut c_void,
    name: *const u8,
    name_n: usize,
    vtx: P,
    frag: P,
    add_to_list: bool,
) -> P {
    // SAFETY: caller contract.
    unsafe { pcm(h) }.map_or(core::ptr::null_mut(), |p| {
        // SAFETY: caller contract.
        p.program_from_shaders(unsafe { bytes((name, name_n)) }, vtx, frag, add_to_list)
    })
}

/// # Safety
/// `h` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_pcm_destroy_all(h: *mut c_void) {
    // SAFETY: caller contract.
    if let Some(p) = unsafe { pcm(h) } {
        p.destroy_shaders_and_programs();
    }
}
