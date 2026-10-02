#include "SomaSave.h"
#include "SomaBase.h"
#include "SomaLux.h"
#include "SomaLuxEntity.h"
#include "SomaLuxPlayer.h"
#include "SomaScriptBind.h"
#include "SomaScriptBuilder.h"

#include "impl/scriptarray.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <set>
#include <sstream>
#include <sys/stat.h>
#include <ctime>


namespace
{
	const char kMagic[] = "OHPLSAV3", kMagicV2[] = "OHPLSAV2";

	tString gsMapFile, gsStartPos;
	bool gbExplorationMode = false;
	std::string gsPendingState;
	bool gbHoldAfterLoad = false;
	int glSaveNameCount = 0;
	cDate gLatestSaveDate;
	const int kMaxAutoSaves = 20; // game.cfg Saving/MaxAutoSaves

	tWString GetSaveName(const tWString &asPrefix)
	{
		cSomaLuxMap *pMap = cSomaLuxMap::GetCurrent();
		tString sMapName = pMap && pMap->msDisplayNameEntry != "" ? pMap->msDisplayNameEntry : "NULL";
		cDate d = cPlatform::GetDate();
		tWString sName = asPrefix + _W("_") + cString::To16Char(sMapName);
		for (int lX : {d.year, d.month + 1, d.month_day, d.hours, d.minutes, d.seconds, glSaveNameCount})
			sName += _W("_") + cString::ToStringW(lX);
		if (++glSaveNameCount >= 100 || d != gLatestSaveDate)
			glSaveNameCount = 0;
		gLatestSaveDate = d;
		return sName + _W(".sav");
	}

	void DeleteOldestSaveFiles(const tWString &asDir, int alMax)
	{
		tWStringList lstFiles;
		cPlatform::FindFilesInDir(lstFiles, asDir, _W("*.sav"));
		std::vector<std::pair<cDate, tWString>> vFiles;
		for (const tWString &sFile : lstFiles)
			vFiles.push_back({cPlatform::FileModifiedDate(asDir + sFile), sFile});
		std::sort(vFiles.begin(), vFiles.end(), [](const auto &a, const auto &b) { return a.first < b.first; });
		for (size_t i = 0; (int)(vFiles.size() - i) >= alMax; ++i)
			cPlatform::RemoveFile(asDir + vFiles[i].second);
	}

	// HPL3 lists "Prefix_map_Y_M_D_h_m_s_n" saves as "Prefix - <level> - ", "D/M-Y hh:mm:ss"
	void GetProperSaveName(const tWString &asFile, tWString &asName, tString &asDate)
	{
		tWStringVec vParts;
		tWString sSep = _W("_");
		cString::GetStringVecW(cString::SetFileExtW(asFile, _W("")), vParts, &sSep);
		if (vParts.size() != 9)
		{
			asName = cString::SetFileExtW(asFile, _W(""));
			struct stat st = {};
			stat(cString::To8Char(cSomaSaveHandler::GetSaveDir() + asFile).c_str(), &st);
			struct tm t = {};
			localtime_r(&st.st_mtime, &t);
			char vBuf[64];
			snprintf(vBuf, sizeof(vBuf), "%d/%d-%d %d:%02d:%d", t.tm_mday, t.tm_mon + 1, t.tm_year + 1900, t.tm_hour, t.tm_min, t.tm_sec);
			asDate = vBuf;
			return;
		}
		asName = vParts[0] + _W(" - ") + gpSomaBase->mpEngine->GetResources()->Translate("Levels", cString::To8Char(vParts[1])) + _W(" - ");
		for (int i = 5; i <= 7; ++i)
			if (vParts[i].size() == 1)
				vParts[i] = _W("0") + vParts[i];
		asDate = cString::To8Char(vParts[4] + _W("/") + vParts[3] + _W("-") + vParts[2] + _W(" ") + vParts[5] + _W(":") + vParts[6] + _W(":") + vParts[7]);
	}

	struct cOut
	{
		std::string s;
		void Bytes(const void *p, size_t n) { s.append((const char *)p, n); }
		template <class T> void Pod(const T &x) { Bytes(&x, sizeof(T)); }
		void Str(const std::string &x)
		{
			Pod((uint32_t)x.size());
			s += x;
		}
	};

	struct cIn
	{
		const std::string &s;
		size_t p = 0;
		bool ok = true;
		explicit cIn(const std::string &x) : s(x) {}
		bool Bytes(void *d, size_t n)
		{
			if (ok == false || p + n > s.size())
				return ok = false;
			memcpy(d, s.data() + p, n);
			p += n;
			return true;
		}
		template <class T> T Pod()
		{
			T x{};
			Bytes(&x, sizeof(T));
			return x;
		}
		std::string Str()
		{
			uint32_t n = Pod<uint32_t>();
			if (ok == false || p + n > s.size())
			{
				ok = false;
				return "";
			}
			std::string r = s.substr(p, n);
			p += n;
			return r;
		}
		void SkipObject()
		{
			uint32_t n = Pod<uint32_t>();
			for (uint32_t i = 0; i < n && ok; ++i)
			{
				Str();
				Str();
				p += Pod<uint32_t>();
			}
		}
	};

	// Script objects: properties by name and declaration, [nosave]/[volatile] members skipped

	void WriteObject(cOut &o, asIScriptObject *apObj, std::set<void *> &aVisited);
	void ReadObject(cIn &in, asIScriptObject *apObj);

	bool IsString(asITypeInfo *t) { return strcmp(t->GetName(), "tString") == 0 || strcmp(t->GetName(), "string") == 0; }
	bool IsWString(asITypeInfo *t) { return strcmp(t->GetName(), "tWString") == 0; }
	bool IsArray(asITypeInfo *t) { return strcmp(t->GetName(), "array") == 0; }
	bool IsRawValue(asITypeInfo *t) { return (t->GetFlags() & asOBJ_VALUE) && t->GetSize() > 0 && t->GetSize() <= 256; }

	void WriteValue(cOut &o, asIScriptEngine *e, int alTypeId, const void *apAddr, std::set<void *> &aVisited)
	{
		if (alTypeId & asTYPEID_OBJHANDLE)
		{
			void *pObj = *(void *const *)apAddr;
			asITypeInfo *t = e->GetTypeInfoById(alTypeId);
			if (pObj == NULL || t == NULL || (t->GetFlags() & asOBJ_SCRIPT_OBJECT) == 0 || aVisited.count(pObj))
			{
				o.Pod((uint8_t)0);
				return;
			}
			o.Pod((uint8_t)1);
			o.Str(((asIScriptObject *)pObj)->GetObjectType()->GetName());
			WriteObject(o, (asIScriptObject *)pObj, aVisited);
			return;
		}
		if (alTypeId <= asTYPEID_DOUBLE)
		{
			o.Bytes(apAddr, e->GetSizeOfPrimitiveType(alTypeId));
			return;
		}
		asITypeInfo *t = e->GetTypeInfoById(alTypeId);
		if (t == NULL)
			return;
		if (t->GetFlags() & asOBJ_ENUM)
			o.Bytes(apAddr, 4);
		else if (t->GetFlags() & asOBJ_SCRIPT_OBJECT)
			WriteObject(o, (asIScriptObject *)apAddr, aVisited);
		else if (IsString(t))
			o.Str(*(const std::string *)apAddr);
		else if (IsWString(t))
		{
			const tWString &w = *(const tWString *)apAddr;
			o.Pod((uint32_t)w.size());
			o.Bytes(w.data(), w.size() * sizeof(wchar_t));
		}
		else if (IsArray(t))
		{
			CScriptArray *pArr = (CScriptArray *)apAddr;
			o.Pod((uint32_t)pArr->GetSize());
			for (asUINT i = 0; i < pArr->GetSize(); ++i)
				WriteValue(o, e, pArr->GetElementTypeId(), pArr->At(i), aVisited);
		}
		else if (IsRawValue(t))
			o.Bytes(apAddr, t->GetSize());
	}

	void ReadValue(cIn &in, asIScriptEngine *e, int alTypeId, void *apAddr)
	{
		if (alTypeId & asTYPEID_OBJHANDLE)
		{
			if (in.Pod<uint8_t>() == 0)
				return;
			std::string sClass = in.Str();
			asITypeInfo *t = e->GetTypeInfoById(alTypeId);
			asIScriptObject *&pObj = *(asIScriptObject **)apAddr;
			asITypeInfo *pClass = t && t->GetModule() ? t->GetModule()->GetTypeInfoByName(sClass.c_str()) : NULL;
			if (pObj == NULL && pClass && (pClass->GetFlags() & asOBJ_SCRIPT_OBJECT))
				pObj = (asIScriptObject *)e->CreateScriptObject(pClass);
			if (pObj && sClass == pObj->GetObjectType()->GetName())
				ReadObject(in, pObj);
			else
				in.SkipObject();
			return;
		}
		if (alTypeId <= asTYPEID_DOUBLE)
		{
			in.Bytes(apAddr, e->GetSizeOfPrimitiveType(alTypeId));
			return;
		}
		asITypeInfo *t = e->GetTypeInfoById(alTypeId);
		if (t == NULL)
			return;
		if (t->GetFlags() & asOBJ_ENUM)
			in.Bytes(apAddr, 4);
		else if (t->GetFlags() & asOBJ_SCRIPT_OBJECT)
			ReadObject(in, (asIScriptObject *)apAddr);
		else if (IsString(t))
			*(std::string *)apAddr = in.Str();
		else if (IsWString(t))
		{
			uint32_t n = in.Pod<uint32_t>();
			if (n > 1000000)
			{
				in.ok = false;
				return;
			}
			tWString w(n, 0);
			in.Bytes(w.data(), n * sizeof(wchar_t));
			*(tWString *)apAddr = w;
		}
		else if (IsArray(t))
		{
			CScriptArray *pArr = (CScriptArray *)apAddr;
			uint32_t n = in.Pod<uint32_t>();
			if (in.ok == false || n > 1000000)
			{
				in.ok = false;
				return;
			}
			pArr->Resize(n);
			for (asUINT i = 0; i < n && in.ok; ++i)
				ReadValue(in, e, pArr->GetElementTypeId(), pArr->At(i));
		}
		else if (IsRawValue(t))
			in.Bytes(apAddr, t->GetSize());
	}

	void WriteObject(cOut &o, asIScriptObject *apObj, std::set<void *> &aVisited)
	{
		aVisited.insert(apObj);
		asIScriptEngine *e = apObj->GetEngine();
		const std::set<std::string> &setNoSave = SomaScriptNoSaveNames();
		std::vector<asUINT> vProps;
		for (asUINT i = 0; i < apObj->GetPropertyCount(); ++i)
			if (setNoSave.count(apObj->GetPropertyName(i)) == 0)
				vProps.push_back(i);
		o.Pod((uint32_t)vProps.size());
		for (asUINT i : vProps)
		{
			int lTypeId = apObj->GetPropertyTypeId(i);
			o.Str(apObj->GetPropertyName(i));
			o.Str(e->GetTypeDeclaration(lTypeId, true));
			cOut value;
			WriteValue(value, e, lTypeId, apObj->GetAddressOfProperty(i), aVisited);
			o.Pod((uint32_t)value.s.size());
			o.s += value.s;
		}
	}

	void ReadObject(cIn &in, asIScriptObject *apObj)
	{
		asIScriptEngine *e = apObj->GetEngine();
		uint32_t n = in.Pod<uint32_t>();
		for (uint32_t k = 0; k < n && in.ok; ++k)
		{
			std::string sName = in.Str();
			std::string sDecl = in.Str();
			uint32_t lLen = in.Pod<uint32_t>();
			size_t lEnd = in.p + lLen;
			for (asUINT i = 0; i < apObj->GetPropertyCount(); ++i)
			{
				int lTypeId = apObj->GetPropertyTypeId(i);
				if (sName != apObj->GetPropertyName(i) || sDecl != e->GetTypeDeclaration(lTypeId, true))
					continue;
				cIn value(in.s);
				value.p = in.p;
				ReadValue(value, e, lTypeId, apObj->GetAddressOfProperty(i));
				break;
			}
			in.p = lEnd;
		}
	}

	void WriteScript(cOut &o, asIScriptObject *apObj)
	{
		std::set<void *> setVisited;
		o.Pod((uint8_t)(apObj != NULL));
		if (apObj)
		{
			o.Str(apObj->GetObjectType()->GetName());
			WriteObject(o, apObj, setVisited);
		}
	}

	void ReadScript(cIn &in, asIScriptObject *apObj)
	{
		if (in.Pod<uint8_t>() == 0)
			return;
		std::string sClass = in.Str();
		if (apObj && sClass == apObj->GetObjectType()->GetName())
			ReadObject(in, apObj);
		else
			in.SkipObject();
	}

	tString ScriptableKey(cSomaLuxScriptable *p)
	{
		return p->msScriptName + "|" + (p->GetScript() ? p->GetScript()->GetObjectType()->GetName() : "");
	}
}

class cSomaSaveState
{
public:
	static void WriteTimers(cOut &o, cSomaLuxScriptable *p)
	{
		o.Pod((uint32_t)p->mvTimers.size());
		for (auto &t : p->mvTimers)
		{
			o.Pod(t.mlId);
			o.Pod(t.mfLength);
			o.Pod(t.mfTimeLeft);
			o.Str(t.msFunc);
			o.Pod(t.mbRepeat);
			o.Pod(t.mbPaused);
		}
		o.Pod((uint32_t)p->mvFaders.size());
		for (auto &f : p->mvFaders)
			o.Pod(f);
	}

	static void ReadTimers(cIn &in, cSomaLuxScriptable *p)
	{
		p->mvTimers.clear();
		uint32_t n = in.Pod<uint32_t>();
		for (uint32_t i = 0; i < n && in.ok; ++i)
		{
			cSomaLuxScriptable::cTimer t;
			t.mlId = in.Pod<uint64_t>();
			t.mfLength = in.Pod<float>();
			t.mfTimeLeft = in.Pod<float>();
			t.msFunc = in.Str();
			t.mbRepeat = in.Pod<bool>();
			t.mbPaused = in.Pod<bool>();
			p->mvTimers.push_back(t);
		}
		p->mvFaders.clear();
		n = in.Pod<uint32_t>();
		for (uint32_t i = 0; i < n && in.ok; ++i)
			p->mvFaders.push_back(in.Pod<cSomaLuxScriptable::cFader>());
	}

	static void WriteEntity(cOut &o, cSomaLuxEntity *p)
	{
		o.Str(p->msName);
		o.Pod(p->mbActive);
		o.Pod(p->mbInteractionDisabled);
		o.Pod(p->mbInteractedWith);
		o.Pod(p->mbEffectsActive);
		o.Pod(p->mlParentType);
		o.Pod(p->mParentID);
		o.Str(p->msParentName);
		o.Str(p->msInteractCallback);
		o.Pod(p->mbInteractCallbackAutoRemove);
		o.Str(p->msLookAtCallback);
		o.Pod(p->mbLookAtCallbackAutoRemove);
		o.Pod((uint32_t)p->mvCollideCallbacks.size());
		for (auto &c : p->mvCollideCallbacks)
		{
			o.Str(c.msChild);
			o.Str(c.msFunc);
		}
		o.Pod((uint32_t)p->mvBodies.size());
		for (iPhysicsBody *b : p->mvBodies)
			o.Pod(b->GetWorldMatrix());
		WriteTimers(o, p);
		WriteScript(o, p->GetScript());
	}

	static void ReadEntity(cIn &in, cSomaLuxMap *apMap)
	{
		tString sName = in.Str();
		cSomaLuxEntity *p = apMap->GetEntity(sName);
		cSomaLuxEntity dummy;
		cSomaLuxEntity *t = p ? p : &dummy;
		bool bActive = in.Pod<bool>();
		t->mbInteractionDisabled = in.Pod<bool>();
		t->mbInteractedWith = in.Pod<bool>();
		bool bEffects = in.Pod<bool>();
		t->mlParentType = in.Pod<int>();
		t->mParentID = in.Pod<cSomaID>();
		t->msParentName = in.Str();
		t->msInteractCallback = in.Str();
		t->mbInteractCallbackAutoRemove = in.Pod<bool>();
		t->msLookAtCallback = in.Str();
		t->mbLookAtCallbackAutoRemove = in.Pod<bool>();
		t->mvCollideCallbacks.clear();
		uint32_t n = in.Pod<uint32_t>();
		for (uint32_t i = 0; i < n && in.ok; ++i)
		{
			tString sChild = in.Str();
			tString sFunc = in.Str();
			t->mvCollideCallbacks.push_back(cSomaLuxEntity::cCollideCallback{sChild, sFunc});
		}
		n = in.Pod<uint32_t>();
		for (uint32_t i = 0; i < n && in.ok; ++i)
		{
			cMatrixf m = in.Pod<cMatrixf>();
			if (p && i < p->mvBodies.size() && p->mvBodies[i]->GetMass() > 0)
				p->mvBodies[i]->SetMatrix(m);
		}
		ReadTimers(in, t);
		ReadScript(in, t->GetScript());
		if (p && p->mbEffectsActive != bEffects)
			p->SetEffectsActive(bEffects && p->mbActive);
		t->mbEffectsActive = bEffects;
		if (p && p->mbActive != bActive)
			p->SetActive(bActive);
	}

	static void WriteWorld(cOut &o)
	{
		cSomaLuxMap *pMap = cSomaLuxMap::GetCurrent();
		cSomaLuxPlayer *pPlayer = cSomaLuxPlayer::Get();
		iCharacterBody *pBody = pPlayer ? pPlayer->GetCharacterBody() : NULL;
		o.Pod((uint8_t)(pBody != NULL));
		if (pBody)
		{
			o.Pod(pBody->GetFeetPosition());
			o.Pod(pPlayer->GetCamera() ? pPlayer->GetCamera()->GetYaw() : pBody->GetYaw());
			o.Pod(pPlayer->GetCamera() ? pPlayer->GetCamera()->GetPitch() : 0.0f);
			o.Pod(pPlayer->GetState() ? pPlayer->GetState()->mlId : -1);
			o.Pod(pPlayer->GetMoveState() ? pPlayer->GetMoveState()->mlId : -1);
		}

		o.Pod((uint32_t)pMap->GetTimers().size());
		for (const cSomaLuxTimer &t : pMap->GetTimers())
		{
			o.Str(t.msName);
			o.Str(t.msFunction);
			o.Pod(t.mfTime);
			o.Pod(t.mbPaused);
			o.Pod(t.mfUserFloat);
			o.Pod(t.mlUserInt);
			o.Str(t.msUserString);
			o.Pod(t.mfLength);
		}
		WriteScript(o, pMap->GetScript());

		o.Pod((uint32_t)pMap->GetEntities().size());
		for (cSomaLuxEntity *p : pMap->GetEntities())
			WriteEntity(o, p);

		std::vector<cSomaLuxScriptable *> vOther;
		for (cSomaLuxScriptable *p : cSomaLuxScriptable::GetAll())
			if (dynamic_cast<cSomaLuxEntity *>(p) == NULL && p->GetScript())
				vOther.push_back(p);
		o.Pod((uint32_t)vOther.size());
		for (cSomaLuxScriptable *p : vOther)
		{
			o.Str(ScriptableKey(p));
			WriteTimers(o, p);
			WriteScript(o, p->GetScript());
		}

		std::vector<iLight *> vLights;
		cLightListIterator it = pMap->GetWorld()->GetLightIterator();
		while (it.HasNext())
			if (iLight *l = it.Next(); l->IsSaved() && l->GetParent() == NULL && l->GetEntityParent() == NULL)
				vLights.push_back(l);
		o.Pod((uint32_t)vLights.size());
		for (iLight *l : vLights)
		{
			bool bFlicker = l->GetFlickerActive();
			o.Str(l->GetName());
			o.Pod(l->IsActive());
			o.Pod(l->GetVisibleVar());
			o.Pod(bFlicker);
			o.Pod(bFlicker ? l->GetFlickerOnColor() : l->IsFading() ? l->GetDestColor() : l->GetDiffuseColor());
			o.Pod(bFlicker ? l->GetFlickerOnRadius() : l->IsFading() ? l->GetDestRadius() : l->GetRadius());
			o.Pod(l->GetBrightness());
		}

		std::vector<cParticleSystem *> vPS;
		cParticleSystemIterator psIt = pMap->GetWorld()->GetParticleSystemIterator();
		while (psIt.HasNext())
			if (cParticleSystem *ps = psIt.Next(); ps->IsSaved() && ps->GetParent() == NULL && ps->GetEntityParent() == NULL && ps->IsDying() == false)
				vPS.push_back(ps);
		o.Pod((uint32_t)vPS.size());
		for (cParticleSystem *ps : vPS)
		{
			o.Str(ps->GetName());
			o.Str(ps->GetDataName());
			o.Pod(ps->GetDataSize());
			o.Pod(ps->GetLocalMatrix());
			o.Pod(ps->GetColor());
			o.Pod(ps->GetBrightness());
			o.Pod(ps->IsActive());
			o.Pod(ps->IsVisible());
			o.Pod(ps->GetFadeAtDistance());
			for (float f : {ps->GetMinFadeDistanceStart(), ps->GetMinFadeDistanceEnd(), ps->GetMaxFadeDistanceStart(), ps->GetMaxFadeDistanceEnd()})
				o.Pod(f);
			uint32_t lDead = 0;
			for (int i = 0; i < ps->GetEmitterNum() && i < 32; ++i)
				if (ps->GetEmitter(i)->IsDying())
					lDead |= 1u << i;
			o.Pod(lDead);
		}
	}

	static void ReadWorld(cIn &in)
	{
		cSomaLuxMap *pMap = cSomaLuxMap::GetCurrent();
		bool bPlayer = in.Pod<uint8_t>() != 0;
		cVector3f vFeet(0);
		float fYaw = 0, fPitch = 0;
		int lState = -1, lMoveState = -1;
		if (bPlayer)
		{
			vFeet = in.Pod<cVector3f>();
			fYaw = in.Pod<float>();
			fPitch = in.Pod<float>();
			lState = in.Pod<int>();
			lMoveState = in.Pod<int>();
		}

		pMap->GetTimers().clear();
		uint32_t n = in.Pod<uint32_t>();
		for (uint32_t i = 0; i < n && in.ok; ++i)
		{
			cSomaLuxTimer t;
			t.msName = in.Str();
			t.msFunction = in.Str();
			t.mfTime = in.Pod<float>();
			t.mbPaused = in.Pod<bool>();
			t.mfUserFloat = in.Pod<float>();
			t.mlUserInt = in.Pod<int>();
			t.msUserString = in.Str();
			t.mfLength = in.Pod<float>();
			pMap->GetTimers().push_back(t);
		}
		ReadScript(in, pMap->GetScript());

		n = in.Pod<uint32_t>();
		for (uint32_t i = 0; i < n && in.ok; ++i)
			ReadEntity(in, pMap);

		std::map<tString, cSomaLuxScriptable *> mapOther;
		for (cSomaLuxScriptable *p : cSomaLuxScriptable::GetAll())
			if (dynamic_cast<cSomaLuxEntity *>(p) == NULL && p->GetScript())
				mapOther.emplace(ScriptableKey(p), p);
		n = in.Pod<uint32_t>();
		for (uint32_t i = 0; i < n && in.ok; ++i)
		{
			tString sKey = in.Str();
			auto it = mapOther.find(sKey);
			cSomaLuxScriptable dummy;
			cSomaLuxScriptable *p = it != mapOther.end() ? it->second : &dummy;
			ReadTimers(in, p);
			ReadScript(in, p->GetScript());
		}

		n = in.Pod<uint32_t>();
		for (uint32_t i = 0; i < n && in.ok; ++i)
		{
			iLight *l = pMap->GetWorld()->GetLight(in.Str());
			bool bActive = in.Pod<bool>(), bVisible = in.Pod<bool>(), bFlicker = in.Pod<bool>();
			cColor col = in.Pod<cColor>();
			float fRadius = in.Pod<float>(), fBrightness = in.Pod<float>();
			if (l == NULL || in.ok == false)
				continue;
			l->SetActive(bActive);
			l->SetVisible(bVisible);
			l->SetDiffuseColor(col);
			l->SetRadius(fRadius);
			l->SetBrightness(fBrightness);
			l->SetFlickerActive(bFlicker);
		}

		std::multimap<tString, cParticleSystem *> mapPS;
		cParticleSystemIterator psIt = pMap->GetWorld()->GetParticleSystemIterator();
		while (psIt.HasNext())
			if (cParticleSystem *ps = psIt.Next(); ps->IsSaved() && ps->GetParent() == NULL && ps->GetEntityParent() == NULL)
				mapPS.emplace(ps->GetName(), ps);
		n = in.Pod<uint32_t>();
		bool bHasPS = in.ok;
		for (uint32_t i = 0; i < n && in.ok; ++i)
		{
			tString sName = in.Str(), sData = in.Str();
			cVector3f vSize = in.Pod<cVector3f>();
			cMatrixf m = in.Pod<cMatrixf>();
			cColor col = in.Pod<cColor>();
			float fBrightness = in.Pod<float>();
			bool bActive = in.Pod<bool>(), bVisible = in.Pod<bool>(), bFade = in.Pod<bool>();
			float vFade[4];
			for (float &f : vFade)
				f = in.Pod<float>();
			uint32_t lDead = in.Pod<uint32_t>();
			if (in.ok == false)
				break;
			cParticleSystem *ps = NULL;
			if (auto it = mapPS.find(sName); it != mapPS.end())
			{
				ps = it->second;
				mapPS.erase(it);
			}
			else if ((ps = pMap->GetWorld()->CreateParticleSystem(sName, sData, vSize)) == NULL)
				continue;
			ps->SetMatrix(m);
			ps->SetColor(col);
			ps->SetBrightness(fBrightness);
			ps->SetActive(bActive);
			ps->SetVisible(bVisible);
			ps->SetFadeAtDistance(bFade);
			ps->SetMinFadeDistanceStart(vFade[0]);
			ps->SetMinFadeDistanceEnd(vFade[1]);
			ps->SetMaxFadeDistanceStart(vFade[2]);
			ps->SetMaxFadeDistanceEnd(vFade[3]);
			for (int e = 0; e < ps->GetEmitterNum() && e < 32; ++e)
				if (lDead & (1u << e))
					ps->GetEmitter(e)->KillInstantly();
		}
		if (bHasPS)
			for (auto &it : mapPS)
				it.second->KillInstantly();

		cSomaLuxPlayer *pPlayer = cSomaLuxPlayer::Get();
		if (bPlayer && pPlayer)
		{
			if (lMoveState >= 0)
				pPlayer->ChangeMoveState(lMoveState);
			if (lState >= 0)
				pPlayer->ChangeState(lState);
			pPlayer->PlaceAtStart(vFeet, fYaw);
			if (pPlayer->GetCamera())
				pPlayer->GetCamera()->SetPitch(fPitch);
		}
	}
};

tWString cSomaSaveHandler::GetSaveDir()
{
	tWString sDir = cPlatform::GetSystemSpecialPath(eSystemPath_XDGDataHome);
	for (const wchar_t *pPart : {L"open-hpl/", L"soma/", L"saves/"})
	{
		sDir += pPart;
		if (cPlatform::FolderExists(sDir) == false)
			cPlatform::CreateFolder(sDir);
	}
	return sDir;
}

tWString cSomaSaveHandler::GetLatestSave()
{
	tWStringList lstFiles;
	cPlatform::FindFilesInDir(lstFiles, GetSaveDir(), _W("*.sav"));
	tWString sBest;
	cDate bestDate;
	for (const tWString &sFile : lstFiles)
	{
		cDate date = cPlatform::FileModifiedDate(GetSaveDir() + sFile);
		if (sBest.empty() || date > bestDate)
		{
			sBest = sFile;
			bestDate = date;
		}
	}
	return sBest;
}

void cSomaSaveHandler::OnMapEnter(const tString &asMapFile, const tString &asStartPos)
{
	gsMapFile = asMapFile;
	gsStartPos = asStartPos;
}

bool cSomaSaveHandler::Save(const tWString &asFile)
{
	if (gsMapFile.empty() || cSomaLuxMap::GetCurrent() == NULL)
		return false;
	cOut o;
	o.Bytes(kMagic, 8);
	o.Str(gsMapFile);
	o.Str(gsStartPos);
	o.Pod((uint32_t)gpSomaBase->GetVisitedMaps().size());
	for (const tString &sMap : gpSomaBase->GetVisitedMaps())
		o.Str(sMap);
	o.Str(SomaSerializeGlobalVars());
	o.Pod(gbExplorationMode);
	cSomaSaveState::WriteWorld(o);

	tWString sPath = GetSaveDir() + cString::GetFileNameW(asFile);
	std::ofstream file(cString::To8Char(sPath).c_str(), std::ios::binary | std::ios::trunc);
	if (file.is_open() == false || file.write(o.s.data(), o.s.size()).good() == false)
	{
		Error("SOMA save: could not write '%s'\n", cString::To8Char(sPath).c_str());
		return false;
	}
	Log("SOMA save: %s (%s, %d bytes)\n", cString::To8Char(sPath).c_str(), gsMapFile.c_str(), (int)o.s.size());
	return true;
}

bool cSomaSaveHandler::AutoSave(bool abCheckpoint)
{
	DeleteOldestSaveFiles(GetSaveDir(), kMaxAutoSaves);
	bool bOk = Save(GetSaveName(_W("AutoSave")));
	if (abCheckpoint)
		bOk = Save(_W("CheckPoint.sav")) && bOk;
	return bOk;
}

bool cSomaSaveHandler::Load(const tWString &asFile, bool abImmediate)
{
	tWString sPath = GetSaveDir() + cString::GetFileNameW(asFile);
	std::ifstream file(cString::To8Char(sPath).c_str(), std::ios::binary);
	std::stringstream data;
	data << file.rdbuf();
	std::string sData = data.str();
	cIn in(sData);
	char vMagic[8] = {};
	in.Bytes(vMagic, 8);
	bool bV2 = memcmp(vMagic, kMagicV2, 8) == 0;
	if (file.is_open() == false || (memcmp(vMagic, kMagic, 8) != 0 && bV2 == false))
	{
		Error("SOMA save: could not read '%s'\n", cString::To8Char(sPath).c_str());
		return false;
	}
	tString sMap = in.Str();
	tString sPos = in.Str();
	std::set<tString> setVisited;
	uint32_t n = in.Pod<uint32_t>();
	for (uint32_t i = 0; i < n && in.ok; ++i)
		setVisited.insert(in.Str());
	tString sVars = in.Str();
	bool bExploration = bV2 ? false : in.Pod<bool>();
	if (in.ok == false || sMap.empty())
		return false;

	gbExplorationMode = bExploration;

	SomaDeserializeGlobalVars(sVars);
	// The saved state replaces OnStart
	setVisited.insert(sMap);
	gpSomaBase->GetVisitedMaps() = setVisited;
	gsPendingState = sData.substr(in.p);
	Log("SOMA save: loading %s (%s)\n", cString::To8Char(sPath).c_str(), sMap.c_str());
	if (abImmediate == false)
	{
		SomaRequestMapChange(sMap, sPos);
		return true;
	}
	tString sError;
	if (gpSomaBase->LoadMap(sMap, cVector3f(0), sError, sPos.empty() ? "*" : sPos))
		return true;
	gsPendingState.clear();
	Error("SOMA save: %s\n", sError.c_str());
	return false;
}

bool cSomaSaveHandler::ApplyPendingState()
{
	if (gsPendingState.empty() || cSomaLuxMap::GetCurrent() == NULL)
		return false;
	std::string sState;
	sState.swap(gsPendingState);
	cIn in(sState);
	cSomaSaveState::ReadWorld(in);
	if (in.ok == false)
		Warning("SOMA save: saved state is truncated\n");
	if (cSomaLuxPlayer::Get())
		cSomaLuxPlayer::Get()->SetActive(true);
	if (gbHoldAfterLoad)
		SomaSetGamePaused(true);
	return true;
}

void cSomaSaveHandler::RegisterNatives(asIScriptEngine *e)
{
	const char *T = "cLuxSaveHandler";
	static char gHandler;
	typedef const tWString &W;
	SOMA_FUNC(e, "void cLux_SetExplorationModeActive(bool abX)", +[](bool b) { gbExplorationMode = b; });
	SOMA_FUNC(e, "bool cLux_GetExplorationModeActive()", +[]() { return gbExplorationMode; });
	SOMA_FUNC(e, "cLuxSaveHandler@ cLux_GetSaveHandler()", +[]() { return (void *)&gHandler; });
	SOMA_METHOD(e, T, "void SaveGameToFile(const tWString&in asSaveFile)", +[](void *, W f) { Save(f); });
	SOMA_METHOD(e, T, "void LoadGameFromFile(const tWString&in asSaveFile)", +[](void *, W f) { Load(f); });
	SOMA_METHOD(e, T, "bool AutoSave(bool abSaveCheckpoint, bool abDelayed=true)", +[](void *, bool c, bool) { return AutoSave(c); });
	SOMA_METHOD(e, T, "bool GetSaveThreadActive()", +[](void *) { return false; });
	SOMA_METHOD(e, T, "bool HasLoadError(tString&out asError)", +[](void *, tString &) { return false; });
	SOMA_METHOD(e, T, "void DelayedLoadGameFromFile(const tWString&in asSaveFile, const tString&in asCallbackObject, const tString&in asCallbackFunction, bool abWaitAfterHeader, bool abWaitAfterLoad)",
				+[](void *, W f, const tString &, const tString &, bool, bool bWait) { gbHoldAfterLoad = Load(f) && bWait; });
	SOMA_METHOD(e, T, "void DelayedSaveGameToFile(const tWString&in asSaveFile, bool abSaveAsCheckpoint)", +[](void *, W f, bool) { Save(f); });
	SOMA_METHOD(e, T, "void DeleteSaveFile(const tWString&in asSaveFile)", +[](void *, W f) { cPlatform::RemoveFile(GetSaveDir() + cString::GetFileNameW(f)); });
	SOMA_METHOD(e, T, "bool IsDoneLoadingHeader()", +[](void *) { return true; });
	SOMA_METHOD(e, T, "void ContinueLoading(bool abDisableWaits)", +[](void *, bool b) { gbHoldAfterLoad &= !b; });
	SOMA_METHOD(e, T, "bool IsDoneLoadingSavedGame()", +[](void *) { return gsPendingState.empty(); });
	SOMA_METHOD(e, T, "void StartLoadedGame()", +[](void *) {
		if (gbHoldAfterLoad)
			SomaSetGamePaused(false);
		gbHoldAfterLoad = false;
	});
	SOMA_METHOD(e, T, "bool GetSaveFiles(array<tWString> &inout avNames, array<tString> &inout avDates, array<tWString> &inout avFiles)",
				+[](void *, CScriptArray &names, CScriptArray &dates, CScriptArray &files) {
					names.Resize(0);
					dates.Resize(0);
					files.Resize(0);
					tWStringList lstFiles;
					cPlatform::FindFilesInDir(lstFiles, GetSaveDir(), _W("*.sav"));
					std::vector<std::pair<cDate, tWString>> vSaves;
					for (const tWString &sFile : lstFiles)
						vSaves.push_back({cPlatform::FileModifiedDate(GetSaveDir() + sFile), sFile});
					std::sort(vSaves.begin(), vSaves.end(), [](const auto &a, const auto &b) { return b.first < a.first; });
					for (auto &it : vSaves)
					{
						tWString sName;
						tString sDate;
						GetProperSaveName(it.second, sName, sDate);
						names.InsertLast(&sName);
						dates.InsertLast(&sDate);
						files.InsertLast(&it.second);
					}
					return true;
				});
	SOMA_FUNC(e, "bool cLux_HasConfigLoadError(tString&out asError)", +[](tString &) { return false; });
}
