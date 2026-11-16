#ifndef SOMA_RUST_AGENT_S4_H
#define SOMA_RUST_AGENT_S4_H

// C ABI of rust/src/somaagent/s4.rs

#include <cstddef>
#include <cstdint>

extern "C"
{
	void ohpl_s4_nav_path_text(const float *nodes, size_t n, void *ctx, void (*sink)(void *, const unsigned char *, size_t));
}

#endif
