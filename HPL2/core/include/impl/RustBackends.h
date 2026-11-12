#ifndef HPL_RUST_BACKENDS_H
#define HPL_RUST_BACKENDS_H

#include <cstdlib>
#include <cstring>

namespace hpl {

// OHPL_IMPL=cpp|rust, read once. L stages consult it in their factories.
inline bool OhplBackendRustRequested()
{
	static const bool requested = []() {
		const char *v = std::getenv("OHPL_IMPL");
		return v && std::strcmp(v, "rust") == 0;
	}();
	return requested;
}

// Always the C++ path until an L stage flips its backend.
inline bool OhplBackendUseRust()
{
	return false;
}

} // namespace hpl

#endif
