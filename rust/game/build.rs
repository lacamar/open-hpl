//! Link line of the game executables. CMake writes one manifest per game, one linker argument per
//! line (`<game>.link` for the game library, `common.link` for the engine), into `OHPL_LINK_MANIFEST_DIR`; without it (plain `cargo build`) the bins link bare.

use std::path::{Path, PathBuf};

const GAMES: [&str; 3] = ["amnesia", "amfp", "soma"];

fn main() {
    println!("cargo:rerun-if-env-changed=OHPL_LINK_MANIFEST_DIR");
    let Some(dir) = std::env::var_os("OHPL_LINK_MANIFEST_DIR") else {
        return;
    };
    let out = PathBuf::from(std::env::var_os("OUT_DIR").unwrap_or_default());
    // OUT_DIR = <target>/<profile>/build/<pkg>/out; the uplifted lib sits in <profile>/
    let rlib = out.ancestors().nth(3).map(|p| p.join("libohpl.rlib"));
    for game in GAMES {
        let manifest = Path::new(&dir).join(format!("{game}.link"));
        let common = Path::new(&dir).join("common.link");
        let (Ok(game_part), Ok(common_part)) = (
            std::fs::read_to_string(&manifest),
            std::fs::read_to_string(&common),
        ) else {
            continue;
        };
        let text = game_part + &common_part;
        println!("cargo:rerun-if-changed={}", manifest.display());
        println!("cargo:rerun-if-changed={}", common.display());
        let arg = |a: &str| println!("cargo:rustc-link-arg-bin={game}={a}");
        // the C++ archives and the Rust lib reference each other
        arg("-no-pie"); // the C++ objects are not PIC
        arg("-Wl,--start-group");
        for line in text.lines().filter(|l| !l.is_empty()) {
            if Path::new(line).is_absolute() {
                println!("cargo:rerun-if-changed={line}");
            }
            arg(line);
        }
        if let Some(r) = rlib.as_ref().filter(|r| r.exists()) {
            println!("cargo:rerun-if-changed={}", r.display());
            arg(&r.display().to_string());
        }
        arg("-Wl,--end-group");
        arg("-lstdc++");
        arg("-lm");
        arg("-lc");
    }
}
