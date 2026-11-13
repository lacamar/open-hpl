H20 golden vectors: TextBox (text and numeric), ComboBox, ListBox, MultiPropertyListBox, TabFrame (Tab, TabLabel),
ContextMenu, MenuItem and MainMenu.

widgets_<skin>.txt  h20_widgets stdout with the trace replaced by its line count and FNV-1a hash (H20_DIGEST=1):
                    the widgets in one scene over the skin parsed from the installed game's .skin (attributes,
                    fonts' size/colour, gfx material/offset/active size, image sizes from the .tga headers; glyphs
                    are a stub font), 320 seeded steps of mouse (move/down/up/double click/wheel, modifiers), typing
                    and editing keys, UI arrows/buttons, gamepad, setters (text, selection, numeric value, can-edit,
                    list/combo selection, cursor, tab switching and adding, menu show/hide/check) and updates;
                    callbacks in order, widget and widget state per step, then the RecordingLowLevelGraphics trace of
                    3 Draw/Render frames. Recorded from OHPL_IMPL=cpp; harness/verify.sh checks OHPL_IMPL=rust
                    against these files, harness/compare.sh diffs cpp against rust byte for byte.
                    The harness allocates from a bump arena: draw order ties are broken by clip/material/texture
                    addresses (cGuiRenderObjectCompare), so heap layout must not differ between the two runs.

cargo test (gui::h20_tests) checks the host op table against RustGuiH20.h and runs the list selection logic
against a mock host.

Record (needs a build dir of this tree with `make TraceRecorderTests`):
  harness/build.sh <repo> <build dir> <out>     builds <out>/h20_widgets
  harness/compare.sh <out> 40                   OHPL_IMPL=cpp vs rust over 6 skins x seeds
  harness/verify.sh <out>                       rust vs the committed files
  H20_DIGEST=1 H20_SKIN=<skin> H20_SEED=<n> OHPL_IMPL=cpp <out>/h20_widgets > widgets_<skin>.txt
