#ifndef HPL_RUST_WIDGET_H
#define HPL_RUST_WIDGET_H

// C ABI of rust/src/gui/ffi_widget.rs. The widget classes stay C++; Rust decides, C++ applies.

#include <cstddef>
#include <cstdint>
#include "math/RustTypes.h"
#include "gui/GuiTypes.h"
#include "gui/GuiSet.h"
#include "gui/GuiGfxElement.h"
#include "graphics/FontData.h"
#include "system/String.h"

namespace hpl {

extern "C" {
int ohpl_wd_file_type(const wchar_t* ext, size_t len);
uint64_t ohpl_wd_human_size(uint64_t size, size_t* unit);
size_t ohpl_wd_drive_letters(uint64_t mask, wchar_t* out, size_t cap);
int ohpl_wd_navigate_target(const wchar_t* path, size_t len);
int ohpl_wd_path_piece(const wchar_t* piece, size_t len, size_t depth);

}

}

#endif // HPL_RUST_WIDGET_H
