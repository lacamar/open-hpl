use super::*;

/// The calls of the music snippet in `rust/tests/golden/S7/scenario-*.txt` and the `MP`/`MS` lines the
/// C++ `SomaSound.cpp` wrote for them in the real run (`OHPL_S7_DUMP`).
#[test]
fn scenario_matches_cpp_dump() {
    let mut m = Music::new();
    m.play(b"music_a.ogg", true, 0.5, 2.0, 3, false);
    m.play(b"music_a.ogg", true, 0.7, 2.0, 5, true);
    m.play(b"music_b.ogg", false, 0.4, 0.0, 7, false);
    for (fade, prio) in [(1.0, 5), (1.0, 7), (1.0, 3), (1.0, 3)] {
        m.stop(fade, prio);
    }
    m.play(b"music_a.ogg", true, 0.5, 2.0, 99, false);
    m.stop(0.0, -4);
    assert_eq!(
        dump::captured(),
        include_str!("../../tests/golden/S7/music-scenario.txt")
    );
}

#[test]
fn save_text_round_trips() {
    let mut m = Music::new();
    m.play(b"a b.ogg", true, 0.3, 1.0, 2, true);
    m.play(b"c.ogg", true, 1.0, 0.0, 4, false);
    m.tracks.push(Track {
        id: (3, -7, 11),
        track_prio: 2,
        music_prio: 6,
        file: b"d.ogg".to_vec(),
        volume: 0.25,
        fade_in: 1.5,
        fade_out: 2.0,
    });
    let text = m.serialize();
    assert_eq!(
        text,
        b"M 2 0.300000012 1 1 a b.ogg\nM 4 1 1 0 c.ogg\nD 3 -7 11 2 6 0.25 1.5 2 d.ogg\n"
    );
    let mut n = Music::new();
    n.deserialize(&text);
    assert_eq!(n.serialize(), text);
}
