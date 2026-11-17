/*
 * Copyright © 2009-2020 Frictional Games
 *
 * This file is part of Amnesia: The Dark Descent.
 *
 * Amnesia: The Dark Descent is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * Amnesia: The Dark Descent is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Amnesia: The Dark Descent.  If not, see <https://www.gnu.org/licenses/>.
 */

// Engine side of the debug handler (rust/src/luxbase/debug_ui.rs): widgets are ids, container
// nodes and renderables are pointers; the op numbers are `dop` there.

#include "LuxDebugHandler.h"
#include "LuxInputHost.h"
#include "LuxA12dDump.h"
#include "RustLuxBase.h"

#include "LuxMap.h"
#include "LuxAreaNodes.h"
#include "LuxPlayer.h"
#include "LuxPlayerHands.h"
#include "LuxPlayerHelpers.h"
#include "LuxMapHandler.h"
#include "LuxInputHandler.h"
#include "LuxMoveState_Normal.h"
#include "LuxPlayerState.h"
#include "LuxEntity.h"
#include "LuxConfigHandler.h"
#include "LuxSavedGame.h"
#include "LuxInsanityHandler.h"
#include "LuxHelpFuncs.h"
#include "LuxInventory.h"
#include "LuxGlobalDataHandler.h"
#include "LuxLoadScreenHandler.h"

#include "scene/RenderableContainer_DynBoxTree.h"

#include <stdarg.h>

enum
{
	eDbgOp_GuiInit = 200, eDbgOp_GuiStart, eDbgOp_WNew, eDbgOp_WChecked, eDbgOp_WUv, eDbgOp_WCb, eDbgOp_WSize, eDbgOp_WSlider,
	eDbgOp_WComboAdd, eDbgOp_WComboClear, eDbgOp_WComboSel, eDbgOp_WComboGet, eDbgOp_WComboText, eDbgOp_WText, eDbgOp_WVis,
	eDbgOp_WEn, eDbgOp_WDestroy, eDbgOp_WFont, eDbgOp_GuiVirtW, eDbgOp_FontLen, eDbgOp_Picker, eDbgOp_Picked, eDbgOp_SetActive,
	eDbgOp_GuiFocus, eDbgOp_PickSet, eDbgOp_CfgDebug,
	eDbgOp_InState = 230, eDbgOp_Lock, eDbgOp_Speed, eDbgOp_FreeCam, eDbgOp_FreeCamSpeed, eDbgOp_OcclSet, eDbgOp_OcclGet,
	eDbgOp_ResLogSet, eDbgOp_ResLogGet, eDbgOp_FastPhysSet, eDbgOp_FastPhysGet, eDbgOp_InsCount, eDbgOp_InsName, eDbgOp_InsReload,
	eDbgOp_InsStart, eDbgOp_StartCount, eDbgOp_StartName, eDbgOp_Place,
	eDbgOp_ResReload = 250, eDbgOp_Transl, eDbgOp_Recompile, eDbgOp_MusicStop, eDbgOp_PlayerV, eDbgOp_PlayerSet, eDbgOp_MapFile,
	eDbgOp_BaseReset, eDbgOp_DestroyUnused, eDbgOp_StartGame, eDbgOp_CacheNew, eDbgOp_CacheFree, eDbgOp_MenuScreen, eDbgOp_MapSave,
	eDbgOp_MapDestroy, eDbgOp_SavedRoundtrip, eDbgOp_MapLoad, eDbgOp_MapSetCur, eDbgOp_MapLoadSaved, eDbgOp_MapRun, eDbgOp_ReadFile,
	eDbgOp_ContRoot = 280, eDbgOp_DynRebuild, eDbgOp_NodeUpdate, eDbgOp_NodeParent, eDbgOp_NodeBound, eDbgOp_NodeNChild,
	eDbgOp_NodeChild, eDbgOp_NodeNObj, eDbgOp_NodeObj,
	eDbgOp_ObjIsSub = 290, eDbgOp_ObjName, eDbgOp_ObjEType, eDbgOp_ObjBound, eDbgOp_LineBv, eDbgOp_ObjBvHit, eDbgOp_ObjLineTri,
	eDbgOp_MeshName, eDbgOp_MeshMat, eDbgOp_MeshTex, eDbgOp_RenderWire, eDbgOp_RenderPhys, eDbgOp_TexPath,
	eDbgOp_DrawFmt = 305, eDbgOp_DrawText, eDbgOp_ScreenH,
	eDbgOp_Q = 310, eDbgOp_QS, eDbgOp_StateDraw, eDbgOp_EntSnap, eDbgOp_EntActive, eDbgOp_EntDraw, eDbgOp_SndSnap, eDbgOp_SndV, eDbgOp_SndS,
	eDbgOp_Log = 320, eDbgOp_LogCb
};

static std::vector<iWidget*> gvWidgets;
static cGui *gpGui = NULL;
static cGuiSkin *gpSkin = NULL;
static cGuiSet *gpSet = NULL;
static tWStringVec gvPicked;
static cRendererCallbackFunctions *gpFunctions = NULL;
static std::vector<cLuxModelCache*> gvCaches;
static cLuxMap *gpTestMap = NULL;
static cLuxSavedGameMapCollection *gpSavedMaps = NULL;
static cBoundingVolume *gpLineBV = NULL;
static tString gsRecompile;
static cAnimationState *gpAnim = NULL;
static cMusicEntry *gpMusic = NULL;
static iRenderableContainerNode *gpKidNode = NULL, *gpObjNode = NULL;
static std::vector<iRenderableContainerNode*> gvKids;
static std::vector<iRenderable*> gvObjs;
static std::vector<iLuxEntity*> gvEnts;
static std::vector<cSoundEntry*> gvSounds;

static cLuxDebugHandler *Handler() { return gpBase->mpDebugHandler; }
static iWidget *Widget(int64_t alId) { return alId >= 0 && alId < (int64_t)gvWidgets.size() ? gvWidgets[alId] : NULL; }

static bool WidgetCb(void *apObj, iWidget *apWidget, const cGuiMessageData &aData)
{
	ohpl_dbg_event(Handler()->mpRust, (int)(intptr_t)apWidget->GetUserData() - 1, apWidget->GetUserValue(), aData.mlVal);
	return true;
}

static bool PickWorldCb(void *apObj, iWidget *apWidget, const cGuiMessageData &aData)
{
	ohpl_dbg_event(Handler()->mpRust, -1, 0, 0);
	return true;
}

static bool PickBatchCb(void *apObj, iWidget *apWidget, const cGuiMessageData &aData)
{
	ohpl_dbg_event(Handler()->mpRust, -2, 0, 0);
	return true;
}

static void LogMessageCallback(eLogOutputType aType, const char* asMessage)
{
	if (aType == eLogOutputType_Normal) return;

	gpBase->mpDebugHandler->AddErrorOrWarningMessage(cString::To16Char(asMessage));
}

static cVector3f V3(const OhplGa9Arg *a, int i) { return cVector3f(a[i].f, a[i + 1].f, a[i + 2].f); }

static void RetWide(OhplGa9Ret *r, const tString &asText) { OhplGa9RetW(r, cString::To16Char(asText)); }

static iRenderableContainer *Container(int alWhich)
{
	cWorld *pWorld = gpBase->mpMapHandler->GetCurrentMap()->GetWorld();
	return pWorld->GetRenderableContainer(alWhich == 0 ? eWorldContainerType_Dynamic : eWorldContainerType_Static);
}

static iWidget *NewWidget(const OhplGa9Arg *a)
{
	cVector3f vPos = V3(a, 1);
	cVector2f vSize(a[4].f, a[5].f);
	iWidget *pParent = Widget(a[6].i);
	tWString sText = OhplGa9GetW(a[7]);
	int lExtra = (int)a[8].i;
	switch((int)a[0].i)
	{
	case 0: return gpSet->CreateWidgetWindow(0, vPos, vSize, sText);
	case 1: return gpSet->CreateWidgetFrame(vPos, vSize, (lExtra & 1) != 0, pParent, (lExtra & 2) != 0, (lExtra & 4) != 0);
	case 2: return gpSet->CreateWidgetGroup(vPos, vSize, sText, pParent);
	case 3: return gpSet->CreateWidgetCheckBox(vPos, vSize, sText, pParent);
	case 4: return gpSet->CreateWidgetButton(vPos, vSize, sText, pParent);
	case 5: return gpSet->CreateWidgetComboBox(vPos, vSize, sText, pParent);
	case 6: return gpSet->CreateWidgetSlider(eWidgetSliderOrientation_Horizontal, vPos, vSize, lExtra, pParent, cString::To8Char(sText));
	}
	return gpSet->CreateWidgetLabel(vPos, vSize, sText, pParent);
}

static float PlayerVal(int alK)
{
	cLuxPlayer *pPlayer = gpBase->mpPlayer;
	iCharacterBody *pBody = pPlayer->GetCharacterBody();
	cCamera *pCam = pPlayer->GetCamera();
	switch(alK)
	{
	case 60: return pCam->GetPosition().x;
	case 61: return pCam->GetPosition().y;
	case 62: return pCam->GetPosition().z;
	case 63: return pCam->GetForward().x;
	case 64: return pCam->GetForward().y;
	case 65: return pCam->GetForward().z;
	case 70: return pBody->GetPosition().x;
	case 71: return pBody->GetPosition().y;
	case 72: return pBody->GetPosition().z;
	case 73: return pCam->GetPitch();
	case 74: return pCam->GetYaw();
	}
	return 0;
}

static double Query(int alK)
{
	cLuxPlayer *pPlayer = gpBase->mpPlayer;
	iCharacterBody *pBody = pPlayer->GetCharacterBody();
	if(alK >= 60 && alK <= 74) return PlayerVal(alK);
	switch(alK)
	{
	case 0: return pBody->IsOnGround();
	case 1: return pBody->GetMovedLastUpdate();
	case 2: return pBody->GetMass();
	case 3: return pBody->GetActiveSize();
	case 4: return pBody->GetMoveSpeed(eCharDir_Forward);
	case 5: return pBody->GetMoveSpeed(eCharDir_Right);
	case 6: return pBody->GetVelocity(gpBase->mpEngine->GetStepSize()).Length();
	case 7: return pPlayer->GetInteractionMoveSpeedMul();
	case 8: return pPlayer->GetAvgSpeed();
	case 9: return pBody->IsClimbing();
	case 10: return pPlayer->GetCurrentMoveState() == eLuxMoveState_Normal;
	case 11: return static_cast<cLuxMoveState_Normal*>(pPlayer->GetCurrentMoveStateData())->IsCrouching();
	case 12: return static_cast<cLuxMoveState_Normal*>(pPlayer->GetCurrentMoveStateData())->IsRunning();
	case 13: return static_cast<cLuxMoveState_Normal*>(pPlayer->GetCurrentMoveStateData())->GetMoveSpeedMul();
	case 14: return static_cast<cLuxMoveState_Normal*>(pPlayer->GetCurrentMoveStateData())->GetRunSpeedMul();
	case 15: return pPlayer->GetHelperLightLevel()->GetExtendedLightLevel();
	case 16: return pPlayer->GetHelperLightLevel()->GetNormalLightLevel();
	case 17: return pPlayer->GetHealth();
	case 18: return pPlayer->GetTerror();
	case 19: return pPlayer->GetLampOil();
	case 20: return pPlayer->GetSanity();
	case 21: return pPlayer->GetTinderboxes();
	case 22: return pPlayer->GetCurrentState();
	case 23:
		gpAnim = pPlayer->GetHands()->GetHandsEntity()->GetAnimationStateFromName(pPlayer->GetHands()->GetCurrentAnimation());
		return gpAnim != NULL;
	case 24: return gpAnim->GetTimePosition();
	case 25: return gpAnim->GetRelativeTimePosition();
	case 26: return static_cast<cLuxMoveState_Normal*>(pPlayer->GetMoveStateData(eLuxMoveState_Normal))->GetHeadBobCount();
	case 27: return gpBase->mpInsanityHandler->GetNewEventCount();
	case 28: return pPlayer->GetHelperSanity()->GetAtLowSanityCount();
	case 29: return gpBase->mbHardMode;
	case 40: return gpBase->mpEngine->GetAvgFrameTimeInMS();
	case 41: return gpBase->mpEngine->GetFPS();
	case 50:
		gpMusic = gpBase->mpEngine->GetSound()->GetMusicHandler()->GetCurrentSong();
		return gpMusic != NULL;
	case 51: return gpMusic->mpStream->GetVolume();
	case 52: return gpMusic->mpStream->IsPlaying();
	case 53: return gpMusic->mpStream->GetPriority();
	case 54: return gpMusic->mpStream->GetElapsedTime();
	case 55: return gpMusic->mpStream->GetTotalTime();
	case 56: return gpMusic->mpStream->IsBufferUnderrun();
	}
	return 0;
}

static void QueryS(int alK, OhplGa9Ret *r)
{
	cLuxPlayer *pPlayer = gpBase->mpPlayer;
	iCharacterBody *pBody = pPlayer->GetCharacterBody();
	switch(alK)
	{
	case 0: RetWide(r, pBody->GetPosition().ToString()); break;
	case 1: RetWide(r, pBody->GetForceVelocity().ToString()); break;
	case 2: RetWide(r, pPlayer->GetHands()->GetCurrentAnimation()); break;
	case 50: RetWide(r, gpMusic->mpStream->GetData()->GetName()); break;
	}
}

static void DrawLine(const OhplGa9Arg *a, bool abFormat)
{
	cVector3f vPos = V3(a, 0);
	tWString sText = OhplGa9GetW(a[3]);
	OhplA12dDrawLog(vPos, sText.c_str());
	if(abFormat) gpBase->mpGameDebugSet->DrawFont(gpBase->mpDefaultFont, vPos, 14, cColor(1,1), _W("%ls"), sText.c_str());
	else gpBase->mpGameDebugSet->DrawFont(sText, gpBase->mpDefaultFont, vPos, 14, cColor(1,1));
}

static void SoundOp(int alOp, const OhplGa9Arg *a, OhplGa9Ret *r)
{
	switch(alOp)
	{
	case eDbgOp_SndSnap:
	{
		gvSounds.clear();
		cSoundHandler *pHandler = gpBase->mpEngine->GetSound()->GetSoundHandler();
		for(size_t i=0; i<pHandler->GetEntryNum(); ++i) gvSounds.push_back(pHandler->GetEntryAt(i));
		r->i = (int64_t)gvSounds.size();
		break;
	}
	case eDbgOp_SndV:
	{
		iSoundChannel *pChannel = gvSounds[a[0].i]->GetChannel();
		switch((int)a[1].i)
		{
		case 0: r->d = pChannel->GetData()->IsStream(); break;
		case 1: r->d = pChannel->IsBufferUnderrun(); break;
		case 2: r->d = pChannel->GetVolume(); break;
		case 3: r->d = pChannel->GetPriority(); break;
		case 4: r->d = pChannel->GetElapsedTime(); break;
		case 5: r->d = pChannel->GetTotalTime(); break;
		}
		break;
	}
	case eDbgOp_SndS: RetWide(r, gvSounds[a[0].i]->GetChannel()->GetData()->GetName()); break;
	}
}

static void MeshOp(int alOp, const OhplGa9Arg *a, OhplGa9Ret *r)
{
	cSubMeshEntity *pMesh = (cSubMeshEntity*)a[0].i;
	cMaterial *pMat = pMesh->GetMaterial();
	switch(alOp)
	{
	case eDbgOp_MeshName: OhplGa9RetS(r, pMesh->GetName()); break;
	case eDbgOp_MeshMat: OhplGa9RetW(r, pMat->GetFullPath()); break;
	case eDbgOp_TexPath: OhplGa9RetW(r, pMat->GetTexture((eMaterialTexture)a[1].i)->GetFullPath()); break;
	case eDbgOp_MeshTex:
	{
		if(a[1].i < 0) { r->i = eMaterialTexture_LastEnum; break; }
		iTexture *pTex = pMat->GetTexture((eMaterialTexture)a[1].i);
		switch((int)a[2].i)
		{
		case 0: r->i = pTex != NULL; break;
		case 1: r->i = pTex->GetSize().x; break;
		case 2: r->i = pTex->GetSize().y; break;
		case 3: r->i = pTex->GetSize().z; break;
		case 4: r->f = ((float)pTex->GetMemorySize()) / (1024*1024); break;
		case 5: r->i = (int)pTex->GetPixelFormat(); break;
		}
		break;
	}
	case eDbgOp_RenderWire:
		gpFunctions->SetBlendMode(eMaterialBlendMode_None);
		gpFunctions->SetDepthTest(true);
		gpFunctions->SetDepthWrite(false);

		gpFunctions->SetMatrix(pMesh->GetModelMatrix(NULL));
		gpFunctions->DrawWireFrame(pMesh->GetVertexBuffer(), cColor(1,1,1,1));
		gpFunctions->SetMatrix(NULL);
		break;
	}
}

static void ContainerOp(int alOp, const OhplGa9Arg *a, OhplGa9Ret *r)
{
	switch(alOp)
	{
	case eDbgOp_ContRoot:
	{
		iRenderableContainer *pContainer = Container((int)a[0].i);
		pContainer->UpdateBeforeRendering();
		r->i = (int64_t)pContainer->GetRoot();
		break;
	}
	case eDbgOp_DynRebuild: static_cast<cRenderableContainer_DynBoxTree*>(Container(0))->RebuildNodes(); break;
	case eDbgOp_LineBv:
		if(gpLineBV == NULL) gpLineBV = new cBoundingVolume();
		gpLineBV->SetLocalMinMax(V3(a, 0), V3(a, 3));
		break;
	}
	if(alOp < eDbgOp_NodeUpdate || alOp > eDbgOp_NodeObj) return;
	iRenderableContainerNode *pNode = (iRenderableContainerNode*)a[0].i;
	switch(alOp)
	{
	case eDbgOp_NodeUpdate: pNode->UpdateBeforeUse(); break;
	case eDbgOp_NodeParent: r->i = (int64_t)pNode->GetParent(); break;
	case eDbgOp_NodeBound:
	{
		const cVector3f &v = a[1].i < 3 ? pNode->GetMin() : pNode->GetMax();
		int k = (int)a[1].i % 3;
		r->f = k == 0 ? v.x : k == 1 ? v.y : v.z;
		break;
	}
	case eDbgOp_NodeNChild:
		gpKidNode = pNode;
		gvKids.assign(pNode->GetChildNodeList()->begin(), pNode->GetChildNodeList()->end());
		r->i = (int64_t)gvKids.size();
		break;
	case eDbgOp_NodeChild: r->i = (int64_t)gvKids[a[1].i]; break;
	case eDbgOp_NodeNObj:
		gpObjNode = pNode;
		gvObjs.assign(pNode->GetObjectList()->begin(), pNode->GetObjectList()->end());
		r->i = (int64_t)gvObjs.size();
		break;
	case eDbgOp_NodeObj: r->i = (int64_t)gvObjs[a[1].i]; break;
	}
}

static void ObjectOp(int alOp, const OhplGa9Arg *a, OhplGa9Ret *r)
{
	iRenderable *pObject = (iRenderable*)a[0].i;
	switch(alOp)
	{
	case eDbgOp_ObjIsSub: r->i = pObject->GetRenderType() == eRenderableType_SubMesh; break;
	case eDbgOp_ObjName: OhplGa9RetS(r, pObject->GetName()); break;
	case eDbgOp_ObjEType: OhplGa9RetS(r, pObject->GetEntityType()); break;
	case eDbgOp_ObjBound:
	{
		cBoundingVolume *pBV = pObject->GetBoundingVolume();
		const cVector3f &v = a[1].i < 3 ? pBV->GetMin() : pBV->GetMax();
		int k = (int)a[1].i % 3;
		r->f = k == 0 ? v.x : k == 1 ? v.y : v.z;
		break;
	}
	case eDbgOp_ObjBvHit: r->i = cMath::CheckBVIntersection(*pObject->GetBoundingVolume(), *gpLineBV); break;
	case eDbgOp_ObjLineTri:
	{
		float fT = a[7].f;
		cMatrixf mtxInvModel = cMath::MatrixInverse(pObject->GetWorldMatrix());
		r->i = cMath::CheckLineTriVertexBufferIntersection(V3(a, 1), V3(a, 4), mtxInvModel, pObject->GetVertexBuffer(), NULL, &fT, NULL, true);
		r->f = fT;
		break;
	}
	}
}

static void GuiOp(int alOp, const OhplGa9Arg *a, OhplGa9Ret *r)
{
	iWidget *pW = alOp > eDbgOp_WNew ? Widget(a[0].i) : NULL;
	switch(alOp)
	{
	case eDbgOp_GuiInit:
		gpGui = gpBase->mpEngine->GetGui();
		gpSkin = a[0].i ? gpGui->CreateSkin("gui_default.skin") : NULL;
		gpSet = gpGui->CreateSet("DebugHandler", gpSkin);
		break;
	case eDbgOp_GuiStart:
		gpBase->mpMapHandler->GetViewport()->AddGuiSet(gpSet);
		gpSet->SetActive(false);
		break;
	case eDbgOp_WNew:
	{
		iWidget *pNew = NewWidget(a);
		gvWidgets.push_back(pNew);
		pNew->SetUserData((void*)(intptr_t)gvWidgets.size());
		r->i = (int64_t)gvWidgets.size() - 1;
		break;
	}
	case eDbgOp_WChecked: ((cWidgetCheckBox*)pW)->SetChecked(a[1].i != 0, a[2].i != 0); break;
	case eDbgOp_WUv: pW->SetUserValue((int)a[1].i); break;
	case eDbgOp_WCb:
	{
		static const eGuiMessage vMsg[3] = { eGuiMessage_CheckChange, eGuiMessage_ButtonPressed, eGuiMessage_SliderMove };
		pW->AddCallback(vMsg[a[1].i], NULL, WidgetCb);
		break;
	}
	case eDbgOp_WSize: pW->SetSize(cVector2f(a[1].f, a[2].f)); break;
	case eDbgOp_WSlider: ((cWidgetSlider*)pW)->SetValue((int)a[1].i, a[2].i != 0); break;
	case eDbgOp_WComboAdd: ((cWidgetComboBox*)pW)->AddItem(OhplGa9GetW(a[1])); break;
	case eDbgOp_WComboClear: ((cWidgetComboBox*)pW)->ClearItems(); break;
	case eDbgOp_WComboSel: ((cWidgetComboBox*)pW)->SetSelectedItem((int)a[1].i); break;
	case eDbgOp_WComboGet: r->i = ((cWidgetComboBox*)pW)->GetSelectedItem(); break;
	case eDbgOp_WComboText: OhplGa9RetW(r, ((cWidgetComboBox*)pW)->GetItem((int)a[1].i)->GetText()); break;
	case eDbgOp_WText: pW->SetText(OhplGa9GetW(a[1])); break;
	case eDbgOp_WVis: pW->SetVisible(a[1].i != 0); break;
	case eDbgOp_WEn: pW->SetEnabled(a[1].i != 0); break;
	case eDbgOp_WDestroy: gpSet->DestroyWidget(pW); gvWidgets[a[0].i] = NULL; break;
	case eDbgOp_WFont: ((cWidgetLabel*)pW)->SetDefaultFontSize(cVector2f(a[1].f)); break;
	}
}

static void GuiMiscOp(int alOp, const OhplGa9Arg *a, OhplGa9Ret *r)
{
	switch(alOp)
	{
	case eDbgOp_GuiVirtW: r->f = gpSet->GetVirtualSize().x; break;
	case eDbgOp_FontLen: r->f = gpSet->GetSkin()->GetFont(eGuiSkinFont_Default)->mpFont->GetLength(cVector2f(a[0].f), OhplGa9GetW(a[1]).c_str()); break;
	case eDbgOp_Picker:
	{
		gvPicked.clear();
		bool bBatch = a[0].i != 0;
		cGuiPopUpFilePicker *pPicker = gpSet->CreatePopUpLoadFilePicker(gvPicked, false, OhplGa9GetW(a[1]), false, NULL, bBatch ? PickBatchCb : PickWorldCb);
		if(bBatch) pPicker->AddCategory(_W("MapBatch"), _W("*.mapbatch"));
		else
		{
			pPicker->AddCategory(_W("Scenes"), _W("*.map"));
			pPicker->AddFilter(0, _W("*.dae"));
		}
		break;
	}
	case eDbgOp_Picked:
		r->i = (int64_t)gvPicked.size();
		if(gvPicked.empty() == false) OhplGa9RetW(r, gvPicked[0]);
		break;
	case eDbgOp_SetActive: gpSet->SetActive(a[0].i != 0); break;
	case eDbgOp_GuiFocus: gpGui->SetFocus(a[0].i ? gpSet : NULL); break;
	case eDbgOp_PickSet: gvPicked.clear(); gvPicked.push_back(OhplGa9GetW(a[0])); break;
	case eDbgOp_CfgDebug: r->i = (gpBase->mpConfigHandler->mbLoadDebugMenu ? 1 : 0) | (gpBase->mpConfigHandler->mbFullscreen ? 2 : 0); break;
	case eDbgOp_DrawFmt: DrawLine(a, true); break;
	case eDbgOp_DrawText: DrawLine(a, false); break;
	case eDbgOp_ScreenH: r->f = gpBase->mpEngine->GetGraphics()->GetLowLevel()->GetScreenSizeFloat().y; break;
	}
}

static void EngineOp(int alOp, const OhplGa9Arg *a, OhplGa9Ret *r)
{
	cLuxMap *pMap = gpBase->mpMapHandler->GetCurrentMap();
	switch(alOp)
	{
	case eDbgOp_InState: gpBase->mpInputHandler->ChangeState(a[0].i == 0 ? eLuxInputState_Debug : eLuxInputState_Game); break;
	case eDbgOp_Lock:
		gpBase->mpEngine->GetInput()->GetLowLevel()->LockInput(a[0].i != 0);
		gpBase->mpEngine->GetInput()->GetLowLevel()->RelativeMouse(a[0].i != 0);
		break;
	case eDbgOp_Speed:
		gpBase->mpEngine->SetSpeedMul(a[0].f);
		gpBase->mpEngine->GetSound()->GetSoundHandler()->SetGlobalSpeed(a[0].f, eSoundEntryType_All, eLuxGlobalVolumeType_DebugMenu);
		break;
	case eDbgOp_FreeCam: gpBase->mpPlayer->SetFreeCamActive(a[0].i != 0); break;
	case eDbgOp_FreeCamSpeed: gpBase->mpPlayer->SetFreeCamSpeed(a[0].f); break;
	case eDbgOp_OcclSet: gpBase->mpMapHandler->GetViewport()->GetRenderSettings()->mbUseOcclusionCulling = a[0].i != 0; break;
	case eDbgOp_OcclGet: r->i = gpBase->mpMapHandler->GetViewport()->GetRenderSettings()->mbUseOcclusionCulling; break;
	case eDbgOp_ResLogSet: iResourceBase::SetLogCreateAndDelete(a[0].i != 0); break;
	case eDbgOp_ResLogGet: r->i = iResourceBase::GetLogCreateAndDelete(); break;
	case eDbgOp_FastPhysSet: gpBase->mpConfigHandler->mbFastPhysicsLoad = a[0].i != 0; break;
	case eDbgOp_FastPhysGet: r->i = gpBase->mpConfigHandler->mbFastPhysicsLoad; break;
	case eDbgOp_InsCount: r->i = gpBase->mpInsanityHandler->GetEventNum(); break;
	case eDbgOp_InsName: OhplGa9RetS(r, gpBase->mpInsanityHandler->GetEventName((int)a[0].i)); break;
	case eDbgOp_InsReload: gpBase->mpInsanityHandler->ReloadEvents(); break;
	case eDbgOp_InsStart: gpBase->mpInsanityHandler->StartEvent((int)a[0].i); break;
	case eDbgOp_StartCount: r->i = pMap->GetPlayerStartNodeNum(); break;
	case eDbgOp_StartName: OhplGa9RetS(r, pMap->GetPlayerStartNode((int)a[0].i)->GetName()); break;
	case eDbgOp_Place: pMap->PlacePlayerAtStartPos(cString::To8Char(OhplGa9GetW(a[0]))); break;
	case eDbgOp_ResReload:
		gpBase->mpEngine->GetResources()->LoadResourceDirsFile(gpBase->msResourceConfigPath
#ifdef USERDIR_RESOURCES
															   ,gpBase->msUserResourceDir
#endif
															   );
		break;
	case eDbgOp_Transl:
		gpBase->mpEngine->GetResources()->ClearTranslations();
		gpBase->LoadLanguage(gpBase->mpMainConfig->GetString("Game", "StartLanguage", gpBase->msDefaultGameLanguage), true);
		break;
	case eDbgOp_Recompile:
		if(a[0].i == 0) gsRecompile.clear();
		if(a[0].i == 0) r->i = pMap->RecompileScript(&gsRecompile);
		else if(a[0].i == 1) r->i = gpBase->mpInventory->RecompileScript(&gsRecompile);
		else r->i = gpBase->mpGlobalDataHandler->RecompileScript(&gsRecompile);
		OhplGa9RetS(r, gsRecompile);
		break;
	case eDbgOp_MusicStop: gpBase->mpEngine->GetSound()->GetMusicHandler()->Stop((float)a[0].i); break;
	case eDbgOp_PlayerSet:
	{
		iCharacterBody *pCharBody = gpBase->mpPlayer->GetCharacterBody();
		cCamera *pCam = gpBase->mpPlayer->GetCamera();
		pCam->SetPitch(a[0].f);
		pCam->SetYaw(a[1].f);
		pCam->SetRoll(a[2].f);
		pCharBody->SetPosition(V3(a, 3));
		pCharBody->SetYaw(pCam->GetYaw());
		break;
	}
	case eDbgOp_MapFile: OhplGa9RetS(r, pMap->GetFileName()); break;
	case eDbgOp_BaseReset: gpBase->Reset(); break;
	case eDbgOp_DestroyUnused:
		gpBase->mpEngine->GetResources()->GetSoundManager()->DestroyUnused(0);
		gpBase->mpEngine->GetResources()->GetParticleManager()->DestroyUnused(0);
		gpBase->mpEngine->GetResources()->GetSoundEntityManager()->DestroyUnused(0);
		break;
	case eDbgOp_StartGame: gpBase->StartGame(OhplGa9GetS(a[0]), "", ""); break;
	case eDbgOp_CacheNew:
		gvCaches.push_back(new cLuxModelCache());
		gvCaches.back()->Create();
		break;
	case eDbgOp_CacheFree:
		gvCaches.back()->Destroy();
		delete gvCaches.back();
		gvCaches.pop_back();
		break;
	case eDbgOp_MenuScreen: gpBase->mpLoadScreenHandler->DrawMenuScreen(); break;
	case eDbgOp_MapSave:
		gpSavedMaps = gpBase->mpMapHandler->GetSavedMapCollection();
		gpTestMap = pMap;
		gpSavedMaps->SaveMap(gpTestMap);
		break;
	case eDbgOp_MapDestroy: gpBase->mpMapHandler->DestroyMap(gpTestMap, false); break;
	case eDbgOp_SavedRoundtrip:
	{
		tWString sFile = OhplGa9GetW(a[0]);
		cSerializeClass::SaveToFile(gpSavedMaps, sFile, "SavedMaps");
		gpSavedMaps->Reset();
		cSerializeClass::LoadFromFile(gpSavedMaps, sFile);
		break;
	}
	case eDbgOp_MapLoad: gpTestMap = gpBase->mpMapHandler->LoadMap(OhplGa9GetS(a[0]), true); break;
	case eDbgOp_MapSetCur: gpBase->mpMapHandler->SetCurrentMap(gpTestMap, false, false, ""); break;
	case eDbgOp_MapLoadSaved: gpSavedMaps->LoadMap(gpTestMap); break;
	case eDbgOp_MapRun: gpTestMap->RunScript(OhplGa9GetS(a[0])); break;
	case eDbgOp_ReadFile:
	{
		tWString sPath = OhplGa9GetW(a[0]);
		tString sData;
		unsigned int lSize = cPlatform::GetFileSize(sPath);
		sData.resize(lSize);
		cPlatform::CopyFileToBuffer(sPath, &sData[0], lSize);
		OhplGa9RetS(r, sData);
		break;
	}
	}
}

static void ScreenOp(int alOp, const OhplGa9Arg *a, OhplGa9Ret *r)
{
	switch(alOp)
	{
	case eDbgOp_Q: r->d = Query((int)a[0].i); break;
	case eDbgOp_QS: QueryS((int)a[0].i, r); break;
	case eDbgOp_StateDraw:
	{
		cLuxPlayer *pPlayer = gpBase->mpPlayer;
		r->f = pPlayer->GetStateData(pPlayer->GetCurrentState())->DrawDebug(gpBase->mpGameDebugSet, gpBase->mpDefaultFont, a[0].f);
		break;
	}
	case eDbgOp_EntSnap:
	{
		gvEnts.clear();
		cLuxEntityIterator entIt = gpBase->mpMapHandler->GetCurrentMap()->GetEntityIterator();
		while(entIt.HasNext()) gvEnts.push_back(entIt.Next());
		r->i = (int64_t)gvEnts.size();
		break;
	}
	case eDbgOp_EntActive: r->i = gvEnts[a[0].i]->IsActive(); break;
	case eDbgOp_EntDraw: r->f = gvEnts[a[0].i]->DrawDebug(gpBase->mpGameDebugSet, gpBase->mpDefaultFont, a[1].f); break;
	case eDbgOp_Log: Log("%s", OhplGa9GetS(a[0]).c_str()); break;
	case eDbgOp_LogCb: SetLogMessageCallback(a[0].i ? LogMessageCallback : NULL); break;
	}
}

void OhplDbgHostCall(uint32_t alOp, const OhplGa9Arg *a, OhplGa9Ret *r)
{
	if(alOp == eDbgOp_RenderPhys)
		gpBase->mpMapHandler->GetCurrentMap()->GetPhysicsWorld()->RenderDebugGeometry(gpFunctions->GetLowLevelGfx(), cColor(0.5f));
	else if(alOp >= eDbgOp_SndSnap && alOp <= eDbgOp_SndS) SoundOp(alOp, a, r);
	else if(alOp >= eDbgOp_Q) ScreenOp(alOp, a, r);
	else if(alOp >= eDbgOp_DrawFmt) GuiMiscOp(alOp, a, r);
	else if(alOp >= eDbgOp_MeshName) MeshOp(alOp, a, r);
	else if(alOp >= eDbgOp_ObjIsSub && alOp != eDbgOp_LineBv) ObjectOp(alOp, a, r);
	else if(alOp >= eDbgOp_ContRoot) ContainerOp(alOp, a, r);
	else if(alOp >= eDbgOp_InState) EngineOp(alOp, a, r);
	else if(alOp >= eDbgOp_GuiVirtW) GuiMiscOp(alOp, a, r);
	else GuiOp(alOp, a, r);
}

//-----------------------------------------------------------------------

cLuxDebugHandler::cLuxDebugHandler() : iLuxUpdateable("LuxDebugHandler")
{
	if(OhplA12dEnabled()) gpBase->mpConfigHandler->mbLoadDebugMenu = true;
	ohpl_in_set_host(OhplInHostCall);

	mpRust = ohpl_ga9_debug_new();
	mpF = ohpl_ga9_debug_flags(mpRust);
	ohpl_dbg_init(mpRust);
}

cLuxDebugHandler::~cLuxDebugHandler()
{
	ohpl_dbg_shutdown(mpRust);
	ohpl_ga9_debug_free(mpRust);
}

void cLuxDebugHandler::LoadUserConfig()
{
	auto call = [&](uint32_t op, const OhplGa9Arg *a, size_t, OhplGa9Ret *r) { OhplGa9CfgCall(gpBase->mpUserConfig, op, a, r); };
	ohpl_ga9_debug_load_user_config(mpRust, gpBase->mbPTestActivated, &call, OhplGa9CallOf<decltype(call)>::Fn());
}

void cLuxDebugHandler::SaveUserConfig()
{
	auto call = [&](uint32_t op, const OhplGa9Arg *a, size_t, OhplGa9Ret *r) { OhplGa9CfgCall(gpBase->mpUserConfig, op, a, r); };
	ohpl_ga9_debug_save_user_config(mpRust, &call, OhplGa9CallOf<decltype(call)>::Fn());
}

void cLuxDebugHandler::Update(float afTimeStep)
{
	ohpl_dbg_update(mpRust, afTimeStep);
	OhplA12dTick();
}

void cLuxDebugHandler::RenderSolid(cRendererCallbackFunctions* apFunctions)
{
	gpFunctions = apFunctions;
	ohpl_dbg_render_solid(mpRust);
}

iWidget *cLuxDebugHandler::DumpDebugRoot() { return Widget(ohpl_dbg_root(mpRust, 0)); }
iWidget *cLuxDebugHandler::DumpScriptRoot() { return Widget(ohpl_dbg_root(mpRust, 1)); }

tString cLuxDebugHandler::DumpInspectName()
{
	cSubMeshEntity *pMesh = (cSubMeshEntity*)ohpl_dbg_inspect(mpRust);
	return pMesh ? pMesh->GetName() : tString("-");
}

void cLuxDebugHandler::DumpPick(const tWString& asPath, bool abBatch)
{
	gvPicked.clear();
	gvPicked.push_back(asPath);
	ohpl_dbg_event(mpRust, abBatch ? -2 : -1, 0, 0);
}
