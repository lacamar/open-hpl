/*
 * Phase 0 scaffolding for a SOMA game module running on the HPL2 engine.
 * See SomaBase.h for scope notes.
 */

#include "SomaBase.h"
#include "SomaSound.h"
#include "SomaSoundscape.h"

#include <cstring>
#include "HpslTranspilerSelfTest.h"
#include "HpslTranspiler.h"
#include "SomaToneMapping.h"
#include "SomaLoaders.h"
#include "SomaFsb.h"
#include "SomaSplash.h"
#include "SomaLuxPlayer.h"
#include "SomaImGui.h"
#include "SomaScriptApi.h"
#include "SomaSave.h"
#include <algorithm>
#include "SomaLuxEntity.h"
#include "SomaLux.h"
#include "SomaScriptRuntime.h"
#include "SomaLuxScriptable.h"

#include "system/HeadlessControl.h"
#include "resources/GpuShaderManager.h"
#include "graphics/RendererDeferred.h"
#include "graphics/Graphics.h"
#include "graphics/GraphicsTypes.h"
#include "graphics/Texture.h"
#include "system/EngineDiagnostics.h"
#include "impl/MeshLoaderCollada.h"
#include "physics/PhysicsBody.h"
#include "physics/PhysicsJoint.h"
#include "physics/PhysicsWorld.h"
#include "resources/WorldLoaderHpm.h"

#include <vector>
#include <set>
#include <limits>
#include <cmath>

#if defined(__linux__)
#include <unistd.h>
#endif

//---------------------------------------

cSomaBase *gpSomaBase = NULL;

//---------------------------------------

static void cSomaBase_HeadlessCmd_CameraState(void *apUserData, const cHeadlessRequest &aReq, cHeadlessResponse &aResp)
{
	cSomaBase *pBase = (cSomaBase*)apUserData;
	if(pBase->GetDebugCamera() == NULL)
	{
		aResp.SetError("no camera yet");
		return;
	}

	const cVector3f &vPos = pBase->GetDebugCamera()->GetPosition();
	aResp.Set("pos_x", vPos.x);
	aResp.Set("pos_y", vPos.y);
	aResp.Set("pos_z", vPos.z);
	aResp.Set("pitch", pBase->GetDebugCamera()->GetPitch());
	aResp.Set("yaw", pBase->GetDebugCamera()->GetYaw());
	aResp.Set("fps", pBase->mpEngine->GetFPS());
	aResp.Set("fov_deg", cMath::ToDeg(pBase->GetDebugCamera()->GetFOV()));
}

static void cSomaBase_HeadlessCmd_PlayerState(void *apUserData, const cHeadlessRequest &aReq, cHeadlessResponse &aResp)
{
	cSomaLuxPlayer *pPlayer = cSomaLuxPlayer::Get();
	if (pPlayer == NULL)
	{
		aResp.SetError("no script player");
		return;
	}
	aResp.Set("active", pPlayer->IsActive());
	aResp.Set("state", pPlayer->GetState() ? pPlayer->GetState()->msName : tString(""));
	aResp.Set("move_state", pPlayer->GetMoveState() ? pPlayer->GetMoveState()->msName : tString(""));
	aResp.Set("health", pPlayer->mfHealth);
	if (iCharacterBody *pBody = pPlayer->GetCharacterBody())
	{
		cVector3f v = pBody->GetFeetPosition();
		aResp.Set("feet_x", v.x);
		aResp.Set("feet_y", v.y);
		aResp.Set("feet_z", v.z);
		aResp.Set("yaw", pBody->GetYaw());
		aResp.Set("on_ground", pBody->IsOnGround());
		aResp.Set("max_fwd_speed", pBody->GetMaxPositiveMoveSpeed(eCharDir_Forward));
	}
}

static void cSomaBase_HeadlessCmd_LuxEntity(void *apUserData, const cHeadlessRequest &aReq, cHeadlessResponse &aResp)
{
	cSomaLuxEntity *pEnt = cSomaLuxMap::GetCurrent() ? cSomaLuxMap::GetCurrent()->GetEntity(aReq.GetString("name", "")) : NULL;
	if (pEnt == NULL)
	{
		aResp.SetError("no such entity");
		return;
	}
	aResp.Set("name", pEnt->msName);
	aResp.Set("type", pEnt->meType);
	aResp.Set("class", pEnt->msClassName);
	aResp.Set("script", pEnt->GetScript() ? tString(pEnt->GetScript()->GetObjectType()->GetName()) : tString(""));
	aResp.Set("file", pEnt->msFileName);
	aResp.Set("health", pEnt->mfHealth);
	aResp.Set("active", pEnt->mbActive);
	aResp.Set("interaction_disabled", pEnt->mbInteractionDisabled);
	aResp.Set("interact_callback", pEnt->msInteractCallback);
	aResp.Set("collide_callbacks", (int)pEnt->mvCollideCallbacks.size());
	cVector3f v = pEnt->GetPosition();
	aResp.Set("x", v.x);
	aResp.Set("y", v.y);
	aResp.Set("z", v.z);
	aResp.Set("size", cString::ToString(pEnt->mvSize.x) + " " + cString::ToString(pEnt->mvSize.y) + " " + cString::ToString(pEnt->mvSize.z));
	if (pEnt->mpImGui)
	{
		aResp.Set("gui_active", pEnt->mbGuiActive);
		aResp.Set("gui_func", pEnt->msOnGuiFunc);
		aResp.Set("gui_screen", pEnt->mpGuiSubMesh != NULL);
		aResp.Set("gui_calls", pEnt->mlGuiCalls);
		aResp.Set("gui_draws", pEnt->mlGuiDraws);
		aResp.Set("gui_ops", pEnt->mpImGui->DebugOps(12));
		cGuiSet *pSet = pEnt->mpImGui->GetSet();
		aResp.Set("gui_mtx", pSet->Get3DTransform().ToString());
		aResp.Set("gui_size", pSet->Get3DSize().ToString());
		aResp.Set("gui_virtual", pSet->GetVirtualSize().ToString());
		if (pEnt->mpGuiSubMesh)
		{
			aResp.Set("gui_color_mul", pEnt->mpGuiSubMesh->GetColorMul().ToString());
			aResp.Set("gui_material", pEnt->mpGuiSubMesh->GetMaterial() ? pEnt->mpGuiSubMesh->GetMaterial()->GetName() : tString("-"));
		}
	}
}

extern std::string gsSomaExecOutput;
cSomaImGui *SomaHudImGui();

class cSomaJointFrameFilter : public iPhysicsBodyCallback
{
public:
	std::map<iPhysicsBody *, std::set<iPhysicsBody *>> mmapIgnored;
	bool OnAABBCollide(iPhysicsBody *apBody, iPhysicsBody *apCollideBody) override
	{
		auto it = mmapIgnored.find(apBody);
		return it == mmapIgnored.end() || it->second.count(apCollideBody) == 0;
	}
	void OnBodyCollide(iPhysicsBody *, iPhysicsBody *, cPhysicsContactData *) override {}
};

static void cSomaBase_HeadlessCmd_ImGuiStats(void *apUserData, const cHeadlessRequest &aReq, cHeadlessResponse &aResp)
{
	cSomaImGui *pHud = SomaHudImGui();
	aResp.Set("hud_ops", pHud->GetDrawnOpNum());
	aResp.Set("hud_virtual_w", pHud->GetSet()->GetVirtualSize().x);
	aResp.Set("hud_virtual_h", pHud->GetSet()->GetVirtualSize().y);
	cSomaImGui *pFocus = cSomaImGui::GetInputFocus();
	aResp.Set("focus", pFocus ? pFocus->GetName() : tString(""));
	if (pFocus)
	{
		aResp.Set("mouse_x", pFocus->GetMousePosition().x);
		aResp.Set("mouse_y", pFocus->GetMousePosition().y);
		aResp.Set("virtual_w", pFocus->GetSet()->GetVirtualSize().x);
		aResp.Set("virtual_h", pFocus->GetSet()->GetVirtualSize().y);
	}
}

static cSomaImGui *HeadlessImGui(const cHeadlessRequest &aReq)
{
	tString sName = aReq.GetString("name", "");
	if (sName.empty())
		return cSomaImGui::GetInputFocus();
	cSomaLuxMap *pMap = cSomaLuxMap::GetCurrent();
	cSomaLuxEntity *pEnt = pMap ? pMap->GetEntity(sName) : NULL;
	return pEnt ? pEnt->mpImGui : NULL;
}

static void cSomaBase_HeadlessCmd_ImGuiOps(void *apUserData, const cHeadlessRequest &aReq, cHeadlessResponse &aResp)
{
	cSomaImGui *pGui = HeadlessImGui(aReq);
	if (pGui == NULL)
	{
		aResp.SetError("no imgui");
		return;
	}
	aResp.Set("ops", pGui->DebugOps(aReq.GetInt("n", 400)));
}

static void cSomaBase_HeadlessCmd_ImGuiCursor(void *apUserData, const cHeadlessRequest &aReq, cHeadlessResponse &aResp)
{
	cSomaImGui *pGui = HeadlessImGui(aReq);
	if (pGui == NULL)
	{
		aResp.SetError("no imgui");
		return;
	}
	cVector2f vPos(aReq.GetFloat("x", 0), aReq.GetFloat("y", 0));
	cVector2f vRel = vPos - pGui->mvCursor3D;
	pGui->mvCursor3D = vPos;
	pGui->SendMouseVirtualPosition(vPos, vRel);
}

static void cSomaBase_HeadlessCmd_SoundStats(void *apUserData, const cHeadlessRequest &aReq, cHeadlessResponse &aResp)
{
	std::map<tString, int> mapCount;
	tSoundEntryList *pList = gpSomaBase->mpEngine->GetSound()->GetSoundHandler()->GetEntryList();
	for (cSoundEntry *pEntry : *pList)
		mapCount[pEntry->GetName() + (pEntry->GetChannel()->GetLooping() ? " (loop)" : "")]++;
	tString sEntries;
	for (auto &it : mapCount)
		sEntries += cString::ToString(it.second) + " " + it.first + "\n";
	cVector3f vListener = gpSomaBase->mpEngine->GetSound()->GetLowLevel()->GetListenerPosition();
	tString sDetail;
	for (cSoundEntry *pEntry : *pList)
	{
		iSoundChannel *pCh = pEntry->GetChannel();
		float fDist = pCh->Get3D() ? cMath::Vector3Dist(pCh->GetPositionIsRelative() ? vListener + pCh->GetRelPosition() : pCh->GetPosition(), vListener) : 0;
		sDetail += pEntry->GetName() + "|" + (pCh->GetData() ? pCh->GetData()->GetName() : "") + "|" + cString::ToString(pEntry->GetType()) + "|" +
				   cString::ToString(pCh->GetVolume()) + "|" + cString::ToString(pEntry->GetVolumeMul()) + "|" + (pCh->GetLooping() ? "1" : "0") + "|" +
				   (pCh->Get3D() ? "1" : "0") + "|" + cString::ToString(fDist) + "|" + cString::ToString(pCh->GetMinDistance()) + "|" +
				   cString::ToString(pCh->GetMaxDistance()) + "|" + cString::ToString((float)pCh->GetElapsedTime()) + "|" +
				   cString::ToString((float)pCh->GetTotalTime()) + "|" + (pCh->GetPaused() ? "1" : "0") + "\n";
	}
	aResp.Set("count", (int)pList->size());
	aResp.Set("entries", sEntries);
	aResp.Set("detail", sDetail);
	tString sRecent;
	for (const tString &sLine : gpSomaBase->mpEngine->GetSound()->GetSoundHandler()->GetRecentStarts())
		sRecent += sLine + "\n";
	aResp.Set("recent", sRecent);
	aResp.Set("now", (int)cPlatform::GetApplicationTime());
	aResp.Set("listener", vListener.ToString());
	aResp.Set("soundscape", cSomaSoundscape::Get()->Describe());
	tString sEvents;
	for (cSomaSoundInstance *pInst : cSomaSoundEvents::Get()->GetInstances())
		sEvents += pInst->Describe() + "\n";
	aResp.Set("events", sEvents);
	cMusicHandler *pMusic = gpSomaBase->mpEngine->GetSound()->GetMusicHandler();
	aResp.Set("music", pMusic->GetCurrentSongName());
	aResp.Set("music_volume", pMusic->GetCurrentSongVolume());
}

static void cSomaBase_HeadlessCmd_StubReport(void *apUserData, const cHeadlessRequest &aReq, cHeadlessResponse &aResp)
{
	std::vector<std::pair<int, std::string>> v;
	for (auto &it : cSomaScriptApi::GetStubCallCounts())
		v.push_back(std::make_pair(it.second, it.first));
	std::sort(v.rbegin(), v.rend());
	tString sOut;
	for (int i = 0; i < (int)v.size() && i < aReq.GetInt("n", 60); ++i)
		sOut += cString::ToString(v[i].first) + " " + v[i].second + "\n";
	aResp.Set("count", (int)v.size());
	aResp.Set("stubs", sOut);
}

static void cSomaBase_HeadlessCmd_BodyContacts(void *apUserData, const cHeadlessRequest &aReq, cHeadlessResponse &aResp)
{
	cSomaLuxMap *pMap = cSomaLuxMap::GetCurrent();
	cSomaLuxEntity *pEnt = pMap ? pMap->GetEntity(aReq.GetString("name", "")) : NULL;
	if (pEnt == NULL)
	{
		aResp.SetError("no such entity");
		return;
	}
	iPhysicsWorld *pWorld = pMap->GetWorld()->GetPhysicsWorld();
	tString sOut;
	for (iPhysicsBody *pBody : pEnt->mvBodies)
	{
		cPhysicsBodyIterator it = pWorld->GetBodyIterator();
		while (it.HasNext())
		{
			iPhysicsBody *pOther = it.Next();
			if (pOther == pBody || pOther->GetCollide() == false || cMath::CheckBVIntersection(*pBody->GetBoundingVolume(), *pOther->GetBoundingVolume()) == false)
				continue;
			cCollideData data;
			data.SetMaxSize(4);
			if (pWorld->CheckShapeCollision(pBody->GetShape(), pBody->GetLocalMatrix(), pOther->GetShape(), pOther->GetLocalMatrix(), data, 4, true))
				sOut += pBody->GetName() + " x " + pOther->GetName() + " depth " + cString::ToString(data.mvContactPoints[0].mfDepth) + "\n";
		}
	}
	aResp.Set("contacts", sOut);
}

// Body counts and the awake dynamic bodies (physics cost)
static void cSomaBase_HeadlessCmd_PhysicsStats(void *apUserData, const cHeadlessRequest &aReq, cHeadlessResponse &aResp)
{
	cSomaLuxMap *pMap = cSomaLuxMap::GetCurrent();
	if (pMap == NULL)
	{
		aResp.SetError("no map");
		return;
	}
	int lStatic = 0, lDynamic = 0, lAwake = 0, lChar = 0;
	std::vector<std::pair<float, tString>> vAwake;
	cPhysicsBodyIterator it = pMap->GetWorld()->GetPhysicsWorld()->GetBodyIterator();
	while (it.HasNext())
	{
		iPhysicsBody *pBody = it.Next();
		if (pBody->IsCharacter())
			++lChar;
		else if (pBody->GetMass() == 0)
			++lStatic;
		else
		{
			++lDynamic;
			if (pBody->GetEnabled())
			{
				++lAwake;
				vAwake.push_back(std::make_pair(pBody->GetLinearVelocity().Length() + pBody->GetAngularVelocity().Length(), pBody->GetName()));
			}
		}
	}
	std::sort(vAwake.rbegin(), vAwake.rend());
	tString sOut;
	for (size_t i = 0; i < vAwake.size() && i < (size_t)aReq.GetInt("n", 20); ++i)
		sOut += cString::ToString(vAwake[i].first) + " " + vAwake[i].second + "\n";
	aResp.Set("static", lStatic);
	aResp.Set("dynamic", lDynamic);
	aResp.Set("awake", lAwake);
	aResp.Set("character", lChar);
	aResp.Set("awake_top", sOut);
}

// Every body a ray hits: distance, body, owning map entity (collision conformance)
static void cSomaBase_HeadlessCmd_Raycast(void *apUserData, const cHeadlessRequest &aReq, cHeadlessResponse &aResp)
{
	cSomaLuxMap *pMap = cSomaLuxMap::GetCurrent();
	if (pMap == NULL)
	{
		aResp.SetError("no map");
		return;
	}
	cVector3f vFrom(aReq.GetFloat("x", 0), aReq.GetFloat("y", 0), aReq.GetFloat("z", 0));
	cVector3f vTo(aReq.GetFloat("x2", vFrom.x), aReq.GetFloat("y2", vFrom.y - 10), aReq.GetFloat("z2", vFrom.z));
	std::map<iPhysicsBody *, tString> mapOwner;
	for (cSomaLuxEntity *pEnt : pMap->GetEntities())
		for (iPhysicsBody *pBody : pEnt->mvBodies)
			mapOwner[pBody] = pEnt->msName;
	struct cHits : iPhysicsRayCallback
	{
		std::vector<std::pair<float, iPhysicsBody *>> mvHits;
		bool OnIntersect(iPhysicsBody *pBody, cPhysicsRayParams *apParams) override
		{
			mvHits.push_back(std::make_pair(apParams->mfDist, pBody));
			return true;
		}
	} hits;
	pMap->GetWorld()->GetPhysicsWorld()->CastRay(&hits, vFrom, vTo, true, false, false);
	std::sort(hits.mvHits.begin(), hits.mvHits.end());
	tString sOut;
	for (auto &h : hits.mvHits)
		sOut += cString::ToString(h.first) + " " + h.second->GetName() + " entity=" + mapOwner[h.second] + " mass=" +
				cString::ToString(h.second->GetMass()) + " collide=" + cString::ToString(h.second->GetCollide()) +
				" char=" + cString::ToString(h.second->GetCollideCharacter()) + "\n";
	aResp.Set("hits", sOut);
}

static void cSomaBase_HeadlessCmd_ScriptExec(void *apUserData, const cHeadlessRequest &aReq, cHeadlessResponse &aResp)
{
	cSomaScriptRuntime *pRuntime = cSomaScriptRuntime::Get();
	if (pRuntime == NULL)
	{
		aResp.SetError("no script runtime");
		return;
	}
	std::string sError;
	gsSomaExecOutput.clear();
	bool bOk = pRuntime->Exec(aReq.GetString("code", ""), sError);
	aResp.Set("output", gsSomaExecOutput);
	if (bOk == false)
		aResp.SetError(sError);
}

static std::string ScriptValueString(asIScriptEngine *apEngine, int alTypeId, void *apAddr)
{
	char sBuf[64];
	switch (alTypeId)
	{
	case asTYPEID_BOOL: return *(bool *)apAddr ? "true" : "false";
	case asTYPEID_INT8: return std::to_string(*(int8_t *)apAddr);
	case asTYPEID_INT16: return std::to_string(*(int16_t *)apAddr);
	case asTYPEID_INT32: return std::to_string(*(int32_t *)apAddr);
	case asTYPEID_INT64: return std::to_string(*(int64_t *)apAddr);
	case asTYPEID_UINT8: return std::to_string(*(uint8_t *)apAddr);
	case asTYPEID_UINT16: return std::to_string(*(uint16_t *)apAddr);
	case asTYPEID_UINT32: return std::to_string(*(uint32_t *)apAddr);
	case asTYPEID_UINT64: return std::to_string(*(uint64_t *)apAddr);
	case asTYPEID_FLOAT: snprintf(sBuf, sizeof(sBuf), "%g", *(float *)apAddr); return sBuf;
	case asTYPEID_DOUBLE: snprintf(sBuf, sizeof(sBuf), "%g", *(double *)apAddr); return sBuf;
	}
	asITypeInfo *pType = apEngine->GetTypeInfoById(alTypeId);
	if (pType == NULL)
		return "?";
	tString sName = pType->GetName();
	if (pType->GetFlags() & asOBJ_ENUM)
		return std::to_string(*(int *)apAddr);
	if (alTypeId & asTYPEID_OBJHANDLE)
		return *(void **)apAddr ? sName + "@" : "null";
	if (sName == "tString")
		return "\"" + *(tString *)apAddr + "\"";
	if (sName == "cVector3f")
	{
		cVector3f v = *(cVector3f *)apAddr;
		snprintf(sBuf, sizeof(sBuf), "(%g %g %g)", v.x, v.y, v.z);
		return sBuf;
	}
	return sName;
}

static void cSomaBase_HeadlessCmd_ScriptVars(void *apUserData, const cHeadlessRequest &aReq, cHeadlessResponse &aResp)
{
	tString sName = aReq.GetString("name", "");
	std::string sOut;
	for (cSomaLuxScriptable *p : cSomaLuxScriptable::GetAll())
	{
		asIScriptObject *pObj = p->GetScript();
		if (pObj == NULL)
			continue;
		tString sClass = pObj->GetObjectType()->GetName();
		if (sName.empty())
		{
			sOut += p->msScriptName + " " + sClass + "\n";
			continue;
		}
		if (p->msScriptName != sName && sClass != sName)
			continue;
		sOut += "[" + p->msScriptName + " " + sClass + "]\n";
		for (asUINT i = 0; i < pObj->GetPropertyCount(); ++i)
			sOut += tString(pObj->GetPropertyName(i)) + "=" +
					ScriptValueString(pObj->GetEngine(), pObj->GetPropertyTypeId(i), pObj->GetAddressOfProperty(i)) + "\n";
	}
	aResp.Set("output", sOut);
}

// Headless debug hook onto cRendererDeferred's own existing debug quad-view
// of the raw G-buffer contents (color/diffuse top-left, normal+depth top-
// right, specular bottom-left, a 4th target bottom-right if present) - lets
// a headless screenshot inspect each render target directly instead of only
// the final composited frame. Used to root-cause the SOMA "real lights get
// routed for rendering but contribute ~0 visible brightness" investigation
// (see PORTING_NOTES.md) - confirmed live that the color/diffuse target is
// correctly populated (real, plausible lit texture data) while the normal+
// depth target renders solid black regardless of cRendererDeferred's own
// 32-bit vs 64-bit G-buffer texture format, meaning the deferred G-buffer
// solid pass's fragment shader isn't reaching that attachment at all - real,
// still-open investigation, not yet root-caused further than that (needs
// real GPU frame-capture tooling this headless workflow doesn't have).
static void cSomaBase_HeadlessCmd_SetDebugGbuffer(void *apUserData, const cHeadlessRequest &aReq, cHeadlessResponse &aResp)
{
	cRendererDeferred::SetDebugRenderFrameBuffers(aReq.GetBool("enabled", false));
}

// Numeric follow-up to set_debug_gbuffer above: reads a real G-buffer render
// target's actual GPU pixel data back via iTexture::GetRawPixelsRGBAFloat()
// (glGetTexImage, bypassing RenderGbufferContent()'s own quad-view draw
// entirely) and reports per-channel min/max/mean plus one sampled pixel -
// added because the debug quad-view being solid black is consistent with
// *either* the G-buffer target itself never being written, *or* the debug
// view's own draw (texture unit, blend state) being wrong while the real
// target holds good data. "target": 0=color/diffuse, 1=normal+depth (the
// one under investigation - see PORTING_NOTES.md), 2=specular, 3=4th
// attachment if present. "x"/"y": pixel to sample directly, default center.
// Writes a debug target as PFM (bottom-up float RGB, same row order as GL).
static void cSomaBase_HeadlessCmd_DumpTarget(void *apUserData, const cHeadlessRequest &aReq, cHeadlessResponse &aResp)
{
	cSomaBase *pBase = (cSomaBase*)apUserData;
	cRendererDeferred *pDeferred = static_cast<cRendererDeferred*>(pBase->mpEngine->GetGraphics()->GetRenderer(eRenderer_Main));
	iTexture *pTex = pDeferred ? pDeferred->GetDebugGBufferTexture(aReq.GetInt("target", 4)) : NULL;
	if (aReq.HasKey("light") && pDeferred)
	{
		cWorld *pWorld = pBase->GetCurrentWorld();
		iLight *pLight = pWorld ? pWorld->GetLight(aReq.GetString("light", "")) : NULL;
		pTex = pLight ? pDeferred->GetDebugShadowTexture(pLight) : NULL;
	}
	if (aReq.HasKey("screen"))
	{
		cSomaLuxEntity *pEnt = cSomaLuxMap::GetCurrent() ? cSomaLuxMap::GetCurrent()->GetEntity(aReq.GetString("screen", "")) : NULL;
		cMaterial *pMat = pEnt && pEnt->mpGuiSubMesh ? pEnt->mpGuiSubMesh->GetCustomMaterial() : NULL;
		pTex = pMat ? pMat->GetTexture(eMaterialTexture_Diffuse) : NULL;
	}
	std::vector<float> vPixels;
	if(pTex == NULL || pTex->GetRawPixelsRGBAFloat(vPixels) == false) { aResp.SetError("no such target or no GPU data"); return; }
	tString sPath = aReq.GetString("path", "");
	FILE *pFile = sPath != "" ? fopen(sPath.c_str(), "wb") : NULL;
	if(pFile == NULL) { aResp.SetError("cannot open path"); return; }
	int lW = pTex->GetWidth(), lH = pTex->GetHeight();
	fprintf(pFile, "PF\n%d %d\n-1.0\n", lW, lH);
	int lCh = aReq.GetInt("channel", -1);
	for(size_t i=0; i<(size_t)lW*lH; ++i)
	{
		if(lCh < 0) fwrite(&vPixels[i*4], sizeof(float), 3, pFile);
		else for(int c=0; c<3; ++c) fwrite(&vPixels[i*4+lCh], sizeof(float), 1, pFile);
	}
	fclose(pFile);
	aResp.Set("width", lW);
	aResp.Set("height", lH);
}

// Translucent render list of the last frame; skip=N hides entry N (-1 none).
static void cSomaBase_HeadlessCmd_Translucents(void *apUserData, const cHeadlessRequest &aReq, cHeadlessResponse &aResp)
{
	cSomaBase *pBase = (cSomaBase*)apUserData;
	cRendererDeferred *pDeferred = static_cast<cRendererDeferred*>(pBase->mpEngine->GetGraphics()->GetRenderer(eRenderer_Main));
	if(pDeferred == NULL || pDeferred->GetCurrentRenderList() == NULL) { aResp.SetError("no render list"); return; }
	cRendererDeferred::mlDebugSkipTranslucent = aReq.GetInt("skip", -1);
	tString sOut = "[";
	cRenderableVecIterator it = pDeferred->GetCurrentRenderList()->GetArrayIterator(eRenderListType_Translucent);
	for(int i=0; it.HasNext(); ++i)
	{
		iRenderable *pObj = it.Next();
		cMaterial *pMat = pObj->GetMaterial();
		if(i>0) sOut += ",";
		iTexture *pDiffuse = pMat ? pMat->GetTexture(eMaterialTexture_Diffuse) : NULL;
		sOut += "\"" + cString::ToString(i) + " " + pObj->GetName() + " " + (pMat ? pMat->GetName() : tString("-")) +
				" diffuse=" + (pDiffuse ? pDiffuse->GetName() + ":" + cString::ToString((int)pDiffuse->GetPixelFormat()) : tString("none")) + "\"";
	}
	aResp.SetRaw("objects", sOut + "]");
}

static void cSomaBase_HeadlessCmd_ReadGbufferStats(void *apUserData, const cHeadlessRequest &aReq, cHeadlessResponse &aResp)
{
	cSomaBase *pBase = (cSomaBase*)apUserData;

	iRenderer *pRenderer = pBase->mpEngine->GetGraphics()->GetRenderer(eRenderer_Main);
	cRendererDeferred *pDeferred = static_cast<cRendererDeferred*>(pRenderer);
	if(pDeferred == NULL)
	{
		aResp.SetError("no deferred renderer yet");
		return;
	}

	int lTarget = aReq.GetInt("target", 1);
	iTexture *pTex = pDeferred->GetDebugGBufferTexture(lTarget);
	if(pTex == NULL)
	{
		aResp.SetError("no such G-buffer target");
		return;
	}

	std::vector<float> vPixels;
	if(pTex->GetRawPixelsRGBAFloat(vPixels) == false)
	{
		aResp.SetError("G-buffer target has no GPU data yet");
		return;
	}

	int lWidth = pTex->GetWidth();
	int lHeight = pTex->GetHeight();

	float fMin[4], fMax[4], fSum[4];
	int lNanCount[4], lZeroCount[4];
	for(int i=0; i<4; ++i)
	{
		fMin[i] = std::numeric_limits<float>::max();
		fMax[i] = -std::numeric_limits<float>::max();
		fSum[i] = 0;
		lNanCount[i] = 0;
		lZeroCount[i] = 0;
	}

	size_t lNumPixels = (size_t)lWidth * (size_t)lHeight;
	for(size_t lPix=0; lPix<lNumPixels; ++lPix)
	{
		for(int c=0; c<4; ++c)
		{
			float fVal = vPixels[lPix*4 + c];
			if(std::isnan(fVal)) { ++lNanCount[c]; continue; }
			if(fVal == 0.0f) ++lZeroCount[c];
			if(fVal < fMin[c]) fMin[c] = fVal;
			if(fVal > fMax[c]) fMax[c] = fVal;
			fSum[c] += fVal;
		}
	}

	aResp.Set("width", lWidth);
	aResp.Set("height", lHeight);

	const char *pChannelNames[4] = {"r", "g", "b", "a"};
	for(int c=0; c<4; ++c)
	{
		size_t lFiniteCount = lNumPixels - lNanCount[c];
		aResp.Set(tString(pChannelNames[c]) + "_min", fMin[c]);
		aResp.Set(tString(pChannelNames[c]) + "_max", fMax[c]);
		aResp.Set(tString(pChannelNames[c]) + "_mean", lFiniteCount > 0 ? (fSum[c] / (float)lFiniteCount) : 0.0f);
		aResp.Set(tString(pChannelNames[c]) + "_nan_count", lNanCount[c]);
		aResp.Set(tString(pChannelNames[c]) + "_zero_count", lZeroCount[c]);
	}

	int lSampleX = aReq.GetInt("x", lWidth/2);
	int lSampleY = aReq.GetInt("y", lHeight/2);
	if(lSampleX >= 0 && lSampleX < lWidth && lSampleY >= 0 && lSampleY < lHeight)
	{
		size_t lIdx = ((size_t)lSampleY * lWidth + lSampleX) * 4;
		aResp.Set("sample_r", vPixels[lIdx+0]);
		aResp.Set("sample_g", vPixels[lIdx+1]);
		aResp.Set("sample_b", vPixels[lIdx+2]);
		aResp.Set("sample_a", vPixels[lIdx+3]);
	}
}

static void cSomaBase_HeadlessCmd_SetCamera(void *apUserData, const cHeadlessRequest &aReq, cHeadlessResponse &aResp)
{
	cSomaBase *pBase = (cSomaBase*)apUserData;
	if(pBase->GetDebugCamera() == NULL)
	{
		aResp.SetError("no camera yet");
		return;
	}

	if(aReq.HasKey("x") || aReq.HasKey("y") || aReq.HasKey("z"))
	{
		const cVector3f &vCur = pBase->GetDebugCamera()->GetPosition();
		cVector3f vPos(aReq.GetFloat("x", vCur.x), aReq.GetFloat("y", vCur.y), aReq.GetFloat("z", vCur.z));
		pBase->GetDebugCamera()->SetPosition(vPos);
	}
	if(aReq.HasKey("pitch")) pBase->GetDebugCamera()->SetPitch(aReq.GetFloat("pitch", 0));
	if(aReq.HasKey("yaw")) pBase->GetDebugCamera()->SetYaw(aReq.GetFloat("yaw", 0));
}

static void cSomaBase_HeadlessCmd_StartMap(void *apUserData, const cHeadlessRequest &aReq, cHeadlessResponse &aResp)
{
	cSomaBase *pBase = (cSomaBase*)apUserData;
	tString sMap = aReq.GetString("map", "");
	if(sMap == "")
	{
		aResp.SetError("missing 'map' field");
		return;
	}

	cVector3f vPos(aReq.GetFloat("x", 0), aReq.GetFloat("y", 1.7f), aReq.GetFloat("z", 0));
	tString sStartPosName = aReq.GetString("pos", aReq.HasKey("x") ? "" : "*");

	tString sError;
	if(pBase->LoadMap(sMap, vPos, sError, sStartPosName) == false)
	{
		aResp.SetError(sError);
		return;
	}

	aResp.SetRaw("load_report", cWorldLoaderHpm::GetLastLoadReportJson());
}

static void cSomaBase_HeadlessCmd_LoadReport(void *apUserData, const cHeadlessRequest &aReq, cHeadlessResponse &aResp)
{
	const tString &sReport = cWorldLoaderHpm::GetLastLoadReportJson();
	if(sReport == "") { aResp.SetError("no hpm map loaded yet"); return; }
	aResp.SetRaw("load_report", sReport);
}

static void cSomaBase_HeadlessCmd_WorldStats(void *apUserData, const cHeadlessRequest &aReq, cHeadlessResponse &aResp)
{
	cSomaBase *pBase = (cSomaBase*)apUserData;
	if(pBase->GetCurrentWorld() == NULL) { aResp.SetError("no world loaded"); return; }
	aResp.SetRaw("world", cEngineDiagnostics::GetWorldStatsJson(pBase->GetCurrentWorld()));
}

static void cSomaBase_HeadlessCmd_RenderStats(void *apUserData, const cHeadlessRequest &aReq, cHeadlessResponse &aResp)
{
	cSomaBase *pBase = (cSomaBase*)apUserData;
	if(pBase->GetCurrentViewport() == NULL) { aResp.SetError("no viewport yet"); return; }
	aResp.SetRaw("render", cEngineDiagnostics::GetRenderStatsJson(pBase->GetCurrentViewport(), pBase->mpEngine->GetGraphics()));
	aResp.Set("fps", pBase->mpEngine->GetFPS());
}

static void cSomaBase_HeadlessCmd_EntityInfo(void *apUserData, const cHeadlessRequest &aReq, cHeadlessResponse &aResp)
{
	cSomaBase *pBase = (cSomaBase*)apUserData;
	if(pBase->GetCurrentWorld() == NULL) { aResp.SetError("no world loaded"); return; }
	tString sInfo = cEngineDiagnostics::GetEntityInfoJson(pBase->GetCurrentWorld(), aReq.GetString("name", ""));
	if(sInfo == "") { aResp.SetError("no mesh entity with that name"); return; }
	aResp.SetRaw("entity", sInfo);
}

// Names the submeshes under a screen pixel, or along x,y,z -> x2,y2,z2 (ray vs current triangles, skinned included)
static void cSomaBase_HeadlessCmd_PickEntity(void *apUserData, const cHeadlessRequest &aReq, cHeadlessResponse &aResp)
{
	cSomaBase *pBase = (cSomaBase*)apUserData;
	cWorld *pWorld = pBase->GetCurrentWorld();
	cCamera *pCam = pBase->GetDebugCamera();
	if(pWorld == NULL || pCam == NULL) { aResp.SetError("no world loaded"); return; }
	cVector3f vStart, vDir;
	float fMaxT = 1e30f;
	if(aReq.HasKey("x2"))
	{
		vStart = cVector3f(aReq.GetFloat("x", 0), aReq.GetFloat("y", 0), aReq.GetFloat("z", 0));
		vDir = cVector3f(aReq.GetFloat("x2", 0), aReq.GetFloat("y2", 0), aReq.GetFloat("z2", 0)) - vStart;
		fMaxT = vDir.Length();
		vDir.Normalize();
	}
	else
		pCam->UnProject(&vStart, &vDir, cVector2f(aReq.GetFloat("x", 0.5f), aReq.GetFloat("y", 0.5f)), 1);
	std::vector<std::pair<float, tString>> vHits;
	auto test = [&](cMeshEntity *pEnt) {
		if(pEnt->IsVisible() == false) return;
		for(int i = 0; i < pEnt->GetSubMeshEntityNum(); ++i)
		{
			cSubMeshEntity *pSub = pEnt->GetSubMeshEntity(i);
			if(pSub->IsVisible() == false) continue;
			iVertexBuffer *pVtx = pSub->GetVertexBuffer();
			cMatrixf *pModel = pSub->GetModelMatrix(NULL);
			cMatrixf mtxInv = pModel ? cMath::MatrixInverse(*pModel) : cMatrixf::Identity;
			cVector3f vS = cMath::MatrixMul(mtxInv, vStart), vD = cMath::MatrixMul3x3(mtxInv, vDir);
			const float *pPos = pVtx->GetFloatArray(eVertexBufferElement_Position);
			int lStride = pVtx->GetElementNum(eVertexBufferElement_Position);
			const unsigned int *pIdx = pVtx->GetIndices();
			float fBest = 1e30f;
			for(int t = 0; t + 2 < pVtx->GetIndexNum(); t += 3)
			{
				cVector3f v[3];
				for(int k = 0; k < 3; ++k) v[k] = cVector3f(pPos[pIdx[t+k]*lStride], pPos[pIdx[t+k]*lStride+1], pPos[pIdx[t+k]*lStride+2]);
				cVector3f e1 = v[1]-v[0], e2 = v[2]-v[0], p = cMath::Vector3Cross(vD, e2);
				float det = cMath::Vector3Dot(e1, p);
				if(std::fabs(det) < 1e-12f) continue;
				cVector3f tv = vS - v[0];
				float u = cMath::Vector3Dot(tv, p)/det;
				if(u < 0 || u > 1) continue;
				cVector3f q = cMath::Vector3Cross(tv, e1);
				float w = cMath::Vector3Dot(vD, q)/det;
				if(w < 0 || u + w > 1) continue;
				float fT = cMath::Vector3Dot(e2, q)/det;
				if(fT > 0 && fT < fBest) fBest = fT;
			}
			if(fBest < 1e30f)
			{
				cVector3f vHit = pModel ? cMath::MatrixMul(*pModel, vS + vD*fBest) : vS + vD*fBest;
				float fDist = cMath::Vector3Dist(vStart, vHit);
				if(fDist < fMaxT)
					vHits.push_back(std::make_pair(fDist, pEnt->GetName() + "/" + pSub->GetName() + (pSub->GetMaterial() ? " " + pSub->GetMaterial()->GetName() : "") +
						(pSub->GetRenderFlagBit(eRenderableFlag_ShadowCaster) ? "" : " noshadow")));
			}
		}
	};
	cMeshEntityIterator it = pWorld->GetDynamicMeshEntityIterator();
	while(it.HasNext()) test(it.Next());
	it = pWorld->GetStaticMeshEntityIterator();
	while(it.HasNext()) test(it.Next());
	std::sort(vHits.begin(), vHits.end());
	tString sOut = "";
	for(size_t i = 0; i < vHits.size() && i < 8; ++i)
		sOut += cString::ToString(vHits[i].first) + " " + vHits[i].second + "\n";
	aResp.Set("hits", sOut);
}

static void cSomaBase_HeadlessCmd_Lights(void *apUserData, const cHeadlessRequest &aReq, cHeadlessResponse &aResp)
{
	cSomaBase *pBase = (cSomaBase*)apUserData;
	if(pBase->GetCurrentWorld() == NULL || pBase->GetDebugCamera() == NULL) { aResp.SetError("no world loaded"); return; }
	iRenderer *pRenderer = pBase->mpEngine->GetGraphics()->GetRenderer(eRenderer_Main);
	aResp.SetRaw("lights", cEngineDiagnostics::GetLightsJson(pBase->GetCurrentWorld(), pBase->GetDebugCamera()->GetPosition(), aReq.GetInt("n", 8), pRenderer ? pRenderer->GetCurrentRenderList() : NULL));
}

static void cSomaBase_HeadlessCmd_SetLight(void *apUserData, const cHeadlessRequest &aReq, cHeadlessResponse &aResp)
{
	cSomaBase *pBase = (cSomaBase*)apUserData;
	if(pBase->GetCurrentWorld() == NULL) { aResp.SetError("no world loaded"); return; }
	iLight *pLight = pBase->GetCurrentWorld()->GetLight(aReq.GetString("name", ""));
	if(pLight == NULL) { aResp.SetError("no light with that name"); return; }
	pLight->SetVisible(aReq.GetBool("visible", true));
}

// A/B switches for cRenderSettings: occlusion_culling, ssao, shadows, edge_smooth, fxaa; fog is the world's.
static void cSomaBase_HeadlessCmd_SetRenderSetting(void *apUserData, const cHeadlessRequest &aReq, cHeadlessResponse &aResp)
{
	cSomaBase *pBase = (cSomaBase*)apUserData;
	if(pBase->GetCurrentViewport() == NULL) { aResp.SetError("no viewport yet"); return; }
	cRenderSettings *pSettings = pBase->GetCurrentViewport()->GetRenderSettings();

	tString sName = aReq.GetString("name", "");
	bool bValue = aReq.GetBool("value", true);
	if(sName == "occlusion_culling") pSettings->mbUseOcclusionCulling = bValue;
	else if(sName == "ssao") pSettings->mbSSAOActive = bValue;
	else if(sName == "shadows") pSettings->mbRenderShadows = bValue;
	else if(sName == "edge_smooth") pSettings->mbUseEdgeSmooth = bValue;
	else if(sName == "fxaa") pSettings->mbUseFxaa = bValue;
	else if(sName == "light_depth_cull") cRendererDeferred::SetDepthCullLights(bValue);
	else if(sName == "log") pSettings->mbLog = bValue;
	else if(sName == "shadow_cull") iRenderer::SetShadowCull(bValue);
	else if(sName == "shadow_depth_clamp") iRenderer::SetShadowDepthClamp(bValue);
	else if(sName == "decals" || sName == "illumination" || sName == "skybox" || sName == "translucent")
	{
		int lBit = sName == "decals" ? 1 : sName == "illumination" ? 2 : sName == "skybox" ? 4 : 8;
		cRendererDeferred::mlDebugSkipPasses = bValue ? (cRendererDeferred::mlDebugSkipPasses & ~lBit) : (cRendererDeferred::mlDebugSkipPasses | lBit);
	}
	else if(sName == "fog" && pBase->GetCurrentWorld()) pBase->GetCurrentWorld()->SetFogActive(bValue);
	else aResp.SetError("unknown setting '" + sName + "'");
}

// Raw G-buffer values under one pixel: target 0 color, 1 normal+depth, 2 specular.
static void cSomaBase_HeadlessCmd_Pick(void *apUserData, const cHeadlessRequest &aReq, cHeadlessResponse &aResp)
{
	cSomaBase *pBase = (cSomaBase*)apUserData;
	cRendererDeferred *pDeferred = static_cast<cRendererDeferred*>(pBase->mpEngine->GetGraphics()->GetRenderer(eRenderer_Main));
	if(pDeferred == NULL) { aResp.SetError("no deferred renderer yet"); return; }

	tString sOut = "[";
	static const int vTargets[] = {0, 1, 2, 4};
	for(int t=0; t<4; ++t)
	{
		int lTarget = vTargets[t];
		iTexture *pTex = pDeferred->GetDebugGBufferTexture(lTarget);
		std::vector<float> vPixels;
		if(pTex == NULL || pTex->GetRawPixelsRGBAFloat(vPixels) == false) { aResp.SetError("G-buffer target has no GPU data yet"); return; }

		tString sX = aReq.GetString("x", "0.5"), sY = aReq.GetString("y", "0.5");
		float fX = cString::ToFloat(sX.c_str(), 0), fY = cString::ToFloat(sY.c_str(), 0);
		if(sX.find('.') != tString::npos) fX *= pTex->GetWidth();
		if(sY.find('.') != tString::npos) fY *= pTex->GetHeight();
		int lX = cMath::Clamp((int)fX, 0, pTex->GetWidth()-1);
		int lY = cMath::Clamp((int)fY, 0, pTex->GetHeight()-1);
		// GL rows are bottom-up, request coordinates are top-down.
		size_t lIdx = ((size_t)(pTex->GetHeight()-1-lY) * pTex->GetWidth() + lX) * 4;

		if(lTarget>0) sOut += ",";
		sOut += "[";
		for(int c=0; c<4; ++c)
		{
			float fVal = vPixels[lIdx+c];
			if(c>0) sOut += ",";
			sOut += std::isnan(fVal) ? tString("null") : cString::ToString(fVal, 6, true);
		}
		sOut += "]";
	}
	aResp.SetRaw("gbuffer", sOut + "]");
}

//---------------------------------------

cSomaBase::cSomaBase()
{
	mpEngine = NULL;

	mpSplash = NULL;
	mpGammaScreen = NULL;

	mpTestWorld = NULL;
	mpDebugCamera = NULL;
	mpDebugViewport = NULL;
	mpDebugCameraController = NULL;
	mbUseRealPlayer = true;

	mpScriptRuntime = NULL;
	mpLuxMap = NULL;
	mpLuxUpdater = NULL;
	mpLuxGame = NULL;
}

void SomaReadUserScreenConfig(cSomaConfig *apCfg);
void SomaApplyWindowMode(const cSomaConfig *apCfg);

//-----------------------------------------------------------------------

cSomaBase::~cSomaBase()
{
}

//-----------------------------------------------------------------------

bool cSomaBase::Init(const tString &asCommandline)
{
	/////////////////////////////
	// Set the real log destination FIRST, before anything below that could
	// Log()/Error() - see SetupLogFile()'s own comment for why this can't
	// wait until InitEngine() (that used to be where this happened, and
	// InitMainConfig() below - which can genuinely fail and Error() on a
	// missing/malformed main_init.cfg - ran before it).
	SetupLogFile();

	/////////////////////////////
	// Parse the command line (an alternate init config file path, same
	// convention as cLuxBase::ParseCommandLine)
	if (ParseCommandLine(asCommandline) == false)
		return false;

	/////////////////////////////
	// Load SOMA's real main_init.cfg to get resource/material config paths
	// and the game name - unlike Amnesia's main_init.cfg, SOMA's has no
	// separate "main settings"/Menu/PreMenu/Demo config file entries, so
	// InitMainConfig() here only pulls out what Phase 0 actually needs.
	if (InitMainConfig() == false)
		return false;

	/////////////////////////////
	// Wire the HPSL->GLSL transpiler (soma/src/game/HpslTranspiler.cpp) into
	// cGpuShaderManager as its .glsl-not-found fallback, so SOMA's real
	// .hpsl material shaders get transpiled-and-compiled instead of just
	// erroring (see PORTING_NOTES.md "SOMA" section). HPL2/core can't call
	// TranspileHpslToGlsl() directly (that's game-module code), so this is
	// the only place in this codebase that calls SetHpslTranspileCallback -
	// Dark Descent/AMFP never do, so cGpuShaderManager's fallback path stays
	// dead code for them.
	//
	// MUST run before InitEngine(): CreateHPLEngine() (called from inside
	// InitEngine() below) constructs cGraphics, which in turn constructs
	// cRendererDeferred/cRendererSimple/the post-effect types/some material
	// types - several of which build GPU programs (deferred_base_vtx.glsl
	// and friends) directly in their own constructors, not lazily on first
	// use. When this call came after InitEngine(), every one of those
	// engine-init-time shader lookups saw mpHpslTranspileCallback still
	// NULL, so cGpuShaderManager::CreateShader()'s HPSL-fallback branch
	// never even triggered - straight to "Couldn't find file
	// 'deferred_base_vtx.glsl' in resources", permanently (those program
	// pointers are cached NULL for the object's lifetime, never retried).
	// Found live: a real headless boot of 00_01_apartment.hpm showed 14
	// "Couldn't find file 'deferred_base_vtx.glsl'" / 6 "Could not load
	// material ... shader 'deferred_base_vtx.glsl'" lines in hpl.log even
	// though most materials (loaded later, well after Init() returns and
	// the callback is registered) resolved through the same fallback fine.
	cGpuShaderManager::SetHpslTranspileCallback(TranspileHpslToGlsl);

	/////////////////////////////
	// Real SOMA's own .hpsl corpus (deferred_gbuffer_solid_frag.hpsl,
	// deferred_light_frag.hpsl, ...) always writes/reads the deferred
	// normal G-buffer raw and unconditionally stores linear (unpacked)
	// depth in its alpha channel - there is no HPL2-style "@ifdef
	// Deferred_32bit ... *0.5+0.5 ... @elseif Deferred_64bit ... raw ...
	// @endif" branch anywhere in the real shipped source (confirmed via a
	// full grep of the real corpus: every gl_FragData[1]/out_vNormal write
	// is unconditional). That's exactly what HPL2's existing
	// eDeferredGBuffer_64Bit renderer mode already means (see
	// RendererDeferred.cpp's own matching @ifdef Deferred_32bit/@elseif
	// Deferred_64bit branches in the real Dark Descent .glsl corpus) - but
	// cRendererDeferred::mGBufferType defaults to (and, with no call
	// anywhere in this codebase to ever change it, always stays)
	// eDeferredGBuffer_32Bit, a standard 8-bit-per-channel UNSIGNED G-buffer
	// texture. Writing SOMA's real raw signed (-1..1) normal components
	// into that unsigned target silently clamps every negative component
	// to 0 on write - corrupting the surface normal on nearly every pixel
	// in the game and starving real lights of any dot(N,L) contribution.
	// Root-caused live: a real New-Game boot into 00_01_apartment.hpm
	// rendered almost entirely black despite 38 real lights being
	// correctly culled into view near the camera - a LIGHTDIAG-SHADER
	// dump of the real transpiled deferred_gbuffer_solid_frag.glsl (with
	// the real Deferred_32bit=1 combo var actually set) showed the
	// unconditional, un-biased `gl_FragData[1].xyz = vScreenNormal;` this
	// comment describes. Must run before InitEngine() (which constructs
	// cRendererDeferred and allocates the G-buffer textures based on this
	// value) - same ordering constraint as SetHpslTranspileCallback()
	// above.
	cRendererDeferred::SetGBufferType(eDeferredGBuffer_64Bit);
	cRendererDeferred::SetGBufferTextureType(eTextureType_2D);
	cGraphics::SetTempFrameBufferTextureType(eTextureType_2D);
	cRendererDeferred::SetDepthInNormalAlpha(true);
	cMeshLoaderCollada::SetConvertUnitFromAnyTool(true);
	cMeshLoaderCollada::SetLoadVertexColors(true);
	cMeshLoaderCollada::SetUnscaledSkeleton(true);

	cRendererDeferred::SetShadowDistanceNone(1e6f);
	cImageManager::SetDefaultFrameSize(cVector2l(1024,1024));

	const char *pHdr = getenv("OPENHPL_SOMA_HDR");
	cEntityLoader_Object::SetSubMeshScaleIncludesModelScale(true);
	iLight::SetHpl3Visibility(true);
	cRendererDeferred::SetHpl3SSAO(true);
	if (pHdr == NULL || pHdr[0] != '0')
	{
		cRendererDeferred::SetHdr(true);
		SetHpslStripHdrBoost(false);
		cGpuShaderManager::AddGlobalDefine("UseLinearColorSpaceCorrection");
		cGpuShaderManager::AddGlobalDefine("LinearColorSpaceCorrectionType_Standard");
	}

	// cRendererDeferred::InitLightRendering() (RendererDeferred.cpp) attaches
	// a real GPU occlusion query (GetOcclusionQuery()) to any light whose
	// projected screen area exceeds mlMinLargeLightArea, then skips
	// re-rendering that light entirely on a later frame once the query's
	// sample count comes back at or below mlSampleVisiblilityLimit - a real
	// performance optimization (never re-shade a light that's fully hidden
	// behind other geometry). Root-caused live: a real New Game boot into
	// 00_01_apartment.hpm rendered near pure black (mean pixel value ~10/255)
	// despite 38 real lights correctly culled into view - a temporary
	// counter on cRenderSettings::mlNumberOfLightsRendered showed a real,
	// nonzero count (16, then 19) in the first couple of rendered frames,
	// then permanently 0 from then on. Every light in this small apartment
	// bedroom sits very close to the camera, so each has a large projected
	// screen area and takes this occlusion-query path - consistent with
	// queries reporting these lights as occluded/invisible once their
	// results are actually available (a 1-frame-later readback), which
	// would never happen for a real light this close to the camera. This
	// GPU/driver stack (Mesa on Apple Silicon, not what HPL2's occlusion
	// query path was ever verified against) is the plausible source, not
	// verified further than that. Disabling this test for SOMA (verified
	// live: mean pixel value rose from ~10/255 to ~26/255 and real
	// previously-invisible room geometry/detail became visible in a
	// screenshot at the identical camera pose) trades a real-but-broken
	// perf optimization for correct light visibility - Dark Descent/AMFP
	// are untouched by this (SOMA-only call, and mbOcclusionTestLargeLights
	// is a process-wide static, but each game module is its own process).
	cRendererDeferred::SetOcclusionTestLargeLights(false);

	// CRITICAL, must run before CreateHPLEngine() below, not merely before
	// map/resource loading: cMeshLoaderCollada's real, original behavior is
	// to treat a shipped `.msh` as a rebuildable cache of the `.dae` source,
	// recompiling and unconditionally SaveMesh()-ing over it. On this Linux
	// port that install directory is the real Steam depot, and this
	// (correctly, for everything loaded later) used to be disabled by a
	// call placed inside InitEngine() itself, right after CreateHPLEngine().
	// That was too late for one specific case: CreateHPLEngine() (below)
	// constructs cGraphics, which constructs cRendererDeferred, whose own
	// constructor immediately calls LoadVertexBufferFromMesh("core_box.dae",
	// ...) (and core_pyramid/core_*_sphere) to build its debug/light-volume
	// shapes - before InitEngine() ever returns, let alone reaches its own
	// old call site. Found live: a real (non-headless) SOMA launch just
	// rewrote exactly these 5 files' real `.msh` caches in the real Steam
	// install directory (confirmed via mtimes matching the launch, and
	// Steam's own "5 files failed to validate" integrity check reacting to
	// it) - the exact zero-tolerance class of bug already fixed once for
	// every other, later-loaded mesh, just never for these five, since
	// nothing reached this code path at all before this session's earlier
	// fix made core_box.dae loadable in the first place (previously the
	// engine just crashed here instead - see the FatalError fix earlier
	// this session). Hardcoded true, not config-driven, same reasoning as
	// this call's own original site.
	cResources::SetForceCacheLoadingAndSkipSaving(true);

	// Never next to the game data; also skips probing SOMA's own (HPL3-format) .msh files
	cResources::SetMeshCacheDir(cSomaFsb::GetCacheDir(_W("meshcache")));

	/////////////////////////////
	// Init the engine: create the window, load resources.cfg/materials.cfg,
	// and get to a state where an empty scene can be rendered.
	if (InitEngine() == false)
		return false;

	// Safe wherever this sits now - SetupLogFile() at the very top of Init()
	// already set a real, XDG-routed log destination before anything else
	// in this function could Log()/Error(). This used to need to sit after
	// InitEngine() specifically (which used to be the only place
	// SetLogFile() was called) - confirmed live, once: running a build with
	// this Log() call in its old spot (right after InitMainConfig(), before
	// SetLogFile() existed anywhere) from a scratch test directory that (per
	// this project's own established headless-testing pattern) symlinks
	// "hpl.log" back to the real install for tailing wrote this exact line
	// into the real Steam SOMA install's hpl.log - exactly what this
	// project has a zero-tolerance policy against.
	Log("SOMA game module - Phase 0 scaffolding (%s)\n", msGameName.c_str());

	/////////////////////////////
	// Headless control: register camera commands if a control server is
	// active (OPENHPL_HEADLESS_SOCKET) - see HeadlessControl.h.
	if (mpEngine->GetHeadlessControl())
	{
		cHeadlessControlServer *pCtrl = mpEngine->GetHeadlessControl();
		pCtrl->RegisterHandler("camera_state", cSomaBase_HeadlessCmd_CameraState, this);
		pCtrl->RegisterHandler("player_state", cSomaBase_HeadlessCmd_PlayerState, this);
		pCtrl->RegisterHandler("lux_entity", cSomaBase_HeadlessCmd_LuxEntity, this);
		pCtrl->RegisterHandler("script_exec", cSomaBase_HeadlessCmd_ScriptExec, this);
		pCtrl->RegisterHandler("script_vars", cSomaBase_HeadlessCmd_ScriptVars, this);
		pCtrl->RegisterHandler("imgui_stats", cSomaBase_HeadlessCmd_ImGuiStats, this);
		pCtrl->RegisterHandler("imgui_ops", cSomaBase_HeadlessCmd_ImGuiOps, this);
		pCtrl->RegisterHandler("imgui_cursor", cSomaBase_HeadlessCmd_ImGuiCursor, this);
		pCtrl->RegisterHandler("sound_stats", cSomaBase_HeadlessCmd_SoundStats, this);
		pCtrl->RegisterHandler("body_contacts", cSomaBase_HeadlessCmd_BodyContacts, this);
		pCtrl->RegisterHandler("raycast", cSomaBase_HeadlessCmd_Raycast, this);
		pCtrl->RegisterHandler("physics_stats", cSomaBase_HeadlessCmd_PhysicsStats, this);
		pCtrl->RegisterHandler("stub_report", cSomaBase_HeadlessCmd_StubReport, this);
		pCtrl->RegisterHandler("set_debug_gbuffer", cSomaBase_HeadlessCmd_SetDebugGbuffer, this);
		pCtrl->RegisterHandler("read_gbuffer_stats", cSomaBase_HeadlessCmd_ReadGbufferStats, this);
		pCtrl->RegisterHandler("dump_target", cSomaBase_HeadlessCmd_DumpTarget, this);
		pCtrl->RegisterHandler("translucents", cSomaBase_HeadlessCmd_Translucents, this);
		pCtrl->RegisterHandler("set_camera", cSomaBase_HeadlessCmd_SetCamera, this);
		pCtrl->RegisterHandler("start_map", cSomaBase_HeadlessCmd_StartMap, this);
		pCtrl->RegisterHandler("load_report", cSomaBase_HeadlessCmd_LoadReport, this);
		pCtrl->RegisterHandler("world_stats", cSomaBase_HeadlessCmd_WorldStats, this);
		pCtrl->RegisterHandler("render_stats", cSomaBase_HeadlessCmd_RenderStats, this);
		pCtrl->RegisterHandler("entity_info", cSomaBase_HeadlessCmd_EntityInfo, this);
		pCtrl->RegisterHandler("lights", cSomaBase_HeadlessCmd_Lights, this);
		pCtrl->RegisterHandler("pick", cSomaBase_HeadlessCmd_Pick, this);
		pCtrl->RegisterHandler("pick_entity", cSomaBase_HeadlessCmd_PickEntity, this);
		pCtrl->RegisterHandler("set_light", cSomaBase_HeadlessCmd_SetLight, this);
		pCtrl->RegisterHandler("set_render_setting", cSomaBase_HeadlessCmd_SetRenderSetting, this);
	}

	/////////////////////////////
	// One-shot HPSL->GLSL transpiler proof-of-concept - see
	// HpslTranspilerSelfTest.h. Not part of real rendering yet; just
	// proves whether the transpiled clear_vtx/clear_frag pair compiles as
	// real GLSL against the live GL context. Safe to run every boot: it
	// only reads shader files and compiles throwaway GL shader objects.
	RunHpslTranspilerSelfTest(mpEngine);

	mpEngine->GetUpdater()->AddGlobalUpdate(hplNew(cSomaToneMapping, ()));

	/////////////////////////////
	// Real boot sequence: show the splash logos, then (via
	// OnSplashFinished(), called back from cSomaSplash once its sequence
	// ends) load SOMA's own declared main menu scene. No map is loaded
	// synchronously here anymore - see SomaSplash.h/cpp.
	// Headless sweeps: no splash, no first-run gamma screen.
	if (getenv("OPENHPL_SOMA_SKIP_BOOT") != NULL)
	{
		ProceedPastBoot();
		return true;
	}

	mpSplash = hplNew(cSomaSplash, (mpEngine, this));
	mpEngine->GetUpdater()->AddGlobalUpdate(mpSplash);

	return true;
}

//-----------------------------------------------------------------------

void cSomaBase::OnSplashFinished()
{
	// Real SOMA only shows its gamma-calibration screen once, on a
	// completely fresh install (MenuHandler.hps's mbPremenuActive flag) -
	// see cSomaGammaScreen::ShouldShowAndMarkSeen() for how that's tracked
	// here. On every later boot this goes straight to ProceedPastBoot().
	if (cSomaGammaScreen::ShouldShowAndMarkSeen())
	{
		mpGammaScreen = hplNew(cSomaGammaScreen, (mpEngine, this));
		mpEngine->GetUpdater()->AddGlobalUpdate(mpGammaScreen);
		return;
	}

	ProceedPastBoot();
}

//-----------------------------------------------------------------------

void cSomaBase::OnGammaScreenFinished()
{
	ProceedPastBoot();
}

//-----------------------------------------------------------------------

void cSomaBase::ProceedPastBoot()
{
	const char *pTestMap = getenv("OPENHPL_SOMA_MAP");
	if (pTestMap != NULL && pTestMap[0] != '\0')
	{
		tString sError;
		const char *pStartPos = getenv("OPENHPL_SOMA_MAP_STARTPOS");
		if (LoadMap(pTestMap, cVector3f(0, 1.7f, 0), sError, pStartPos ? pStartPos : "*"))
			return;
		Log("SOMA: OPENHPL_SOMA_MAP='%s' failed to load (%s)\n", pTestMap, sError.c_str());
	}

	if (cSomaLuxMap::GetCurrent() == NULL)
		LoadScriptMainMenu();
}

//-----------------------------------------------------------------------

void cSomaBase::Exit()
{
	if (mpEngine)
		DestroyHPLEngine(mpEngine);
	mpEngine = NULL;
}

//-----------------------------------------------------------------------

void cSomaBase::Run()
{
	mpEngine->Run();
}

//-----------------------------------------------------------------------

bool cSomaBase::ParseCommandLine(const tString &asCommandline)
{
	msInitConfigFile = cString::To16Char(asCommandline);
	if (msInitConfigFile == _W(""))
		msInitConfigFile = _W("config/main_init.cfg");

	return true;
}

//-----------------------------------------------------------------------

bool cSomaBase::InitMainConfig()
{
	cConfigFile *pInitCfg = hplNew(cConfigFile, (msInitConfigFile));
	if (pInitCfg->Load() == false)
	{
		msErrorMessage = _W("Could not load main init file: ") + msInitConfigFile;
		hplDelete(pInitCfg);
		return false;
	}

	msResourceConfigPath = pInitCfg->GetString("ConfigFiles", "Resources", "resources.cfg");
	msMaterialConfigPath = pInitCfg->GetString("ConfigFiles", "Materials", "materials.cfg");
	msGameName = pInitCfg->GetString("Variables", "GameName", "SOMA");

	hplDelete(pInitCfg);

	return true;
}

//-----------------------------------------------------------------------

// Split out of InitEngine() and called first thing from Init(), before
// InitMainConfig() - InitMainConfig()'s cConfigFile::Load() calls Error()
// on a missing/malformed main_init.cfg, which (like cSomaConfig::Load()'s
// own Log() call, see the comment below) needs a real log destination
// already set up to avoid falling back to the engine's pre-SetLogFile()
// default: a bare relative "hpl.log" resolved against cwd, which for a
// real Steam launch (or a headless-check.sh run against a real install -
// see that script's own guard, added for exactly this reason) is the real
// Steam install directory. This was a real, confirmed-live gap: this one
// call site was missed when the equivalent pre-SetLogFile() ordering bug
// was fixed for cSomaConfig::Load()/the old "Phase 0 scaffolding" Log()
// call (see PORTING_NOTES.md).
void cSomaBase::SetupLogFile()
{
#if defined(__linux__)
	// hpl.log otherwise defaults to a bare relative "hpl.log" (see
	// LowLevelSystemSDL.cpp), landing wherever cwd happens to be at first
	// Log() - the real game's Steam install directory, since that's where
	// this binary gets deployed and run from. XDG_STATE_HOME is the
	// correct home for transient log/state data (see amnesia/src/game/
	// LuxBasePersonal.h's equivalent for the real Amnesia game module).
	tWString sStateRoot = cPlatform::GetSystemSpecialPath(eSystemPath_XDGStateHome);
	tWString sStateDir = sStateRoot + _W("open-hpl/");
	if(cPlatform::FolderExists(sStateDir) == false) cPlatform::CreateFolder(sStateDir);
	sStateDir += _W("soma/");
	if(cPlatform::FolderExists(sStateDir) == false) cPlatform::CreateFolder(sStateDir);

	// A fixed hpl.log path collides across concurrent headless test runs
	// (now a normal occurrence with multiple agents each testing their own
	// Soma.<branch>.aarch64 build) - cLogWriter::ReopenFile() truncates on
	// open, so a second process launched while a first is still running
	// silently wipes whatever the first had already logged. Suffix with the
	// PID under OPENHPL_HEADLESS_SOCKET only, so normal interactive play
	// keeps the stable, predictable filename.
	tWString sLogFile = sStateDir + _W("hpl.log");
	if(getenv("OPENHPL_HEADLESS_SOCKET") != NULL)
	{
		sLogFile = sStateDir + _W("hpl-") + cString::ToStringW((int)getpid()) + _W(".log");
	}
	SetLogFile(sLogFile);
#endif
}

//-----------------------------------------------------------------------

bool cSomaBase::InitEngine()
{
	const char *pFreeCam = getenv("OPENHPL_SOMA_FREECAM");
	mbUseRealPlayer = pFreeCam == NULL || pFreeCam[0] == 0 || strcmp(pFreeCam, "0") == 0;

	cEngineInitVars vars;
	// FMOD virtualises voices past its 64 (MaxVirtualChannels=1000); OpenAL fails instead
	vars.mSound.mlMaxChannels = 128;
	vars.mSound.mbUseEnvironmentalAudio = true;
	vars.mGraphics.msWindowCaption = msGameName;

	// Load persisted settings (see SomaConfig.h) - deliberately AFTER
	// SetLogFile() above: cConfigFile::Load()/cSomaConfig::Load() both Log()
	// on a missing/fresh-install config file (the common case), and doing
	// this any earlier sends that Log() to the engine's pre-SetLogFile()
	// default destination - a bare relative "hpl.log" in whatever the
	// process's cwd happens to be (see the comment above). A real headless
	// test run from a scratch directory containing an "hpl.log" symlink
	// (this project's own established pattern, e.g. for tailing it via the
	// headless control socket) turned that into a real, confirmed write
	// into the actual Steam install directory the very first time this bug
	// existed - exactly what this project has a zero-tolerance policy
	// against. mbFullscreen only takes effect at window-creation time
	// (cLowLevelGraphics::Init()'s abFullscreen param), so it has to be
	// read back and applied to vars here, before CreateHPLEngine() below -
	// unlike Gamma/Volume/VSync, applied live further down once cGraphics/
	// cSound exist.
	mConfig.Load();
	SomaReadUserScreenConfig(&mConfig);
	vars.mGraphics.mbFullscreen = mConfig.mbFullscreen;

	// Real Resolution row's own restart-required contract - see
	// SomaConfig.h's mlScreenWidth/mlScreenHeight comment. Was hardcoded
	// 1280x720 here before those fields existed.
	vars.mGraphics.mvScreenSize = cVector2l(mConfig.mlScreenWidth, mConfig.mlScreenHeight);

	// TEMP DEBUG ONLY - not for commit: lets a large windowed boot size be
	// tested headlessly (a hidden window never picks up real monitor
	// dimensions for SDL_WINDOW_FULLSCREEN_DESKTOP, so this is the only way
	// to reproduce "booted directly at a large real resolution" headlessly).
	// Deliberately AFTER the config-driven default above so this always wins
	// over a persisted Resolution setting for debugging.
	if (getenv("OPENHPL_SOMA_DEBUG_SCREENSIZE"))
	{
		int lW = 1280, lH = 720;
		sscanf(getenv("OPENHPL_SOMA_DEBUG_SCREENSIZE"), "%dx%d", &lW, &lH);
		vars.mGraphics.mvScreenSize = cVector2l(lW, lH);
	}

	/////////////////////////
	// Create the engine
	mpEngine = CreateHPLEngine(eHplAPI_OpenGL, eHplSetup_All, &vars);
	if (mpEngine == NULL)
	{
		msErrorMessage = _W("Could not create HPL engine!");
		return false;
	}
	SomaApplyWindowMode(&mConfig);

	/////////////////////////
	// Load SOMA's real resource directory listing and physics surface data.
	// Both parsers are fully generic (no Amnesia-specific assumptions), so
	// SOMA's own files load unmodified.
	mpEngine->GetResources()->LoadResourceDirsFile(msResourceConfigPath);
	mpEngine->GetPhysics()->LoadSurfaceData(msMaterialConfigPath);

	// See SomaLoaders.h - without these, cWorldLoaderHpm silently drops
	// every <Entity>/<Area> element in a real SOMA map (confirmed via a real
	// boot log against real game data).
	RegisterSomaLoaders(mpEngine->GetResources());

	const char *pScripts = getenv("OPENHPL_SOMA_SCRIPTS");
	if (pScripts == NULL || strcmp(pScripts, "0") != 0)
	{
		mpScriptRuntime = hplNew(cSomaScriptRuntime, ());
		if (mpScriptRuntime->Init(cString::To8Char(cPlatform::GetWorkingDir())))
		{
			mpLuxGame = new cSomaLuxGame(mpScriptRuntime);
			mpLuxGame->Load();
			mpLuxUpdater = hplNew(cSomaLuxUpdater, ());
			mpEngine->GetUpdater()->AddGlobalUpdate(mpLuxUpdater);
		}
		else
		{
			hplDelete(mpScriptRuntime);
			mpScriptRuntime = NULL;
		}
	}

	/////////////////////////
	// Apply the persisted settings that DO have a live/runtime API (unlike
	// Fullscreen above, which only applies at the next InitEngine()) - same
	// APIs amnesia/src/game/LuxMainMenu_Options.cpp's own Options menu uses
	// for these.
	mpEngine->GetSound()->GetLowLevel()->SetVolume(mConfig.mfMasterVolume);
	mpEngine->GetGraphics()->GetLowLevel()->SetGammaCorrection(mConfig.mfGamma);
	mpEngine->GetGraphics()->GetLowLevel()->SetVsyncActive(mConfig.mbVSync, false);

	return true;
}

//-----------------------------------------------------------------------

void cSomaBase::LoadScriptMainMenu()
{
	tString sError, sMenu = GetInitConfigString("MainMenu", "File");
	if (LoadMap(sMenu.empty() ? "main_menu.hpm" : sMenu, cVector3f(0), sError, "*") == false)
		Error("SOMA: main menu failed (%s)\n", sError.c_str());
}

tString cSomaBase::GetInitConfigString(const tString &asLevel, const tString &asName)
{
	cConfigFile cfg(msInitConfigFile);
	return cfg.Load() ? cfg.GetString(asLevel, asName, "") : "";
}

//-----------------------------------------------------------------------

bool cSomaBase::LoadMap(const tString &asMapFile, const cVector3f &avStartPos, tString &asErrorOut,
						 const tString &asStartPosName)
{
	if (mfGameStartTime < 0)
		mfGameStartTime = mpEngine->GetGameTime();
	cWorld *pNewWorld = mpEngine->GetScene()->LoadWorld(asMapFile, 0);
	if (pNewWorld == NULL)
	{
		asErrorOut = "Could not load map '" + asMapFile + "'";
		return false;
	}

	if (mpLuxMap)
	{
		mpLuxMap->OnLeave();
		if (mpLuxGame)
			mpLuxGame->LeaveMap(mpLuxMap);
		hplDelete(mpLuxMap);
		mpLuxMap = NULL;
	}

	if (mpTestWorld) mpEngine->GetScene()->DestroyWorld(mpTestWorld);
	mpTestWorld = pNewWorld;
	cSomaToneMapping::Get()->OnMapLoaded(mpTestWorld);

	// cUpdater has no remove, so the camera, viewport and controller are reused
	if (mpDebugCamera == NULL)
	{
		mpDebugCamera = mpEngine->GetScene()->CreateCamera(eCameraMoveMode_Fly);
		mpDebugCamera->SetFarClipPlane(200.0f);
		mpDebugViewport = mpEngine->GetScene()->CreateViewport(mpDebugCamera, mpTestWorld, true);
		mpEngine->GetScene()->SetCurrentListener(mpDebugViewport);
	}
	else
	{
		mpDebugViewport->SetWorld(mpTestWorld);
	}

	// Each new viewport's cRenderSettings starts with FXAA off
	mpDebugViewport->GetRenderSettings()->mbUseFxaa = mConfig.mbAntiAliasing;
	// CHC occlusion culling reads queries back synchronously: 0.1 fps and everything culled on AGX
	mpDebugViewport->GetRenderSettings()->mbUseOcclusionCulling = false;

	cSomaLuxPlayer *pPlayer = cSomaLuxPlayer::Get();
	if (mbUseRealPlayer)
	{
		if (pPlayer)
			pPlayer->SetCamera(mpDebugCamera);
	}
	else
	{
		if (mpDebugCameraController == NULL)
		{
			mpDebugCameraController = hplNew(cSomaDebugFreeCamera, (mpDebugCamera, mpEngine->GetInput()));
			mpEngine->GetUpdater()->AddGlobalUpdate(mpDebugCameraController);
		}
		// The player script still needs a camera; this one is never rendered
		if (pPlayer && pPlayer->GetCamera() == NULL)
			pPlayer->SetCamera(mpEngine->GetScene()->CreateCamera(eCameraMoveMode_Fly));
	}

	if (pNewWorld->GetPhysicsWorld())
	{
		// Doors and drawers are held by their joints; contacts with the static frame they are
		// mounted in and what rests against it would pin them with friction
		static cSomaJointFrameFilter gFrameFilter;
		gFrameFilter.mmapIgnored.clear();
		std::vector<iPhysicsBody*> vStatic;
		cPhysicsBodyIterator staticIt = pNewWorld->GetPhysicsWorld()->GetBodyIterator();
		while (staticIt.HasNext())
		{
			iPhysicsBody *pBody = staticIt.Next();
			if (pBody->GetMass() <= 0 && pBody->GetCollide()) vStatic.push_back(pBody);
		}
		cPhysicsJointIterator frameIt = pNewWorld->GetPhysicsWorld()->GetJointIterator();
		while (frameIt.HasNext())
		{
			iPhysicsJoint *pJoint = frameIt.Next();
			iPhysicsBody *pChild = pJoint->GetChildBody();
			if (pChild == NULL || pChild->GetMass() <= 0 || (pJoint->GetParentBody() && pJoint->GetParentBody()->GetMass() > 0))
				continue;
			cBoundingVolume *pBV = pChild->GetBoundingVolume();
			cVector3f vMin = pBV->GetMin() - 0.02f, vMax = pBV->GetMax() + 0.02f;
			for (iPhysicsBody *pStatic : vStatic)
			{
				cBoundingVolume *pOther = pStatic->GetBoundingVolume();
				if (cMath::CheckAABBIntersection(vMin, vMax, pOther->GetMin(), pOther->GetMax()))
					gFrameFilter.mmapIgnored[pChild].insert(pStatic);
			}
			if (gFrameFilter.mmapIgnored.count(pChild))
				pChild->AddBodyCallback(&gFrameFilter);
		}

		// Everything is authored at rest; Newton wakes a body again on contact.
		cPhysicsBodyIterator bodyIt = pNewWorld->GetPhysicsWorld()->GetBodyIterator();
		while (bodyIt.HasNext())
		{
			iPhysicsBody *pBody = bodyIt.Next();
			if (pBody->GetMass() > 0) pBody->Sleep();
		}
	}

	cVector3f vAreaPos = avStartPos;
	float fAreaYaw = 0;
	bool bFoundArea = false;
	tString sStartName;
	if (asStartPosName != "")
	{
		// "*" = the map's first PlayerStart area
		cStartPosEntity *pStartPos = asStartPosName == "*" ? NULL : pNewWorld->GetStartPosEntity(asStartPosName);
		if (pStartPos == NULL)
		{
			if (asStartPosName != "*")
				Log("SOMA: map '%s' has no PlayerStart Area named '%s', using the first\n", asMapFile.c_str(), asStartPosName.c_str());
			pStartPos = pNewWorld->GetFirstStartPosEntity();
		}
		if (pStartPos)
		{
			vAreaPos = pStartPos->GetWorldMatrix().GetTranslation();
			fAreaYaw = SomaStartYaw(pStartPos->GetWorldMatrix());
			bFoundArea = true;
			sStartName = pStartPos->GetName();
		}
	}

	cVector3f vCamPos = bFoundArea ? (vAreaPos + cVector3f(0, 0.5f, 0)) : avStartPos;
	mpDebugCamera->SetPosition(vCamPos);
	mpDebugCamera->SetPitch(0);
	mpDebugCamera->SetYaw(fAreaYaw);

	if (mpScriptRuntime)
	{
		tWString sHpm = mpEngine->GetResources()->GetFileSearcher()->GetFilePath(asMapFile);
		mpLuxMap = hplNew(cSomaLuxMap, (mpTestWorld, asMapFile));
		cSomaLuxMap::SetCurrent(mpLuxMap);
		if (sHpm != _W("") && mpLuxMap->CreateScript(mpScriptRuntime, cString::To8Char(cString::SetFileExtW(sHpm, _W("hps")))))
		{
			if (mpLuxGame)
			{
				mpLuxGame->PreloadData(mpLuxMap);
				mpLuxGame->EnterMap(mpLuxMap);
				if (pPlayer && mbUseRealPlayer)
					pPlayer->PlaceAtStart(vAreaPos, fAreaYaw, SomaStartPosCrouching(sStartName));
			}
			cSomaSaveHandler::OnMapEnter(asMapFile, asStartPosName);
			cSomaSaveHandler::ApplyPendingState();
			bool bFirstTime = msetVisitedMaps.insert(asMapFile).second;
			mpLuxMap->OnEnter(bFirstTime);
		}
		mpScriptRuntime->LogStubReport(40);
	}

	return true;
}
