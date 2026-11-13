H19 golden vectors: iWidget base (state, hierarchy, message dispatch, callbacks, draw traversal) and the nine
simple widgets (Window, Frame, Button, CheckBox, Label, Slider, Image, Group, Dummy).

base_<seed>.txt     h19_base stdout, recorded from the original C++ iWidget with probe widgets. "w" = widget setup,
                    "cb i m" = callback registered, "op" = one scripted call, "ev" = subclass virtual fired,
                    "cb wN mM" = user callback fired, "=" = result, "s"/"g" = widget state / global position
                    (printed when changed), "sort" = cGuiSet re-sort requested. cargo test (gui::h19_tests)
                    replays every file through h19_widget with a model of the probes and compares every line,
                    without the C++.
widgets_<skin>.txt  h19_widgets stdout with the trace replaced by its line count and FNV-1a hash (H19_DIGEST=1):
                    the nine widgets in a scene over the skin parsed from the installed game's .skin (attributes,
                    fonts' size/colour, gfx material/offset/active size, image sizes from the .tga headers; glyphs
                    are a stub font), seeded mouse/key/gamepad/UI input, setters and updates; callbacks in order,
                    widget state per step, then the RecordingLowLevelGraphics trace of 3 Draw/Render frames.
                    Compared OHPL_IMPL=cpp vs rust only (harness/verify.sh checks rust against these files).
                    The harness allocates from a bump arena: draw order ties are broken by clip/material/texture
                    addresses (cGuiRenderObjectCompare), so heap layout must not differ between the two runs.

Record (needs a build dir of this tree with `make TraceRecorderTests`):
  harness/build.sh <repo> <build dir> <out>     builds <out>/h19_base and <out>/h19_widgets
  harness/compare.sh <out> 40                   OHPL_IMPL=cpp vs rust: base seeds, and widgets over 6 skins x seeds
  harness/verify.sh <out>                       rust vs the committed files
  H19_SEED=<n> OHPL_IMPL=cpp <out>/h19_base > base_<nnn>.txt   (seeds 1..12 committed)
