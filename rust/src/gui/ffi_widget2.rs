//! C ABI for the state initialisers of the combo box and text box, declared in `gui/RustWidget2.h`.

use super::widgets_list::ComboState;
use super::widgets_text::TextState;

const _: () = {
    assert!(std::mem::size_of::<ComboState>() == 40);
    assert!(std::mem::size_of::<TextState>() == 80);
};
