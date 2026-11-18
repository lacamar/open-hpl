//! C ABI for the state initialisers of the combo box and text box, declared in `gui/RustWidget2.h`.

use super::widgets_list::ComboState;
use super::widgets_text::TextState;

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_w2_combo_init() -> ComboState {
    ComboState::new()
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_w2_tb_init(numeric: bool) -> TextState {
    TextState::new(numeric)
}

const _: () = {
    assert!(std::mem::size_of::<ComboState>() == 40);
    assert!(std::mem::size_of::<TextState>() == 80);
};
