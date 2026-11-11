#include "SomaSave.h"
#include "RustSomaSave.h"
#include "SomaAgent.h"
#include "SomaBase.h"
#include "SomaImGui.h"
#include "SomaLux.h"
#include "SomaLuxEntity.h"
#include "SomaLuxPlayer.h"
#include "SomaScriptBind.h"
#include "SomaScriptBuilder.h"
#include "SomaScriptNatives.h"
#include "SomaSound.h"
#include "SomaSplash.h"
#include "SomaLuxVoice.h"
#include "SomaToneMapping.h"

#include "impl/scriptarray.h"

#include <algorithm>
#include <cstring>
#include <deque>
#include <fstream>
#include <set>
#include <sstream>
#include <sys/stat.h>
#include <ctime>


namespace
{
	tString gsMapFile, gsStartPos;
	bool gbExplorationMode = false;
	std::string gsPendingState;
	int gnSavedUnderwater = -1;
	int glPendingVersion = 22;
	int glSavedGravityActive = -1;
	cVector3f gvSavedGravity(0);
	tString gsPendingPreload;
	bool gbHoldAfterLoad = false;
	bool gbHoldAfterHeader = false;
	tString gsHeldMap, gsHeldPos;
	tString gsLoadCallbackObject, gsLoadCallbackFunc;
	OhplGs4NameState gNameState = {};
	tWString gsSaveSuffix;
	const int kMaxAutoSaves = 20; // game.cfg Saving/MaxAutoSaves

	// cLuxSaveHandler::LoadSavedGame_StartGame
	void RunLoadCallback()
	{
		tString sObj, sFunc;
		sObj.swap(gsLoadCallbackObject);
		sFunc.swap(gsLoadCallbackFunc);
		if (sFunc.empty() == false)
			SomaRunGlobalFunc(sObj, "", sFunc);
	}

	OhplGs4Str RS(const std::string &s) { return OhplGs4Str{(const unsigned char *)s.data(), s.size()}; }
	std::string FromRS(OhplGs4Str s) { return std::string((const char *)s.p, s.n); }
	template <class T> OhplGs4Slice<T> Sl(const std::vector<T> &v) { return OhplGs4Slice<T>{v.data(), v.size()}; }

	template <size_t N, class T> void CopyF(float (&d)[N], const T &s)
	{
		static_assert(sizeof(T) == N * sizeof(float), "pod size");
		memcpy(d, &s, sizeof(T));
	}
	template <class T, size_t N> void CopyB(unsigned char (&d)[N], const T &s)
	{
		static_assert(sizeof(T) == N, "pod size");
		memcpy(d, &s, N);
	}
	template <class T, size_t N> void FromF(T &d, const float (&s)[N])
	{
		static_assert(sizeof(T) == N * sizeof(float), "pod size");
		memcpy(&d, s, sizeof(T));
	}
	template <class T, size_t N> void FromB(T &d, const unsigned char (&s)[N])
	{
		static_assert(sizeof(T) == N, "pod size");
		memcpy(&d, s, N);
	}

	void SinkW(void *c, const uint32_t *p, size_t n) { ((tWString *)c)->assign((const wchar_t *)p, n); }
	void SinkS(void *c, const unsigned char *p, size_t n) { ((std::string *)c)->assign((const char *)p, n); }

	struct cNamesCtx
	{
		tWString msTranslated;
	};

	void NamesTranslate(void *apCtx, const unsigned char *apKey, size_t alKey, const uint32_t **appOut, size_t *apLen)
	{
		cNamesCtx *c = (cNamesCtx *)apCtx;
		c->msTranslated = gpSomaBase->mpEngine->GetResources()->Translate("Levels", std::string((const char *)apKey, alKey));
		*appOut = (const uint32_t *)c->msTranslated.data();
		*apLen = c->msTranslated.size();
	}

	void NamesMTime(void *, const uint32_t *apFile, size_t alFile, int32_t *apOut)
	{
		tWString sFile((const wchar_t *)apFile, alFile);
		struct stat st = {};
		stat(cString::To8Char(cSomaSaveHandler::GetSaveDir() + sFile).c_str(), &st);
		struct tm t = {};
		localtime_r(&st.st_mtime, &t);
		apOut[0] = t.tm_mday;
		apOut[1] = t.tm_mon + 1;
		apOut[2] = t.tm_year + 1900;
		apOut[3] = t.tm_hour;
		apOut[4] = t.tm_min;
		apOut[5] = t.tm_sec;
	}

	OhplGs4Date ToRustDate(const cDate &d) { return OhplGs4Date{d.seconds, d.minutes, d.hours, d.month_day, d.month, d.year}; }

	tWString GetSaveName(const tWString &asPrefix)
	{
		cSomaLuxMap *pMap = cSomaLuxMap::GetCurrent();
		tString sMapName = pMap ? pMap->msDisplayNameEntry : "";
		OhplGs4Date d = ToRustDate(cPlatform::GetDate());
		tWString sName;
		ohpl_gs4_save_name(&gNameState, (const uint32_t *)asPrefix.data(), asPrefix.size(), (const unsigned char *)sMapName.data(),
						   sMapName.size(), &d, (const uint32_t *)gsSaveSuffix.data(), gsSaveSuffix.size(), SinkW, &sName);
		return sName;
	}

	struct cSaveInfo
	{
		tWString msFile, msName, msSuffix;
		tString msDate;
	};

	std::vector<cSaveInfo> ListSaves()
	{
		tWStringList lstFiles;
		cPlatform::FindFilesInDir(lstFiles, cSomaSaveHandler::GetSaveDir(), _W("*.sav"));
		std::vector<tWString> vFiles(lstFiles.begin(), lstFiles.end());
		std::vector<OhplGs4SaveFile> vRecs;
		for (const tWString &sFile : vFiles)
			vRecs.push_back(OhplGs4SaveFile{(const uint32_t *)sFile.data(), sFile.size(),
											ToRustDate(cPlatform::FileModifiedDate(cSomaSaveHandler::GetSaveDir() + sFile))});
		struct cList
		{
			std::vector<tWString> *mpFiles;
			std::vector<cSaveInfo> mvOut;
		} list = {&vFiles, {}};
		cNamesCtx names;
		OhplGs4NamesHost host = {&names, NamesTranslate, NamesMTime};
		ohpl_gs4_list_saves(&host, vRecs.data(), vRecs.size(),
							[](void *c, size_t alSrc, const uint32_t *apName, size_t alName, const unsigned char *apDate, size_t alDate,
							   const uint32_t *apSuffix, size_t alSuffix) {
								cList *l = (cList *)c;
								cSaveInfo i;
								i.msFile = (*l->mpFiles)[alSrc];
								i.msName.assign((const wchar_t *)apName, alName);
								i.msDate.assign((const char *)apDate, alDate);
								i.msSuffix.assign((const wchar_t *)apSuffix, alSuffix);
								l->mvOut.push_back(i);
							},
							&list);
		return list.mvOut;
	}

	void DeleteOldestSaveFiles(int alMax)
	{
		std::vector<cSaveInfo> vSaves = ListSaves();
		std::vector<OhplGs4W> vSuffixes;
		for (const cSaveInfo &i : vSaves)
			vSuffixes.push_back(OhplGs4W{(const uint32_t *)i.msSuffix.data(), i.msSuffix.size()});
		std::vector<unsigned char> vRemove(vSaves.size());
		ohpl_gs4_rotation(vSuffixes.data(), vSuffixes.size(), (const uint32_t *)gsSaveSuffix.data(), gsSaveSuffix.size(), alMax, vRemove.data());
		for (size_t i = 0; i < vSaves.size(); ++i)
			if (vRemove[i])
				cPlatform::RemoveFile(cSomaSaveHandler::GetSaveDir() + vSaves[i].msFile);
	}

	// Script objects: the walker is Rust; these answer for the AngelScript engine

	bool IsString(asITypeInfo *t) { return strcmp(t->GetName(), "tString") == 0 || strcmp(t->GetName(), "string") == 0; }
	bool IsWString(asITypeInfo *t) { return strcmp(t->GetName(), "tWString") == 0; }
	bool IsArray(asITypeInfo *t) { return strcmp(t->GetName(), "array") == 0; }
	bool IsRawValue(asITypeInfo *t) { return (t->GetFlags() & asOBJ_VALUE) && t->GetSize() > 0 && t->GetSize() <= 256; }

	struct cScriptCtx
	{
		asIScriptEngine *mpEngine;
		std::string msDecl;
		std::vector<tString> mvSet;
		OhplGs4ScriptHost mHost;

		explicit cScriptCtx(asIScriptEngine *apEngine) : mpEngine(apEngine)
		{
			mHost.ctx = this;
			mHost.handle_obj = HandleObj;
			mHost.resolve_handle = ResolveHandle;
			mHost.obj_class = ObjClass;
			mHost.prim_size = PrimSize;
			mHost.kind = Kind;
			mHost.string_get = StringGet;
			mHost.string_set = StringSet;
			mHost.wstring_get = WStringGet;
			mHost.wstring_set = WStringSet;
			mHost.array_len = ArrayLen;
			mHost.array_elem_type = ArrayElemType;
			mHost.array_at = ArrayAt;
			mHost.array_resize = ArrayResize;
			mHost.set_items = SetItems;
			mHost.set_item = SetItem;
			mHost.set_clear = SetClear;
			mHost.set_insert = SetInsert;
			mHost.prop_count = PropCount;
			mHost.prop_saved = PropSaved;
			mHost.prop_name = PropName;
			mHost.prop_type_id = PropTypeId;
			mHost.prop_addr = PropAddr;
			mHost.type_decl = TypeDecl;
		}

		static cScriptCtx *C(void *p) { return (cScriptCtx *)p; }
		static asIScriptObject *O(size_t a) { return (asIScriptObject *)a; }

		static bool HandleObj(void *c, size_t alAddr, int alTypeId, size_t *apObj)
		{
			void *pObj = *(void *const *)alAddr;
			asITypeInfo *t = C(c)->mpEngine->GetTypeInfoById(alTypeId);
			if (pObj == NULL || t == NULL || (t->GetFlags() & asOBJ_SCRIPT_OBJECT) == 0)
				return false;
			*apObj = (size_t)pObj;
			return true;
		}

		static bool ResolveHandle(void *c, size_t alAddr, int alTypeId, const unsigned char *apClass, size_t alClass, size_t *apObj)
		{
			asIScriptEngine *e = C(c)->mpEngine;
			std::string sClass((const char *)apClass, alClass);
			asITypeInfo *t = e->GetTypeInfoById(alTypeId);
			asIScriptObject *&pObj = *(asIScriptObject **)alAddr;
			asITypeInfo *pClass = t && t->GetModule() ? t->GetModule()->GetTypeInfoByName(sClass.c_str()) : NULL;
			if (pObj == NULL && pClass && (pClass->GetFlags() & asOBJ_SCRIPT_OBJECT))
				pObj = (asIScriptObject *)e->CreateScriptObject(pClass);
			if (pObj && sClass == pObj->GetObjectType()->GetName())
			{
				*apObj = (size_t)pObj;
				return true;
			}
			return false;
		}

		static void ObjClass(void *, size_t alObj, const unsigned char **app, size_t *apLen)
		{
			const char *s = O(alObj)->GetObjectType()->GetName();
			*app = (const unsigned char *)s;
			*apLen = strlen(s);
		}

		static size_t PrimSize(void *c, int alTypeId) { return C(c)->mpEngine->GetSizeOfPrimitiveType(alTypeId); }

		static int Kind(void *c, int alTypeId, size_t *apRaw)
		{
			asITypeInfo *t = C(c)->mpEngine->GetTypeInfoById(alTypeId);
			if (t == NULL)
				return 0;
			if (t->GetFlags() & asOBJ_ENUM)
				return 1;
			if (t->GetFlags() & asOBJ_SCRIPT_OBJECT)
				return 2;
			if (IsString(t))
				return 3;
			if (IsWString(t))
				return 4;
			if (IsArray(t))
				return 5;
			if (strcmp(t->GetName(), "cScriptStringSet") == 0)
				return 6;
			if (IsRawValue(t))
			{
				*apRaw = t->GetSize();
				return 7;
			}
			return 8;
		}

		static void StringGet(void *, size_t a, const unsigned char **app, size_t *apLen)
		{
			const std::string &s = *(const std::string *)a;
			*app = (const unsigned char *)s.data();
			*apLen = s.size();
		}
		static void StringSet(void *, size_t a, const unsigned char *ap, size_t alLen) { ((std::string *)a)->assign((const char *)ap, alLen); }
		static void WStringGet(void *, size_t a, const uint32_t **app, size_t *apLen)
		{
			const tWString &w = *(const tWString *)a;
			*app = (const uint32_t *)w.data();
			*apLen = w.size();
		}
		static void WStringSet(void *, size_t a, const uint32_t *ap, size_t alLen) { *(tWString *)a = tWString((const wchar_t *)ap, alLen); }
		static uint32_t ArrayLen(void *, size_t a) { return ((CScriptArray *)a)->GetSize(); }
		static int ArrayElemType(void *, size_t a) { return ((CScriptArray *)a)->GetElementTypeId(); }
		static size_t ArrayAt(void *, size_t a, uint32_t i) { return (size_t)((CScriptArray *)a)->At(i); }
		static void ArrayResize(void *, size_t a, uint32_t n) { ((CScriptArray *)a)->Resize(n); }

		static size_t SetItems(void *c, size_t a)
		{
			cScriptCtx *s = C(c);
			s->mvSet.clear();
			for (const tString &x : SomaStringSetOf((void *)a))
				s->mvSet.push_back(x);
			return s->mvSet.size();
		}
		static void SetItem(void *c, size_t i, const unsigned char **app, size_t *apLen)
		{
			const tString &s = C(c)->mvSet[i];
			*app = (const unsigned char *)s.data();
			*apLen = s.size();
		}
		static void SetClear(void *, size_t a) { SomaStringSetOf((void *)a).clear(); }
		static void SetInsert(void *, size_t a, const unsigned char *ap, size_t alLen) { SomaStringSetOf((void *)a).insert(std::string((const char *)ap, alLen)); }

		static uint32_t PropCount(void *, size_t o) { return O(o)->GetPropertyCount(); }
		static bool PropSaved(void *, size_t o, uint32_t i) { return SomaScriptIsNoSave(O(o)->GetObjectType(), O(o)->GetPropertyName(i)) == false; }
		static void PropName(void *, size_t o, uint32_t i, const unsigned char **app, size_t *apLen)
		{
			const char *s = O(o)->GetPropertyName(i);
			*app = (const unsigned char *)s;
			*apLen = strlen(s);
		}
		static int PropTypeId(void *, size_t o, uint32_t i) { return O(o)->GetPropertyTypeId(i); }
		static size_t PropAddr(void *, size_t o, uint32_t i) { return (size_t)O(o)->GetAddressOfProperty(i); }
		static void TypeDecl(void *c, size_t, int alTypeId, const unsigned char **app, size_t *apLen)
		{
			cScriptCtx *s = C(c);
			s->msDecl = s->mpEngine->GetTypeDeclaration(alTypeId, true);
			*app = (const unsigned char *)s->msDecl.data();
			*apLen = s->msDecl.size();
		}
	};

	// ponytail: primitives only; [nosave] handles/objects set up in Init keep their value
	void ResetNoSave(asIScriptObject *apObj)
	{
		asIScriptEngine *e = apObj->GetEngine();
		asIScriptObject *pFresh = (asIScriptObject *)e->CreateScriptObject(apObj->GetObjectType());
		if (pFresh == NULL)
			return;
		for (asUINT i = 0; i < apObj->GetPropertyCount(); ++i)
		{
			int lTypeId = apObj->GetPropertyTypeId(i);
			asITypeInfo *t = e->GetTypeInfoById(lTypeId);
			bool bEnum = t && (t->GetFlags() & asOBJ_ENUM);
			if (SomaScriptIsNoSave(apObj->GetObjectType(), apObj->GetPropertyName(i)) && (lTypeId <= asTYPEID_DOUBLE || bEnum))
				memcpy(apObj->GetAddressOfProperty(i), pFresh->GetAddressOfProperty(i), bEnum ? 4 : e->GetSizeOfPrimitiveType(lTypeId));
		}
		pFresh->Release();
	}

	tString ScriptableKey(cSomaLuxScriptable *p)
	{
		return p->msScriptName + "|" + (p->GetScript() ? p->GetScript()->GetObjectType()->GetName() : "");
	}

	struct cStrPool
	{
		std::deque<std::string> mStrs;
		OhplGs4Str Add(const std::string &s)
		{
			mStrs.push_back(s);
			return RS(mStrs.back());
		}
	};

	struct cEntScratch
	{
		cStrPool mPool;
		std::vector<OhplGs4Collide> mCollide;
		std::vector<OhplGs4Body> mBodies;
		std::vector<float> mDynMass;
		std::vector<OhplGs4Joint> mJoints;
		std::vector<OhplGs4Timer> mTimers;
		std::vector<OhplGs4Fader> mFaders;
		std::vector<OhplGs4VarPair> mVars;
		std::vector<OhplGs4ImTimer> mImTimers;
		std::vector<OhplGs4ImStatePair> mImStates;
		std::vector<OhplGs4Anim> mAnims;
		std::vector<OhplGs4Conn> mConns;

		void Clear() { *this = cEntScratch(); }
	};

	struct cWriteCtx
	{
		cSomaLuxMap *mpMap = NULL;
		cSomaLuxPlayer *mpPlayer = NULL;
		cStrPool mPool;
		std::deque<std::vector<float>> mParams;
		std::vector<cSomaLuxEntity *> mvEnts;
		std::vector<cSomaLuxScriptable *> mvOthers;
		cEntScratch mHead, mTail, mOther;
		std::vector<OhplGs4MapTimer> mMapTimers;
		std::vector<OhplGs4Spawned> mSpawned;
		std::vector<OhplGs4Light> mLights;
		std::vector<OhplGs4Particle> mParticles;
		std::vector<OhplGs4Sound> mSounds;
		std::vector<OhplGs4Billboard> mBillboards;
		std::vector<OhplGs4Agent> mAgents;
		std::vector<OhplGs4CamTex> mCamTextures;
		std::vector<OhplGs4FogArea> mFogAreas;
		std::vector<OhplGs4Colliding> mColliding;
		std::vector<OhplGs4OwnedLight> mOwnedLights;
		std::unique_ptr<cScriptCtx> mpScript{new cScriptCtx(NULL)};
	};

	struct cReadCtx
	{
		cSomaLuxMap *mpMap = NULL;
		std::unique_ptr<cScriptCtx> mpScript;
		std::unique_ptr<cSomaLuxEntity> mpDummyEnt;
		std::unique_ptr<cSomaLuxScriptable> mpDummyOther;
		std::map<tString, cSomaLuxScriptable *> mmapOther;
		bool mbOtherBuilt = false;
		std::multimap<tString, cParticleSystem *> mmapPS, mmapOwnedPS;
		std::multimap<tString, cSoundEntity *> mmapSounds;
	};
}

class cSomaSaveState
{
public:
	static void GatherTimers(cEntScratch &s, OhplGs4Timers &aOut, cSomaLuxScriptable *p)
	{
		for (auto &t : p->mvTimers)
			s.mTimers.push_back(OhplGs4Timer{s.mPool.Add(t.msFunc), t.mlId, t.mfLength, t.mfTimeLeft, t.mbRepeat, t.mbPaused});
		for (auto &f : p->mvFaders)
		{
			OhplGs4Fader b;
			static_assert(sizeof(b) == sizeof(f), "cFader size");
			memcpy(&b, &f, sizeof(f));
			s.mFaders.push_back(b);
		}
		aOut.timers = Sl(s.mTimers);
		aOut.faders = Sl(s.mFaders);
	}

	static void ApplyTimers(cSomaLuxScriptable *p, const OhplGs4Timers *aT)
	{
		p->mvTimers.clear();
		for (size_t i = 0; i < aT->timers.n; ++i)
		{
			const OhplGs4Timer &x = aT->timers.p[i];
			cSomaLuxScriptable::cTimer t;
			t.mlId = x.id;
			t.mfLength = x.length;
			t.mfTimeLeft = x.time_left;
			t.msFunc = FromRS(x.func);
			t.mbRepeat = x.repeat;
			t.mbPaused = x.paused;
			p->mvTimers.push_back(t);
		}
		p->mvFaders.clear();
		for (size_t i = 0; i < aT->faders.n; ++i)
		{
			cSomaLuxScriptable::cFader f;
			static_assert(sizeof(f) == sizeof(OhplGs4Fader), "cFader size");
			memcpy(&f, &aT->faders.p[i], sizeof(f));
			p->mvFaders.push_back(f);
		}
	}

	static void GatherEntityHead(cEntScratch &s, cSomaLuxEntity *p, OhplGs4EntityHead &h)
	{
		s.Clear();
		memset(&h, 0, sizeof(h));
		h.name = s.mPool.Add(p->msName);
		h.active = p->mbActive;
		h.interaction_disabled = p->mbInteractionDisabled;
		h.interacted_with = p->mbInteractedWith;
		h.effects_active = p->mbEffectsActive;
		h.parent_type = p->mlParentType;
		CopyB(h.parent_id, p->mParentID);
		h.parent_name = s.mPool.Add(p->msParentName);
		h.interact_cb = s.mPool.Add(p->msInteractCallback);
		h.interact_auto_remove = p->mbInteractCallbackAutoRemove;
		h.look_cb = s.mPool.Add(p->msLookAtCallback);
		h.look_auto_remove = p->mbLookAtCallbackAutoRemove;
		h.look_center = p->mbLookAtCheckCenter;
		h.look_ray = p->mbLookAtCheckRay;
		h.look_max = p->mfLookAtMaxDistance;
		h.look_delay = p->mfLookAtDelay;
		for (auto &c : p->mvCollideCallbacks)
			s.mCollide.push_back(OhplGs4Collide{s.mPool.Add(c.msChild), s.mPool.Add(c.msFunc)});
		h.collide = Sl(s.mCollide);
		for (iPhysicsBody *b : p->mvBodies)
		{
			OhplGs4Body x;
			memset(&x, 0, sizeof(x));
			CopyF(x.mtx, b->GetWorldMatrix());
			x.active = b->IsActive();
			x.mass = b->GetMass();
			x.gravity = b->GetGravity();
			s.mBodies.push_back(x);
		}
		h.bodies = Sl(s.mBodies);
		if (p->mvBodies.empty())
			CopyF(h.mtx, p->GetMatrix());
		h.static_physics = p->mbStaticPhysics;
		for (float f : p->mvDynamicMass)
			s.mDynMass.push_back(f);
		h.dyn_mass = Sl(s.mDynMass);
		cSomaLuxEntity::cAttachment *a = p->mpAttachment;
		h.has_attach = a != NULL;
		if (a)
		{
			h.attach.parent = s.mPool.Add(a->msParent);
			h.attach.body = s.mPool.Add(a->mpBody ? a->mpBody->GetName() : "");
			h.attach.socket = s.mPool.Add(a->msSocket);
			h.attach.use_rotation = a->mbUseRotation;
			h.attach.locked = a->mbLocked;
			CopyF(h.attach.parent_prev, a->m_mtxParentPrev);
			CopyF(h.attach.offset, a->m_mtxOffset);
		}
		for (iPhysicsJoint *j : p->Joints())
		{
			OhplGs4Joint x;
			x.name = s.mPool.Add(j->GetName());
			cVector2f v(0, 0);
			if (j->GetType() == ePhysicsJointType_Hinge)
				v = cVector2f(static_cast<iPhysicsJointHinge *>(j)->GetMinAngle(), static_cast<iPhysicsJointHinge *>(j)->GetMaxAngle());
			else if (j->GetType() == ePhysicsJointType_Slider)
				v = cVector2f(static_cast<iPhysicsJointSlider *>(j)->GetMinDistance(), static_cast<iPhysicsJointSlider *>(j)->GetMaxDistance());
			CopyF(x.limits, v);
			s.mJoints.push_back(x);
		}
		h.joints = Sl(s.mJoints);
		GatherTimers(s, h.timers, p);
	}

	static void GatherEntityTail(cEntScratch &s, cSomaLuxEntity *p, OhplGs4EntityTail &t)
	{
		s.Clear();
		memset(&t, 0, sizeof(t));
		for (auto &v : p->mmapScriptVars)
			s.mVars.push_back(OhplGs4VarPair{s.mPool.Add(v.first), s.mPool.Add(v.second)});
		t.vars = Sl(s.mVars);
		t.gui_active = p->mbGuiActive;
		t.on_gui = s.mPool.Add(p->msOnGuiFunc);
		if (p->mpImGui)
		{
			for (auto &tm : p->mpImGui->mvTimers)
			{
				OhplGs4ImTimer b;
				static_assert(sizeof(b) == sizeof(tm), "imgui timer size");
				memcpy(&b, &tm, sizeof(tm));
				s.mImTimers.push_back(b);
			}
			for (auto &st : p->mpImGui->mmapStates)
			{
				OhplGs4ImStatePair b;
				static_assert(sizeof(b.state) == sizeof(st.second), "imgui state size");
				memset(&b, 0, sizeof(b));
				b.id = st.first;
				memcpy(&b.state, &st.second, sizeof(st.second));
				s.mImStates.push_back(b);
			}
		}
		t.imgui_timers = Sl(s.mImTimers);
		t.imgui_states = Sl(s.mImStates);
		int lAnims = p->mpMesh ? p->mpMesh->GetAnimationStateNum() : 0;
		for (int i = 0; i < lAnims; ++i)
		{
			cAnimationState *a = p->mpMesh->GetAnimationState(i);
			s.mAnims.push_back(OhplGs4Anim{s.mPool.Add(a->GetName()), a->GetTimePosition(), a->GetWeight(), a->GetSpeed(), a->GetFadeStep(),
										   a->IsActive(), a->IsLooping(), a->IsPaused()});
		}
		t.anims = Sl(s.mAnims);
		t.conn_cb = s.mPool.Add(p->msConnectionCallback);
		for (auto &c : p->mvConnections)
			s.mConns.push_back(OhplGs4Conn{s.mPool.Add(c.msName), s.mPool.Add(c.msEntity), c.mlStatesUsed, c.mbInvert});
		t.conns = Sl(s.mConns);
	}

	static size_t WObj(cWriteCtx *w, asIScriptObject *apObj)
	{
		if (apObj)
			w->mpScript->mpEngine = apObj->GetEngine();
		return (size_t)apObj;
	}
	static void WMapScript(void *c, OhplGs4Out *o)
	{
		cWriteCtx *w = (cWriteCtx *)c;
		ohpl_gs4_write_script(o, &w->mpScript->mHost, WObj(w, w->mpMap->GetScript()));
	}
	static void WEntityHead(void *c, size_t i, OhplGs4EntityHead *h)
	{
		cWriteCtx *w = (cWriteCtx *)c;
		GatherEntityHead(w->mHead, w->mvEnts[i], *h);
	}
	static void WEntityScript(void *c, size_t i, OhplGs4Out *o)
	{
		cWriteCtx *w = (cWriteCtx *)c;
		ohpl_gs4_write_script(o, &w->mpScript->mHost, WObj(w, w->mvEnts[i]->GetScript()));
	}
	static void WEntityTail(void *c, size_t i, OhplGs4EntityTail *t)
	{
		cWriteCtx *w = (cWriteCtx *)c;
		GatherEntityTail(w->mTail, w->mvEnts[i], *t);
	}
	static void WOtherKey(void *c, size_t i, OhplGs4Str *s)
	{
		cWriteCtx *w = (cWriteCtx *)c;
		w->mOther.Clear();
		*s = w->mOther.mPool.Add(ScriptableKey(w->mvOthers[i]));
	}
	static void WOtherTimers(void *c, size_t i, OhplGs4Timers *t)
	{
		cWriteCtx *w = (cWriteCtx *)c;
		GatherTimers(w->mOther, *t, w->mvOthers[i]);
	}
	static void WOtherScript(void *c, size_t i, OhplGs4Out *o)
	{
		cWriteCtx *w = (cWriteCtx *)c;
		ohpl_gs4_write_script(o, &w->mpScript->mHost, WObj(w, w->mvOthers[i]->GetScript()));
	}

	static void GatherWorld(cWriteCtx &c, OhplGs4World &w)
	{
		memset(&w, 0, sizeof(w));
		cSomaLuxMap *pMap = c.mpMap = cSomaLuxMap::GetCurrent();
		cSomaLuxPlayer *pPlayer = c.mpPlayer = cSomaLuxPlayer::Get();
		iCharacterBody *pBody = pPlayer ? pPlayer->GetCharacterBody() : NULL;
		w.player.present = pBody != NULL;
		w.player.gravity_active = 0;
		if (pBody)
		{
			CopyF(w.player.feet, pBody->GetFeetPosition());
			w.player.yaw = pPlayer->GetCamera() ? pPlayer->GetCamera()->GetYaw() : pBody->GetYaw();
			w.player.pitch = pPlayer->GetCamera() ? pPlayer->GetCamera()->GetPitch() : 0.0f;
			w.player.state = pPlayer->GetState() ? pPlayer->GetState()->mlId : -1;
			w.player.move_state = pPlayer->GetMoveState() ? pPlayer->GetMoveState()->mlId : -1;
			// int return read as its low byte: 0/1 crouching
			w.player.char_crouching = (int)pPlayer->CallBool("int GetCharacterState()", nullptr, false);
			w.player.gravity_active = (uint8_t)pBody->GetCustomGravityActive();
			CopyF(w.player.gravity, pBody->GetCustomGravity());
		}

		// a save from inside a timer callback must not keep that expired timer
		for (const cSomaLuxTimer &t : pMap->GetTimers())
		{
			if (pMap->IsTimerLive(t) == false)
				continue;
			c.mMapTimers.push_back(OhplGs4MapTimer{c.mPool.Add(t.msName), c.mPool.Add(t.msFunction), c.mPool.Add(t.msUserString), t.mfTime,
												   t.mfUserFloat, t.mfLength, t.mlUserInt, t.mbPaused});
		}
		w.map_timers = Sl(c.mMapTimers);

		for (cSomaLuxEntity *p : pMap->GetEntities())
			if (p->mbSpawned)
			{
				OhplGs4Spawned x;
				memset(&x, 0, sizeof(x));
				x.name = c.mPool.Add(p->msName);
				x.file = c.mPool.Add(p->msFileName);
				CopyF(x.mtx, p->GetMatrix());
				CopyF(x.scale, p->mvScale);
				CopyB(x.id, p->mID);
				c.mSpawned.push_back(x);
			}
		w.spawned = Sl(c.mSpawned);

		c.mvEnts = pMap->GetEntities();
		w.entity_count = c.mvEnts.size();
		for (cSomaLuxScriptable *p : cSomaLuxScriptable::GetAll())
			if (dynamic_cast<cSomaLuxEntity *>(p) == NULL && p->GetScript() && p->mbSaved)
				c.mvOthers.push_back(p);
		w.other_count = c.mvOthers.size();

		cLightListIterator it = pMap->GetWorld()->GetLightIterator();
		while (it.HasNext())
			if (iLight *l = it.Next(); l->IsSaved() && l->GetParent() == NULL && l->GetEntityParent() == NULL)
			{
				bool bFlicker = l->GetFlickerActive();
				OhplGs4Light x;
				memset(&x, 0, sizeof(x));
				x.name = c.mPool.Add(l->GetName());
				x.active = l->IsActive();
				x.visible = l->GetVisibleVar();
				x.flicker = bFlicker;
				CopyF(x.color, bFlicker ? l->GetFlickerOnColor() : l->IsFading() ? l->GetDestColor() : l->GetDiffuseColor());
				x.radius = bFlicker ? l->GetFlickerOnRadius() : l->IsFading() ? l->GetDestRadius() : l->GetRadius();
				x.brightness = l->GetBrightness();
				c.mLights.push_back(x);
			}
		w.lights = Sl(c.mLights);

		cParticleSystemIterator psIt = pMap->GetWorld()->GetParticleSystemIterator();
		while (psIt.HasNext())
			if (cParticleSystem *ps = psIt.Next(); ps->IsSaved() && (ps->GetParent() == NULL || ps->GetEntityParent()) && ps->IsDying() == false)
			{
				OhplGs4Particle x;
				memset(&x, 0, sizeof(x));
				x.name = c.mPool.Add(ps->GetName());
				x.data = c.mPool.Add(ps->GetDataName());
				CopyF(x.size, ps->GetDataSize());
				CopyF(x.mtx, ps->GetLocalMatrix());
				CopyF(x.color, ps->GetColor());
				x.brightness = ps->GetBrightness();
				x.active = ps->IsActive();
				x.visible = ps->IsVisible();
				x.fade_at_distance = ps->GetFadeAtDistance();
				x.fade[0] = ps->GetMinFadeDistanceStart();
				x.fade[1] = ps->GetMinFadeDistanceEnd();
				x.fade[2] = ps->GetMaxFadeDistanceStart();
				x.fade[3] = ps->GetMaxFadeDistanceEnd();
				uint32_t lDead = 0;
				for (int i = 0; i < ps->GetEmitterNum() && i < 32; ++i)
					if (ps->GetEmitter(i)->IsDying())
						lDead |= 1u << i;
				x.dead = lDead;
				x.owned = ps->GetEntityParent() != NULL;
				c.mParticles.push_back(x);
			}
		w.particles = Sl(c.mParticles);

		cSoundEntityIterator sIt = pMap->GetWorld()->GetSoundEntityIterator();
		while (sIt.HasNext())
			if (cSoundEntity *s = sIt.Next(); s->IsSaved() && s->GetParent() == NULL && s->GetEntityParent() == NULL && s->GetData())
			{
				OhplGs4Sound x;
				memset(&x, 0, sizeof(x));
				x.name = c.mPool.Add(s->GetName());
				x.data = c.mPool.Add(s->GetData()->GetName());
				x.remove = s->GetRemoveWhenOver();
				x.active = s->IsActive();
				x.stopped = s->IsStopped() || s->IsFadingOut();
				x.min_dist = s->GetMinDistance();
				x.max_dist = s->GetMaxDistance();
				x.volume = s->GetVolume();
				CopyF(x.pos, s->GetLocalPosition());
				cSomaSoundInstance *pInst = (cSomaSoundInstance *)s->GetEvent();
				x.fade = pInst ? pInst->mfFadeDest : 1.0f;
				x.volume_mul = pInst ? pInst->mfVolumeMulDest : 1.0f;
				c.mParams.emplace_back();
				for (int i = 0; pInst && i < pInst->GetParamNum(); ++i)
					c.mParams.back().push_back(pInst->GetParamValue(i));
				x.params = Sl(c.mParams.back());
				c.mSounds.push_back(x);
			}
		w.sounds = Sl(c.mSounds);

		cBillboardIterator bIt = pMap->GetWorld()->GetBillboardIterator();
		while (bIt.HasNext())
			if (cBillboard *b = bIt.Next(); b->IsSaved() && b->GetParent() == NULL && b->GetEntityParent() == NULL)
			{
				OhplGs4Billboard x;
				memset(&x, 0, sizeof(x));
				x.name = c.mPool.Add(b->GetName());
				x.visible = b->GetVisibleVar();
				CopyF(x.color, b->GetColor());
				c.mBillboards.push_back(x);
			}
		w.billboards = Sl(c.mBillboards);

		w.health = pPlayer ? pPlayer->mfHealth : 1.0f;
		cMatrixf m;
		for (cSomaLuxEntity *p : pMap->GetEntities())
			if (SomaAgentGetMatrix(p, m))
			{
				OhplGs4Agent x;
				memset(&x, 0, sizeof(x));
				x.name = c.mPool.Add(p->msName);
				CopyF(x.mtx, m);
				x.state = SomaAgentGetState(p);
				float fYaw;
				bool bSenses, bDetection;
				SomaAgentSaveExtra(p, fYaw, bSenses, bDetection);
				x.yaw = fYaw;
				x.senses = bSenses;
				x.detection = bDetection;
				x.path = c.mPool.Add(SomaAgentSavePath(p));
				c.mAgents.push_back(x);
			}
		w.agents = Sl(c.mAgents);

		w.underwater = pMap->mbIsUnderwater;
		cWorld *wd = pMap->GetWorld();
		bool vFlags[5] = {wd->GetFogActive(), wd->GetFogCulling(), wd->GetFogUnderwater(), wd->GetSecondaryFogActive(), wd->GetSkyBoxActive()};
		for (int i = 0; i < 5; ++i)
			w.fog.flags[i] = vFlags[i];
		float vFloats[9] = {wd->GetFogStart(), wd->GetFogEnd(), wd->GetFogFalloffExp(), wd->GetFogBrightness(), wd->GetSecondaryFogStart(),
							wd->GetSecondaryFogEnd(), wd->GetSecondaryFogFalloffExp(), wd->GetSecondaryFogBrightness(), wd->GetSkyBoxBrightness()};
		for (int i = 0; i < 9; ++i)
			w.fog.floats[i] = vFloats[i];
		CopyF(w.fog.colors[0], wd->GetFogColor());
		CopyF(w.fog.colors[1], wd->GetSecondaryFogColor());
		CopyF(w.fog.colors[2], wd->GetSkyBoxColor());

		for (const cSomaCameraTextureState &t : SomaGetCameraTextures())
		{
			OhplGs4CamTex x;
			memset(&x, 0, sizeof(x));
			x.name = c.mPool.Add(t.msName);
			x.attached = c.mPool.Add(t.msAttached);
			static_assert(sizeof(t.mvSize) == sizeof(x.size), "cVector2l size");
			memcpy(x.size, &t.mvSize, sizeof(x.size));
			x.fps = t.mlFPS;
			x.fov = t.mfFOV;
			x.near_plane = t.mfNear;
			x.far_plane = t.mfFar;
			CopyF(x.rot, t.mtxRotation);
			CopyF(x.pos, t.mvPosition);
			c.mCamTextures.push_back(x);
		}
		w.cam_textures = Sl(c.mCamTextures);

		cFogAreaIterator fIt = wd->GetFogAreaIterator();
		while (fIt.HasNext())
		{
			cFogArea *f = fIt.Next();
			c.mFogAreas.push_back(OhplGs4FogArea{c.mPool.Add(f->GetName()), f->IsVisible()});
		}
		w.fog_areas = Sl(c.mFogAreas);

		for (auto &[pParent, pChild, sFunc] : pMap->msetColliding)
			c.mColliding.push_back(OhplGs4Colliding{c.mPool.Add(pParent->msName), c.mPool.Add(pChild->msName), c.mPool.Add(sFunc)});
		w.colliding = Sl(c.mColliding);
		w.display_name = c.mPool.Add(pMap->msDisplayNameEntry);

		cCamera *pCam = pPlayer ? pPlayer->GetCamera() : NULL;
		w.has_camera = pCam != NULL;
		if (pCam)
		{
			w.camera.detached = (uint8_t)(pBody && pBody->GetCamera() == NULL);
			CopyF(w.camera.pos, pCam->GetPosition());
			w.camera.limits[0] = pCam->GetPitchMinLimit();
			w.camera.limits[1] = pCam->GetPitchMaxLimit();
			w.camera.limits[2] = pCam->GetYawMinLimit();
			w.camera.limits[3] = pCam->GetYawMaxLimit();
			static_assert(sizeof(pPlayer->mFOVMul) == sizeof(w.camera.fov_mul), "cFadeValue size");
			memcpy(&w.camera.fov_mul, &pPlayer->mFOVMul, sizeof(w.camera.fov_mul));
			memcpy(&w.camera.aspect_mul, &pPlayer->mAspectMul, sizeof(w.camera.aspect_mul));
		}

		cLightListIterator ownedIt = wd->GetLightIterator();
		while (ownedIt.HasNext())
			if (iLight *l = ownedIt.Next(); l->IsSaved() && l->GetParent() == NULL && l->GetEntityParent() && l->GetFlickerActive() == false)
			{
				OhplGs4OwnedLight x;
				memset(&x, 0, sizeof(x));
				x.name = c.mPool.Add(l->GetName());
				x.visible = l->GetVisibleVar();
				CopyF(x.color, l->IsFading() ? l->GetDestColor() : l->GetDiffuseColor());
				x.radius = l->IsFading() ? l->GetDestRadius() : l->GetRadius();
				c.mOwnedLights.push_back(x);
			}
		w.owned_lights = Sl(c.mOwnedLights);

		cSomaToneMapping *pTone = cSomaToneMapping::Get();
		w.has_tone = pTone != NULL;
		if (pTone)
		{
			w.tone.exposure = pTone->mfWorldExposure;
			w.tone.white_cut = pTone->mfWorldWhiteCut;
		}
	}

	// The whole save file; the music state is taken after the world, like the original stream
	static std::string Build(const tString &asMapFile, const tString &asStartPos, bool abExploration)
	{
		cWriteCtx c;
		OhplGs4World w;
		cStrPool pool;
		tString sVars = SomaSerializeGlobalVars();
		tString sPreload = SomaPreloadMap();
		std::vector<OhplGs4Str> vVisited, vPlayed;
		for (const tString &sMap : gpSomaBase->GetVisitedMaps())
			vVisited.push_back(pool.Add(sMap));
		std::set<tString> setPlayed;
		if (cSomaLuxVoiceHandler::Get())
			setPlayed = cSomaLuxVoiceHandler::Get()->msetPlayedLines;
		for (const tString &sLine : setPlayed)
			vPlayed.push_back(pool.Add(sLine));
		GatherWorld(c, w);
		tString sMusic = SomaSerializeMusic();
		OhplGs4HeaderOut h = {RS(asMapFile), RS(asStartPos), Sl(vVisited), RS(sVars), RS(sPreload), Sl(vPlayed), RS(sMusic), abExploration};
		OhplGs4WriteHost host = {&c, WMapScript, WEntityHead, WEntityScript, WEntityTail, WOtherKey, WOtherTimers, WOtherScript};
		std::string sOut;
		ohpl_gs4_encode_save(&h, &w, &host, SinkS, &sOut);
		return sOut;
	}

	// ReadWorld host: the stream parse and its order are Rust; these apply to the engine objects
	static cReadCtx *R(void *c) { return (cReadCtx *)c; }

	static bool RHasPlayer(void *) { return cSomaLuxPlayer::Get() != NULL; }
	static bool RHasCamera(void *)
	{
		cSomaLuxPlayer *pPlayer = cSomaLuxPlayer::Get();
		return pPlayer && pPlayer->GetCamera();
	}
	static void RMapTimersBegin(void *c) { R(c)->mpMap->GetTimers().clear(); }
	static void RMapTimer(void *c, const OhplGs4MapTimer *x)
	{
		cSomaLuxTimer t;
		t.msName = FromRS(x->name);
		t.msFunction = FromRS(x->func);
		t.mfTime = x->time;
		t.mbPaused = x->paused;
		t.mfUserFloat = x->user_float;
		t.mlUserInt = x->user_int;
		t.msUserString = FromRS(x->user_str);
		t.mfLength = x->length;
		R(c)->mpMap->GetTimers().push_back(t);
	}
	static size_t RObj(cReadCtx *r, asIScriptObject *apObj)
	{
		if (apObj)
			r->mpScript->mpEngine = apObj->GetEngine();
		return (size_t)apObj;
	}
	static size_t RMapScript(void *c) { return RObj(R(c), R(c)->mpMap->GetScript()); }
	static void RSpawned(void *c, const OhplGs4Spawned *x)
	{
		cSomaLuxMap *pMap = R(c)->mpMap;
		tString sName = FromRS(x->name), sFile = FromRS(x->file);
		cMatrixf m;
		FromF(m, x->mtx);
		cVector3f vScale;
		FromF(vScale, x->scale);
		cSomaID id;
		FromB(id, x->id);
		cSomaLuxEntity *p = pMap->GetEntity(sName);
		if (p == NULL || p->mbSpawned == false)
			p = pMap->CreateEntity(sName, sFile, m, vScale);
		if (p == NULL)
			return;
		p->mID = id;
		pMap->mlNextId = std::max(pMap->mlNextId, id.mB + 1);
	}

	static size_t REntityBegin(void *c, OhplGs4Str aName)
	{
		cReadCtx *r = R(c);
		cSomaLuxEntity *p = r->mpMap->GetEntity(FromRS(aName));
		if (p == NULL)
			r->mpDummyEnt.reset(new cSomaLuxEntity);
		return (size_t)p;
	}

	static void REntityHead(void *c, size_t h, const OhplGs4EntityHead *e)
	{
		cReadCtx *r = R(c);
		cSomaLuxEntity *p = (cSomaLuxEntity *)h;
		cSomaLuxEntity *t = p ? p : r->mpDummyEnt.get();
		cSomaLuxMap *apMap = r->mpMap;
		t->mbInteractionDisabled = e->interaction_disabled;
		t->mbInteractedWith = e->interacted_with;
		t->mlParentType = e->parent_type;
		FromB(t->mParentID, e->parent_id);
		t->msParentName = FromRS(e->parent_name);
		t->msInteractCallback = FromRS(e->interact_cb);
		t->mbInteractCallbackAutoRemove = e->interact_auto_remove;
		t->msLookAtCallback = FromRS(e->look_cb);
		t->mbLookAtCallbackAutoRemove = e->look_auto_remove;
		if (glPendingVersion >= 8)
		{
			t->mbLookAtCheckCenter = e->look_center;
			t->mbLookAtCheckRay = e->look_ray;
			t->mfLookAtMaxDistance = e->look_max;
			t->mfLookAtDelay = e->look_delay;
		}
		t->mvCollideCallbacks.clear();
		for (size_t i = 0; i < e->collide.n; ++i)
			t->mvCollideCallbacks.push_back(cSomaLuxEntity::cCollideCallback{FromRS(e->collide.p[i].child), FromRS(e->collide.p[i].func)});
		for (size_t i = 0; i < e->bodies.n; ++i)
		{
			cMatrixf m;
			FromF(m, e->bodies.p[i].mtx);
			bool bBodyActive = e->bodies.p[i].active;
			float fMass = e->bodies.p[i].mass;
			bool bGravity = e->bodies.p[i].gravity;
			if (p == NULL || i >= p->mvBodies.size())
				continue;
			if (fMass >= 0)
			{
				if (glPendingVersion < 17 && fMass == 0 && p->mvBodies[i]->GetMass() > 0)
					p->SetStaticPhysics(true);
				p->mvBodies[i]->SetMass(fMass);
				p->mvBodies[i]->SetGravity(bGravity);
			}
			if (p->mpMesh && p->mpMesh->IsStatic() && m != p->mvBodies[i]->GetLocalMatrix())
				p->MakeDynamic();
			p->mvBodies[i]->SetMatrix(m);
			if (glPendingVersion >= 5)
				p->mvBodies[i]->SetActive(bBodyActive);
		}
		if (glPendingVersion >= 6)
		{
			if (e->has_mtx)
			{
				cMatrixf m;
				FromF(m, e->mtx);
				if (p && p->mvBodies.empty() && m != p->GetMatrix())
					p->SetMatrix(m);
			}
			if (glPendingVersion >= 17)
			{
				std::vector<float> vMass(e->dyn_mass.p, e->dyn_mass.p + e->dyn_mass.n);
				if (p)
				{
					p->mbStaticPhysics = e->static_physics;
					p->mvDynamicMass = vMass;
				}
			}
			if (p)
				p->RemoveAttachment();
			if (e->has_attach)
			{
				cSomaLuxEntity::cAttachment a;
				a.msParent = FromRS(e->attach.parent);
				tString sBody = FromRS(e->attach.body);
				a.msSocket = FromRS(e->attach.socket);
				a.mbUseRotation = e->attach.use_rotation;
				a.mbLocked = e->attach.locked;
				FromF(a.m_mtxParentPrev, e->attach.parent_prev);
				FromF(a.m_mtxOffset, e->attach.offset);
				cSomaLuxEntity *pParent = apMap->GetEntity(a.msParent);
				if (p && pParent)
				{
					for (iPhysicsBody *b : pParent->mvBodies)
						if (b->GetName() == sBody)
							a.mpBody = b;
					if (sBody == "" || a.mpBody)
						p->mpAttachment = new cSomaLuxEntity::cAttachment(a);
				}
			}
		}
		std::vector<iPhysicsJoint *> vBroken = p ? p->Joints() : std::vector<iPhysicsJoint *>();
		for (size_t i = 0; i < e->joints.n; ++i)
		{
			tString sJoint = FromRS(e->joints.p[i].name);
			cVector2f v;
			FromF(v, e->joints.p[i].limits);
			iPhysicsJoint *j = p && i < p->mvJoints.size() ? p->mvJoints[i] : NULL;
			if (glPendingVersion >= 11)
			{
				auto it = std::find_if(vBroken.begin(), vBroken.end(), [&](iPhysicsJoint *x) { return x->GetName() == sJoint; });
				j = it != vBroken.end() ? *it : NULL;
			}
			std::erase(vBroken, j);
			if (j && j->GetType() == ePhysicsJointType_Hinge)
			{
				static_cast<iPhysicsJointHinge *>(j)->SetMinAngle(v.x);
				static_cast<iPhysicsJointHinge *>(j)->SetMaxAngle(v.y);
			}
			else if (j && j->GetType() == ePhysicsJointType_Slider)
			{
				static_cast<iPhysicsJointSlider *>(j)->SetMinDistance(v.x);
				static_cast<iPhysicsJointSlider *>(j)->SetMaxDistance(v.y);
			}
		}
		if (glPendingVersion >= 11)
			for (iPhysicsJoint *j : vBroken)
				apMap->GetWorld()->GetPhysicsWorld()->DestroyJoint(j);
		ApplyTimers(t, &e->timers);
	}

	static size_t REntityScript(void *c, size_t h)
	{
		cReadCtx *r = R(c);
		cSomaLuxEntity *t = h ? (cSomaLuxEntity *)h : r->mpDummyEnt.get();
		return RObj(r, t->GetScript());
	}

	static void REntityTail(void *c, size_t h, const OhplGs4EntityHead *e, const OhplGs4EntityTail *x)
	{
		cReadCtx *r = R(c);
		cSomaLuxEntity *p = (cSomaLuxEntity *)h;
		cSomaLuxEntity *t = p ? p : r->mpDummyEnt.get();
		if (glPendingVersion >= 12)
		{
			t->mmapScriptVars.clear();
			for (size_t i = 0; i < x->vars.n; ++i)
				t->mmapScriptVars[FromRS(x->vars.p[i].key)] = FromRS(x->vars.p[i].value);
		}
		if (glPendingVersion >= 13)
		{
			t->mbGuiActive = x->gui_active;
			t->mfGuiFade = t->mbGuiActive;
		}
		if (glPendingVersion >= 23)
		{
			t->msOnGuiFunc = FromRS(x->on_gui);
			for (size_t i = 0; i < x->imgui_timers.n; ++i)
			{
				cSomaImGui::cTimer tm;
				static_assert(sizeof(tm) == sizeof(OhplGs4ImTimer), "imgui timer size");
				memcpy(&tm, &x->imgui_timers.p[i], sizeof(tm));
				if (t->mpImGui)
					t->mpImGui->mvTimers.push_back(tm);
			}
		}
		for (size_t i = 0; i < x->imgui_states.n; ++i)
		{
			cSomaImGui::cState st;
			static_assert(sizeof(st) == sizeof(OhplGs4ImState), "imgui state size");
			memcpy(&st, &x->imgui_states.p[i].state, sizeof(st));
			if (t->mpImGui)
				t->mpImGui->mmapStates[x->imgui_states.p[i].id] = st;
		}
		for (size_t i = 0; i < x->anims.n; ++i)
		{
			const OhplGs4Anim &an = x->anims.p[i];
			cAnimationState *a = t->mpMesh ? t->mpMesh->GetAnimationStateFromName(FromRS(an.name)) : NULL;
			if (a == NULL)
				continue;
			a->SetActive(an.active);
			a->SetLoop(an.looping);
			a->SetPaused(an.paused);
			a->SetTimePosition(an.time);
			a->SetWeight(an.weight);
			a->SetSpeed(an.speed);
			a->SetFadeStep(an.fade);
		}
		if (glPendingVersion >= 20)
		{
			t->msConnectionCallback = FromRS(x->conn_cb);
			t->mvConnections.clear();
			for (size_t i = 0; i < x->conns.n; ++i)
			{
				cSomaLuxEntity::cConnection cn;
				cn.msName = FromRS(x->conns.p[i].name);
				cn.msEntity = FromRS(x->conns.p[i].entity);
				cn.mbInvert = x->conns.p[i].invert;
				cn.mlStatesUsed = x->conns.p[i].states_used;
				t->mvConnections.push_back(cn);
			}
		}
		bool bEffects = e->effects_active, bActive = e->active;
		if (p && p->mbEffectsActive != bEffects)
			p->SetEffectsActive(bEffects && p->mbActive);
		t->mbEffectsActive = bEffects;
		if (p && p->mbActive != bActive)
			p->SetActive(bActive);
		r->mpDummyEnt.reset();
	}

	static void REntitiesDone(void *c, const OhplGs4Str *apSaved, size_t alCount)
	{
		cSomaLuxMap *pMap = R(c)->mpMap;
		std::set<tString> setSaved;
		for (size_t i = 0; i < alCount; ++i)
			setSaved.insert(FromRS(apSaved[i]));
		// entities destroyed before the save (broken windows) come back with the fresh map load
		for (cSomaLuxEntity *p : std::vector<cSomaLuxEntity *>(pMap->GetEntities()))
			if (setSaved.count(cString::ToLowerCase(p->msName)) == 0)
				pMap->DestroyEntity(p);
	}

	static size_t ROtherBegin(void *c, OhplGs4Str aKey)
	{
		cReadCtx *r = R(c);
		if (r->mbOtherBuilt == false)
		{
			r->mbOtherBuilt = true;
			for (cSomaLuxScriptable *p : cSomaLuxScriptable::GetAll())
				if (dynamic_cast<cSomaLuxEntity *>(p) == NULL && p->GetScript() && p->mbSaved)
					r->mmapOther.emplace(ScriptableKey(p), p);
		}
		auto it = r->mmapOther.find(FromRS(aKey));
		if (it != r->mmapOther.end())
			return (size_t)it->second;
		r->mpDummyOther.reset(new cSomaLuxScriptable);
		return 0;
	}
	static void ROtherTimers(void *c, size_t h, const OhplGs4Timers *t)
	{
		cReadCtx *r = R(c);
		ApplyTimers(h ? (cSomaLuxScriptable *)h : r->mpDummyOther.get(), t);
	}
	static void ROtherReset(void *c, size_t h)
	{
		cReadCtx *r = R(c);
		cSomaLuxScriptable *p = h ? (cSomaLuxScriptable *)h : r->mpDummyOther.get();
		if (dynamic_cast<cSomaLuxModule *>(p))
			ResetNoSave(p->GetScript());
	}
	static size_t ROtherScript(void *c, size_t h)
	{
		cReadCtx *r = R(c);
		cSomaLuxScriptable *p = h ? (cSomaLuxScriptable *)h : r->mpDummyOther.get();
		size_t lObj = RObj(r, p->GetScript());
		if (h == 0)
			r->mpDummyOther.reset();
		return lObj;
	}

	static void RLight(void *c, const OhplGs4Light *x)
	{
		iLight *l = R(c)->mpMap->GetWorld()->GetLight(FromRS(x->name));
		if (l == NULL)
			return;
		cColor col;
		FromF(col, x->color);
		l->SetActive(x->active);
		l->SetVisible(x->visible);
		l->SetDiffuseColor(col);
		l->SetRadius(x->radius);
		l->SetBrightness(x->brightness);
		l->SetFlickerActive(x->flicker);
	}

	static void RParticlesBegin(void *c)
	{
		cReadCtx *r = R(c);
		cParticleSystemIterator psIt = r->mpMap->GetWorld()->GetParticleSystemIterator();
		while (psIt.HasNext())
			if (cParticleSystem *ps = psIt.Next(); ps->IsSaved() && ps->GetParent() == NULL && ps->GetEntityParent() == NULL)
				r->mmapPS.emplace(ps->GetName(), ps);
			else if (ps->IsSaved() && ps->GetEntityParent())
				r->mmapOwnedPS.emplace(ps->GetName(), ps);
	}
	static void RParticle(void *c, const OhplGs4Particle *x)
	{
		cReadCtx *r = R(c);
		tString sName = FromRS(x->name), sData = FromRS(x->data);
		cVector3f vSize;
		FromF(vSize, x->size);
		cMatrixf m;
		FromF(m, x->mtx);
		cColor col;
		FromF(col, x->color);
		cParticleSystem *ps = NULL;
		auto &mapSrc = x->owned ? r->mmapOwnedPS : r->mmapPS;
		if (auto it = mapSrc.find(sName); it != mapSrc.end())
		{
			ps = it->second;
			mapSrc.erase(it);
		}
		else if (x->owned || (ps = r->mpMap->GetWorld()->CreateParticleSystem(sName, sData, vSize)) == NULL)
			return;
		ps->SetMatrix(m);
		ps->SetColor(col);
		ps->SetBrightness(x->brightness);
		ps->SetActive(x->active);
		ps->SetVisible(x->visible);
		ps->SetFadeAtDistance(x->fade_at_distance);
		ps->SetMinFadeDistanceStart(x->fade[0]);
		ps->SetMinFadeDistanceEnd(x->fade[1]);
		ps->SetMaxFadeDistanceStart(x->fade[2]);
		ps->SetMaxFadeDistanceEnd(x->fade[3]);
		for (int e = 0; e < ps->GetEmitterNum() && e < 32; ++e)
			if (x->dead & (1u << e))
				ps->GetEmitter(e)->KillInstantly();
	}
	static void RParticlesEnd(void *c, bool abHas)
	{
		if (abHas)
			for (auto &it : R(c)->mmapPS)
				it.second->KillInstantly();
	}

	static void RSoundsBegin(void *c)
	{
		cReadCtx *r = R(c);
		cSoundEntityIterator sIt = r->mpMap->GetWorld()->GetSoundEntityIterator();
		while (sIt.HasNext())
			if (cSoundEntity *s = sIt.Next(); s->IsSaved() && s->GetParent() == NULL && s->GetEntityParent() == NULL)
				r->mmapSounds.emplace(s->GetName(), s);
	}
	static void RSound(void *c, const OhplGs4Sound *x)
	{
		cReadCtx *r = R(c);
		tString sName = FromRS(x->name), sData = FromRS(x->data);
		cVector3f vPos;
		FromF(vPos, x->pos);
		cSoundEntity *s = NULL;
		if (auto it = r->mmapSounds.find(sName); it != r->mmapSounds.end())
		{
			s = it->second;
			r->mmapSounds.erase(it);
		}
		else if ((s = r->mpMap->GetWorld()->CreateSoundEntity(sName, sData, x->remove)) == NULL)
			return;
		s->SetActive(x->active);
		s->SetMinDistance(x->min_dist);
		s->SetMaxDistance(x->max_dist);
		s->SetVolume(x->volume);
		s->SetPosition(vPos);
		if (cSomaSoundInstance *pInst = (cSomaSoundInstance *)s->GetEvent())
		{
			pInst->FadeInTo(x->fade, 0);
			pInst->SetVolumeMul(x->volume_mul);
			for (size_t p = 0; p < x->params.n; ++p)
				pInst->SetParam((int)p, x->params.p[p]);
		}
		if (x->stopped)
			s->Stop(false);
	}
	static void RSoundsEnd(void *c, bool abHas)
	{
		if (abHas)
			for (auto &it : R(c)->mmapSounds)
				it.second->Stop(false);
	}

	static void RBillboard(void *c, const OhplGs4Billboard *x)
	{
		cColor col;
		FromF(col, x->color);
		if (cBillboard *b = R(c)->mpMap->GetWorld()->GetBillboard(FromRS(x->name)))
		{
			b->SetVisible(x->visible);
			b->SetColor(col);
		}
	}

	static void RPlayerApply(void *, const OhplGs4Player *x)
	{
		if (x->gravity_active >= 0)
		{
			glSavedGravityActive = x->gravity_active;
			FromF(gvSavedGravity, x->gravity);
		}
		cSomaLuxPlayer *pPlayer = cSomaLuxPlayer::Get();
		if (pPlayer)
		{
			cVector3f vFeet;
			FromF(vFeet, x->feet);
			if (x->move_state >= 0)
				pPlayer->ChangeMoveState(x->move_state);
			if (x->state >= 0)
				pPlayer->ChangeState(x->state);
			pPlayer->PlaceAtStart(vFeet, x->yaw, x->char_crouching > 0);
			if (pPlayer->GetCamera())
				pPlayer->GetCamera()->SetPitch(x->pitch);
		}
	}
	static void RHealth(void *, float afHealth) { cSomaLuxPlayer::Get()->mfHealth = afHealth; }

	static void RAgent(void *c, const OhplGs4Agent *x)
	{
		cSomaLuxEntity *p = R(c)->mpMap->GetEntity(FromRS(x->name));
		cMatrixf m;
		FromF(m, x->mtx);
		if (p && SomaAgentSetMatrix(p, m))
		{
			if (glPendingVersion >= 10)
				SomaAgentLoadExtra(p, x->yaw, x->senses, x->detection);
			SomaAgentChangeState(p, x->state);
			SomaAgentLoadPath(p, FromRS(x->path));
		}
	}
	static void RUnderwater(void *, bool ab) { gnSavedUnderwater = ab; }
	static void RFog(void *c, const OhplGs4Fog *x)
	{
		cWorld *w = R(c)->mpMap->GetWorld();
		cColor col[3];
		for (int i = 0; i < 3; ++i)
			FromF(col[i], x->colors[i]);
		const float *f = x->floats;
		const bool *b = x->flags;
		w->SetFogActive(b[0]);
		w->SetFogCulling(b[1]);
		w->SetFogUnderwater(b[2]);
		w->SetSecondaryFogActive(b[3]);
		w->SetSkyBoxActive(b[4]);
		w->SetFogStart(f[0]);
		w->SetFogEnd(f[1]);
		w->SetFogFalloffExp(f[2]);
		w->SetFogBrightness(f[3]);
		w->SetSecondaryFogStart(f[4]);
		w->SetSecondaryFogEnd(f[5]);
		w->SetSecondaryFogFalloffExp(f[6]);
		w->SetSecondaryFogBrightness(f[7]);
		w->SetSkyBoxBrightness(f[8]);
		w->SetFogColor(col[0]);
		w->SetSecondaryFogColor(col[1]);
		w->SetSkyBoxColor(col[2]);
	}
	static void RCamTexture(void *, const OhplGs4CamTex *x)
	{
		cSomaCameraTextureState c;
		c.msName = FromRS(x->name);
		c.msAttached = FromRS(x->attached);
		memcpy(&c.mvSize, x->size, sizeof(x->size));
		c.mlFPS = x->fps;
		c.mfFOV = x->fov;
		c.mfNear = x->near_plane;
		c.mfFar = x->far_plane;
		FromF(c.mtxRotation, x->rot);
		FromF(c.mvPosition, x->pos);
		SomaRestoreCameraTexture(c);
	}
	static void RFogArea(void *c, const OhplGs4FogArea *x)
	{
		if (cFogArea *f = R(c)->mpMap->GetWorld()->GetFogArea(FromRS(x->name)))
			f->SetVisible(x->visible);
	}
	static void RColliding(void *c, const OhplGs4Colliding *x)
	{
		cSomaLuxMap *pMap = R(c)->mpMap;
		cSomaLuxEntity *pParent = pMap->GetEntity(FromRS(x->parent)), *pChild = pMap->GetEntity(FromRS(x->child));
		if (pParent && pChild)
			pMap->msetColliding.emplace(pParent, pChild, FromRS(x->func));
	}
	// Maps that set it in OnEnter, which a load skips
	static void RDisplayName(void *c, OhplGs4Str aName) { R(c)->mpMap->msDisplayNameEntry = FromRS(aName); }
	// mid-sequence saves: custom-control cameras are detached and limited by the script
	static void RCamera(void *, const OhplGs4Camera *x)
	{
		cSomaLuxPlayer *pPlayer = cSomaLuxPlayer::Get();
		cCamera *pCam = pPlayer->GetCamera();
		cVector3f vPos;
		FromF(vPos, x->pos);
		memcpy(&pPlayer->mFOVMul, &x->fov_mul, sizeof(x->fov_mul));
		memcpy(&pPlayer->mAspectMul, &x->aspect_mul, sizeof(x->aspect_mul));
		pCam->SetPitchLimits(x->limits[0], x->limits[1]);
		pCam->SetYawLimits(x->limits[2], x->limits[3]);
		if (x->detached != 0 && pPlayer->GetCharacterBody())
		{
			pPlayer->GetCharacterBody()->SetCamera(NULL);
			pCam->SetPosition(vPos);
		}
	}
	static void ROwnedLight(void *c, const OhplGs4OwnedLight *x)
	{
		iLight *l = R(c)->mpMap->GetWorld()->GetLight(FromRS(x->name));
		if (l == NULL)
			return;
		cColor col;
		FromF(col, x->color);
		l->SetVisible(x->visible);
		l->SetDiffuseColor(col);
		l->SetRadius(x->radius);
	}
	static void RTone(void *, const OhplGs4Tone *x)
	{
		if (cSomaToneMapping *pTone = cSomaToneMapping::Get())
			pTone->FadeExposure(x->exposure, x->white_cut, 0);
	}

	static void ReadWorld(OhplGs4In *apIn)
	{
		cReadCtx r;
		r.mpMap = cSomaLuxMap::GetCurrent();
		r.mpScript.reset(new cScriptCtx(NULL));
		OhplGs4ReadHost host = {&r,
								&r.mpScript->mHost,
								RHasPlayer,
								RHasCamera,
								RMapTimersBegin,
								RMapTimer,
								RMapScript,
								RSpawned,
								REntityBegin,
								REntityHead,
								REntityScript,
								REntityTail,
								REntitiesDone,
								ROtherBegin,
								ROtherTimers,
								ROtherReset,
								ROtherScript,
								RLight,
								RParticlesBegin,
								RParticle,
								RParticlesEnd,
								RSoundsBegin,
								RSound,
								RSoundsEnd,
								RBillboard,
								RPlayerApply,
								RHealth,
								RAgent,
								RUnderwater,
								RFog,
								RCamTexture,
								RFogArea,
								RColliding,
								RDisplayName,
								RCamera,
								ROwnedLight,
								RTone};
		ohpl_gs4_read_world(apIn, glPendingVersion, &host);
	}

	// Setup() may have run Map_SetUnderwater; redo it so globals and gravity follow the save
	static void ApplyUnderwater(cSomaLuxMap *pMap, bool bUnderwater)
	{
		asIScriptModule *pModule = pMap->GetScript() ? pMap->GetScript()->GetObjectType()->GetModule() : NULL;
		asIScriptFunction *pFunc = pModule ? pModule->GetFunctionByName("Map_SetUnderwater") : NULL;
		if (bUnderwater != pMap->mbIsUnderwater && pFunc)
		{
			asIScriptContext *pCtx = pModule->GetEngine()->RequestContext();
			pCtx->Prepare(pFunc);
			pCtx->SetArgByte(0, bUnderwater);
			pCtx->SetArgByte(1, false);
			pCtx->Execute();
			pModule->GetEngine()->ReturnContext(pCtx);
		}
		else if (bUnderwater && pMap->mbIsUnderwater == false)
		{
			pMap->mbIsUnderwater = true;
			gbSomaUnderwaterEffects = true;
			++glSomaUnderwaterUsers;
		}
	}
};

tWString cSomaSaveHandler::GetSaveDir()
{
	struct cDirs
	{
		tWString msLast;
	} dirs;
	tWString sRoot = cPlatform::GetSystemSpecialPath(eSystemPath_XDGDataHome);
	const std::string &sId = SomaGameId();
	ohpl_gs4_paths(0, (const uint32_t *)sRoot.data(), sRoot.size(), (const unsigned char *)sId.data(), sId.size(), false, 0,
				   [](void *c, const uint32_t *apDir, size_t alDir, bool) {
					   tWString sDir((const wchar_t *)apDir, alDir);
					   if (cPlatform::FolderExists(sDir) == false)
						   cPlatform::CreateFolder(sDir);
					   ((cDirs *)c)->msLast = sDir;
				   },
				   &dirs);
	return dirs.msLast;
}

void cSomaSaveHandler::OnMapEnter(const tString &asMapFile, const tString &asStartPos)
{
	gsMapFile = asMapFile;
	gsStartPos = asStartPos;
}

bool cSomaSaveHandler::Save(const tWString &asFile)
{
	if (gsMapFile.empty() || cSomaLuxMap::GetCurrent() == NULL ||
		cString::GetFileName(gsMapFile) == cString::GetFileName(gpSomaBase->GetInitConfigString("MainMenu", "File")))
		return false;
	std::string sData = cSomaSaveState::Build(gsMapFile, gsStartPos, gbExplorationMode);

	tWString sPath = GetSaveDir() + cString::GetFileNameW(asFile);
	std::ofstream file(cString::To8Char(sPath).c_str(), std::ios::binary | std::ios::trunc);
	if (file.is_open() == false || file.write(sData.data(), sData.size()).good() == false)
	{
		Error("SOMA save: could not write '%s'\n", cString::To8Char(sPath).c_str());
		return false;
	}
	Log("SOMA save: %s (%s, %d bytes)\n", cString::To8Char(sPath).c_str(), gsMapFile.c_str(), (int)sData.size());
	return true;
}

static int glDelayedAutoSave = 0;

void cSomaSaveHandler::RunDelayedAutoSave()
{
	if (glDelayedAutoSave == 0 || cSomaLuxMap::GetCurrent() == NULL)
		return;
	bool bCheckpoint = glDelayedAutoSave == 2;
	glDelayedAutoSave = 0;
	AutoSave(bCheckpoint);
}

bool cSomaSaveHandler::AutoSave(bool abCheckpoint)
{
	DeleteOldestSaveFiles(kMaxAutoSaves);
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
	OhplGs4Header *pHeader = NULL;
	int lStatus = ohpl_gs4_header_read((const unsigned char *)sData.data(), sData.size(), file.is_open(), &pHeader);
	struct cHeaderGuard
	{
		OhplGs4Header *mp;
		~cHeaderGuard() { ohpl_gs4_header_free(mp); }
	} guard = {pHeader};
	auto Field = [&](size_t alWhich) {
		const unsigned char *p = NULL;
		size_t n = 0;
		ohpl_gs4_header_str(pHeader, alWhich, &p, &n);
		return std::string((const char *)p, n);
	};
	if (lStatus == 1)
	{
		Error("SOMA save: could not read '%s'\n", cString::To8Char(sPath).c_str());
		return false;
	}
	OhplGs4HeaderInfo info = ohpl_gs4_header_info(pHeader);
	int lVersion = info.version;
	gsPendingPreload = lVersion >= 19 ? Field(3) : "";
	if (lStatus == 2)
		return false;
	tString sMap = Field(0);
	tString sPos = Field(1);
	std::set<tString> setVisited;
	for (size_t i = 0; i < info.visited; ++i)
		setVisited.insert(Field(4 + i));
	tString sVars = Field(2);
	bool bExploration = info.exploration;
	std::set<tString> setPlayed;
	for (size_t i = 0; i < info.played; ++i)
		setPlayed.insert(Field(0x10000 + i));

	gbExplorationMode = bExploration;
	if (cSomaLuxVoiceHandler::Get())
		cSomaLuxVoiceHandler::Get()->msetPlayedLines = setPlayed;

	SomaDeserializeGlobalVars(sVars);
	// The saved state replaces OnStart
	setVisited.insert(sMap);
	gpSomaBase->GetVisitedMaps() = setVisited;
	gsPendingState = sData.substr(info.body);
	glPendingVersion = lVersion;
	Log("SOMA save: loading %s (%s)\n", cString::To8Char(sPath).c_str(), sMap.c_str());
	if (abImmediate == false)
	{
		if (gbHoldAfterHeader)
			gsHeldMap = sMap, gsHeldPos = sPos;
		else
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
	OhplGs4In *pIn = ohpl_gs4_in_new((const unsigned char *)sState.data(), sState.size());
	SomaPreloadMap() = gsPendingPreload;
	gnSavedUnderwater = -1;
	glSavedGravityActive = -1;
	cSomaSaveState::ReadWorld(pIn);
	if (glPendingVersion >= 27)
	{
		const unsigned char *pMusic = NULL;
		size_t lMusic = 0;
		ohpl_gs4_in_str(pIn, &pMusic, &lMusic);
		SomaDeserializeMusic(std::string((const char *)pMusic, lMusic));
	}
	bool bTruncated = ohpl_gs4_in_ok(pIn) == false;
	ohpl_gs4_in_free(pIn);
	cSomaLuxMap::GetCurrent()->Setup();
	if (gnSavedUnderwater >= 0)
		cSomaSaveState::ApplyUnderwater(cSomaLuxMap::GetCurrent(), gnSavedUnderwater != 0);
	// the body took the pre-load session's gravity when the map created it
	iCharacterBody *pBody = cSomaLuxPlayer::Get() ? cSomaLuxPlayer::Get()->GetCharacterBody() : NULL;
	if (glSavedGravityActive >= 0 && pBody)
	{
		pBody->SetCustomGravityActive(glSavedGravityActive != 0);
		pBody->SetCustomGravity(gvSavedGravity);
	}
	if (bTruncated)
		Warning("SOMA save: saved state is truncated\n");
	if (cSomaLuxPlayer::Get())
		cSomaLuxPlayer::Get()->SetActive(true);
	if (cSomaLuxVoiceHandler::Get())
		cSomaLuxVoiceHandler::Get()->FadeTo(0, 0);
	if (gbHoldAfterLoad)
		SomaSetGamePaused(true);
	else
		RunLoadCallback();
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
	SOMA_METHOD(e, T, "bool AutoSave(bool abSaveCheckpoint, bool abDelayed=true)", +[](void *, bool c, bool d) {
		if (d == false)
			return AutoSave(c);
		glDelayedAutoSave = std::max(glDelayedAutoSave, c ? 2 : 1);
		return true;
	});
	SOMA_METHOD(e, T, "bool GetSaveThreadActive()", +[](void *) { return false; });
	SOMA_METHOD(e, T, "bool HasLoadError(tString&out asError)", +[](void *, tString &) { return false; });
	SOMA_METHOD(e, T, "void DelayedLoadGameFromFile(const tWString&in asSaveFile, const tString&in asCallbackObject, const tString&in asCallbackFunction, bool abWaitAfterHeader, bool abWaitAfterLoad)",
				+[](void *, W f, const tString &o, const tString &fn, bool bWaitHeader, bool bWait) {
					gsLoadCallbackObject = o;
					gsLoadCallbackFunc = fn;
					gbHoldAfterHeader = bWaitHeader;
					gbHoldAfterLoad = Load(f) && bWait;
					gbHoldAfterHeader = false;
				});
	SOMA_METHOD(e, T, "void DelayedSaveGameToFile(const tWString&in asSaveFile, bool abSaveAsCheckpoint)", +[](void *, W f, bool) { Save(f); });
	SOMA_METHOD(e, T, "void DeleteSaveFile(const tWString&in asSaveFile)", +[](void *, W f) { cPlatform::RemoveFile(GetSaveDir() + cString::GetFileNameW(f)); });
	SOMA_METHOD(e, T, "bool IsDoneLoadingHeader()", +[](void *) { return true; });
	SOMA_METHOD(e, T, "void ContinueLoading(bool abDisableWaits)", +[](void *, bool b) {
		if (gsHeldMap != "")
		{
			if (gpSomaBase->GetSplash())
				gpSomaBase->GetSplash()->StartLoad();
			SomaRequestMapChange(gsHeldMap, gsHeldPos);
			gsHeldMap.clear();
		}
		gbHoldAfterLoad &= !b;
	});
	SOMA_METHOD(e, T, "bool IsDoneLoadingSavedGame()", +[](void *) { return gsPendingState.empty() && gsHeldMap.empty(); });
	SOMA_FUNC(e, "void cLux_LoadScreenSetBarPosAndSize(const cVector2f&in avPos, const cVector2f&in avSize)", +[](const cVector2f &p, const cVector2f &sz) {
		if (gpSomaBase->GetSplash())
			gpSomaBase->GetSplash()->SetLoadBar(p, sz);
	});
	SOMA_METHOD(e, T, "void StartLoadedGame()", +[](void *) {
		if (gbHoldAfterLoad)
			SomaSetGamePaused(false);
		gbHoldAfterLoad = false;
		RunLoadCallback();
	});
	SOMA_METHOD(e, T, "bool GetSaveFiles(array<tWString> &inout avNames, array<tString> &inout avDates, array<tWString> &inout avFiles, array<tWString> &inout avSuffixes)",
				+[](void *, CScriptArray &names, CScriptArray &dates, CScriptArray &files, CScriptArray &suffixes) {
					for (CScriptArray *a : {&names, &dates, &files, &suffixes})
						a->Resize(0);
					for (cSaveInfo &i : ListSaves())
					{
						names.InsertLast(&i.msName);
						dates.InsertLast(&i.msDate);
						files.InsertLast(&i.msFile);
						suffixes.InsertLast(&i.msSuffix);
					}
					return true;
				});
	SOMA_METHOD(e, T, "bool GetSaveFiles(array<tWString> &inout avNames, array<tString> &inout avDates, array<tWString> &inout avFiles)",
				+[](void *, CScriptArray &names, CScriptArray &dates, CScriptArray &files) {
					for (CScriptArray *a : {&names, &dates, &files})
						a->Resize(0);
					for (cSaveInfo &i : ListSaves())
					{
						names.InsertLast(&i.msName);
						dates.InsertLast(&i.msDate);
						files.InsertLast(&i.msFile);
					}
					return true;
				});
	SOMA_METHOD(e, T, "void SetSaveSuffix(const tWString&in asSaveSuffix)", +[](void *, W s) { gsSaveSuffix = s; });
	SOMA_METHOD(e, T, "const tWString& GetSaveSuffix()", +[](void *) -> const tWString & { return gsSaveSuffix; });
	SOMA_METHOD(e, T, "tWString GetSaveName(const tWString&in asPrefix)", +[](void *, W s) { return GetSaveName(s); });
	SOMA_METHOD(e, T, "bool LoadLastSave(bool abDelayed=true)", +[](void *, bool) {
		for (const cSaveInfo &i : ListSaves())
			if (i.msSuffix == gsSaveSuffix)
				return Load(i.msFile);
		return false;
	});
	SOMA_FUNC(e, "bool cLux_HasConfigLoadError(tString&out asError)", +[](tString &) { return false; });
}
