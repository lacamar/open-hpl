A1 (LuxScriptHandler registrations).
decls.txt: the 322 script declarations in registration order, replayed by cargo test (luxscript); scripts/a1-script-check.sh compiles every Amnesia TDD and AMFP .hps against them.
scripts/a1b-trace.sh records OHPL_A1B_TRACE call traces of every Amnesia/AMFP map; the C++ and Rust traces compare byte for byte.
