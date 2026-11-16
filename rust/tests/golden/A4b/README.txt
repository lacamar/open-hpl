A4b (enemy mover and pathfinder state): dumps of the original C++ (5f82f7b plus the dump hook, OHPL_A4B_DUMP) on real maps.

a4b-archives.txt  Amnesia 03_archives: servant_grunt_1/2, five player waypoints 400 map updates apart (scripts/a4b-scenario.sh).
a4b-prison.txt    Amnesia 15_prison_north: servant_grunt_1/10, five waypoints.
sha256.txt        digests of these two and of the AMFP dumps (06_factory wretches, 09_bilge water lurkers: 4041 and 12177 lines),
                  which are too large to commit; the full set lives in the gate baseline.
One line per enemy update: state/next/previous, position, velocity, mover state, turning, stuck counter, pathfinder moving, goal, node ids
(path order), window length, animation, health. The scenario script runs inside the map update and the libc seed is reset every update,
so a run of the same build repeats line for line.
Re-record: build a4b-basewt (5f82f7b + the hook), scripts/a4b-all.sh <dir>; replay check: `cargo test luxenemy::nav_tests::golden`
(path nodes only ever leave from the back, the stuck window restarts on every pop).
