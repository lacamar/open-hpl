//! `cImageManager`: frame bookkeeping and the image load/destroy flows.

use super::h3_ops::{Create, Flow, WStr, c, k, null};
use crate::resmgr;
use std::collections::BTreeMap;
use std::collections::btree_map::Entry;
use std::ffi::c_void;

pub struct Frames {
    pub bitmap: Vec<usize>,
    pub textures: BTreeMap<i32, usize>,
    pub handle: i32,
    pub size: (i32, i32),
}

impl Default for Frames {
    fn default() -> Self {
        Self {
            bitmap: Vec::new(),
            textures: BTreeMap::new(),
            handle: 0,
            size: (512, 512),
        }
    }
}

fn ptr(p: usize) -> *mut c_void {
    std::ptr::with_exposed_provenance_mut(p)
}

fn handle_arg(h: i32) -> i64 {
    i64::from(h)
}

impl Flow<'_> {
    pub fn image_bitmap_frame(&mut self, w: i32, h: i32) -> *mut c_void {
        let handle = self.m().frames.handle;
        let mut cr = Create::new(k::IMG_BITMAP_FRAME);
        cr.i = [w, h, handle, 0];
        let f = self.ops.create(&mut cr);
        self.m().frames.bitmap.push(f.expose_provenance());
        let tf = self.ops.call(c::BMPFRAME_TEX, f, 0) as usize;
        match self.m().frames.textures.entry(handle) {
            Entry::Occupied(_) => {
                let shown = tf as u32 as i32;
                self.ops.err(&[
                    b"Could not add texture frame ",
                    shown.to_string().as_bytes(),
                    b" with handle ",
                    handle.to_string().as_bytes(),
                    b"! Handle already exist!\n",
                ]);
            }
            Entry::Vacant(v) => {
                v.insert(tf);
            }
        }
        self.m().frames.handle = handle.wrapping_add(1);
        f
    }

    fn image_add_bitmap(
        &mut self,
        frame: *mut c_void,
        bmp: *mut c_void,
        path: &[u32],
    ) -> *mut c_void {
        let mut cr = Create::new(k::IMG_ADD_BITMAP);
        cr.ptr = frame;
        cr.ptr2 = bmp;
        cr.path = WStr::of(path);
        self.ops.create(&mut cr)
    }

    pub fn image_add_to_frame(
        &mut self,
        bmp: *mut c_void,
        path: &[u32],
        handle: i32,
    ) -> *mut c_void {
        let mut img = null();
        if self.m().frames.bitmap.is_empty() {
            let (w, h) = self.m().frames.size;
            self.image_bitmap_frame(w, h);
        }
        if handle < 0 {
            let mut found = false;
            for f in self.m().frames.bitmap.clone().into_iter().map(ptr) {
                if self.ops.call(c::BMPFRAME_IS_FULL, f, 0) == 0
                    && self.ops.call(c::BMPFRAME_IS_LOCKED, f, 0) == 0
                {
                    img = self.image_add_bitmap(f, bmp, path);
                    if img.is_null() && self.ops.call(c::BMPFRAME_ADDITIONS, f, 0) > 1 {
                        self.ops.call(c::BMPFRAME_REORGANIZE, f, 0);
                        img = self.image_add_bitmap(f, bmp, path);
                    }
                    if !img.is_null() {
                        found = true;
                        break;
                    }
                }
            }
            if !found {
                let bw = self.ops.call(c::BMP_WIDTH, bmp, 0) as i32;
                let bh = self.ops.call(c::BMP_HEIGHT, bmp, 0) as i32;
                let (w, h) = resmgr::image_frame_size(self.m().frames.size, (bw, bh));
                let f = self.image_bitmap_frame(w, h);
                if !f.is_null() {
                    img = self.image_add_bitmap(f, bmp, path);
                    if img.is_null() {
                        self.ops.log(super::h3_ops::LOG, b"No fit in new frame!\n");
                    }
                }
            }
        } else {
            for f in self.m().frames.bitmap.clone().into_iter().map(ptr) {
                if self.ops.call(c::BMPFRAME_HANDLE, f, 0) == handle_arg(handle) {
                    img = self.image_add_bitmap(f, bmp, path);
                    break;
                }
            }
            if img.is_null() {
                self.ops.err(&[
                    b"Image didn't fit frame ",
                    handle.to_string().as_bytes(),
                    b"!\n",
                ]);
            }
        }
        img
    }

    pub fn image_create_in_frame(
        &mut self,
        name: &[u8],
        handle: i32,
        exts: &[&[u8]],
    ) -> *mut c_void {
        self.begin(name);
        let lk = resmgr::find_with_formats(&mut self.env(), name, exts);
        let mut img = lk.res;
        if img.is_null() && !lk.path.is_empty() {
            let mut cr = Create::new(k::IMAGE);
            cr.path = WStr::of(&lk.path);
            let bmp = self.ops.create(&mut cr);
            if bmp.is_null() {
                self.ops.err(&[
                    b"Imagemanager Couldn't load bitmap '",
                    &super::h3_ops::wide8(&lk.path),
                    b"'\n",
                ]);
                self.end();
                return null();
            }
            img = self.image_add_to_frame(bmp, &lk.path, handle);
            self.ops.call(c::BMP_DELETE, bmp, 0);
            if img.is_null() {
                self.ops
                    .err(&[b"Imagemanager couldn't create image '", name, b"'\n"]);
            } else {
                self.add(img, true, true);
            }
        }
        if img.is_null() {
            self.ops.err(&[b"Couldn't load image '", name, b"'\n"]);
        } else {
            self.inc_user(img);
        }
        self.end();
        img
    }

    pub fn image_from_bitmap(&mut self, bmp: *mut c_void, handle: i32) -> *mut c_void {
        if bmp.is_null() {
            return null();
        }
        let img = self.image_add_to_frame(bmp, &[], handle);
        if !img.is_null() {
            self.add(img, false, true);
            self.inc_user(img);
        }
        img
    }

    pub fn image_custom_frame(&mut self, tex: *mut c_void) -> *mut c_void {
        let handle = self.m().frames.handle;
        let mut cr = Create::new(k::IMG_CUSTOM_FRAME);
        cr.ptr = tex;
        cr.i[0] = handle;
        let f = self.ops.create(&mut cr);
        self.m()
            .frames
            .textures
            .entry(handle)
            .or_insert(f.expose_provenance());
        self.m().frames.handle = handle.wrapping_add(1);
        f
    }

    pub fn image_frame_texture(&self, handle: i32) -> *mut c_void {
        self.m_ref()
            .frames
            .textures
            .get(&handle)
            .map_or(null(), |p| ptr(*p))
    }

    pub fn image_create_frame(&mut self, w: i32, h: i32) -> i32 {
        let f = self.image_bitmap_frame(w, h);
        if f.is_null() {
            return -1;
        }
        self.ops.call(c::BMPFRAME_HANDLE, f, 0) as i32
    }

    pub fn image_set_frame_locked(&mut self, handle: i32, locked: bool) {
        for f in self.m().frames.bitmap.clone().into_iter().map(ptr) {
            if self.ops.call(c::BMPFRAME_HANDLE, f, 0) == handle_arg(handle) {
                self.ops.call(c::BMPFRAME_SET_LOCKED, f, i64::from(locked));
                break;
            }
        }
    }

    pub fn image_flush_all(&mut self) -> i32 {
        let mut n = 0;
        for f in self.m().frames.bitmap.clone().into_iter().map(ptr) {
            if self.ops.call(c::BMPFRAME_FLUSH, f, 0) != 0 {
                n += 1;
            }
        }
        n
    }

    pub fn image_reorganize_all(&mut self) {
        for f in self.m().frames.bitmap.clone().into_iter().map(ptr) {
            self.ops.call(c::BMPFRAME_REORGANIZE, f, 0);
        }
    }

    pub fn image_destroy(&mut self, img: *mut c_void) {
        let frame = ptr(self.ops.call(c::IMG_FRAME_TEX, img, 0) as usize);
        let bmp_frame = ptr(self.ops.call(c::IMG_FRAME_BMP, img, 0) as usize);
        if self.dec_user_is_free(img) {
            self.ops.call(c::FRAME_DEC_PIC, frame, 0);
            if !bmp_frame.is_null() {
                self.ops.call(c::BMPFRAME_DEC_PIC, bmp_frame, 0);
            }
            self.m().remove(img);
            self.ops.destroy(img);
        }
        let after = resmgr::destroy_image_frame(
            self.ops.call(c::FRAME_IS_EMPTY, frame, 0) != 0,
            self.ops.call(c::FRAME_IS_CUSTOM, frame, 0) != 0,
            !bmp_frame.is_null(),
        );
        if after.delete_frame {
            if after.delete_frame_bitmap {
                let want = frame.expose_provenance() as i64;
                let list = self.m_ref().frames.bitmap.clone();
                let pos = list
                    .iter()
                    .position(|&f| self.ops.call(c::BMPFRAME_TEX, ptr(f), 0) == want);
                if let Some(pos) = pos {
                    let f = self.m().frames.bitmap.remove(pos);
                    self.ops.call(c::BMPFRAME_DELETE, ptr(f), 0);
                }
            }
            let h = self.ops.call(c::FRAME_HANDLE, frame, 0) as i32;
            self.m().frames.textures.remove(&h);
            self.ops.call(c::FRAME_DELETE, frame, 0);
        }
    }
}
