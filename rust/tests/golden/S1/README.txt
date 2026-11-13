bindings.txt: "<type or empty>\t<declaration>" for every generated engine binding, in registration order, from the compiler-verified table that scripts/soma-gen-bindings.py wrote.
somascript::s1_tests::generated_registrations_match_golden checks s1_gen.rs registers exactly this sequence.
The whole-engine declaration dump (OHPL_S2_DECL_DUMP) and the script-check reports were byte-identical to the pre-flip C++ for soma, rebirth and bunker.
