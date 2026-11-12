//! Opaque file searcher for `OHPL_IMPL=rust`: owns the path store and the directory scan.

use super::FileSearcher;
use crate::backend::sdl::fs;
use crate::string;

type Wide = Vec<u32>;

#[derive(Default)]
pub struct Searcher {
    reg: FileSearcher,
    paths: Vec<Wide>,
}

const BACKSLASH: [u32; 1] = [b'\\' as u32];
const SLASH: [u32; 1] = [b'/' as u32];

impl Searcher {
    /// `cFileSearcher::AddDirectory`; `mask` is the UTF-8 mask.
    pub fn add_directory(&mut self, search_path: &[u32], mask: &[u8], sub_dirs: bool) {
        let path = string::replace_char_to(search_path, &BACKSLASH, &SLASH);
        let path8 = string::wide_to_utf8(&path);
        let full8 = fs::full_path(&path8).unwrap_or_default();
        let full = string::replace_char_to(&fs::to_wide(&full8), &BACKSLASH, &SLASH);
        for file in fs::find_files(&path8, &fs::to_wide(mask)) {
            let low = string::to_lower(&string::wide_to_utf8(&file));
            let file_path = string::set_file_path(&file, &full);
            let paths = &self.paths;
            let added = self.reg.add(&low, &file_path, |id| {
                paths.get(id as usize) == Some(&file_path)
            });
            if added.is_some() {
                self.paths.push(file_path);
            }
        }
        if sub_dirs {
            for dir in fs::find_folders(&path8, false, false) {
                let sub = string::set_file_path(&dir, &path);
                self.add_directory(&sub, mask, true);
            }
        }
    }

    pub fn clear(&mut self) {
        self.reg.clear();
        self.paths.clear();
    }

    /// Path id and the `apEqualCount` value; `None` when the name is unknown.
    pub fn get(&self, name: &[u8], want_count: bool) -> Option<(u32, Option<i32>)> {
        self.reg
            .get(name, want_count)
            .map(|f| (f.id, f.equal_count))
    }

    pub fn path(&self, id: u32) -> &[u32] {
        self.paths.get(id as usize).map_or(&[], Vec::as_slice)
    }
}
