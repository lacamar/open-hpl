cmake_minimum_required (VERSION 3.10...3.28)
project(HPL2Tests)

# Focused, dependency-free regression tests for the Newton 2.x -> 3.14 port
# (see PhysicsNewtonTests.cpp). Uses the AddTestTarget() helper defined in
# ../core/CMakeLists.txt, which links against the full HPL2 static library.
AddTestTarget(PhysicsNewtonTests
    PhysicsNewtonTests.cpp
)

add_test(NAME PhysicsNewtonTests COMMAND PhysicsNewtonTests)
# A hang here (e.g. a future Newton serialization-format regression) should
# fail fast as a test timeout, not hang CI/local runs indefinitely - this is
# exactly the failure mode that motivated this test file.
set_tests_properties(PhysicsNewtonTests PROPERTIES TIMEOUT 30)

# Regression tests for SOMA's HPSL->GLSL transpiler (soma/src/game/
# HpslTranspiler.cpp) - string-level syntax checks only, no GL/game-data
# needed (see HpslTranspilerTests.cpp's header comment for what this does
# and doesn't verify - a real GL compile still needs a live SOMA process,
# see soma/src/game/HpslTranspilerSelfTest.cpp). Pulls the transpiler's own
# .cpp straight from the soma module rather than duplicating/relinking it as
# a library, since it has no other dependents yet.
AddTestTarget(HpslTranspilerTests
    HpslTranspilerTests.cpp
    ../../soma/src/game/HpslTranspiler.cpp
)
add_test(NAME HpslTranspilerTests COMMAND HpslTranspilerTests)
set_tests_properties(HpslTranspilerTests PROPERTIES TIMEOUT 30)
