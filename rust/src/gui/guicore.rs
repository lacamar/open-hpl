//! `cGui` lookup tables: skin type names, material blend selection and mouse button translation.

pub const MATERIAL_LAST: u32 = 6;
pub const MATERIAL_ALPHA: u32 = 1;

pub const BLEND_ZERO: i32 = 0;
pub const BLEND_ONE: i32 = 1;
pub const BLEND_DEST_COLOR: i32 = 4;
pub const BLEND_SRC_ALPHA: i32 = 6;
pub const BLEND_ONE_MINUS_SRC_ALPHA: i32 = 7;

pub const MOUSE_LAST: i32 = 9;
pub const GUI_MOUSE_LAST: i32 = 0x200;

/// `eGuiSkinFont_*` without the prefix, in enum order.
pub const FONTS: [&str; 3] = ["Default", "Disabled", "WindowLabel"];

/// `eGuiSkinAttribute_*` without the prefix, in enum order.
pub const ATTRIBUTES: [&str; 32] = [
    "WindowLabelTextOffset",
    "WindowButtonSize",
    "WindowBordersOutside",
    "ButtonPressedContentOffset",
    "SliderButtonSize",
    "ListBoxSliderWidth",
    "ComboBoxButtonWidth",
    "ComboBoxSliderWidth",
    "ContextMenuItemArrowSize",
    "ContextMenuItemCheckSize",
    "ContextMenuItemHPadding",
    "ContextMenuItemVPadding",
    "ContextMenuItemSeparation",
    "ContextMenuItemTextLeftPadding",
    "ContextMenuItemTextRightPadding",
    "ContextMenuSeparatorPadding",
    "MainMenuItemTextPadding",
    "MainMenuItemVPadding",
    "MainMenuItemSeparation",
    "MenuSeparatorPadding",
    "MenuSeparatorHeight",
    "DefaultFontHighlightColor",
    "TabFrameButtonSize",
    "TabFrameIconPadding",
    "TabFrameTextLeftPadding",
    "TabFrameTextRightPadding",
    "TabFrameLabelInactiveOffset",
    "TabFrameLabelActiveOffset",
    "FirstTabOffset",
    "TabSeparation",
    "GroupHeaderOffset",
    "GroupHeaderOverUpperBorder",
];

/// `eGuiSkinGfx_*` without the prefix, in enum order.
pub const GFXS: [&str; 114] = [
    "PointerNormal",
    "PointerText",
    "WindowBorderRight",
    "WindowBorderLeft",
    "WindowBorderUp",
    "WindowBorderDown",
    "WindowCornerLU",
    "WindowCornerRU",
    "WindowCornerRD",
    "WindowCornerLD",
    "WindowLabel",
    "WindowBackground",
    "WindowButtonCross",
    "FrameBorderRight",
    "FrameBorderLeft",
    "FrameBorderUp",
    "FrameBorderDown",
    "FrameCornerLU",
    "FrameCornerRU",
    "FrameCornerRD",
    "FrameCornerLD",
    "FrameBackground",
    "CheckBoxEnabledUnchecked",
    "CheckBoxEnabledChecked",
    "CheckBoxDisabledUnchecked",
    "CheckBoxDisabledChecked",
    "TextBoxBackground",
    "TextBoxSelectedTextBack",
    "TextBoxMarker",
    "ListBoxBackground",
    "ComboBoxButtonIcon",
    "ComboBoxBorderRight",
    "ComboBoxBorderLeft",
    "ComboBoxBorderUp",
    "ComboBoxBorderDown",
    "ComboBoxCornerLU",
    "ComboBoxCornerRU",
    "ComboBoxCornerRD",
    "ComboBoxCornerLD",
    "ComboBoxBackground",
    "SliderHandleBorderRight",
    "SliderHandleBorderLeft",
    "SliderHandleBorderUp",
    "SliderHandleBorderDown",
    "SliderHandleCornerLU",
    "SliderHandleCornerRU",
    "SliderHandleCornerRD",
    "SliderHandleCornerLD",
    "SliderHandleBackground",
    "SliderVertArrowUp",
    "SliderVertArrowDown",
    "SliderVertBackground",
    "SliderHoriArrowUp",
    "SliderHoriArrowDown",
    "SliderHoriBackground",
    "ButtonUpBorderRight",
    "ButtonUpBorderLeft",
    "ButtonUpBorderUp",
    "ButtonUpBorderDown",
    "ButtonUpCornerLU",
    "ButtonUpCornerRU",
    "ButtonUpCornerRD",
    "ButtonUpCornerLD",
    "ButtonUpBackground",
    "ButtonDownBorderRight",
    "ButtonDownBorderLeft",
    "ButtonDownBorderUp",
    "ButtonDownBorderDown",
    "ButtonDownCornerLU",
    "ButtonDownCornerRU",
    "ButtonDownCornerRD",
    "ButtonDownCornerLD",
    "ButtonDownBackground",
    "MenuEnabledChecked",
    "MenuDisabledChecked",
    "MenuSeparatorLine",
    "MenuItemArrow",
    "TabLabelBorderRight",
    "TabLabelBorderLeft",
    "TabLabelBorderUp",
    "TabLabelBorderDown",
    "TabLabelCornerLU",
    "TabLabelCornerRU",
    "TabLabelCornerRD",
    "TabLabelCornerLD",
    "TabBorderRight",
    "TabBorderLeft",
    "TabBorderUp",
    "TabBorderDown",
    "TabCornerLU",
    "TabCornerRU",
    "TabCornerRD",
    "TabCornerLD",
    "TabLabelBackground",
    "TabLabelBackgroundActive",
    "TabBackground",
    "TabFrameArrowLeft",
    "TabFrameArrowRight",
    "GroupBorderLeft",
    "GroupBorderRight",
    "GroupBorderUp",
    "GroupBorderDown",
    "GroupCornerLU",
    "GroupCornerRU",
    "GroupCornerRD",
    "GroupCornerLD",
    "GroupBackground",
    "FilePickerUpButton",
    "FilePickerIconGraphics",
    "FilePickerIconModels",
    "FilePickerIconSounds",
    "FilePickerIconText",
    "FilePickerIconOther",
    "FilePickerIconFolder",
];

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum SkinKind {
    Font,
    Attribute,
    Gfx,
}

impl SkinKind {
    fn names(self) -> &'static [&'static str] {
        match self {
            Self::Font => &FONTS,
            Self::Attribute => &ATTRIBUTES,
            Self::Gfx => &GFXS,
        }
    }

    /// `cGui::GetSkin*FromString`: `LastEnum` (the table length) when unknown.
    pub fn from_name(self, name: &[u8]) -> u32 {
        let names = self.names();
        names
            .iter()
            .position(|n| n.as_bytes() == name)
            .map_or(names.len() as u32, |i| i as u32)
    }

    /// `cGui::GetSkin*String`: `"Unknown"` outside the table.
    pub fn name(self, index: u32) -> &'static str {
        usize::try_from(index)
            .ok()
            .and_then(|i| self.names().get(i))
            .copied()
            .unwrap_or("Unknown")
    }
}

/// `cGui::GetMaterial` slot: out-of-range types fall back to alpha.
pub fn material_slot(ty: u32) -> u32 {
    if ty < MATERIAL_LAST {
        ty
    } else {
        MATERIAL_ALPHA
    }
}

/// `(source, destination)` blend factors `cGuiMaterial_*::BeforeRender` sets; `None` for an
/// unknown material.
pub fn material_blend(ty: u32) -> Option<(i32, i32)> {
    match ty {
        0 => Some((BLEND_ONE, BLEND_ZERO)),
        1 | 2 => Some((BLEND_SRC_ALPHA, BLEND_ONE_MINUS_SRC_ALPHA)),
        3 => Some((BLEND_ONE, BLEND_ONE)),
        4 => Some((BLEND_DEST_COLOR, BLEND_ZERO)),
        5 => Some((BLEND_ONE, BLEND_ONE_MINUS_SRC_ALPHA)),
        _ => None,
    }
}

/// `cGui::TranslateGuiMouseToMouse`.
pub fn gui_mouse_to_mouse(button: i32) -> i32 {
    match button {
        0x001 => 0,
        0x002 => 1,
        0x004 => 2,
        0x008 => 3,
        0x010 => 4,
        0x020 => 5,
        0x040 => 6,
        0x080 => 7,
        0x100 => 8,
        _ => MOUSE_LAST,
    }
}

/// `cGui::TranslateMouseToGuiMouse`.
pub fn mouse_to_gui_mouse(button: i32) -> i32 {
    match button {
        0..=8 => 1 << button,
        _ => GUI_MOUSE_LAST,
    }
}
