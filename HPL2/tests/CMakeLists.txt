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

# cString: file path handling and string<->value conversions used throughout
# every resource loader, config file, and save-game field.
AddTestTarget(CStringTests
    CStringTests.cpp
)

add_test(NAME CStringTests COMMAND CStringTests)
set_tests_properties(CStringTests PROPERTIES TIMEOUT 30)

# XDG Base Directory / xdg-user-dirs path resolution (PlatformUnix.cpp),
# added to stop the engine writing saves/config/cache/logs directly under
# ~/.frictionalgames or the game's own install directory.
AddTestTarget(PlatformXdgPathTests
    PlatformXdgPathTests.cpp
)

add_test(NAME PlatformXdgPathTests COMMAND PlatformXdgPathTests)
set_tests_properties(PlatformXdgPathTests PROPERTIES TIMEOUT 30)
