// The C++ objects the Rust sound layer reaches: channels, resource managers, the physics world,
// sound entities and their events. Everything else lives in rust/src/sound/.

#include "sound/SoundHandler.h"
#include "sound/MusicHandler.h"
#include "sound/SoundEntityData.h"
#include "sound/SoundChannel.h"
#include "sound/SoundData.h"
#include "sound/LowLevelSound.h"
#include "scene/SoundEntity.h"
#include "scene/World.h"
#include "physics/PhysicsWorld.h"
#include "physics/PhysicsBody.h"
#include "resources/Resources.h"
#include "resources/SoundManager.h"
#include "resources/SoundEntityManager.h"
#include "resources/LowLevelResources.h"
#include "system/LowLevelSystem.h"
#include "system/Platform.h"
#include "system/String.h"

namespace hpl {

	namespace
	{
		class cSoundRayCallback : public iPhysicsRayCallback
		{
		public:
			void Reset(){ mbHasCollided = false; }
			bool HasCollided(){ return mbHasCollided; }
			bool BeforeIntersect(iPhysicsBody *pBody){ return pBody->GetBlocksSound(); }
			bool OnIntersect(iPhysicsBody *pBody,cPhysicsRayParams *apParams){ mbHasCollided = true; return false; }
		private:
			bool mbHasCollided;
		};

		iSoundChannel* Ch(void *ap){ return static_cast<iSoundChannel*>(ap); }
		cResources* Res(void *ap){ return static_cast<cResources*>(ap); }
		iLowLevelSound* Low(void *ap){ return static_cast<iLowLevelSound*>(ap); }
		iSoundEntityEvent* Ev(void *ap){ return static_cast<iSoundEntityEvent*>(ap); }
		cSoundEntity* Ent(void *ap){ return static_cast<cSoundEntity*>(ap); }
		tString Str(const unsigned char *p, size_t n){ return tString((const char*)p, n); }

		const OhplSoundChannelOps kChannelOps = {
			[](void *c){ Ch(c)->Play(); },
			[](void *c){ Ch(c)->Stop(); },
			[](void *c, bool x){ Ch(c)->SetPaused(x); },
			[](void *c){ return Ch(c)->GetPaused(); },
			[](void *c){ return Ch(c)->IsPlaying(); },
			[](void *c){ return Ch(c)->GetStopUsed(); },
			[](void *c){ return Ch(c)->GetSpeed(); },
			[](void *c, float x){ Ch(c)->SetSpeed(x); },
			[](void *c){ return Ch(c)->GetVolume(); },
			[](void *c, float x){ Ch(c)->SetVolume(x); },
			[](void *c){ return Ch(c)->GetLooping(); },
			[](void *c, bool x){ Ch(c)->SetLooping(x); },
			[](void *c, bool x){ Ch(c)->Set3D(x); },
			[](void *c, int x){ Ch(c)->SetPriority(x); },
			[](void *c, int x){ Ch(c)->SetPriorityModifier(x); },
			[](void *c, bool x){ Ch(c)->SetAffectedByEnv(x); },
			[](void *c){ return Ch(c)->GetPositionIsRelative(); },
			[](void *c, bool x){ Ch(c)->SetPositionIsRelative(x); },
			[](void *c, cVector3f *o){ *o = Ch(c)->GetPosition(); },
			[](void *c, const cVector3f *v){ Ch(c)->SetPosition(*v); },
			[](void *c, cVector3f *o){ *o = Ch(c)->GetRelPosition(); },
			[](void *c, const cVector3f *v){ Ch(c)->SetRelPosition(*v); },
			[](void *c){ return Ch(c)->GetMinDistance(); },
			[](void *c, float x){ Ch(c)->SetMinDistance(x); },
			[](void *c){ return Ch(c)->GetMaxDistance(); },
			[](void *c, float x){ Ch(c)->SetMaxDistance(x); },
			[](void *c){ return Ch(c)->GetBlockVolumeMul(); },
			[](void *c){ return Ch(c)->GetElapsedTime(); },
			[](void *c){ return Ch(c)->GetTotalTime(); },
			[](void *c, double x){ Ch(c)->SetElapsedTime(x); },
			[](void *c, bool x){ Ch(c)->SetBlockable(x); },
			[](void *c, float x){ Ch(c)->SetBlockVolumeMul(x); },
			[](void *c, int which, float f){
				if(which == 0) Ch(c)->SetFilterGainHF(f);
				else if(which == 1) Ch(c)->SetFilterGainLF(f);
				else Ch(c)->SetEnvSendGain(f);
			},
		};

		const OhplSndEventOps kEventOps = {
			[](void *e){ Ev(e)->Play(); },
			[](void *e, bool b){ Ev(e)->Stop(b); },
			[](void *e, float f){ Ev(e)->FadeIn(f); },
			[](void *e, float f){ Ev(e)->FadeOut(f); },
			[](void *e){ return Ev(e)->IsStopped(); },
			[](void *e, float f){ Ev(e)->Update(f); },
			[](void *e){ hplDelete(Ev(e)); },
		};

		const OhplSndHost kHost = {
			&kChannelOps,
			&kEventOps,
			[](void *apRes, const unsigned char *asName, size_t alLen, int alPriority, bool abStream, bool *apNotEnough) -> void*
			{
				*apNotEnough = false;
				tString sName = Str(asName, alLen);
				iSoundData *pData = Res(apRes)->GetSoundManager()->CreateSoundData(sName, abStream);
				if(pData == NULL)
				{
					Error("Could not load sound '%s'\n", sName.c_str());
					return NULL;
				}
				iSoundChannel *pSound = pData->CreateChannel(alPriority);
				if(pSound == NULL)
				{
					if(abStream) Res(apRes)->GetSoundManager()->Destroy(pData);
					*apNotEnough = true;
				}
				return pSound;
			},
			[](void *c){ hplDelete(Ch(c)); },
			[](void *apWorld, void *apLow, const cVector3f *apPos)
			{
				cWorld *pWorld = static_cast<cWorld*>(apWorld);
				if(pWorld == NULL || pWorld->GetPhysicsWorld() == NULL) return false;
				static cSoundRayCallback callback;
				callback.Reset();
				pWorld->GetPhysicsWorld()->CastRay(&callback, *apPos, Low(apLow)->GetListenerPosition(), false, false, false, true);
				return callback.HasCollided();
			},
			[](void *l, cVector3f *o){ *o = Low(l)->GetListenerPosition(); },
			[](void *l, cMatrixf *o){ *o = Low(l)->GetListenerMatrix(); },
			[](void *l, float *o)
			{
				cVector3f f = Low(l)->GetListenerForward(), u = Low(l)->GetListenerUp();
				o[0] = f.x; o[1] = f.y; o[2] = f.z; o[3] = u.x; o[4] = u.y; o[5] = u.z;
			},
			[]() -> uint64_t { return cPlatform::GetApplicationTime(); },
			[](int alLevel, const unsigned char *p, size_t n)
			{
				tString s = Str(p, n);
				if(alLevel == 0) Error("%s", s.c_str()); else Warning("%s", s.c_str());
			},
			[](void *apRes, const unsigned char *p, size_t n){ Res(apRes)->GetSoundManager()->CreateSoundData(Str(p, n), false); },
			[](void *e, cVector3f *o){ *o = Ent(e)->GetWorldPosition(); },
			[](void *e, size_t *n) -> const unsigned char* { const tString &s = Ent(e)->GetName(); *n = s.size(); return (const unsigned char*)s.data(); },
			[](void *w){ return static_cast<cWorld*>(w)->IsSoundEmitter(); },
			[](void *d){ static_cast<cSoundEntityData*>(d)->DecUserCount(); },
			[](void *e)
			{
				tSoundEntityGlobalCallbackList &lst = cSoundEntity::GlobalCallbacks();
				for(tSoundEntityGlobalCallbackListIt it = lst.begin(); it != lst.end(); ++it) (*it)->OnStart(Ent(e));
			},
			[](void *apRes, const unsigned char *p, size_t n, bool abLoop) -> void*
			{
				tString sName = Str(p, n);
				iSoundData *pData = Res(apRes)->GetSoundManager()->CreateSoundData(sName, true, abLoop);
				if(pData == NULL)
				{
					Error("Couldn't load music '%s'\n", sName.c_str());
					return NULL;
				}
				iSoundChannel *pStream = pData->CreateChannel(256);
				if(pStream == NULL)
				{
					Res(apRes)->GetSoundManager()->Destroy(pData);
					Error("Couldn't stream music '%s'!\n", sName.c_str());
					return NULL;
				}
				return pStream;
			},
			[](void *apLock, const unsigned char *p, size_t n, float afVolume, bool abLoop)
			{
				cMusicLock *pLock = static_cast<cMusicLock*>(apLock);
				pLock->msFileName = Str(p, n);
				pLock->mfVolume = afVolume;
				pLock->mbLoop = abLoop;
			},
		};
	}

	const OhplSndHost* OhplSndHostTable(){ return &kHost; }

	void iSoundChannel::DestroyData()
	{
		if(mpSoundManger) mpSoundManger->Destroy(mpData);
	}

	//-----------------------------------------------------------------------

	cSoundEntry* cSoundHandler::PlaySoundEntityGui(	const tString& asName,bool abLoop,float afVolume,
													eSoundEntryType aEntryType,
													const cVector3f& avPos, bool *apNotEnoughChannels)
	{
		cSoundEntityData *pData = mpResources->GetSoundEntityManager()->CreateSoundEntity(asName);
		if(pData == NULL) return NULL;

		tString sSoundName = pData->GetRandomSoundName(eSoundEntityType_Main,true);
		if(sSoundName=="") return NULL;

		if(pData->GetStream())
			return PlayGuiStream(sSoundName,abLoop,afVolume * pData->GetVolume(),avPos,aEntryType,apNotEnoughChannels);
		return PlayGui(sSoundName,abLoop,pData->GetVolume() * pData->GetVolume(),avPos,aEntryType,apNotEnoughChannels);
	}

	//-----------------------------------------------------------------------

	bool cSoundEntityData::CreateFromFile(const tWString &asFile)
	{
		SetFullPath(asFile);

		FILE *pFile = cPlatform::OpenFile(asFile, _W("rb"));
		OhplXmlDocPtr doc;
		bool bLoaded = false;
		if(pFile)
		{
			bLoaded = OhplXmlLoadFile(doc.p, pFile);
			fclose(pFile);
		}

		int lResult = ohpl_sndd_load(mpRs, doc.p, bLoaded);
		if(lResult == 1 || lResult == 2)
		{
			int lRow=0, lCol=0;
			tString sDesc;
			if(lResult == 1) ohpl_xml_doc_error_info(doc.p, &lRow, &lCol, OhplXmlAssign, &sDesc);
			else sDesc = "No root element";
			Log("Failed parsing of XML document %s in line %d, column %d: %s\n", cString::To8Char(asFile).c_str(), lRow, lCol, sDesc.c_str());
			Error("Couldn't load sound entity data '%s'!\n",cString::To8Char(asFile).c_str());
			return false;
		}
		if(lResult == 3)
		{
			Error("Couldn't find SOUNDS element in '%s'!\n",asFile.c_str());
			return false;
		}
		if(lResult == 4)
		{
			Error("Couldn't find PROPERTIES element in '%s'!\n",asFile.c_str());
			return false;
		}
		return true;
	}
}
