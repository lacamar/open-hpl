//! Raw DevIL bindings: the functions, callback types and constants the bitmap loaders use.
#![allow(non_camel_case_types, non_snake_case, non_upper_case_globals)]

use core::ffi::{c_char, c_void};

#[repr(C)]
pub struct FILE {
    _private: [u8; 0],
}

pub type fCloseRProc = unsafe extern "C" fn(a0: *mut c_void);
pub type fEofProc = unsafe extern "C" fn(a0: *mut c_void) -> u8;
pub type fGetcProc = unsafe extern "C" fn(a0: *mut c_void) -> i32;
pub type fOpenRProc = unsafe extern "C" fn(a0: *const c_char) -> *mut c_void;
pub type fReadProc =
    unsafe extern "C" fn(a0: *mut c_void, a1: u32, a2: u32, a3: *mut c_void) -> i32;
pub type fSeekRProc = unsafe extern "C" fn(a0: *mut c_void, a1: i32, a2: i32) -> i32;
pub type fTellRProc = unsafe extern "C" fn(a0: *mut c_void) -> i32;
pub type fCloseWProc = unsafe extern "C" fn(a0: *mut c_void);
pub type fOpenWProc = unsafe extern "C" fn(a0: *const c_char) -> *mut c_void;
pub type fPutcProc = unsafe extern "C" fn(a0: u8, a1: *mut c_void) -> i32;
pub type fSeekWProc = unsafe extern "C" fn(a0: *mut c_void, a1: i32, a2: i32) -> i32;
pub type fTellWProc = unsafe extern "C" fn(a0: *mut c_void) -> i32;
pub type fWriteProc =
    unsafe extern "C" fn(a0: *const c_void, a1: u32, a2: u32, a3: *mut c_void) -> i32;

pub const IL_BGR: u32 = 0x80E0;
pub const IL_BGRA: u32 = 0x80E1;
pub const IL_BMP: u32 = 0x420;
pub const IL_DDS: u32 = 0x437;
pub const IL_DXT1: u32 = 0x706;
pub const IL_DXT3: u32 = 0x708;
pub const IL_DXT5: u32 = 0x70A;
pub const IL_DXTC_DATA_FORMAT: u32 = 0x70D;
pub const IL_DXT_NO_COMP: u32 = 0x70B;
pub const IL_FLOAT: u32 = 0x1406;
pub const IL_IMAGE_BYTES_PER_PIXEL: u32 = 0xDE8;
pub const IL_IMAGE_CUBEFLAGS: u32 = 0xDFD;
pub const IL_IMAGE_DEPTH: u32 = 0xDE6;
pub const IL_IMAGE_FORMAT: u32 = 0xDEA;
pub const IL_IMAGE_HEIGHT: u32 = 0xDE5;
pub const IL_IMAGE_SIZE_OF_DATA: u32 = 0xDE7;
pub const IL_IMAGE_TYPE: u32 = 0xDEB;
pub const IL_IMAGE_WIDTH: u32 = 0xDE4;
pub const IL_JPG: u32 = 0x425;
pub const IL_KEEP_DXTC_DATA: u32 = 0x70C;
pub const IL_LUMINANCE: u32 = 0x1909;
pub const IL_NUM_IMAGES: u32 = 0xDF1;
pub const IL_NUM_MIPMAPS: u32 = 0xDF2;
pub const IL_ORIGIN_MODE: u32 = 0x603;
pub const IL_ORIGIN_SET: u32 = 0x600;
pub const IL_ORIGIN_UPPER_LEFT: u32 = 0x602;
pub const IL_PCX: u32 = 0x428;
pub const IL_PNG: u32 = 0x42A;
pub const IL_PSD: u32 = 0x439;
pub const IL_RGB: u32 = 0x1907;
pub const IL_RGBA: u32 = 0x1908;
pub const IL_SGI: u32 = 0x42C;
pub const IL_TGA: u32 = 0x42D;
pub const IL_TIF: u32 = 0x42E;
pub const IL_UNSIGNED_BYTE: u32 = 0x1401;
pub const IL_VENDOR: u32 = 0x1F00;
pub const IL_VERSION: u32 = 178;
pub const IL_VERSION_NUM: u32 = 0xDE2;
pub const IL_TRUE: u8 = 1;
pub const IL_FALSE: u8 = 0;
pub const IL_SEEK_SET: i32 = 0;
pub const IL_SEEK_END: i32 = 2;

#[cfg_attr(test, link(name = "IL"))]
unsafe extern "C" {
    pub fn ilActiveFace(Number: u32) -> u8;
    pub fn ilActiveImage(Number: u32) -> u8;
    pub fn ilActiveMipmap(Number: u32) -> u8;
    pub fn ilBindImage(Image: u32);
    pub fn ilDeleteImages(Num: usize, Images: *const u32);
    pub fn ilEnable(Mode: u32) -> u8;
    pub fn ilGenImages(Num: usize, Images: *mut u32);
    pub fn ilGetData() -> *mut u8;
    pub fn ilGetDXTCData(Buffer: *mut c_void, BufferSize: u32, DXTCFormat: u32) -> u32;
    pub fn ilGetInteger(Mode: u32) -> i32;
    pub fn ilGetString(StringName: u32) -> *const c_char;
    pub fn ilInit();
    pub fn ilLoadF(Type: u32, File: *mut c_void) -> u8;
    pub fn ilSaveF(Type: u32, File: *mut c_void) -> u32;
    pub fn ilSetInteger(Mode: u32, Param: i32);
    pub fn ilSetRead(
        a0: Option<fOpenRProc>,
        a1: Option<fCloseRProc>,
        a2: Option<fEofProc>,
        a3: Option<fGetcProc>,
        a4: Option<fReadProc>,
        a5: Option<fSeekRProc>,
        a6: Option<fTellRProc>,
    );
    pub fn ilSetWrite(
        a0: Option<fOpenWProc>,
        a1: Option<fCloseWProc>,
        a2: Option<fPutcProc>,
        a3: Option<fSeekWProc>,
        a4: Option<fTellWProc>,
        a5: Option<fWriteProc>,
    );
    pub fn ilTexImage(
        Width: u32,
        Height: u32,
        Depth: u32,
        NumChannels: u8,
        Format: u32,
        Type: u32,
        Data: *mut c_void,
    ) -> u8;
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn constants() {
        assert_eq!(i64::from(IL_BGR), 32992);
        assert_eq!(i64::from(IL_BGRA), 32993);
        assert_eq!(i64::from(IL_BMP), 1056);
        assert_eq!(i64::from(IL_DDS), 1079);
        assert_eq!(i64::from(IL_DXT1), 1798);
        assert_eq!(i64::from(IL_DXT3), 1800);
        assert_eq!(i64::from(IL_DXT5), 1802);
        assert_eq!(i64::from(IL_DXTC_DATA_FORMAT), 1805);
        assert_eq!(i64::from(IL_DXT_NO_COMP), 1803);
        assert_eq!(i64::from(IL_FLOAT), 5126);
        assert_eq!(i64::from(IL_IMAGE_BYTES_PER_PIXEL), 3560);
        assert_eq!(i64::from(IL_IMAGE_CUBEFLAGS), 3581);
        assert_eq!(i64::from(IL_IMAGE_DEPTH), 3558);
        assert_eq!(i64::from(IL_IMAGE_FORMAT), 3562);
        assert_eq!(i64::from(IL_IMAGE_HEIGHT), 3557);
        assert_eq!(i64::from(IL_IMAGE_SIZE_OF_DATA), 3559);
        assert_eq!(i64::from(IL_IMAGE_TYPE), 3563);
        assert_eq!(i64::from(IL_IMAGE_WIDTH), 3556);
        assert_eq!(i64::from(IL_JPG), 1061);
        assert_eq!(i64::from(IL_KEEP_DXTC_DATA), 1804);
        assert_eq!(i64::from(IL_LUMINANCE), 6409);
        assert_eq!(i64::from(IL_NUM_IMAGES), 3569);
        assert_eq!(i64::from(IL_NUM_MIPMAPS), 3570);
        assert_eq!(i64::from(IL_ORIGIN_MODE), 1539);
        assert_eq!(i64::from(IL_ORIGIN_SET), 1536);
        assert_eq!(i64::from(IL_ORIGIN_UPPER_LEFT), 1538);
        assert_eq!(i64::from(IL_PCX), 1064);
        assert_eq!(i64::from(IL_PNG), 1066);
        assert_eq!(i64::from(IL_PSD), 1081);
        assert_eq!(i64::from(IL_RGB), 6407);
        assert_eq!(i64::from(IL_RGBA), 6408);
        assert_eq!(i64::from(IL_SGI), 1068);
        assert_eq!(i64::from(IL_TGA), 1069);
        assert_eq!(i64::from(IL_TIF), 1070);
        assert_eq!(i64::from(IL_UNSIGNED_BYTE), 5121);
        assert_eq!(i64::from(IL_VENDOR), 7936);
        assert_eq!(i64::from(IL_VERSION), 178);
        assert_eq!(i64::from(IL_VERSION_NUM), 3554);
        assert_eq!(i64::from(IL_TRUE), 1);
        assert_eq!(i64::from(IL_FALSE), 0);
        assert_eq!(i64::from(IL_SEEK_SET), 0);
        assert_eq!(i64::from(IL_SEEK_END), 2);
    }
}
