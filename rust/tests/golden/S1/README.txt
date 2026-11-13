bindings.txt: "<type or empty>\t<declaration>" for every generated engine binding, in registration order, taken from
the compiler-verified table of soma/src/game/SomaScriptGenBindings.cpp (scripts/soma-gen-bindings.py writes both).
somascript::s1_tests::generated_registrations_match_cpp_table checks s1_gen.rs registers exactly this sequence.
Live: OHPL_S2_DECL_DUMP of the whole engine (types, methods, behaviours, props, globals, enums) is byte-identical
between OHPL_IMPL=cpp and rust for soma, rebirth and bunker; the script-check reports are identical.
