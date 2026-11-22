//! `cImageManager`: frame bookkeeping and the image load/destroy flows.

use super::h3_frames::FrameApi;
use super::h3_ops::{Create, Flow, WStr, k, null};
use crate::resmgr;
use std::collections::btree_map::Entry;
use std::ffi::c_void;

fn ptr(p: usize) -> *mut c_void {
    std::ptr::with_exposed_provenance_mut(p)
}

impl Flow<'_> {
    fn fr<'b>(&self) -> &'b dyn FrameApi {
        // SAFETY: the backend box lives in the manager, which outlives the flow, and is only
        // replaced between calls.
        unsafe { &*std::ptr::from_ref::<dyn FrameApi>(&*self.m_ref().frames.api) }
    }

    pub fn image_bitmap_frame(&mut self, w: i32, h: i32) -> *mut c_void {
        let handle = self.m().frames.handle;
        let f = self.fr().new_bitmap_frame(self.ops, w, h, handle);
        self.m().frames.bitmap.push(f.expose_provenance());
        let tf = self.fr().bmp_tex(self.ops, f);
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
        self.fr().add_bitmap(self.ops, frame, bmp, path)
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
                if !self.fr().bmp_is_full(self.ops, f) && !self.fr().bmp_is_locked(self.ops, f) {
                    img = self.image_add_bitmap(f, bmp, path);
                    if img.is_null() && self.fr().bmp_additions(self.ops, f) > 1 {
                        self.fr().bmp_reorganize(self.ops, f);
                        img = self.image_add_bitmap(f, bmp, path);
                    }
                    if !img.is_null() {
                        found = true;
                        break;
                    }
                }
            }
            if !found {
                let bsize = self.fr().bitmap_size(self.ops, bmp);
                let (w, h) = resmgr::image_frame_size(self.m().frames.size, bsize);
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
                if self.fr().bmp_handle(self.ops, f) == handle {
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
            self.fr().bitmap_delete(self.ops, bmp);
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
        let f = self.fr().new_custom_frame(self.ops, tex, handle);
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
        self.fr().bmp_handle(self.ops, f)
    }

    pub fn image_set_frame_locked(&mut self, handle: i32, locked: bool) {
        for f in self.m().frames.bitmap.clone().into_iter().map(ptr) {
            if self.fr().bmp_handle(self.ops, f) == handle {
                self.fr().bmp_set_locked(self.ops, f, locked);
                break;
            }
        }
    }

    pub fn image_flush_all(&mut self) -> i32 {
        let mut n = 0;
        for f in self.m().frames.bitmap.clone().into_iter().map(ptr) {
            if self.fr().bmp_flush(self.ops, f) {
                n += 1;
            }
        }
        n
    }

    pub fn image_reorganize_all(&mut self) {
        for f in self.m().frames.bitmap.clone().into_iter().map(ptr) {
            self.fr().bmp_reorganize(self.ops, f);
        }
    }

    pub fn image_destroy(&mut self, img: *mut c_void) {
        let frame = ptr(self.fr().img_tex(self.ops, img));
        let bmp_frame = ptr(self.fr().img_bmp(self.ops, img));
        if self.dec_user_is_free(img) {
            self.fr().tex_dec_pic(self.ops, frame);
            if !bmp_frame.is_null() {
                self.fr().bmp_dec_pic(self.ops, bmp_frame);
            }
            self.m().remove(img);
            self.ops.destroy(img);
        }
        let after = resmgr::destroy_image_frame(
            self.fr().tex_is_empty(self.ops, frame),
            self.fr().tex_is_custom(self.ops, frame),
            !bmp_frame.is_null(),
        );
        if after.delete_frame {
            if after.delete_frame_bitmap {
                let want = frame.expose_provenance();
                let list = self.m_ref().frames.bitmap.clone();
                let pos = list
                    .iter()
                    .position(|&f| self.fr().bmp_tex(self.ops, ptr(f)) == want);
                if let Some(pos) = pos {
                    let f = self.m().frames.bitmap.remove(pos);
                    self.fr().bmp_delete(self.ops, ptr(f));
                }
            }
            let h = self.fr().tex_handle(self.ops, frame);
            self.m().frames.textures.remove(&h);
            self.fr().tex_delete(self.ops, frame);
        }
    }
}
