#ifndef LUX_RUST_SERIALIZE_H
#define LUX_RUST_SERIALIZE_H

#include "system/SerializeClass.h"

// Field table and layout are Rust-owned (rust/src/luxsave/layout*.rs); C++ supplies the factory.
#define kSerializeRustClass(aClass) kSerializeRustClassP(aClass, )

#define kSerializeRustClassP(aClass, aParent) \
	static iSerializable *Create_##aClass() { return hplNew(aClass, ()); } \
	static cSerializeClass gSerializedClass_##aClass(#aClass, #aParent, NULL, sizeof(aClass), Create_##aClass);

#define kSerializeRustClassV(aClass, aParent) \
	static cSerializeClass gSerializedClass_##aClass(#aClass, #aParent, NULL, sizeof(aClass), NULL);

#endif
