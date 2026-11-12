//! Opaque config file for `OHPL_IMPL=rust`: XML document plus file and CRC-container I/O.

use crate::binary_buffer::BinaryBuffer;
use crate::xml::{Doc, config_get, config_set};
use std::ffi::OsStr;
use std::os::unix::ffi::OsStrExt;

const CRC_KEY: u32 = 0x16AF_2C1D;

#[repr(i32)]
#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum Status {
    Ok = 0,
    Open = 1,
    Crc = 2,
    Parse = 3,
}

#[derive(Default)]
pub struct Config {
    doc: Doc,
}

fn os(path: &[u8]) -> &OsStr {
    OsStr::from_bytes(path)
}

impl Config {
    pub fn load(&mut self, file: &[u8], use_crc: bool) -> Status {
        let Ok(data) = std::fs::read(os(file)) else {
            return Status::Open;
        };
        if !use_crc {
            return if self.doc.load(&data, true) {
                Status::Ok
            } else {
                Status::Parse
            };
        }
        if data.is_empty() {
            return Status::Open;
        }
        let mut buf = BinaryBuffer::new();
        buf.assign(&data);
        if !buf.check_internal_crc(CRC_KEY, -1) {
            return Status::Crc;
        }
        self.doc.parse(data.get(buf.pos()..).unwrap_or_default());
        if self.doc.error() {
            Status::Parse
        } else {
            Status::Ok
        }
    }

    pub fn save(&self, file: &[u8], use_crc: bool) -> Status {
        let out = if use_crc {
            let mut text = self.doc.print_stream();
            text.push(0);
            let mut buf = BinaryBuffer::new();
            buf.crc_begin();
            buf.add_data(&text);
            buf.crc_end(CRC_KEY);
            let mut bytes = vec![0; buf.size()];
            buf.set_pos(0);
            buf.get_data(&mut bytes);
            bytes
        } else {
            self.doc.print_file()
        };
        match std::fs::write(os(file), out) {
            Ok(()) => Status::Ok,
            Err(_) => Status::Open,
        }
    }

    pub fn set(&mut self, level: &[u8], name: &[u8], value: &[u8]) {
        config_set(&mut self.doc, level, name, value);
    }

    pub fn get(&self, level: &[u8], name: &[u8]) -> Option<&[u8]> {
        config_get(&self.doc, level, name)
    }
}
