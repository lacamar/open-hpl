#include "resources/HplMapParse.h"

#include "resources/BinaryBuffer.h"
#include "resources/XmlDocument.h"

// C ABI of rust/src/hplmap/abi.rs.
extern "C" {
	int ohpl_map_parse(const OhplXmlView *view, OhplXmlNode doc, const OhplMapBuilder *builder,
						const OhplMapOptions *options, bool cacheLoaded);
	bool ohpl_map_cache_read(OhplBinaryBuffer *buffer, const OhplMapBuilder *builder, const OhplMapOptions *options);
	void ohpl_map_cache_write(OhplBinaryBuffer *buffer, const OhplMapCacheSource *source);
}

namespace hpl {

	static OhplXmlNode ViewFirstChild(void*, OhplXmlNode apNode, OhplStr aName) noexcept
	{
		iXmlNode *pNode = (iXmlNode*)apNode;
		return pNode->GetFirstElement(tString(aName.p, aName.n));
	}

	static void ViewChildren(void*, OhplXmlNode apNode, OhplNodeSink apSink, void *apSinkCtx) noexcept
	{
		cXmlNodeListIterator it = ((iXmlNode*)apNode)->GetChildIterator();
		while(it.HasNext()) apSink(apSinkCtx, it.Next()->ToElement());
	}

	static const char* ViewAttr(void*, OhplXmlNode apNode, OhplStr aName) noexcept
	{
		return ((cXmlElement*)apNode)->GetAttribute(tString(aName.p, aName.n));
	}

	static const char* ViewValue(void*, OhplXmlNode apNode) noexcept
	{
		return ((iXmlNode*)apNode)->GetValue().c_str();
	}

	int HplMapParse(iXmlNode *apDoc, const OhplMapBuilder *apBuilder, const OhplMapOptions *apOptions, bool abCacheLoaded)
	{
		OhplXmlView view = { NULL, ViewFirstChild, ViewChildren, ViewAttr, ViewValue };
		return ohpl_map_parse(&view, apDoc, apBuilder, apOptions, abCacheLoaded);
	}

	bool HplMapCacheRead(cBinaryBuffer& aBuff, const OhplMapBuilder *apBuilder, const OhplMapOptions *apOptions)
	{
		return ohpl_map_cache_read(aBuff.GetImpl(), apBuilder, apOptions);
	}

	void HplMapCacheWrite(cBinaryBuffer& aBuff, const OhplMapCacheSource *apSource)
	{
		ohpl_map_cache_write(aBuff.GetImpl(), apSource);
	}

	static_assert(sizeof(OhplStr) == 16, "OhplStr");
	static_assert(sizeof(OhplMapOptions) == 24, "OhplMapOptions");
	static_assert(sizeof(OhplCacheShape) == 80, "OhplCacheShape");
	static_assert(sizeof(OhplCacheArray) == 40, "OhplCacheArray");
	static_assert(sizeof(OhplCacheBodyOut) == 40, "OhplCacheBodyOut");
	static_assert(sizeof(OhplCacheShapeBodyOut) == 104, "OhplCacheShapeBodyOut");
	static_assert(sizeof(OhplCacheMeshOut) == 72, "OhplCacheMeshOut");
}
