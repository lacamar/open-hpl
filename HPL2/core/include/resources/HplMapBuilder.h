#ifndef HPL_MAP_BUILDER_H
#define HPL_MAP_BUILDER_H

// C ABI between the .map/.map_cache parser and the code that builds engine objects from it.
// Mirrored by rust/src/hplmap/abi.rs.

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
#define OHPL_NOEXCEPT noexcept
extern "C" {
#else
#define OHPL_NOEXCEPT
#endif

typedef struct { const char* p; size_t n; } OhplStr;
typedef struct { float x, y, z; } OhplF3;
typedef struct { float x, y; } OhplF2;
typedef struct { float r, g, b, a; } OhplF4;
typedef struct { float v[16]; } OhplMat4;
typedef const void* OhplXmlNode;

typedef void (*OhplNodeSink)(void*, OhplXmlNode) OHPL_NOEXCEPT;

// Read-only view of an element tree; nodes are opaque and stay valid while the tree lives.
typedef struct
{
	void* ctx;
	OhplXmlNode (*first_child)(void*, OhplXmlNode, OhplStr name) OHPL_NOEXCEPT;
	void (*children)(void*, OhplXmlNode, OhplNodeSink sink, void* sinkCtx) OHPL_NOEXCEPT;
	const char* (*attr)(void*, OhplXmlNode, OhplStr name) OHPL_NOEXCEPT;
	const char* (*value)(void*, OhplXmlNode) OHPL_NOEXCEPT;
} OhplXmlView;

typedef struct
{
	unsigned flags;
	bool logCache;
	OhplStr file;
} OhplMapOptions;

typedef struct { int type; OhplF3 size; OhplMat4 offset; } OhplCacheShape;

// Vertex array of a cached mesh. count = vertices * elemNum elements.
// compression != 0: data holds count floats whatever the format.
typedef struct
{
	int type;
	int format;
	int progVar;
	int elemNum;
	int compression;
	const void* data;
	size_t count;
} OhplCacheArray;

enum
{
	OHPL_MAP_PHASE_STATIC,
	OHPL_MAP_PHASE_STATIC_OBJECTS,
	OHPL_MAP_PHASE_PRIMITIVES,
	OHPL_MAP_PHASE_DECALS,
	OHPL_MAP_PHASE_COMBOS,
	OHPL_MAP_PHASE_ENTITIES
};

enum
{
	OHPL_MAP_OK,
	OHPL_MAP_NO_CONTENTS,
	OHPL_MAP_ABORT
};

typedef struct
{
	void* ctx;

	void (*log)(void*, OhplStr) OHPL_NOEXCEPT;
	void (*warning)(void*, OhplStr) OHPL_NOEXCEPT;
	void (*error)(void*, OhplStr) OHPL_NOEXCEPT;
	void (*phase)(void*, int phase, bool begin) OHPL_NOEXCEPT;

	void (*set_fog)(void*, bool active, OhplF4 color, float falloffExp, float start, float end, bool culling) OHPL_NOEXCEPT;
	void (*set_skybox)(void*, bool active, OhplF4 color, OhplStr texture) OHPL_NOEXCEPT;

	void (*static_begin)(void*) OHPL_NOEXCEPT;
	void (*static_object)(void*, OhplStr name, OhplStr file, OhplF3 pos, OhplF3 rot, OhplF3 scale,
							bool collides, bool castShadows, int id) OHPL_NOEXCEPT;
	void (*static_plane)(void*, OhplStr name, OhplStr material, bool castShadows, bool collides, int id,
							OhplF3 pos, OhplF3 scale, OhplF3 rot, OhplF3 startCorner, OhplF3 endCorner,
							const OhplF2* uv4) OHPL_NOEXCEPT;
	void (*static_decal)(void*, OhplStr name, OhplF4 color, int id, OhplStr material, OhplXmlNode decalMesh) OHPL_NOEXCEPT;
	void (*static_loading_done)(void*) OHPL_NOEXCEPT;
	void (*static_combo)(void*, int groupId, const int* ids, size_t n) OHPL_NOEXCEPT;
	void (*static_compile)(void*) OHPL_NOEXCEPT;

	void (*entities_begin)(void*) OHPL_NOEXCEPT;
	void (*entities_end)(void*) OHPL_NOEXCEPT;
	void (*entity)(void*, OhplStr name, int id, bool active, OhplF3 pos, OhplF3 rot, OhplF3 scale,
					OhplStr file, OhplXmlNode userVars) OHPL_NOEXCEPT;
	void (*area)(void*, OhplStr name, int id, bool active, OhplF3 pos, OhplF3 rot, OhplF3 scale,
					OhplStr areaType, OhplXmlNode userVars) OHPL_NOEXCEPT;
	void (*fog_area)(void*, OhplXmlNode) OHPL_NOEXCEPT;
	void (*particle_system)(void*, OhplXmlNode) OHPL_NOEXCEPT;
	void (*sound)(void*, OhplXmlNode) OHPL_NOEXCEPT;
	void (*billboard)(void*, OhplXmlNode) OHPL_NOEXCEPT;
	void (*light)(void*, OhplXmlNode) OHPL_NOEXCEPT;

	void (*cache_header)(void*, int meshBodies, int shapeBodies, int meshEntities) OHPL_NOEXCEPT;
	// The mesh shape follows in the cache buffer and is read by the builder.
	void (*cache_mesh_body)(void*, OhplStr name, OhplStr material, bool blocksLight, bool charCollider) OHPL_NOEXCEPT;
	void (*cache_shape_body)(void*, OhplStr material, const OhplMat4* transform, bool charCollider,
								bool blocksLight, const OhplCacheShape* colliders, size_t n) OHPL_NOEXCEPT;
	void (*cache_mesh)(void*, OhplStr name, OhplStr material, bool castShadows, int vertexNum,
						const OhplCacheArray* arrays, size_t nArrays, const unsigned* indices, size_t nIndices) OHPL_NOEXCEPT;
} OhplMapBuilder;

typedef struct { OhplStr name; OhplStr material; bool blocksLight; bool charCollider; } OhplCacheBodyOut;
typedef struct
{
	OhplStr material;
	OhplMat4 transform;
	bool charCollider;
	bool blocksLight;
	const OhplCacheShape* colliders;
	size_t nColliders;
} OhplCacheShapeBodyOut;
typedef struct
{
	OhplStr name;
	OhplStr material;
	bool castShadows;
	int vertexNum;
	const OhplCacheArray* arrays;
	size_t nArrays;
	const unsigned* indices;
	size_t nIndices;
} OhplCacheMeshOut;

// Supplies the static data of a world for writing its map cache; next_* are called in order.
typedef struct
{
	void* ctx;
	void (*error)(void*, OhplStr) OHPL_NOEXCEPT;
	void (*counts)(void*, int* meshBodies, int* shapeBodies, int* meshEntities) OHPL_NOEXCEPT;
	void (*next_body)(void*, OhplCacheBodyOut*) OHPL_NOEXCEPT;
	// Appends the mesh shape of the body just returned to the cache buffer.
	void (*write_body_shape)(void*) OHPL_NOEXCEPT;
	void (*next_shape_body)(void*, OhplCacheShapeBodyOut*) OHPL_NOEXCEPT;
	void (*next_mesh)(void*, OhplCacheMeshOut*) OHPL_NOEXCEPT;
} OhplMapCacheSource;

#ifdef __cplusplus
}
#endif

#endif
