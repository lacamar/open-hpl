#ifndef HPL_RUST_RENDERABLE_CONTAINER_H
#define HPL_RUST_RENDERABLE_CONTAINER_H

// C ABI of rust/src/scene/ffi.rs.

#include "math/RustTypes.h"
#include <cstddef>
#include <cstdint>

struct OhplBoxTree;

namespace hpl {
	struct OhplRcObject
	{
		cVector3f mvMin;
		cVector3f mvMax;
		cVector3f mvCenter;
	};

	struct OhplRcParams
	{
		int32_t mlMinLeafObjects;
		float mfMinSideLength;
		float mfMaxSideLength;
		float mfMaxIntersectionAmount;
		int32_t mlMaxVolumeCalcObjects;
		float mfMinForceIntersectionRelativeSize;
	};

	struct OhplRcNode
	{
		cVector3f mvMin;
		cVector3f mvMax;
		cVector3f mvCenter;
		float mfRadius;
		uint32_t mlParent;
		uint32_t mlFirstObject;
		uint32_t mlObjectCount;
	};

	static_assert(sizeof(OhplRcObject) == 36 && sizeof(OhplRcParams) == 24 && sizeof(OhplRcNode) == 52, "scene layout");

	static const uint32_t kOhplRcNoParent = 0xFFFFFFFFu;

extern "C" {

int ohpl_rc_split_group(const cVector3f*, const cVector3f*, float, int, const cVector3f*, float);
bool ohpl_rc_at_edge(const cVector3f*, const cVector3f*, const cVector3f*, const cVector3f*);
int ohpl_rc_longest_axis(const cVector3f*);

OhplBoxTree* ohpl_rc_boxtree_compile(const OhplRcObject*, size_t, const OhplRcParams*);
size_t ohpl_rc_boxtree_node_count(const OhplBoxTree*);
const OhplRcNode* ohpl_rc_boxtree_nodes(const OhplBoxTree*);
const uint32_t* ohpl_rc_boxtree_objects(const OhplBoxTree*);
void ohpl_rc_boxtree_free(OhplBoxTree*);

}
}

#endif
