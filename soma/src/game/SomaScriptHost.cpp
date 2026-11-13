#include "RustSomaScriptS2.h"
#include "SomaImGui.h"
#include "SomaImGuiDefaults.h"
#include "SomaLux.h"
#include "SomaLuxScriptable.h"
#include "SomaScriptNatives.h"
#include "SomaAgent.h"
#include "SomaCritter.h"
#include "SomaLuxEntity.h"
#include "SomaLuxGame.h"
#include "SomaPostEffects.h"
#include "SomaSave.h"
#include "SomaSound.h"
#include "SomaSoundscape.h"
#include "SomaToneMapping.h"
#include "SomaGlobalFuncsTable.h"

#include "impl/scriptarray.h"

#include <string>

const cSomaStructDefaults *FindStructDefaults(const char *apType);
extern std::string gsSomaExecOutput;
int SomaNativeOffset(const std::string &asType, const std::string &asDecl, int alOffset);

namespace
{
	std::string S(const unsigned char *p, size_t n) { return std::string((const char *)p, n); }

	int StructDefaults(const unsigned char *apType, size_t alLen, OhplS2Defaults *apOut)
	{
		const cSomaStructDefaults *d = FindStructDefaults(S(apType, alLen).c_str());
		if (d == NULL)
			return 0;
		apOut->size = d->mlSize;
		apOut->bytes = (const unsigned char *)d->mpBytes;
		apOut->nstr = d->mvStringOffsets.size();
		apOut->str_offsets = d->mvStringOffsets.data();
		return 1;
	}

	const void *InternEmpty() { return SomaIntern(""); }

	int NativeOffset(const unsigned char *apType, size_t alType, const unsigned char *apDecl, size_t alDecl, int alOffset)
	{
		return SomaNativeOffset(S(apType, alType), S(apDecl, alDecl), alOffset);
	}

	void RegisterStep(void *apEngine, unsigned alStep)
	{
		asIScriptEngine *e = (asIScriptEngine *)apEngine;
		switch (alStep)
		{
		case kOhplS2StepLux:
			RegisterSomaScriptLuxNatives(e);
			break;
		case kOhplS2StepGlobalEngine:
			SomaRegisterGlobalEngineBlock(e);
			break;
		default:
			cSomaImGui::RegisterNatives(e);
			cSomaLuxEntity::RegisterNatives(e);
			cSomaLuxGame::RegisterNatives(e);
			cSomaSoundEvents::RegisterNatives(e);
			cSomaSoundscape::RegisterNatives(e);
			cSomaSaveHandler::RegisterNatives(e);
			cSomaToneMapping::RegisterNatives(e);
			cSomaPostEffects::RegisterNatives(e);
			SomaRegisterCritterNatives(e);
			SomaRegisterAgentNatives(e);
		}
	}

	std::multiset<std::string> &Set(void *apSet) { return SomaStringSetOf(apSet); }
	const std::string &Str(const void *apStr) { return *(const std::string *)apStr; }

	void SetReset(void *apSet) { Set(apSet).clear(); }
	void SetAssign(void *apDst, void *apSrc) { Set(apDst) = Set(apSrc); }
	void SetAdd(void *apSet, const void *apStr) { Set(apSet).insert(Str(apStr)); }
	void SetErase(void *apSet, const void *apStr) { Set(apSet).erase(Str(apStr)); }
	int SetCount(void *apSet, const void *apStr) { return (int)Set(apSet).count(Str(apStr)); }
	int SetSize(void *apSet) { return (int)Set(apSet).size(); }
	void SetToArray(void *apSet, void *apArr)
	{
		CScriptArray &a = *(CScriptArray *)apArr;
		a.Resize(0);
		for (const std::string &s : Set(apSet))
			a.InsertLast((void *)&s);
	}

	CScriptArray &Arr(void *apArr) { return *(CScriptArray *)apArr; }
	void ArrInsertLast(void *apArr, void *apValue) { Arr(apArr).InsertLast(apValue); }
	void ArrInsertAt(void *apArr, unsigned alIdx, void *apValue) { Arr(apArr).InsertAt(alIdx, apValue); }
	void ArrRemoveLast(void *apArr) { Arr(apArr).RemoveLast(); }
	void ArrRemoveAt(void *apArr, unsigned alIdx) { Arr(apArr).RemoveAt(alIdx); }
	unsigned ArrSize(const void *apArr) { return ((const CScriptArray *)apArr)->GetSize(); }
	void ArrResize(void *apArr, unsigned alSize) { Arr(apArr).Resize(alSize); }

	bool RunGlobalFunc(const unsigned char *apObj, size_t alObj, const unsigned char *apClass, size_t alClass, const unsigned char *apFunc, size_t alFunc)
	{
		return SomaRunGlobalFunc(S(apObj, alObj), S(apClass, alClass), S(apFunc, alFunc));
	}

	const void *GenOp(size_t alIdx) { return (const void *)SomaScriptGenBindingFunc(alIdx); }

	bool GlobalFuncs(size_t alIdx, const unsigned char **appDecl, const unsigned char **appClass, const unsigned char **appFunc)
	{
		if ((int)alIdx >= glSomaGlobalFuncsNum)
			return false;
		*appDecl = (const unsigned char *)gvSomaGlobalFuncs[alIdx].mpDecl;
		*appClass = (const unsigned char *)gvSomaGlobalFuncs[alIdx].mpClass;
		*appFunc = (const unsigned char *)gvSomaGlobalFuncs[alIdx].mpFunc;
		return true;
	}

	int Misc(unsigned alKind, int alArg, const unsigned char *apText, size_t alLen)
	{
		switch (alKind)
		{
		case kOhplS2MiscUnderwaterGet:
			return gbSomaUnderwaterEffects;
		case kOhplS2MiscUnderwaterSet:
			gbSomaUnderwaterEffects = alArg != 0;
			return 0;
		case kOhplS2MiscUnderwaterInc:
			++glSomaUnderwaterUsers;
			return 0;
		case kOhplS2MiscUnderwaterDec:
			--glSomaUnderwaterUsers;
			return 0;
		case kOhplS2MiscUnderwaterUsers:
			return glSomaUnderwaterUsers;
		case kOhplS2MiscExecPrint:
			gsSomaExecOutput += S(apText, alLen);
			return 0;
		default:
		{
			std::string sText = S(apText, alLen);
			(alArg == 0 ? Log : alArg == 1 ? Warning : Error)("%s", sText.c_str());
			return 0;
		}
		}
	}

	const OhplS2HostOps gHost = {StructDefaults, InternEmpty, NativeOffset, RegisterStep, SetReset, SetAssign, SetAdd, SetErase, SetCount, SetSize, SetToArray,
								 ArrInsertLast, ArrInsertAt, ArrRemoveLast, ArrRemoveAt, ArrSize, ArrResize, RunGlobalFunc, SomaScriptGenBindingCount,
								 GenOp, GlobalFuncs, SomaEasing, Misc};
}

void OhplS2InstallHost()
{
	ohpl_s2_install_host(&gHost);
}
