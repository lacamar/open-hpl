#include "SomaAgent.h"
#include "RustSomaAgentS4.h"
#include "RustSomaAgentS4b.h"
#include "SomaAgentDump.h"
#include "SomaBase.h"
#include "SomaLux.h"
#include "SomaLuxEntity.h"
#include "SomaLuxPlayer.h"
#include "SomaLuxScriptable.h"
#include "SomaLuxVoice.h"
#include "SomaScriptRuntime.h"

#include "impl/RustEasArray.h"
#include "impl/XmlDocumentTiny.h"

#include <algorithm>
#include <angelscript.h>
#include <cmath>
#include <cstring>
#include <map>
#include <memory>

namespace
{
	typedef cSomaLuxEntity E;

	iCharacterBody *PlayerBody() { return cSomaLuxPlayer::Get() ? cSomaLuxPlayer::Get()->GetCharacterBody() : NULL; }

	E *Ent(OhplS4bIo *io) { return (E *)io->p[0]; }
	iCharacterBody *Body(OhplS4bIo *io) { return (iCharacterBody *)io->p[1]; }
	void Put(float *o, const cVector3f &v)
	{
		o[0] = v.x;
		o[1] = v.y;
		o[2] = v.z;
	}
	cVector3f Get(const float *p) { return cVector3f(p[0], p[1], p[2]); }
	tString Str(OhplS4bIo *io, int k) { return io->s[k] ? tString((const char *)io->s[k], io->n[k]) : tString(); }

	tString gOut;
	void Out(OhplS4bIo *io, const tString &s)
	{
		gOut = s;
		io->s[0] = (const unsigned char *)gOut.data();
		io->n[0] = gOut.size();
	}

	struct cNodeData
	{
		std::unique_ptr<cAINodeContainer> mpContainer;
		std::unique_ptr<cAStarHandler> mpAStar;
	};
	std::map<std::pair<cWorld *, tString>, cNodeData> gmapContainers;

	cNodeData *GetContainer(cSomaLuxMap *apMap, const tString &asName, const cVector3f &avSize, float afMaxHeight, float afMaxEdgeDist = 5, bool abAtCenter = false)
	{
		cWorld *pWorld = apMap->GetWorld();
		auto key = std::make_pair(pWorld, asName + "|" + cString::ToString(afMaxHeight) + "|" + cString::ToString(afMaxEdgeDist) + "|" + cString::ToString(abAtCenter));
		auto it = gmapContainers.find(key);
		if (it != gmapContainers.end())
			return &it->second;
		for (auto i = gmapContainers.begin(); i != gmapContainers.end();)
			i = i->first.first != pWorld ? gmapContainers.erase(i) : std::next(i);

		tWString sFile = cString::SetFileExtW(pWorld->GetFilePath(), _W("")) + _W("_") + cString::To16Char(asName) + _W(".nodes");
		std::map<tString, int> mapIds;
		cXmlDocumentTiny doc("");
		FILE *pFile = cPlatform::OpenFile(sFile, _W("rb"));
		const bool bOpened = pFile != NULL;
		if (pFile)
			fclose(pFile);
		bool bHasFile = bOpened && doc.CreateFromFile(sFile);
		if (bHasFile)
			for (cXmlNodeListIterator it = doc.GetChildIterator(); it.HasNext();)
			{
				cXmlElement *p = it.Next()->ToElement();
				if (p->GetValue() == "Node")
					mapIds[cString::ToString(p->GetAttribute("Name"), "")] = cString::ToInt(p->GetAttribute("ID"), -1);
			}

		cNodeData &data = gmapContainers[key];
		data.mpContainer.reset(new cAINodeContainer(asName, "PathNode", pWorld, avSize));
		cAINodeContainer *pCont = data.mpContainer.get();
		pCont->SetMinEdges(2);
		pCont->SetMaxEdges(5);
		pCont->SetMaxEdgeDistance(afMaxEdgeDist);
		pCont->SetMaxHeight(afMaxHeight);
		pCont->SetNodeIsAtCenter(abAtCenter);
		int lNextId = 1 << 30;
		for (cSomaLuxEntity *pEnt : apMap->GetEntities())
			if (pEnt->msClassName == "PathNode" || pEnt->msClassName == "PathNodePatrol" || pEnt->msClassName == "PathNodeInterestPoint")
			{
				auto idIt = mapIds.find(pEnt->msName);
				pCont->AddNode(pEnt->msName, idIt != mapIds.end() ? idIt->second : lNextId++, pEnt->GetPosition(), NULL);
			}
		// official cLuxMap::AddPathNodeGeneratorArea output, cached next to the map
		if (FILE *pGen = cPlatform::OpenFile(cString::SetFileExtW(pWorld->GetFilePath(), _W("gen_node_cache")), _W("rb")))
		{
			fclose(pGen);
			cXmlDocumentTiny genDoc("");
			bool bGen = genDoc.CreateFromFile(cString::SetFileExtW(pWorld->GetFilePath(), _W("gen_node_cache")));
			if (bGen)
				for (cXmlNodeListIterator it = genDoc.GetChildIterator(); it.HasNext();)
				{
					cXmlElement *p = it.Next()->ToElement();
					if (p->GetValue() != "Node")
						continue;
					tString sName = cString::ToString(p->GetAttribute("Name"), "");
					auto idIt = mapIds.find(sName);
					if (idIt != mapIds.end())
						pCont->AddNode(sName, idIt->second, cString::ToVector3f(p->GetAttribute("Pos"), 0), NULL);
				}
		}
		if (bHasFile)
			pCont->LoadFromFile(sFile);
		else
			pCont->Compile();
		data.mpAStar.reset(new cAStarHandler(pCont));
		data.mpAStar->SetMaxIterations(2000);
		Log("SOMA agent: node container '%s' %d nodes%s\n", asName.c_str(), pCont->GetNodeNum(), bHasFile ? "" : " (compiled)");
		return &data;
	}

	// cLuxOutlineEffect::RenderEdgeGlow: additive, after translucents
	struct cEdgeGlowRenderer : iRendererCallback
	{
		struct cGlow { cSomaID mID; cColor mColor; float mfY; };
		cViewport *mpViewport = NULL;
		iGpuProgram *mpProgram = NULL;
		iGpuProgram *mpOutline = NULL;
		std::vector<cGlow> mvGlow;

		void Register()
		{
			cViewport *pViewport = gpSomaBase->GetCurrentViewport();
			if (pViewport == NULL || pViewport == mpViewport)
				return;
			pViewport->AddRendererCallback(this);
			mpViewport = pViewport;
		}

		void OnPostSolidDraw(cRendererCallbackFunctions *) override {}
		void OnPostTranslucentDraw(cRendererCallbackFunctions *f) override
		{
			cFrustum *pFrustum = f->GetFrustum();
			if (mpViewport == NULL || mpViewport->GetCamera() == NULL || pFrustum != mpViewport->GetCamera()->GetFrustum())
				return;
			if (static bool bTried = false; bTried == false)
			{
				bTried = true;
				cParserVarContainer vars;
				vars.Add("UseUv");
				vars.Add("UseNormals");
				vars.Add("UseVertexPosition");
				mpProgram = gpSomaBase->mpEngine->GetGraphics()->CreateGpuProgramFromShaders("SomaEdgeGlow", "deferred_base_vtx.glsl", "game_edge_glow.glsl", &vars);
				if (mpProgram)
				{
					mpProgram->GetVariableAsId("aColor", 0);
					mpProgram->GetVariableAsId("afAlpha", 1);
					mpProgram->GetVariableAsId("afLightLevel", 2);
					mpProgram->GetVariableAsId("afLightLimit", 3);
					mpProgram->GetVariableAsId("afEdgeThickness", 4);
				}
			}
			// cLuxEffectHandler "OutlineProgram"
			if (static bool bTriedOutline = false; bTriedOutline == false && mvGlow.empty() == false)
			{
				bTriedOutline = true;
				cParserVarContainer vars;
				vars.Add("UseUv");
				vars.Add("UseNormals");
				mpOutline = gpSomaBase->mpEngine->GetGraphics()->CreateGpuProgramFromShaders("SomaOutline", "deferred_base_vtx.glsl", "game_object_outline.glsl", &vars);
				if (mpOutline)
				{
					mpOutline->GetVariableAsId("avOutlineColor", 0);
					mpOutline->GetVariableAsId("avScreenSize", 1);
					mpOutline->GetVariableAsId("afEffectY", 2);
					mpOutline->GetVariableAsId("avObjectScreenMinAndSize", 3);
				}
			}
			iLowLevelGraphics *pLowLevel = gpSomaBase->mpEngine->GetGraphics()->GetLowLevel();
			bool bStarted = false;
			auto Begin = [&](iGpuProgram *apProgram) {
				if (bStarted == false)
				{
					bStarted = true;
					f->SetDepthTestFunc(eDepthTestFunc_LessOrEqual);
					f->SetDepthTest(true);
					f->SetDepthWrite(false);
					f->SetBlendMode(eMaterialBlendMode_Add);
					f->SetAlphaMode(eMaterialAlphaMode_Solid);
					f->SetChannelMode(eMaterialChannelMode_RGBA);
					f->SetNormalFrustumProjection();
					pLowLevel->SetPolygonOffsetActive(true);
					pLowLevel->SetPolygonOffset(-5, -2);
					f->SetTextureRange(NULL, 0);
					f->SetCullActive(true);
					f->SetCullMode(eCullMode_CounterClockwise);
				}
				f->SetProgram(apProgram);
			};
			auto DrawSub = [&](cSubMeshEntity *s) {
				cMaterial *pMat = s->GetMaterial();
				iTexture *pDiffuse = pMat ? pMat->GetTexture(eMaterialTexture_Diffuse) : NULL;
				if (pDiffuse == NULL || s->IsVisible() == false || pFrustum->CollideBoundingVolume(s->GetBoundingVolume()) == eCollision_Outside)
					return false;
				f->SetTexture(0, pDiffuse);
				f->SetMatrix(s->GetModelMatrixPtr());
				f->SetVertexBuffer(s->GetVertexBuffer());
				return true;
			};
			struct cCtx
			{
				cEdgeGlowRenderer *mpSelf;
				cFrustum *mpFrustum;
				decltype(Begin) *mpBegin;
				decltype(DrawSub) *mpDraw;
				cRendererCallbackFunctions *mpFunc;
			} ctx{this, pFrustum, &Begin, &DrawSub, f};
			if (mpProgram)
				ohpl_s4b_glow_each(&ctx, +[](void *c, uintptr_t aEnt, const float *aColor, float afAlpha, float afThickness, float afLimit) {
					cCtx &x = *(cCtx *)c;
					E *p = (E *)aEnt;
					if (p->mpMesh == NULL || p->mbActive == false || p->mpMesh->IsVisible() == false ||
						x.mpFrustum->CollideBoundingVolume(p->mpMesh->GetBoundingVolume()) == eCollision_Outside)
						return;
					// ponytail: one light sample per entity, the original samples each submesh BV
					float fLevel = SomaLightLevelAtPos(p->mpMesh->GetBoundingVolume()->GetWorldCenter(), NULL, 0);
					if (fLevel >= afLimit)
						return;
					(*x.mpBegin)(x.mpSelf->mpProgram);
					x.mpSelf->mpProgram->SetVec4f(0, aColor[0], aColor[1], aColor[2], aColor[3]);
					x.mpSelf->mpProgram->SetFloat(1, afAlpha);
					x.mpSelf->mpProgram->SetFloat(2, fLevel);
					x.mpSelf->mpProgram->SetFloat(3, afLimit);
					x.mpSelf->mpProgram->SetFloat(4, afThickness);
					for (int i = 0; i < p->mpMesh->GetSubMeshEntityNum(); ++i)
						if ((*x.mpDraw)(p->mpMesh->GetSubMeshEntity(i)))
							x.mpFunc->DrawCurrent();
				});
			cSomaLuxMap *pMap = cSomaLuxMap::GetCurrent();
			const cVector2l &vScreen = pLowLevel->GetScreenSizeInt();
			float fTanHalfFov = tan(pFrustum->GetFOV() * 0.5f);
			for (cGlow &glow : mvGlow)
			{
				E *p = pMap && mpOutline ? pMap->GetEntity(glow.mID) : NULL;
				if (p == NULL || p->mpMesh == NULL || p->mbActive == false || p->mpMesh->IsVisible() == false)
					continue;
				Begin(mpOutline);
				mpOutline->SetVec4f(0, glow.mColor.r, glow.mColor.g, glow.mColor.b, glow.mColor.a);
				mpOutline->SetVec2f(1, (float)vScreen.x, (float)vScreen.y);
				mpOutline->SetFloat(2, glow.mfY);
				for (int i = 0; i < p->mpMesh->GetSubMeshEntityNum(); ++i)
				{
					cSubMeshEntity *s = p->mpMesh->GetSubMeshEntity(i);
					if (DrawSub(s) == false)
						continue;
					cRect2l r;
					cMath::GetClipRectFromBV(r, *s->GetBoundingVolume(), pFrustum, vScreen, fTanHalfFov);
					mpOutline->SetVec2f(3, (float)(vScreen.y - (r.y + r.h)), (float)r.h);
					f->DrawCurrent();
				}
			}
			mvGlow.clear();
			if (bStarted == false)
				return;
			pLowLevel->SetPolygonOffsetActive(false);
			f->SetProgram(NULL);
			f->SetTexture(0, NULL);
			f->SetDepthWrite(true);
			f->SetBlendMode(eMaterialBlendMode_None);
		}
	} gEdgeGlowRenderer;

	// cLuxUserComponent: scripts hold the cSomaLuxScriptable base
	struct cAgentUserShell : cSomaLuxScriptable
	{
		// ponytail: component timers are not saved
		cAgentUserShell() { mbSaved = false; }
		void Tick(float afTimeStep)
		{
			OnUpdate(afTimeStep);
			OnVariableUpdate(afTimeStep);
		}
	};

	std::vector<iPhysicsBody *> gAvoid;
	std::vector<cAINode *> gPath;

	cNodeData *Nodes(OhplS4bIo *io) { return (cNodeData *)io->p[0]; }

	struct cFloorRay : iPhysicsRayCallback
	{
		float mfDist = 1e9f;
		bool OnIntersect(iPhysicsBody *b, cPhysicsRayParams *p) override
		{
			if (b->IsCharacter() == false && b->GetCollideCharacter() && p->mfDist < mfDist)
				mfDist = p->mfDist;
			return true;
		}
	};

	struct cGroundRay : iPhysicsRayCallback
	{
		bool mbDynamic;
		float mfDist;
		bool OnIntersect(iPhysicsBody *b, cPhysicsRayParams *p) override
		{
			if (b->IsCharacter() == false && (mbDynamic || b->GetMass() == 0) && p->mfDist < mfDist)
				mfDist = p->mfDist;
			return true;
		}
	};

	void SetArgs(asIScriptContext *c, OhplS4bIo *io, E *apEnt)
	{
		for (int k = 0; k < 3; ++k)
		{
			uint64_t v = io->a[k];
			switch (io->i[2 + k])
			{
			case 1: { float f; uint32_t b = (uint32_t)v; memcpy(&f, &b, 4); c->SetArgFloat(k, f); break; }
			case 2: c->SetArgQWord(k, v); break;
			case 3: c->SetArgDWord(k, (int)(uint32_t)v); break;
			case 4: c->SetArgByte(k, (asBYTE)v); break;
			case 5: c->SetArgObject(k, &apEnt->msName); break;
			case 6: c->SetArgAddress(k, (void *)(uintptr_t)v); break;
			}
		}
	}

	int Op(unsigned alOp, OhplS4bIo *io)
	{
		E *e = Ent(io);
		iCharacterBody *b = Body(io);
		cSomaLuxMap *pMap = cSomaLuxMap::GetCurrent();
		switch (alOp)
		{
		case 1: Put(io->f, e->GetPosition()); break;
		case 2: io->f[0] = e->mvScale.y; break;
		case 3:
			io->i[4] = pMap && pMap->GetScript();
			if (e)
			{
				io->i[0] = e->mpMap != NULL;
				io->i[1] = e->mpMesh != NULL;
				io->i[2] = e->mbActive;
				io->i[3] = e->mbGlobalSpaceAnim;
			}
			break;
		case 4:
			io->s[0] = (const unsigned char *)e->msName.data();
			io->n[0] = e->msName.size();
			break;
		case 5: e->SetActive(io->i[0] != 0); break;
		case 6:
		{
			cMatrixf mtx;
			memcpy(mtx.v, (const float *)io->p[1], sizeof(float) * 16);
			e->mpMesh->SetMatrix(mtx);
			break;
		}
		case 7: io->i[0] = e->mlCurrentAnim; break;
		case 8: io->i[0] = e->PlayAnimation(Str(io, 0), io->f[0], io->i[0] != 0, Str(io, 1)); break;
		case 9:
			if (e->mpMesh && e->mlCurrentAnim >= 0)
				if (cAnimationState *pAnim = e->mpMesh->GetAnimationState(e->mlCurrentAnim))
					pAnim->SetSpeed(io->f[0]);
			break;
		case 10:
		{
			int lIdx = e->PlayAnimation(Str(io, 0), 0.3f, true, "");
			if (lIdx >= 0)
				e->mpMesh->GetAnimationState(lIdx)->SetRelativeTimePosition(cMath::RandRectf(0, 1));
			break;
		}
		case 11:
		{
			cAnimationState *pAnim = e->mpMesh ? e->mpMesh->GetAnimationStateFromName(Str(io, 0)) : NULL;
			io->i[0] = pAnim != NULL;
			if (pAnim)
				io->f[0] = pAnim->GetLength();
			break;
		}
		case 12: io->i[0] = e->mpMesh && e->mpMesh->GetAnimationStateFromName(Str(io, 0)); break;
		case 13: Out(io, e->mpMesh && e->mlCurrentAnim >= 0 ? e->mpMesh->GetAnimationState(e->mlCurrentAnim)->GetName() : tString("")); break;
		case 14:
		{
			tString sDecl = Str(io, 0);
			OhplS4bIo a = *io;
			auto ArgsCopy = [&](asIScriptContext *c) { SetArgs(c, &a, e); };
			switch (io->i[0])
			{
			case 0: io->i[0] = e->Call(sDecl, ArgsCopy); break;
			case 1: io->i[0] = e->CallBool(sDecl, ArgsCopy, io->i[1] != 0); break;
			case 2:
				if (pMap && pMap->GetScript())
					cSomaScriptRuntime::Get()->Call(pMap->GetScript(), sDecl, ArgsCopy);
				break;
			default: SomaMapScriptCall(sDecl, ArgsCopy); break;
			}
			break;
		}
		case 15: io->i[0] = e->mpMap ? (int)e->mpMap->GetEntities().size() : 0; break;
		case 16:
		{
			const std::vector<cSomaLuxEntity *> &v = e->mpMap->GetEntities();
			cSomaLuxEntity *pDoor = v[io->i[0]];
			float fDist;
			io->i[1] = 0;
			if (pDoor->mbIsDoor && pDoor->mbIsClosedDoor && pDoor->mbActive && SomaRayHitsEntity(pDoor, Get(io->f), Get(io->f + 3), io->f[6], fDist))
			{
				Put(io->f, pDoor->GetPosition());
				io->i[1] = 1;
			}
			break;
		}
		case 17:
		{
			const std::vector<cSomaLuxEntity *> &v = e->mpMap->GetEntities();
			io->i[0] = io->i[0] >= 0 && (size_t)io->i[0] < v.size();
			if (io->i[0])
				memcpy((void *)io->p[1], &v[io->i[0]]->mID, sizeof(cSomaID));
			break;
		}
		case 18:
		{
			cSomaLuxMap *pFrom = io->p[1] ? (cSomaLuxMap *)io->p[1] : pMap;
			io->p[0] = (uintptr_t)(pFrom ? pFrom->GetEntity(Str(io, 0)) : NULL);
			break;
		}
		case 19:
		{
			io->i[0] = -1;
			for (size_t i = 0; i < e->mvSockets.size(); ++i)
				if (e->mvSockets[i].msName == Str(io, 0))
				{
					io->i[0] = (int)i;
					break;
				}
			break;
		}
		case 20: io->i[0] = (int)e->mvSockets.size(); break;
		case 21:
		{
			cMatrixf mtx = e->mpMesh ? e->mpMesh->GetWorldMatrix() : e->GetMatrix();
			if (io->i[0] >= 0 && io->i[0] < (int)e->mvSockets.size())
				e->GetSocketMatrix(e->mvSockets[io->i[0]].msName, mtx);
			memcpy(io->f, mtx.v, sizeof(float) * 16);
			break;
		}
		case 22: io->i[0] = e->mpMap == (cSomaLuxMap *)io->p[1]; break;
		case 23:
		{
			cParticleSystem *pPS = NULL;
			if (Str(io, 1) != "" && e->mpMap)
			{
				pPS = e->mpMap->GetWorld()->CreateParticleSystem(Str(io, 0), Str(io, 1), 1, io->i[0] != 0);
				if (pPS && e->mpMesh && io->i[1])
					e->mpMesh->AddChild(pPS);
				else if (pPS)
					pPS->SetPosition(e->mpMesh ? e->mpMesh->GetWorldPosition() : e->GetPosition());
			}
			io->p[0] = (uintptr_t)pPS;
			break;
		}
		case 24:
		{
			bool &bFlag = io->i[0] == 0 ? e->mbIsDoor : e->mbIsClosedDoor;
			if (io->i[1])
				bFlag = io->i[2] != 0;
			io->i[3] = bFlag;
			break;
		}
		case 25: Put(io->f, e->mvScale); break;
		case 26:
		{
			cMatrixf &m = e->m_mtxOnLoad;
			Put(io->f, m.GetTranslation());
			io->f[3] = m.m[0][2];
			io->f[4] = m.m[2][2];
			break;
		}
		case 27: io->p[0] = (uintptr_t)(io->i[0] ? &e->mInstanceVars : &e->mVars); break;

		case 40:
			Put(io->f, io->i[0] == 0 ? b->GetPosition() : io->i[0] == 1 ? b->GetFeetPosition() : io->i[0] == 2 ? b->GetSize() : io->i[0] == 3 ? b->GetForward() : io->i[0] == 4 ? b->GetRight() : b->GetLastPosition());
			break;
		case 41:
			io->f[0] = io->i[0] == 0 ? b->GetYaw() : io->i[0] == 1 ? b->GetPitch() : io->i[0] == 2 ? b->GetEntityPitchAmount() : io->i[0] == 3 ? b->GetMoveSpeed(eCharDir_Forward) : b->GetMaxStepSize();
			break;
		case 42:
			io->i[0] = b->IsOnGround();
			io->i[1] = b->GetTestCollision();
			io->i[2] = b->GravityIsActive();
			break;
		case 43: b->SetYaw(io->f[0]); break;
		case 44:
			if (io->i[0] == 0)
				b->SetPosition(Get(io->f));
			else
				b->SetFeetPosition(Get(io->f));
			break;
		case 45:
			switch (io->i[0])
			{
			case 0: b->AddPitch(io->f[0]); break;
			case 1: b->SetMaxPositiveMoveSpeed(eCharDir_Forward, io->f[0]); break;
			case 2: b->SetMoveAcc(eCharDir_Forward, io->f[0]); break;
			default: b->SetMoveDeacc(eCharDir_Forward, io->f[0]); break;
			}
			break;
		case 46:
			if (io->i[0] == 0)
				b->SetGravityActive(io->i[1] != 0);
			else if (io->i[0] == 1)
				b->SetTestCollision(io->i[1] != 0);
			else
				b->SetActive(io->i[1] != 0);
			break;
		case 47: Put(io->f, b->GetVelocity(io->f[0])); break;
		case 48: b->Move(eCharDir_Forward, 1); break;
		case 49:
		{
			iCharacterBody *pBody = e->mpMap->GetWorld()->GetPhysicsWorld()->CreateCharacterBody(e->msName, Get(io->f));
			pBody->SetUserData(e);
			pBody->SetMass(80);
			pBody->SetCustomGravity(cVector3f(0, -12, 0));
			pBody->SetCustomGravityActive(true);
			pBody->SetGravityActive(true);
			pBody->SetMaxPositiveMoveSpeed(eCharDir_Forward, 1);
			pBody->SetMaxNegativeMoveSpeed(eCharDir_Forward, -1);
			io->p[1] = (uintptr_t)pBody;
			break;
		}
		case 50: e->mpMap->GetWorld()->GetPhysicsWorld()->DestroyCharacterBody(b); break;

		case 60:
		{
			iCharacterBody *pP = PlayerBody();
			io->i[0] = pP != NULL;
			io->i[1] = cSomaLuxPlayer::Get() && cSomaLuxPlayer::Get()->IsDead();
			io->f[0] = cSomaLuxPlayer::Get() ? cSomaLuxPlayer::Get()->GetVisibilityRangeMul() : 1.0f;
			io->f[1] = cSomaLuxPlayer::Get() ? cSomaLuxPlayer::Get()->GetVisibilityMaxRange() : -1.0f;
			Put(io->f + 2, pP ? pP->GetPosition() : cVector3f(0));
			Put(io->f + 5, pP ? pP->GetFeetPosition() : cVector3f(0));
			io->f[8] = pP ? pP->GetSize().y : 0.0f;
			io->f[9] = pP ? pP->GetSize().x : 0.0f;
			io->f[10] = pP ? pP->GetSize().z : 0.0f;
			io->f[11] = pP ? pP->GetMass() : 0.0f;
			break;
		}
		case 61:
		{
			iCharacterBody *pP = PlayerBody();
			Put(io->f, pP ? pP->GetVelocity(gpSomaBase->mpEngine->GetStepSize()) : cVector3f(0));
			break;
		}
		case 62:
			io->i[0] = cSomaLuxPlayer::Get() && cSomaLuxPlayer::Get()->GiveDamage(io->f[0], io->i[0], io->f[1], e->mID, io->i[1]);
			break;
		case 63:
			if (PlayerBody())
				PlayerBody()->AddForceVelocity(Get(io->f));
			break;

		case 70: io->i[0] = SomaLineOfSight(Get(io->f), Get(io->f + 3), e); break;
		case 71: io->i[0] = SomaEntityInPlayerLOS(e, io->i[0] != 0); break;
		case 72: io->i[0] = SomaEntityIsOnScreen(e, io->i[0] != 0); break;
		case 73:
		{
			cFloorRay ray;
			e->mpMap->GetWorld()->GetPhysicsWorld()->CastRay(&ray, Get(io->f), Get(io->f + 3), true, false, false);
			io->f[0] = ray.mfDist;
			break;
		}
		case 74:
		{
			float fMax = io->f[0];
			if (b == NULL || e->mpMap == NULL)
			{
				io->f[0] = fMax;
				break;
			}
			bool bShortest = io->i[2] != 0;
			cVector3f vFeet = b->GetFeetPosition();
			float fResult = bShortest ? fMax : 0;
			int lRays = std::max(io->i[1], 1);
			for (int i = 0; i < lRays; ++i)
			{
				float fAngle = k2Pif * i / lRays;
				cVector3f vStart = vFeet + (i == 0 ? cVector3f(0) : cVector3f(std::cos(fAngle), 0, std::sin(fAngle)) * io->f[1]);
				cGroundRay ray;
				ray.mbDynamic = io->i[0] != 0;
				ray.mfDist = fMax;
				e->mpMap->GetWorld()->GetPhysicsWorld()->CastRay(&ray, vStart, vStart - cVector3f(0, fMax, 0), true, false, false);
				fResult = bShortest ? std::min(fResult, ray.mfDist) : fResult + ray.mfDist / lRays;
			}
			io->f[0] = fResult;
			break;
		}
		case 75:
		{
			cVector3f vNormal;
			float fDist = 0;
			io->i[0] = SomaRaycast(e, Get(io->f), Get(io->f + 3), fDist, vNormal);
			io->f[6] = fDist;
			Put(io->f + 7, vNormal);
			break;
		}
		case 76:
		{
			cVector3f vPos = Get(io->f);
			float d = io->f[3];
			cBoundingVolume bv;
			bv.SetLocalMinMax(vPos - d, vPos + d);
			gAvoid.clear();
			e->mpMap->GetWorld()->GetPhysicsWorld()->GetBodiesInBV(&bv, &gAvoid);
			io->i[0] = (int)gAvoid.size();
			break;
		}
		case 77:
		{
			iPhysicsBody *pBody = gAvoid[io->i[0]];
			iCharacterBody *pPlayer = PlayerBody();
			iPhysicsBody *pPlayerBody = pPlayer ? pPlayer->GetCurrentBody() : NULL;
			io->f[4] = pBody->GetMass();
			io->i[1] = pBody->IsActive();
			io->i[2] = pBody->GetCollide();
			io->i[3] = pBody->IsCharacter();
			io->i[4] = pBody == pPlayerBody;
			io->i[5] = pBody == b->GetCurrentBody();
			Put(io->f, pBody->GetBoundingVolume()->GetWorldCenter());
			io->f[3] = pBody->GetBoundingVolume()->GetRadius();
			break;
		}
		case 78:
			if (pMap)
				if (cSoundEntity *pSound = pMap->GetWorld()->CreateSoundEntity(e->msName + "_Bark", Str(io, 0), true))
					pSound->SetPosition(e->GetPosition());
			break;

		case 90:
			io->p[0] = 0;
			if (pMap)
				io->p[0] = (uintptr_t)GetContainer(pMap, Str(io, 0) != "" ? Str(io, 0) : cString::SetFileExt(cString::GetFileName(e->msFileName), ""), cVector3f(0.6f, 1.5f, 0.6f), io->f[0], io->f[1], io->i[0] != 0);
			break;
		case 91:
		{
			tAINodeList lst;
			gPath.clear();
			if (Nodes(io)->mpAStar->GetPath(Get(io->f), Get(io->f + 3), &lst) == false)
				return -1;
			for (cAINode *n : lst)
				gPath.push_back(n);
			return (int)gPath.size();
		}
		case 92:
			io->p[0] = (uintptr_t)gPath[io->i[0]];
			Put(io->f, gPath[io->i[0]]->GetPosition());
			break;
		case 93:
		{
			cAINode *pNode = Nodes(io)->mpContainer->GetNodeFromName(Str(io, 0));
			io->p[0] = (uintptr_t)pNode;
			if (pNode)
				Put(io->f, pNode->GetPosition());
			break;
		}
		case 94: io->i[0] = Nodes(io)->mpContainer->GetNodeNum(); break;
		case 95:
		{
			cAINode *pNode = Nodes(io)->mpContainer->GetNode(io->i[0]);
			io->p[0] = (uintptr_t)pNode;
			Put(io->f, pNode->GetPosition());
			break;
		}
		case 96: io->i[0] = Nodes(io)->mpContainer->FreePath(Get(io->f), Get(io->f + 3), 1, eAIFreePathFlag_SkipDynamic); break;
		case 97: io->p[0] = (uintptr_t)Nodes(io)->mpContainer.get(); break;
		case 98:
		{
			// ponytail: NOCOUNT type, never released; a ring of 64 covers per-frame locals
			static std::vector<cAINodeIterator> vRing;
			static size_t lNext = 0;
			cAINodeContainer *c = (cAINodeContainer *)io->p[0];
			if (vRing.size() < 64)
			{
				vRing.reserve(64);
				vRing.emplace_back(c, Get(io->f), io->f[3]);
				io->p[0] = (uintptr_t)&vRing.back();
			}
			else
			{
				cAINodeIterator *p = &vRing[lNext++ & 63];
				*p = cAINodeIterator(c, Get(io->f), io->f[3]);
				io->p[0] = (uintptr_t)p;
			}
			break;
		}
		case 99: io->i[0] = ((cAINodeIterator *)io->p[0])->HasNext(); break;
		case 100: io->p[0] = (uintptr_t)((cAINodeIterator *)io->p[0])->Next(); break;
		case 101: gmapContainers.clear(); break;
		case 102: Put(io->f, ((cAINode *)io->p[0])->GetPosition()); break;

		case 110:
		{
			cForceField *pField = e->mpMap->GetWorld()->CreateForceField(e->msName + "_ForceField", false, false);
			pField->SetForce(3);
			pField->SetRadius(1.1f);
			pField->SetFreq(2.4f);
			pField->FadeOut(0.001f);
			io->p[0] = (uintptr_t)pField;
			break;
		}
		case 111: e->mpMap->GetWorld()->DestroyForceField((cForceField *)io->p[1]); break;
		case 112: ((cForceField *)io->p[0])->SetActive(io->i[0] != 0); break;
		case 113:
		{
			cForceField *pField = (cForceField *)io->p[0];
			if (io->i[0] == 0)
				pField->SetRadius(io->f[0]);
			else if (io->i[0] == 1)
				pField->SetForce(io->f[0]);
			else
				pField->SetFreq(io->f[0]);
			break;
		}
		case 114:
		{
			cForceField *pField = (cForceField *)io->p[0];
			if (io->i[0] == 0)
				pField->FadeIn(io->f[0]);
			else if (io->i[0] == 1)
				pField->FadeOut(io->f[0]);
			else
				pField->FadeTo(io->f[0], io->f[1]);
			break;
		}
		case 115: io->i[0] = ((cForceField *)io->p[0])->IsDead(); break;
		case 116: ((cForceField *)io->p[0])->SetPosition(Get(io->f)); break;

		case 120:
		{
			cMeshEntity *pMesh = e->mpMesh;
			cBoneState *pBone = NULL;
			if (pMesh && io->i[0] == 1)
			{
				if (pMesh->GetBoneStateNum() > 0)
					pBone = Str(io, 0).empty() ? pMesh->GetBoneState(0) : pMesh->GetBoneStateFromName(Str(io, 0));
			}
			else if (pMesh)
				pBone = pMesh->GetBoneStateFromName(Str(io, 0));
			io->p[0] = (uintptr_t)pBone;
			break;
		}
		case 121: Put(io->f, ((cBoneState *)io->p[0])->GetWorldPosition()); break;
		case 122:
		{
			cMatrixf mtx;
			memcpy(mtx.v, (const float *)io->p[1], sizeof(float) * 16);
			((cBoneState *)io->p[0])->SetPreTransform(mtx);
			break;
		}
		case 123: ((cBoneState *)io->p[0])->SetUsePreTransform(true); break;
		case 124: Put(io->f, e->mpMesh->GetWorldPosition()); break;
		case 125:
		{
			cMatrixf m = e->mpMesh->GetWorldMatrix().GetRotation();
			cVector3f vR = m.GetRight(), vU = m.GetUp(), vF = m.GetForward();
			vR.Normalize();
			vU.Normalize();
			vF.Normalize();
			m.SetRight(vR);
			m.SetUp(vU);
			m.SetForward(vF);
			cQuaternion q(m);
			q.Normalize();
			io->f[0] = q.w;
			io->f[1] = q.v.x;
			io->f[2] = q.v.y;
			io->f[3] = q.v.z;
			break;
		}
		case 126: io->i[0] = e->mpMesh->IsVisible(); break;

		case 130: gEdgeGlowRenderer.Register(); break;
		case 131:
		{
			gEdgeGlowRenderer.Register();
			cColor col(io->f[0], io->f[1], io->f[2], io->f[3]);
			for (auto &g : gEdgeGlowRenderer.mvGlow)
				if (g.mID == e->mID)
				{
					g.mColor = col;
					g.mfY = io->f[4];
					return 0;
				}
			gEdgeGlowRenderer.mvGlow.push_back({e->mID, col, io->f[4]});
			break;
		}

		case 140:
		{
			cResourceVarsObject *v = (cResourceVarsObject *)io->p[0];
			Out(io, v->GetVarString(Str(io, 0), Str(io, 1)));
			break;
		}
		case 141: io->f[0] = ((cResourceVarsObject *)io->p[0])->GetVarFloat(Str(io, 0), io->f[0]); break;
		case 142: io->i[0] = ((cResourceVarsObject *)io->p[0])->GetVarInt(Str(io, 0), io->i[0]); break;
		case 143: io->i[0] = ((cResourceVarsObject *)io->p[0])->GetVarBool(Str(io, 0), io->i[0] != 0); break;
		case 144: Put(io->f, ((cResourceVarsObject *)io->p[0])->GetVarVector3f(Str(io, 0), Get(io->f))); break;
		case 146: io->i[0] = (int)((OhplEasArray *)io->p[0])->GetSize(); break;
		case 147: Out(io, *(const tString *)((OhplEasArray *)io->p[0])->At(io->i[0])); break;

		case 150:
		{
			cAgentUserShell *pShell = new cAgentUserShell();
			if (pShell->LoadScript(cSomaScriptRuntime::Get(), Str(io, 0), Str(io, 1), "cLuxUserComponent") == false)
			{
				delete pShell;
				pShell = NULL;
			}
			io->p[0] = (uintptr_t)pShell;
			break;
		}
		case 151: delete (cAgentUserShell *)io->p[0]; break;
		case 152:
		{
			asIScriptObject *pObj = ((cAgentUserShell *)io->p[0])->GetScript();
			if (pObj)
				pObj->AddRef();
			io->p[0] = (uintptr_t)pObj;
			break;
		}
		case 153:
		{
			((cAgentUserShell *)io->p[0])->Tick(io->f[0]);
			break;
		}
		case 154:
		{
			OhplS4bIo a = *io;
			((cAgentUserShell *)io->p[0])->Call(Str(io, 0), [&](asIScriptContext *c) { SetArgs(c, &a, NULL); });
			break;
		}
		case 155: cSomaLuxScriptable::RegisterTimerNatives((asIScriptEngine *)io->p[0], "cLuxUserComponent"); break;

		case 160: Error("%s", Str(io, 0).c_str()); break;
		case 161: Warning("%s", Str(io, 0).c_str()); break;
		case 162:
			if (io->i[0] == 0)
				OhplS4bLog("call %s %s", e->msName.c_str(), Str(io, 0).c_str());
			else if (io->i[0] == 1)
				OhplS4bLog("ev %s %u %d %d", e->msName.c_str(), (unsigned)io->i[1], io->i[2], io->i[3]);
			else
				OhplS4bLog("msg %s %d %.9g,%.9g,%.9g %d", e->msName.c_str(), io->i[1], io->f[0], io->f[1], io->f[2], io->i[2]);
			break;
		case 163: io->i[0] = OhplS4bOn(); break;
		}
		return 0;
	}

	void Sink(void *c, const unsigned char *p, size_t n) { ((tString *)c)->assign((const char *)p, n); }

	void Install()
	{
		static bool sbDone = false;
		if (sbDone == false)
		{
			sbDone = true;
			ohpl_s4b_install(&Op);
		}
	}
}

iPhysicsBody *SomaAgentBody(cSomaLuxEntity *apEnt)
{
	iCharacterBody *b = (iCharacterBody *)ohpl_s4b_body((uintptr_t)apEnt);
	return b ? b->GetCurrentBody() : NULL;
}

void SomaCreateAgent(cSomaLuxEntity *apEnt)
{
	Install();
	ohpl_s4b_create((uintptr_t)apEnt);
}

void SomaDestroyAgent(cSomaLuxEntity *apEnt) { ohpl_s4b_destroy((uintptr_t)apEnt); }
void SomaUpdateComponents(cSomaLuxEntity *apEnt, float afTimeStep) { ohpl_s4b_update_components((uintptr_t)apEnt, afTimeStep); }
void SomaAgentSetActive(cSomaLuxEntity *apEnt, bool abX) { ohpl_s4b_set_active((uintptr_t)apEnt, abX); }
iCharacterBody *SomaAgentGetBody(cSomaLuxEntity *apEnt) { return (iCharacterBody *)ohpl_s4b_body((uintptr_t)apEnt); }

bool SomaAgentGetMatrix(cSomaLuxEntity *apEnt, cMatrixf &aMtx)
{
	cVector3f v;
	if (ohpl_s4b_get_pos((uintptr_t)apEnt, v.v) == false)
		return false;
	aMtx = cMatrixf::Identity;
	aMtx.SetTranslation(v);
	return true;
}

bool SomaAgentSetMatrix(cSomaLuxEntity *apEnt, const cMatrixf &aMtx) { return ohpl_s4b_set_pos((uintptr_t)apEnt, aMtx.GetTranslation().v); }
int SomaAgentGetState(cSomaLuxEntity *apEnt) { return ohpl_s4b_state((uintptr_t)apEnt); }
void SomaAgentSaveExtra(cSomaLuxEntity *apEnt, float &afYaw, bool &abSenses, bool &abDetection) { ohpl_s4b_save_extra((uintptr_t)apEnt, &afYaw, &abSenses, &abDetection); }
void SomaAgentLoadExtra(cSomaLuxEntity *apEnt, float afYaw, bool abSenses, bool abDetection) { ohpl_s4b_load_extra((uintptr_t)apEnt, afYaw, abSenses, abDetection); }

std::string SomaAgentSavePath(cSomaLuxEntity *apEnt)
{
	std::string s;
	ohpl_s4b_save_path((uintptr_t)apEnt, &s, &Sink);
	return s;
}

void SomaAgentLoadPath(cSomaLuxEntity *apEnt, const std::string &asData) { ohpl_s4b_load_path((uintptr_t)apEnt, (const unsigned char *)asData.data(), asData.size()); }
void SomaAgentChangeState(cSomaLuxEntity *apEnt, int alState) { ohpl_s4b_change_state((uintptr_t)apEnt, alState); }
void SomaAgentSendMessage(cSomaLuxEntity *apEnt, int alMessage, const cVector3f &avX, int alX) { ohpl_s4b_send_message((uintptr_t)apEnt, alMessage, avX.v, alX); }
void SomaBroadcastSoundHeard(const cVector3f &avPos, float afRadius, int alPrio) { ohpl_s4b_broadcast_sound(avPos.v, afRadius, alPrio); }

tString SomaAgentDebug(cSomaLuxEntity *apEnt)
{
	tString s;
	ohpl_s4b_debug((uintptr_t)apEnt, &s, &Sink);
	return s;
}

tString SomaNavPath(const cVector3f &avFrom, const cVector3f &avTo)
{
	cSomaLuxMap *pMap = cSomaLuxMap::GetCurrent();
	if (pMap && std::none_of(gmapContainers.begin(), gmapContainers.end(), [&](auto &it) { return it.first.first == pMap->GetWorld(); }))
	{
		tWString sBase = cString::GetFileNameW(cString::SetFileExtW(pMap->GetWorld()->GetFilePath(), _W(""))) + _W("_");
		tWStringList lstFiles;
		cPlatform::FindFilesInDir(lstFiles, cString::GetFilePathW(pMap->GetWorld()->GetFilePath()), sBase + _W("*.nodes"));
		for (const tWString &sFile : lstFiles)
			GetContainer(pMap, cString::To8Char(cString::SetFileExtW(sFile, _W("")).substr(sBase.size())), cVector3f(0.6f, 1.5f, 0.6f), 1.0f);
	}
	for (auto &it : gmapContainers)
	{
		tAINodeList lst;
		if (pMap == NULL || it.first.first != pMap->GetWorld() || it.second.mpAStar->GetPath(avFrom, avTo, &lst) == false)
			continue;
		tString s;
		std::vector<float> vNodes;
		for (cAINode *n : lst)
			vNodes.insert(vNodes.end(), {n->GetPosition().x, n->GetPosition().y, n->GetPosition().z});
		ohpl_s4_nav_path_text(vNodes.data(), lst.size(), &s, +[](void *c, const unsigned char *p, size_t n) { ((tString *)c)->assign((const char *)p, n); });
		return s;
	}
	return "";
}

void SomaUpdateAgent(cSomaLuxEntity *apEnt, float afTimeStep)
{
	OhplS4bBegin(apEnt);
	ohpl_s4b_update((uintptr_t)apEnt, afTimeStep);
	OhplS4bEnd(apEnt);
}

void SomaRegisterAgentNatives(asIScriptEngine *e)
{
	Install();
	ohpl_s4b_register(e);
}
