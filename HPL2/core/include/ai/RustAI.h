#ifndef HPL_RUST_AI_H
#define HPL_RUST_AI_H

// C ABI of rust/src/ai/ffi.rs.

#include "math/RustTypes.h"
#include <cstddef>
#include <cstdint>

struct OhplAiGraph;
struct OhplAiNode;

namespace hpl {
extern "C" {

typedef bool (*OhplAiCastFn)(void*, uint32_t, const cVector3f*, const cVector3f*);
typedef bool (*OhplAiCanAddFn)(void*, uint32_t, uint32_t);

OhplAiGraph* ohpl_ai_graph_new(OhplAiCastFn, void*, const cVector3f*);
void ohpl_ai_graph_free(OhplAiGraph*);
void ohpl_ai_graph_set_params(OhplAiGraph*, int, int, float, float, bool);
void ohpl_ai_graph_build_grid(OhplAiGraph*);
void ohpl_ai_graph_compile(OhplAiGraph*);
bool ohpl_ai_graph_free_path(const OhplAiGraph*, const cVector3f*, const cVector3f*, int, uint32_t);
size_t ohpl_ai_graph_query(const OhplAiGraph*, const cVector3f*, float);
bool ohpl_ai_graph_astar(const OhplAiGraph*, const cVector3f*, const cVector3f*, int, OhplAiCanAddFn, void*);
size_t ohpl_ai_graph_result_nodes(const OhplAiGraph*, OhplAiNode**, size_t);
OhplAiNode* ohpl_ai_graph_add(OhplAiGraph*, const cVector3f*, const unsigned char*, size_t, int);
OhplAiNode* ohpl_ai_graph_node_at(const OhplAiGraph*, uint32_t);
size_t ohpl_ai_graph_node_count(const OhplAiGraph*);
OhplAiNode* ohpl_ai_graph_node_by_name(const OhplAiGraph*, const unsigned char*, size_t);
OhplAiNode* ohpl_ai_graph_node_by_id(const OhplAiGraph*, int);
bool ohpl_ai_graph_save(const OhplAiGraph*, const unsigned char*, size_t);
int ohpl_ai_graph_load(OhplAiGraph*, const unsigned char*, size_t, void (*)(void*, const char*, size_t), void*);

}
}

#endif
