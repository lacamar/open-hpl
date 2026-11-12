#ifndef OHPL_CSHIM_SHIMS_H
#define OHPL_CSHIM_SHIMS_H

#include <stddef.h>
#include <stdint.h>

/* Structs shared with rust/src/backend/media (oal.rs RawInit, video.rs RawYuv). */

typedef struct OhplOalInit {
	const char* device_name;
	int32_t output_freq;
	int32_t update_freq;
	int32_t num_sources_hint;
	int32_t min_mono_hint;
	int32_t min_stereo_hint;
	int32_t stream_buffer_size;
	int32_t stream_buffer_count;
	uint8_t use_thread, voice_management, use_efx;
} OhplOalInit;

typedef struct OhplThYuv {
	const unsigned char *y, *u, *v;
	size_t y_len, u_len, v_len;
	int y_stride, uv_stride;
} OhplThYuv;

/* assimp_shim.cpp <-> rust/src/backend/meshio/assimp.rs: the shim walks the aiScene and calls the sink. */

typedef struct OhplAiWeight {
	uint32_t vertex;
	float weight;
} OhplAiWeight;

typedef struct OhplAiBone {
	const char* name;
	size_t name_len;
	float offset[16];
	uint32_t weight_num;
	const OhplAiWeight* weights;
} OhplAiBone;

typedef struct OhplAiMesh {
	uint32_t primitive_types;
	uint32_t vertex_num;
	const float *vertices, *normals, *uv0, *tangents, *bitangents;
	uint32_t face_num;
	const uint32_t* indices; /* 3 per face */
	uint32_t material;
	uint32_t bone_num;
	const OhplAiBone* bones;
} OhplAiMesh;

typedef struct OhplAiSink {
	void* ctx;
	void (*scene)(void*, float unit_scale);
	void (*node)(void*, int32_t parent, const char* name, size_t name_len, const float* m16, const uint32_t* meshes, size_t mesh_num);
	void (*mesh)(void*, const OhplAiMesh*);
	void (*material)(void*, const char* texture, size_t texture_len, const char* name, size_t name_len);
} OhplAiSink;

#endif
