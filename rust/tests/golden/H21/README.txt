H21 golden vectors: the five GUI popups (message box, file picker save/load, colour picker, UI keyboard).

cassette_<kind>_<seed>.txt  Host traffic of the Rust popups driving the real C++ widgets over the
                            seeded harness script (Amnesia TDD default skin): "ev" = entry point called by the C++
                            shell (new/event/destroy/...), ">" = host command, "<" = its answer, "=" = entry result;
                            events raised inside a command (combo selection callbacks) nest between ">" and "<".
                            Widget/callback pointers print as order of first appearance. cargo test
                            (gui::h21_tests) replays every file against a mock host that answers from the recording and
                            checks each command and result, without the C++.
digests.txt                 md5 of the full harness stdout of the original C++ (state per step, user/destroy callbacks
                            in order, set focus/attention, RecordingLowLevelGraphics trace of 3 frames) for 6 kinds x
                            6 game skins x seeds 1-3. harness/digests.sh checks the Rust path against it.
                            Needs the installed games (read-only) and the file picker tree under
                            ~/.cache/open-hpl/h21-tmp/fp (harness/make-fpdir.sh: fixed files and mtimes; the harness
                            chdirs into fp/sub, so dates and paths in the output depend on that location).

Record (needs a build dir of this tree with `make TraceRecorderTests`):
  harness/build.sh <repo> <build dir> <out>      builds <out>/h21_popups
  harness/digests.sh <out>                       stdout of 6 kinds x 6 skins x seeds 1-3 against digests.txt
  H21_LOG=<file> H21_KIND=<kind> H21_SEED=<n> <out>/h21_popups   one cassette (run from fp/sub, see digests.sh)
