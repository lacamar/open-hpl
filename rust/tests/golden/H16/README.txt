H16 golden vectors, recorded from the C++ (OHPL_IMPL=cpp) by gen/gen.cpp.
sound.txt: removed with the C++ handler (E-snd); the handler is checked by the OHPL_ESND_DUMP gate dumps.
ai.txt: 12 random node sets (names/ids with duplicates), grid queries and A* paths with a wall cast;
  replayed by rust/src/ai/tests.rs.
Regenerate: gen/gen.sh <build-dir> <bin>; <bin> sound 12 80 > sound.txt; <bin> ai 12 <tmp.xml> > ai.txt
Check both paths: gen/check.sh <build-dir>
