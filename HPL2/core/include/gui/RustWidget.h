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

}

}

#endif // HPL_RUST_WIDGET_H
