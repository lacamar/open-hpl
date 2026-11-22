#ifndef SOMA_RUST_AGENT_S4_H
#define SOMA_RUST_AGENT_S4_H

// C ABI of rust/src/somaagent/s4.rs

#include <cstddef>
#include <cstdint>

struct OhplSoGridHost
{
	void *ctx;
	float (*floor_dist)(void *, const float *, const float *);
	bool (*slab_hit)(void *, const float *);
	bool (*clear)(void *, bool, const float *);
};

extern "C"
{
	void ohpl_so_grid_nav_path(const OhplSoGridHost *host, const float *from, const float *to, int max_expand, void *ctx, void (*sink)(void *, const unsigned char *, size_t));
	void ohpl_s4_nav_path_text(const float *nodes, size_t n, void *ctx, void (*sink)(void *, const unsigned char *, size_t));
}

#endif
