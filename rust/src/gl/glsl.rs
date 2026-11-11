//! `cGLSLProgram` uniform-id cache and current-program filter, `cGLSLShader` log helpers.
//! Uniform locations come from the C++ caller (`glGetUniformLocation`).

/// `GetVariableAsId` refuses ids past this; C++ resized the vector to `id + 1` unchecked.
pub const MAX_ID: i32 = 1 << 16;

struct Param {
    location: i32,
    name: Vec<u8>,
}

#[derive(Default)]
pub struct Params {
    list: Vec<Param>,
}

/// First phase of `GetVariableAsId`, before the C++ side needs the GL lookup.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum AsId {
    Done(bool),
    Lookup,
}

impl Params {
    pub fn find(&self, name: &[u8]) -> Option<usize> {
        self.list.iter().position(|p| p.name == name)
    }

    /// Appends a looked-up uniform and returns its id.
    pub fn add(&mut self, name: &[u8], location: i32) -> i32 {
        self.list.push(Param {
            location,
            name: name.to_vec(),
        });
        i32::try_from(self.list.len() - 1).unwrap_or(i32::MAX)
    }

    pub fn len(&self) -> usize {
        self.list.len()
    }

    pub fn is_empty(&self) -> bool {
        self.list.is_empty()
    }

    pub fn location(&self, id: i32) -> Option<i32> {
        usize::try_from(id)
            .ok()
            .and_then(|i| self.list.get(i))
            .map(|p| p.location)
    }

    pub fn as_id_begin(&self, name: &[u8], id: i32) -> AsId {
        if id < 0 {
            return AsId::Done(false);
        }
        match self.find(name) {
            Some(i) => AsId::Done(usize::try_from(id).is_ok_and(|id| id == i)),
            None => AsId::Lookup,
        }
    }

    /// Second phase with the uniform `location` the driver returned.
    pub fn as_id_finish(&mut self, name: &[u8], id: i32, location: i32) -> bool {
        if location < 0 {
            return false;
        }
        let Ok(slot) = usize::try_from(id) else {
            return false;
        };
        if let Some(p) = self.list.get(slot) {
            if p.location >= 0 {
                return p.location == location;
            }
        } else {
            if id > MAX_ID {
                return false;
            }
            self.list.resize_with(slot.wrapping_add(1), || Param {
                location: -1,
                name: Vec::new(),
            });
        }
        if let Some(p) = self.list.get_mut(slot) {
            p.location = location;
            p.name = name.to_vec();
        }
        true
    }
}

/// `Bind`: true when `glUseProgram(handle)` is due.
pub fn bind(current: &mut i32, handle: u32) -> bool {
    if *current as u32 == handle {
        return false;
    }
    *current = handle as i32;
    true
}

/// `UnBind`: true when `glUseProgram(0)` is due.
pub fn unbind(current: &mut i32) -> bool {
    if *current == 0 {
        return false;
    }
    *current = 0;
    true
}

/// Destructor: forget the program when it was the bound one.
pub fn forget(current: &mut i32, handle: u32) {
    if *current as u32 == handle {
        *current = 0;
    }
}

/// `LogShaderCode`: numbered, `\r`-stripped lines of the NUL-terminated `code`. A last line
/// without `\n` is dropped.
pub fn code_lines(code: &[u8], mut each: impl FnMut(i32, &[u8])) {
    let mut row = 1;
    let mut line: Vec<u8> = Vec::new();
    for &b in code.iter().take_while(|&&b| b != 0) {
        match b {
            b'\r' => {}
            b'\n' => {
                each(row, &line);
                line.clear();
                row += 1;
            }
            _ => line.push(b),
        }
    }
}

/// Dump file name: `<count>_<name with separators replaced>.vert|.frag`.
pub fn dump_name(count: i32, name: &[u8], vertex: bool) -> Vec<u8> {
    let mut out = count.to_string().into_bytes();
    out.push(b'_');
    out.extend(
        name.iter()
            .map(|&b| if b == b'/' || b == b'\\' { b'_' } else { b }),
    );
    out.extend_from_slice(if vertex { b".vert" } else { b".frag" });
    out
}
