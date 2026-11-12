// Saves and reloads probe objects through the real cSerializeClass; run under OHPL_IMPL=cpp and rust.
#include "system/SerializeClass.h"
#include "system/Platform.h"
#include "system/String.h"
#include "math/MathTypes.h"
#include "graphics/GraphicsTypes.h"
#include <cstdarg>
#include <cstdio>

using namespace hpl;

namespace hpl {
	void Log(const char* fmt, ...) { va_list a; va_start(a, fmt); vfprintf(stderr, fmt, a); va_end(a); }
	void Warning(const char* fmt, ...) { va_list a; va_start(a, fmt); vfprintf(stderr, fmt, a); va_end(a); }
	void Error(const char* fmt, ...) { va_list a; va_start(a, fmt); vfprintf(stderr, fmt, a); va_end(a); }
	FILE *cPlatform::OpenFile(const tWString& f, const tWString m) { return fopen(cString::To8Char(f).c_str(), cString::To8Char(m).c_str()); }
}

class cProbeInner : public iSerializable
{
	kSerializableClassInit(cProbeInner)
public:
	cProbeInner() : mlA(0), mfB(0) {}
	int mlA;
	float mfB;
};

class cProbe : public iSerializable
{
	kSerializableClassInit(cProbe)
public:
	cProbe() : mbFlag(false), mlNum(0), mfNum(0), mpInner(NULL) { for(int i=0;i<3;i++) mvArr[i]=0; }
	bool mbFlag;
	int mlNum;
	float mfNum;
	tString msStr;
	tWString msWStr;
	cVector3f mvVec;
	cColor mCol;
	int mvArr[3];
	cProbeInner mInner;
	cProbeInner *mpInner;
};

kBeginSerializeBase(cProbeInner)
kSerializeVar(mlA, eSerializeType_Int32)
kSerializeVar(mfB, eSerializeType_Float32)
kEndSerialize()

kBeginSerializeBase(cProbe)
kSerializeVar(mbFlag, eSerializeType_Bool)
kSerializeVar(mlNum, eSerializeType_Int32)
kSerializeVar(mfNum, eSerializeType_Float32)
kSerializeVar(msStr, eSerializeType_String)
kSerializeVar(msWStr, eSerializeType_WString)
kSerializeVar(mvVec, eSerializeType_Vector3f)
kSerializeVar(mCol, eSerializeType_Color)
kSerializeVarArray(mvArr, eSerializeType_Int32, 3)
kSerializeVar(mInner, eSerializeType_Class)
kSerializeVar(mpInner, eSerializeType_ClassPointer)
kEndSerialize()

int main(int argc, char **argv)
{
	if(argc < 2) return 2;
	cProbe p;
	p.mbFlag = true; p.mlNum = -42; p.mfNum = 3.25f; p.msStr = "a<b>&\"q\""; p.msWStr = L"wå中";
	p.mvVec = cVector3f(1.5f, -2, 3); p.mCol = cColor(0.25f, 0.5f, 0.75f, 1);
	p.mvArr[0] = 7; p.mvArr[1] = 8; p.mvArr[2] = 9;
	p.mInner.mlA = 5; p.mInner.mfB = 0.5f;
	p.mpInner = new cProbeInner(); p.mpInner->mlA = 11; p.mpInner->mfB = 2;
	tWString sFile = cString::To16Char(argv[1]);
	if(!cSerializeClass::SaveToFile(&p, sFile, "Probe")) return 1;

	cProbe q;
	if(!cSerializeClass::LoadFromFile(&q, sFile)) return 1;
	bool ok = q.mbFlag == p.mbFlag && q.mlNum == p.mlNum && q.mfNum == p.mfNum && q.msStr == p.msStr && q.msWStr == p.msWStr
		&& q.mvVec == p.mvVec && q.mvArr[2] == 9 && q.mInner.mlA == 5 && q.mpInner && q.mpInner->mlA == 11;
	printf("roundtrip %s\n", ok ? "ok" : "MISMATCH");
	return ok ? 0 : 1;
}
