#ifndef HPL_MAP_PARSE_H
#define HPL_MAP_PARSE_H

#include "resources/HplMapBuilder.h"

namespace hpl {

	class cBinaryBuffer;
	class iXmlNode;

	// Reads a whole map cache from the buffer, driving the builder. False if the cache was rejected
	// before anything was built.
	bool HplMapCacheRead(cBinaryBuffer& aBuff, const OhplMapBuilder *apBuilder, const OhplMapOptions *apOptions);

	// Appends a whole map cache, with the total size patched in, to the buffer.
	void HplMapCacheWrite(cBinaryBuffer& aBuff, const OhplMapCacheSource *apSource);

	// Walks the MapData element of the document, driving the builder. Returns an OHPL_MAP_* code.
	int HplMapParse(iXmlNode *apDoc, const OhplMapBuilder *apBuilder, const OhplMapOptions *apOptions, bool abCacheLoaded);

}

#endif
