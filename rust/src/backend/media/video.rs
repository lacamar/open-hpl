//! `cVideoStreamTheora`: the stream state machine over the ogg/theora shim
//! (`cshim/theora_shim.c`) and the frame conversion from `crate::imageload::theora`.

use super::cfile::{self, File};
use super::log::{Level, Sink};
use crate::imageload::theora::{self, Planes, Tables, TablesRef};
use std::sync::Arc;

#[repr(C)]
struct Th {
    _private: [u8; 0],
}

#[repr(C)]
struct RawYuv {
    y: *const u8,
    u: *const u8,
    v: *const u8,
    y_len: usize,
    u_len: usize,
    v_len: usize,
    y_stride: i32,
    uv_stride: i32,
}

unsafe extern "C" {
    fn ohpl_th_new() -> *mut Th;
    fn ohpl_th_free(t: *mut Th, video_loaded: i32);
    fn ohpl_th_pageout(t: *mut Th) -> i32;
    fn ohpl_th_page_bos(t: *mut Th) -> i32;
    fn ohpl_th_queue_page(t: *mut Th) -> i32;
    fn ohpl_th_packetout(t: *mut Th) -> i32;
    fn ohpl_th_decode_header_packet(t: *mut Th) -> i32;
    fn ohpl_th_fill(t: *mut Th, f: *mut File, size: i32) -> i32;
    fn ohpl_th_test_begin(t: *mut Th);
    fn ohpl_th_test_theora_header(t: *mut Th) -> i32;
    fn ohpl_th_test_header_bit(t: *mut Th) -> i32;
    fn ohpl_th_test_adopt(t: *mut Th);
    fn ohpl_th_test_clear(t: *mut Th);
    fn ohpl_th_decode_packet(t: *mut Th, time: *mut f64) -> i32;
    fn ohpl_th_decode_init(t: *mut Th);
    fn ohpl_th_state_clear(t: *mut Th);
    fn ohpl_th_info_clear(t: *mut Th);
    fn ohpl_th_comment_clear(t: *mut Th);
    fn ohpl_th_stream_clear(t: *mut Th);
    fn ohpl_th_sync_reset(t: *mut Th);
    fn ohpl_th_drain_packets(t: *mut Th);
    fn ohpl_th_drain_pages(t: *mut Th);
    fn ohpl_th_dims(t: *mut Th, w: *mut i32, h: *mut i32, ox: *mut u32, oy: *mut u32);
    fn ohpl_th_yuv(t: *mut Th, out: *mut RawYuv);
}

pub struct Loader {
    pub tables: Arc<Tables>,
}

impl Default for Loader {
    fn default() -> Self {
        Self {
            tables: Arc::new(theora::build_tables()),
        }
    }
}

impl Loader {
    pub fn new() -> Self {
        Self::default()
    }
}

pub struct Video {
    th: *mut Th,
    file: *mut File,
    path: Vec<u8>,
    tables: Arc<Tables>,
    looping: bool,
    paused: bool,
    playing: bool,
    time: f32,
    frame: Vec<u8>,
    videobuf_time: f64,
    loaded: bool,
    frame_ready: bool,
    buffer_size: i32,
    width: i32,
    height: i32,
}

/// What `%s` printed for a `wchar_t*` path: its bytes up to the first zero byte.
fn wide_as_c_str(path: &[u32]) -> Vec<u8> {
    path.iter()
        .flat_map(|c| c.to_le_bytes())
        .take_while(|&b| b != 0)
        .collect()
}

impl Video {
    pub fn new(loader: &Loader) -> Option<Self> {
        // SAFETY: allocates and initialises the decoder state.
        let th = unsafe { ohpl_th_new() };
        if th.is_null() {
            return None;
        }
        Some(Self {
            th,
            file: core::ptr::null_mut(),
            path: Vec::new(),
            tables: Arc::clone(&loader.tables),
            looping: false,
            paused: false,
            playing: false,
            time: 0.0,
            frame: Vec::new(),
            videobuf_time: 0.0,
            loaded: false,
            frame_ready: false,
            buffer_size: 4096,
            width: 0,
            height: 0,
        })
    }

    pub fn size(&self) -> (i32, i32) {
        (self.width, self.height)
    }

    pub fn is_paused(&self) -> bool {
        self.paused
    }

    pub fn is_looping(&self) -> bool {
        self.looping
    }

    fn fill(&mut self, log: Sink) -> i32 {
        // SAFETY: `th` is live and `file` is open while a stream is loaded.
        let n = unsafe { ohpl_th_fill(self.th, self.file, self.buffer_size) };
        if n < 0 {
            log(
                Level::Warning,
                b"Still pages in buffer do not read another one! Go tell the programmer he messed up!\n",
            );
            return 0;
        }
        n
    }

    fn queue_page(&self) {
        if self.loaded {
            // SAFETY: `th` is live; a refused page is dropped, as before.
            unsafe { ohpl_th_queue_page(self.th) };
        }
    }

    /// `path` is the wide path (kept for reopening); `utf8` the same for `fopen`.
    pub fn load_from_file(&mut self, path: &[u32], utf8: &[u8], log: Sink) -> bool {
        self.file = cfile::open(utf8, c"rb");
        if self.file.is_null() {
            return false;
        }
        self.path = utf8.to_vec();
        self.loaded = false;
        if !self.get_headers(path, log) {
            return false;
        }
        self.init_decoders();
        true
    }

    fn get_headers(&mut self, wide: &[u32], log: Sink) -> bool {
        let shown = wide_as_c_str(wide);
        let msg = |a: &[u8], b: &[u8]| [a, &shown, b].concat();
        let mut header_packets = 0;
        let mut found = false;
        while !found {
            if self.fill(log) == 0 {
                log(
                    Level::Error,
                    &msg(b"Found no header in video file '", b"'\n"),
                );
                return false;
            }
            // SAFETY (all calls below): `th` is live.
            while unsafe { ohpl_th_pageout(self.th) } > 0 {
                if unsafe { ohpl_th_page_bos(self.th) } == 0 {
                    self.queue_page();
                    continue;
                }
                unsafe { ohpl_th_test_begin(self.th) };
                if !self.loaded && unsafe { ohpl_th_test_theora_header(self.th) } >= 0 {
                    unsafe { ohpl_th_test_adopt(self.th) };
                    header_packets = 1;
                    self.loaded = true;
                } else {
                    unsafe { ohpl_th_test_clear(self.th) };
                }
                if self.loaded {
                    found = true;
                }
            }
        }

        while header_packets != 0 && header_packets < 3 {
            while header_packets != 0 && header_packets < 3 {
                // SAFETY: `th` is live.
                let ret = unsafe { ohpl_th_packetout(self.th) };
                if ret == 0 {
                    break;
                }
                if ret < 0 {
                    log(
                        Level::Error,
                        &msg(
                            b"Error parsing Theora stream headers in '",
                            b"'; corrupt stream?\n",
                        ),
                    );
                    return false;
                }
                // SAFETY: `th` is live.
                if unsafe { ohpl_th_decode_header_packet(self.th) } != 0 {
                    log(
                        Level::Error,
                        &msg(
                            b"Error parsing Theora stream headers in '",
                            b"'; corrupt stream?\n",
                        ),
                    );
                    return false;
                }
                header_packets += 1;
                if header_packets == 3 {
                    break;
                }
            }
            // SAFETY: `th` is live.
            if unsafe { ohpl_th_pageout(self.th) } > 0 {
                self.queue_page();
            } else if self.fill(log) == 0 {
                log(
                    Level::Error,
                    &msg(
                        b"End of file while searching for codec headers in '",
                        b"'.\n",
                    ),
                );
                return false;
            }
        }
        true
    }

    fn init_decoders(&mut self) {
        // SAFETY (all calls below): `th` is live.
        if self.loaded {
            unsafe { ohpl_th_decode_init(self.th) };
            let (mut w, mut h, mut ox, mut oy) = (0, 0, 0, 0);
            unsafe { ohpl_th_dims(self.th, &mut w, &mut h, &mut ox, &mut oy) };
            self.width = w;
            self.height = h;
            self.frame = vec![0; theora::frame_buffer_len(w, h)];
        } else {
            unsafe {
                ohpl_th_info_clear(self.th);
                ohpl_th_comment_clear(self.th);
            }
        }
    }

    pub fn update(&mut self, dt: f32, log: Sink) {
        if !self.playing || self.paused {
            return;
        }
        self.time += dt;
        while self.videobuf_time < f64::from(self.time) {
            // SAFETY (all calls below): `th` is live.
            if unsafe { ohpl_th_packetout(self.th) } > 0 {
                let mut t = 0.0;
                if unsafe { ohpl_th_decode_packet(self.th, &mut t) } == 0 {
                    self.videobuf_time = t;
                    self.frame_ready = true;
                }
            } else if unsafe { ohpl_th_pageout(self.th) } > 0 {
                self.queue_page();
            } else if self.fill(log) != 0 {
                while unsafe { ohpl_th_pageout(self.th) } > 0 {
                    self.queue_page();
                }
            } else {
                if !self.looping {
                    self.playing = false;
                }
                self.reset_streams(log);
            }
        }
        // SAFETY: `file` is open while a stream is loaded.
        if !self.file.is_null() && unsafe { cfile::feof(self.file) } != 0 {
            if !self.looping {
                self.playing = false;
            }
            self.reset_streams(log);
        }
    }

    pub fn play(&mut self) {
        self.playing = true;
    }

    pub fn stop(&mut self, log: Sink) {
        self.playing = false;
        self.reset_streams(log);
    }

    pub fn pause(&mut self, p: bool) {
        self.paused = p;
    }

    pub fn set_loop(&mut self, l: bool) {
        self.looping = l;
    }

    /// The converted RGB frame when a new one is ready, `None` otherwise.
    pub fn take_frame(&mut self) -> Option<&[u8]> {
        if !self.loaded || self.frame.is_empty() || !self.frame_ready {
            return None;
        }
        self.draw_frame();
        self.frame_ready = false;
        Some(&self.frame)
    }

    fn draw_frame(&mut self) {
        let mut raw = RawYuv {
            y: core::ptr::null(),
            u: core::ptr::null(),
            v: core::ptr::null(),
            y_len: 0,
            u_len: 0,
            v_len: 0,
            y_stride: 0,
            uv_stride: 0,
        };
        let (mut w, mut h, mut ox, mut oy) = (0, 0, 0u32, 0u32);
        // SAFETY: `th` is live; the planes stay valid until the next decode.
        unsafe {
            ohpl_th_yuv(self.th, &mut raw);
            ohpl_th_dims(self.th, &mut w, &mut h, &mut ox, &mut oy);
        }
        let (Ok(y_stride), Ok(uv_stride)) = (
            usize::try_from(raw.y_stride),
            usize::try_from(raw.uv_stride),
        ) else {
            return;
        };
        // SAFETY: lengths come with the pointers from the decoder.
        let planes = unsafe {
            Planes {
                y: crate::string_ffi::units(raw.y, raw.y_len),
                u: crate::string_ffi::units(raw.u, raw.u_len),
                v: crate::string_ffi::units(raw.v, raw.v_len),
                y_stride,
                uv_stride,
                offset_x: ox as usize,
                offset_y: oy as usize,
            }
        };
        let t = &*self.tables;
        theora::draw_frame(
            &mut self.frame,
            &planes,
            &TablesRef {
                to_r: &t.to_r,
                to_b: &t.to_b,
                g_uv: &t.g_uv,
                g_y_uv: &t.g_y_uv,
            },
            self.width,
            self.height,
        );
    }

    fn reset_streams(&mut self, log: Sink) {
        // SAFETY (all calls below): `th` is live.
        unsafe {
            ohpl_th_drain_packets(self.th);
            ohpl_th_drain_pages(self.th);
        }
        self.time = 0.0;
        self.videobuf_time = 0.0;
        self.frame_ready = false;
        unsafe {
            ohpl_th_stream_clear(self.th);
            ohpl_th_sync_reset(self.th);
            ohpl_th_state_clear(self.th);
            ohpl_th_decode_init(self.th);
        }
        // SAFETY: `file` is the open handle.
        unsafe { cfile::fclose(self.file) };
        self.file = cfile::open(&self.path, c"rb");
        if self.file.is_null() {
            return;
        }
        self.fill(log);

        let mut count = 0;
        while count < 3 {
            while count < 3 && unsafe { ohpl_th_pageout(self.th) } > 0 {
                if count == 0 && unsafe { ohpl_th_page_bos(self.th) } != 0 {
                    unsafe { ohpl_th_test_begin(self.th) };
                    if unsafe { ohpl_th_test_header_bit(self.th) } != 0 {
                        unsafe { ohpl_th_test_adopt(self.th) };
                        count = 1;
                    } else {
                        unsafe { ohpl_th_test_clear(self.th) };
                    }
                }
                if count > 0 {
                    self.queue_page();
                    while count < 3 {
                        if unsafe { ohpl_th_packetout(self.th) } < 1 {
                            break;
                        }
                        count += 1;
                    }
                }
            }
            self.fill(log);
        }
    }
}

impl Drop for Video {
    fn drop(&mut self) {
        if !self.file.is_null() {
            // SAFETY: `file` is open and closed once.
            unsafe { cfile::fclose(self.file) };
        }
        // SAFETY: `th` is live and freed once.
        unsafe { ohpl_th_free(self.th, i32::from(self.loaded)) };
    }
}

const _: () = assert!(size_of::<RawYuv>() == 56 && core::mem::offset_of!(RawYuv, y_stride) == 48);
