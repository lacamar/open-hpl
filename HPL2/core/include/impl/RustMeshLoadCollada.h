#ifndef HPL_RUST_MESH_LOAD_COLLADA_H
#define HPL_RUST_MESH_LOAD_COLLADA_H

// C ABI of rust/src/meshload/collada_ffi.rs.

#include <cstddef>
#include <cstdint>

namespace hpl {

	enum
	{
		eOhplColladaImages = 1, eOhplColladaTextures = 2, eOhplColladaMaterials = 4, eOhplColladaLights = 8,
		eOhplColladaGeometry = 16, eOhplColladaControllers = 32, eOhplColladaAnims = 64, eOhplColladaScene = 128,
		eOhplColladaConvertUnit = 256, eOhplColladaConvertUnitAny = 512
	};

	struct OhplColladaBlob;

extern "C" {

// 0 ok (blob holds the result), 1 XML parse error, 2 input the original crashes on (blob holds the reason)
int ohpl_collada_parse(const uint8_t* data, size_t len, bool readOk, uint32_t flags, OhplColladaBlob** blob);
const uint8_t* ohpl_collada_blob_data(const OhplColladaBlob*);
size_t ohpl_collada_blob_len(const OhplColladaBlob*);
void ohpl_collada_free(OhplColladaBlob*);

}

}

#endif
