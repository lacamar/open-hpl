//! `cSerializeClass::SaveToFile` / `LoadFromFile`: XML (plain or zlib + CRC wrapped) files of a
//! serialised object. Messages and their order match the C++ originals.

use super::ffi::{CHost, OhplSerHost};
use super::serialize::{Host, Level, Obj, Walker};
use crate::binary_buffer::BinaryBuffer;
use crate::xml::Doc;
use std::ffi::c_char;
use std::fs::OpenOptions;
use std::io::{Read, Write};
use std::os::unix::ffi::OsStrExt;
use std::os::unix::fs::OpenOptionsExt;
use std::path::Path;

const CRC_KEY: u32 = 0x12AD11A1;

unsafe extern "C" {
    fn wcstombs(dst: *mut u8, src: *const u32, n: usize) -> usize;
}

/// `cString::To8Char`: empty when the string does not convert.
fn narrow(wide: &[u32]) -> Vec<u8> {
    let src: Vec<u32> = wide.iter().copied().chain([0]).collect();
    // SAFETY: `src` is NUL-terminated; `dst` has the `n` bytes asked for plus the NUL slot.
    unsafe {
        let n = wcstombs(std::ptr::null_mut(), src.as_ptr(), 0);
        if n == usize::MAX {
            return Vec::new();
        }
        let mut dst = vec![0u8; n + 1];
        wcstombs(dst.as_mut_ptr(), src.as_ptr(), n);
        dst.truncate(n);
        dst
    }
}

/// `OhplXmlDumpBytes`: the L9 gate hook (FNV-1a of the bytes).
fn xml_dump(tag: &str, data: &[u8]) {
    let Some(path) = std::env::var_os("OHPL_L9_XMLDUMP") else {
        return;
    };
    let h = data.iter().fold(1469598103934665603u64, |h, &b| {
        (h ^ u64::from(b)).wrapping_mul(1099511628211)
    });
    if let Ok(mut f) = OpenOptions::new().create(true).append(true).open(path) {
        let _ = writeln!(f, "{tag} {} {h:016x}", data.len());
    }
}

struct Ctx<'a> {
    host: CHost<'a>,
    path: Vec<u8>,
}

impl Ctx<'_> {
    fn error(&self, msg: String) {
        self.host.log(Level::Error, msg.as_bytes());
    }

    fn path(&self) -> std::borrow::Cow<'_, str> {
        String::from_utf8_lossy(&self.path)
    }

    fn path_of(&self) -> &Path {
        Path::new(std::ffi::OsStr::from_bytes(&self.path))
    }

    fn write_binary(&self, data: &[u8]) -> bool {
        let Ok(mut f) = open_write(self.path_of(), false) else {
            self.error(format!("Could not open binary file '{}'\n", self.path()));
            return false;
        };
        if data.is_empty() || f.write_all(data).is_err() {
            self.error(format!(
                "Could not write to binary file '{}'\n",
                self.path()
            ));
            return false;
        }
        true
    }

    fn read_binary(&self, bb: &mut BinaryBuffer) -> bool {
        let Ok(mut f) = OpenOptions::new().read(true).open(self.path_of()) else {
            self.error(format!("Could not open binary file '{}'\n", self.path()));
            return false;
        };
        let mut data = Vec::new();
        let ok = f.read_to_end(&mut data).is_ok() && !data.is_empty();
        bb.assign(&data);
        if !ok {
            self.error(format!(
                "Could not read from binary file '{}'\n",
                self.path()
            ));
        }
        ok
    }
}

fn open_write(path: &Path, read: bool) -> std::io::Result<std::fs::File> {
    OpenOptions::new()
        .read(read)
        .write(true)
        .create(true)
        .truncate(true)
        .mode(0o666)
        .open(path)
}

use crate::ffi::slice;

/// # Safety
/// `host` is fully populated; `obj` is a live registered object; `path` and `root` point to
/// `path_len` and `root_len` readable elements.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h1_ser_save_file(
    host: *const OhplSerHost,
    obj: Obj,
    path: *const u32,
    path_len: usize,
    root: *const c_char,
    root_len: usize,
    compress: bool,
) -> bool {
    super::h1::ohpl_h1_ser_flush();
    // SAFETY: caller contract.
    let (host, path, root) = unsafe {
        (
            CHost(&*host),
            slice(path, path_len),
            slice(root.cast::<u8>(), root_len),
        )
    };
    let c = Ctx {
        path: narrow(path),
        host,
    };
    let mut doc = Doc::new();
    // SAFETY: caller contract.
    unsafe { Walker::new(&c.host) }.save_document(&mut doc, obj, root);
    if !compress {
        let Ok(mut f) = open_write(c.path_of(), true) else {
            c.error(format!(
                "Unable to open serialized file '{}' as w+! Invalid filepointer returned!\n",
                c.path()
            ));
            return false;
        };
        let out = doc.print_file();
        let ok = f.write_all(&out).is_ok();
        xml_dump("file", &out);
        if !ok {
            c.error(format!("Couldn't save class to '{}'\n", c.path()));
        }
        return ok;
    }
    let mut data = doc.print_stream();
    xml_dump("stream", &data);
    data.push(0);
    let mut bb = BinaryBuffer::new();
    bb.crc_begin();
    if !bb.compress_and_add(&data, -1, false) {
        c.error(format!(
            "Unable to compress data for serialized data '{}'!\n",
            c.path()
        ));
        return false;
    }
    bb.crc_end(CRC_KEY);
    // SAFETY: `data_ptr` addresses at least `size` initialised bytes.
    let bytes = unsafe { slice(bb.data_ptr().cast_const(), bb.size()) };
    if !c.write_binary(bytes) {
        c.error(format!("Unable to save serialized file '{}'!\n", c.path()));
        return false;
    }
    true
}

/// # Safety
/// As `ohpl_h1_ser_save_file`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h1_ser_load_file(
    host: *const OhplSerHost,
    obj: Obj,
    path: *const u32,
    path_len: usize,
    compress: bool,
) -> bool {
    super::h1::ohpl_h1_ser_flush();
    // SAFETY: caller contract.
    let (host, path) = unsafe { (CHost(&*host), slice(path, path_len)) };
    let c = Ctx {
        path: narrow(path),
        host,
    };
    let mut doc = Doc::new();
    if !compress {
        let Ok(mut f) = OpenOptions::new().read(true).open(c.path_of()) else {
            c.error(format!(
                "Unable to open serialized file '{}' as rb! Invalid filepointer returned!\n",
                c.path()
            ));
            return false;
        };
        let mut data = Vec::new();
        let read_ok = f.read_to_end(&mut data).is_ok();
        if !doc.load(&data, read_ok || data.is_empty()) {
            let e = doc.error_info();
            c.error(format!(
                "Couldn't load saved class file '{}' from {}!\n",
                c.path(),
                e.desc
            ));
            return false;
        }
    } else {
        let mut comp = BinaryBuffer::new();
        if !c.read_binary(&mut comp) {
            c.error(format!("Unable to open serialized file '{}'!\n", c.path()));
            return false;
        }
        if !comp.check_internal_crc(CRC_KEY, -1) {
            c.error(format!(
                "CRC check for serialized file '{}' failed!\n",
                c.path()
            ));
            return false;
        }
        let mut text = BinaryBuffer::new();
        if !text.decompress_from(&mut comp, false) {
            c.error(format!(
                "Unable to decompress serialized data '{}'!\n",
                c.path()
            ));
            return false;
        }
        // SAFETY: `data_ptr` addresses at least `size` initialised bytes.
        let bytes = unsafe { slice(text.data_ptr().cast_const(), text.size()) };
        doc.parse(bytes.split(|&b| b == 0).next().unwrap_or_default());
    }
    // SAFETY: caller contract.
    unsafe { Walker::new(&c.host) }.load_document(&doc, obj);
    true
}
