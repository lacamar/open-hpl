#ifndef HPL_RUST_PHYS_BOOK_H
#define HPL_RUST_PHYS_BOOK_H

// H15: Rust-owned iPhysicsBody / iPhysicsJoint bookkeeping (OHPL_IMPL=rust). Tokens are arena handles; 0 means C++ fields.

#include "impl/RustBackends.h"
#include <cstdint>

extern "C" {
uint64_t ohpl_h15_body_new();
void ohpl_h15_body_free(uint64_t);
bool ohpl_h15_body_flag_get(uint64_t, int32_t);
void ohpl_h15_body_flag_set(uint64_t, int32_t, bool);
uint64_t ohpl_h15_joint_new();
void ohpl_h15_joint_free(uint64_t);
bool ohpl_h15_joint_limit_auto_sleep(uint64_t, bool, float, int32_t, float, float, float);
}

namespace hpl {

	enum OhplH15BodyFlag : int32_t { OhplH15_HasSlide = 0, OhplH15_HasCollision = 1, OhplH15_StaticMovement = 2, OhplH15_DisableAfterSim = 3 };

}

#endif
