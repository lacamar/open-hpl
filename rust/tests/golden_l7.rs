use std::fs;
use std::path::Path;

fn summary(game: &str) -> String {
    let p = Path::new(env!("CARGO_MANIFEST_DIR"))
        .join(format!("tests/golden/L7/script_check_{game}.json"));
    let text = fs::read_to_string(p).unwrap_or_default();
    text.split("\"summary\":")
        .nth(1)
        .unwrap_or_default()
        .trim_end()
        .to_string()
}

#[test]
fn script_check_reports_compile_everything() {
    for (game, modules) in [("soma", 190), ("rebirth", 232), ("bunker", 209)] {
        let s = summary(game);
        assert!(
            s.contains(&format!("\"modules\":{modules},\"compiled\":{modules}")),
            "{game}: {s}"
        );
        assert!(s.contains("\"api_errors\":0"), "{game}: {s}");
    }
}
