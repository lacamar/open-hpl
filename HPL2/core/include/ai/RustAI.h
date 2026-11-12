#ifndef HPL_RUST_AI_H
#define HPL_RUST_AI_H

// C ABI of rust/src/ai/ffi.rs.

#include "math/RustTypes.h"
#include <cstddef>
#include <cstdint>

struct OhplAiGraph;

namespace hpl {
extern "C" {

typedef bool (*OhplAiCastFn)(void*, uint32_t, const cVector3f*, const cVector3f*);
typedef bool (*OhplAiCanAddFn)(void*, uint32_t, uint32_t);

OhplAiGraph* ohpl_ai_graph_new(OhplAiCastFn, void*, const cVector3f*);
void ohpl_ai_graph_free(OhplAiGraph*);
void ohpl_ai_graph_add_node(OhplAiGraph*, const cVector3f*, uint64_t);
void ohpl_ai_graph_set_params(OhplAiGraph*, int, int, float, float, bool);
void ohpl_ai_graph_build_grid(OhplAiGraph*);
void ohpl_ai_graph_compile(OhplAiGraph*);
uint32_t ohpl_ai_graph_edge_count(const OhplAiGraph*, uint32_t);
bool ohpl_ai_graph_edge(const OhplAiGraph*, uint32_t, uint32_t, uint32_t*, float*, float*);
bool ohpl_ai_graph_add_edge(OhplAiGraph*, uint32_t, uint32_t, float, float);
bool ohpl_ai_graph_connect(OhplAiGraph*, uint32_t, uint32_t);
bool ohpl_ai_graph_free_path(const OhplAiGraph*, const cVector3f*, const cVector3f*, int, uint32_t);
size_t ohpl_ai_graph_query(const OhplAiGraph*, const cVector3f*, float);
bool ohpl_ai_graph_astar(const OhplAiGraph*, const cVector3f*, const cVector3f*, int, OhplAiCanAddFn, void*);
void ohpl_ai_graph_name_node(OhplAiGraph*, uint32_t, const unsigned char*, size_t, int);
int64_t ohpl_ai_graph_find_name(const OhplAiGraph*, const unsigned char*, size_t);
int64_t ohpl_ai_graph_find_id(const OhplAiGraph*, int);
size_t ohpl_ai_graph_result(const OhplAiGraph*, uint32_t*, size_t);

}
}

#endif
