#ifndef LUX_RUST_SAVE_A8_H
#define LUX_RUST_SAVE_A8_H

// C ABI of rust/src/luxsave/a8_props.rs: A5 prop state <-> save-data object, addressed by the
// registered field table. `d` is the prop's SaveData object.

#include "RustLuxPropA5.h"

extern "C"
{
void ohpl_a8_button_save(const OhplA5Button *s, void *d);
void ohpl_a8_button_load(OhplA5Button *s, void *d);
void ohpl_a8_photocell_save(const OhplA5Photocell *s, void *d);
void ohpl_a8_photocell_load(OhplA5Photocell *s, void *d);
void ohpl_a8_door_save(const OhplA5Door *s, void *d);
void ohpl_a8_door_load(OhplA5Door *s, void *d);
void ohpl_a8_oil_save(const OhplA5Oil *s, void *d);
void ohpl_a8_oil_load(OhplA5Oil *s, void *d);
}

#endif // LUX_RUST_SAVE_A8_H
