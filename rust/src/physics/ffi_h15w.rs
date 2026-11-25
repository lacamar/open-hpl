use super::host::Str;
use super::wreg::{self, WorldOp};

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_h15w_new(op: WorldOp, me: usize) -> u64 {
    wreg::new(op, me)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_h15w_len(book: u64, kind: usize) -> usize {
    wreg::len(book, kind)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_h15w_at(book: u64, kind: usize, i: usize) -> usize {
    wreg::at(book, kind, i)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_h15w_material_len(book: u64) -> usize {
    wreg::material_len(book)
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h15w_material_at(
    book: u64,
    i: usize,
    name: *mut Str,
    obj: *mut usize,
) -> bool {
    let Some((p, n, h)) = wreg::material_at(book, i) else {
        return false;
    };
    unsafe {
        if let Some(o) = name.as_mut() {
            *o = Str { p, n };
        }
        if let Some(o) = obj.as_mut() {
            *o = h;
        }
    }
    true
}
