#include "../HPL2/core/include/impl/RustBackends.h"

extern "C" bool ohpl_cshim_backend_rust_requested()
{
	return hpl::OhplBackendRustRequested();
}
