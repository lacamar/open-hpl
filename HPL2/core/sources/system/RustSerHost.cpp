#include "system/SerializeClass.h"

#include "system/LowLevelSystem.h"
#include "system/RustSystem.h"
#include "system/Container.h"
#include "graphics/GraphicsTypes.h"
#include "math/MathTypes.h"

#include <new>

static_assert(sizeof(hpl::cVector2l) == 8 && sizeof(hpl::cVector2f) == 8 && sizeof(hpl::cVector3l) == 12 &&
			  sizeof(hpl::cVector3f) == 12 && sizeof(hpl::cMatrixf) == 64 && sizeof(hpl::cColor) == 16 &&
			  sizeof(hpl::cRect2l) == 16 && sizeof(hpl::cRect2f) == 16 && sizeof(hpl::cPlanef) == 16 &&
			  sizeof(wchar_t) == 4 && sizeof(bool) == 1 && sizeof(int) == 4,
			  "serialized value layouts are mirrored in rust/src/system/serialize.rs");

using namespace hpl;

const OhplSerHost* hpl::cSerializeClass::GetHost()
{
	enum { kString = 4, kWString = 14 };
	static const OhplSerHost host = {
		[](int alLevel, const uint8_t *apMsg, size_t alLen) {
			tString sMsg((const char*)apMsg, alLen);
			if(alLevel == 0) Log("%s", sMsg.c_str());
			else if(alLevel == 1) Warning("%s", sMsg.c_str());
			else Error("%s", sMsg.c_str());
		},
		[](void *apObj, void (*apSink)(void*, const uint8_t*, size_t), void *apCtx) {
			tString sName = static_cast<iSerializable*>(apObj)->Serialize_GetTopClass();
			apSink(apCtx, (const uint8_t*)sName.data(), sName.size());
		},
		[](void *apObj) { iSerializable *pObj = static_cast<iSerializable*>(apObj); hplDelete(pObj); },
		[](unsigned alType, size_t alSize) -> void* {
			void *pData = hplMalloc(alSize);
			if(alType == kString) new(pData) tString();
			else if(alType == kWString) new(pData) tWString();
			return pData;
		},
		[](unsigned alType, void *apData) {
			if(alType == kString) static_cast<tString*>(apData)->~tString();
			else if(alType == kWString) static_cast<tWString*>(apData)->~tWString();
			hplFree(apData);
		},
		[](void *apStr, size_t *apLen) -> const uint8_t* {
			const tString &sStr = *static_cast<tString*>(apStr);
			*apLen = sStr.size();
			return (const uint8_t*)sStr.data();
		},
		[](void *apStr, const uint8_t *apData, size_t alLen) {
			static_cast<tString*>(apStr)->assign((const char*)apData, alLen);
		},
		[](void *apStr, size_t *apLen) -> const uint32_t* {
			const tWString &sStr = *static_cast<tWString*>(apStr);
			*apLen = sStr.size();
			return (const uint32_t*)sStr.data();
		},
		[](void *apStr, const uint32_t *apData, size_t alLen) {
			static_cast<tWString*>(apStr)->assign((const wchar_t*)apData, alLen);
		},
		sizeof(tString),
		sizeof(tWString),
		[](void *apCont) -> size_t { return static_cast<iContainer*>(apCont)->Size(); },
		[](void *apCont) { static_cast<iContainer*>(apCont)->Clear(); },
		[](void *apCont, void *apPtr) { static_cast<iContainer*>(apCont)->AddVoidPtr(&apPtr); },
		[](void *apCont, void *apElem) { static_cast<iContainer*>(apCont)->AddVoidClass(apElem); },
		[](void *apCont) -> void* { return static_cast<iContainer*>(apCont)->CreateIteratorPtr(); },
		[](void *apIt) -> bool { return static_cast<iContainerIterator*>(apIt)->HasNext(); },
		[](void *apIt) -> void* { return static_cast<iContainerIterator*>(apIt)->NextPtr(); },
		[](void *apIt) { iContainerIterator *pIt = static_cast<iContainerIterator*>(apIt); hplDelete(pIt); },
	};
	return &host;
}
