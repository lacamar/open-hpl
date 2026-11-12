use std::fs;
use std::path::{Path, PathBuf};

fn files(dir: &Path, out: &mut Vec<PathBuf>) {
    let Ok(entries) = fs::read_dir(dir) else {
        return;
    };
    for path in entries.flatten().map(|e| e.path()) {
        if path.is_dir() {
            files(&path, out);
        } else {
            out.push(path);
        }
    }
}

#[test]
fn golden_files_are_readable() {
    let mut all = Vec::new();
    files(
        &Path::new(env!("CARGO_MANIFEST_DIR")).join("tests/golden"),
        &mut all,
    );
    for path in all {
        assert!(fs::read(&path).is_ok(), "{}", path.display());
    }
}
