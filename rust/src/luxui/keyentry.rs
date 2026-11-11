//! Serial key field (`Fl_Input_Key`): which characters are accepted and how a key press or paste
//! edits the field. The FLTK widget itself stays C++.

const MAX_PASTE: usize = 17;

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum Route {
    Base,
    Key,
    Paste,
}

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum KeyAct {
    /// Hand the keyboard event to the base input widget.
    Base,
    /// Consume the event without editing.
    Swallow,
    Insert {
        pos: i32,
        epos: i32,
        ch: u8,
    },
}

pub fn upper(c: u8) -> u8 {
    c.to_ascii_uppercase()
}

pub fn verify_char(key: u8) -> bool {
    let k = upper(key);
    (b'1'..=b'9').contains(&k)
        || (b'A'..=b'N').contains(&k)
        || (b'P'..=b'W').contains(&k)
        || k == b'Z'
}

pub fn route(keyboard: bool, paste: bool, tab: bool) -> Route {
    if keyboard {
        if tab { Route::Base } else { Route::Key }
    } else if paste {
        Route::Paste
    } else {
        Route::Base
    }
}

pub fn key_action(composed: bool, len: i32, pos: i32, mark: i32, ascii: u8) -> KeyAct {
    if !composed {
        return KeyAct::Base;
    }
    let (pos, epos) = if pos > mark { (mark, pos) } else { (pos, mark) };
    if len.wrapping_sub(epos.wrapping_sub(pos)) < 16 && pos < 17 && verify_char(ascii) {
        KeyAct::Insert {
            pos,
            epos,
            ch: upper(ascii),
        }
    } else {
        KeyAct::Swallow
    }
}

pub fn paste_filter(src: &[u8]) -> Vec<u8> {
    let mut out = Vec::new();
    for &c in src {
        if out.len() >= MAX_PASTE {
            break;
        }
        if verify_char(c) {
            out.push(upper(c));
        }
    }
    out
}
