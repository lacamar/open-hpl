#include "SomaImGui.h"
#include "SomaBase.h"
#include "SomaLux.h"
#include "SomaLuxEntity.h"
#include "SomaLuxGame.h"
#include "SomaLuxScriptable.h"
#include "SomaScriptBind.h"

#include "impl/scriptarray.h"
#include "input/ActionKeyboard.h"
#include "input/ActionMouseButton.h"

#include <algorithm>
#include <cmath>
#include <cstring>

cSomaImGui *cSomaImGui::mpCurrent = NULL;
cSomaImGui *cSomaImGui::mpInputFocus = NULL;
cSomaImGui *cSomaImGui::mpPrevInputFocus = NULL;
bool cSomaImGui::mbGameHudFocus = false;
cSomaImGui *SomaHudImGui();

cSomaImGui *cSomaImGui::GetInputFocus()
{
	return mbGameHudFocus ? SomaHudImGui() : mpInputFocus;
}

// Recovered struct layouts (script_api.txt property offsets, soma-re-struct-defaults.py)
namespace
{
	enum
	{
		kGfxMaterial = 16, kGfxType = 20, kGfxColor = 40, kGfxUVMin = 64, kGfxUVMax = 72, kGfxFile = 152, kGfxSize = 168,
		kFontMaterial = 16, kFontColor = 20, kFontSize = 36, kFontFile = 48,
		kWColorBase = 16, kWColorText = 32, kWColorDisabled = 48, kWUseDisabledColor = 312, kWDefaultSize = 64, kWUseBg = 72, kWGfxBg = 80, kWFont = 248, kWFontAlign = 316,
		kBUseFrame = 320, kBFrame = 328, kBUseInFocusGfx = 1920, kBGfxInFocus = 1928, kBUseInFocusColor = 2096, kBColorInFocus = 2100,
		kBUseTrigGfx = 2116, kBGfxTrig = 2120, kBUseTrigColor = 2288, kBColorTrig = 2292, kBUseTrigFocusColor = 2308, kBColorTrigFocus = 2312,
		kFrameGfxBg = 16,
		kSliderUseButton = 320, kSliderGfxButton = 328, kSliderButtonSize = 496,
		kCheckBoxSize = 496, kCheckGfxBox = 504, kCheckOverlaySize = 672, kCheckGfxOverlay = 680,
		kMultiArrowSize = 496, kMultiGfxArrowRight = 512, kMultiGfxArrowLeft = 848,
		kWindowFrame = 320, kWUseHeader = 1912, kWHeaderType = 1916, kWLabelOffset = 1920, kWCaptionSizeMul = 1928, kWLabelFrame = 1936,
		kWindowLabelPadTop = 3528, kWindowLabelMinWidth = 3544, kWindowPadTop = 3548, kWindowPadRight = 3552, kWindowPadBottom = 3556, kWindowPadLeft = 3560,
		kGaugeFrame = 320, kGaugeUseFrame = 1912, kGaugeFill = 1920, kGaugeOrient = 2092, kGaugePadding = 2096,
	};
	template <class T> T &F(const void *p, int off) { return *(T *)((char *)p + off); }
	const tString &StrAt(const void *p, int off)
	{
		static tString sEmpty;
		const tString *s = F<const tString *>(p, off);
		return s ? *s : sEmpty;
	}
	cColor Mul(const cColor &a, const cColor &b) { return cColor(a.r * b.r, a.g * b.g, a.b * b.b, a.a * b.a); }
	uint64_t Id(const tString &s) { return SomaHash64(s); }
	struct S_
	{
		char c;
	};
}

cSomaImGui::cSomaImGui(const tString &asName, cGuiSet *apSet) : msName(asName), mpSet(apSet)
{
}

cSomaImGui::~cSomaImGui()
{
	for (auto &it : mmapDefaults)
		free(it.second);
	if (mpCurrent == this)
		mpCurrent = NULL;
	if (mpInputFocus == this)
		mpInputFocus = NULL;
	if (mpPrevInputFocus == this)
		mpPrevInputFocus = NULL;
}

void cSomaImGui::SetInputFocus(cSomaImGui *apImGui, bool abShowMouse)
{
	if (apImGui && apImGui != mpInputFocus)
		apImGui->mvCursor3D = apImGui->mpSet->GetVirtualSize() * 0.5f;
	mpInputFocus = apImGui;
	if (apImGui)
		apImGui->mbShowMouse = abShowMouse;
}

void *cSomaImGui::GetDefault(const char *apType)
{
	void *&p = mmapDefaults[apType];
	if (p == NULL)
		p = SomaNewScriptStruct(apType);
	return p;
}

void cSomaImGui::SetDefault(const char *apType, const void *apData)
{
	memcpy(GetDefault(apType), apData, 4096);
}

void cSomaImGui::ClearStates()
{
	mmapStates.clear();
	mmapFades.clear();
	mvTimers.clear();
	mbFirstRun = true;
}

void cSomaImGui::Begin(float afTimeStep)
{
	mfTimeStep = afTimeStep;
	mfTimeCount += afTimeStep;
	mvBuilding.clear();
	mvDrawn.clear();
	mMods = cModifiers();
	mvModStack.clear();
	mvGroups.clear();
	mvLayouts.clear();
	mlAlign = 0;
	mvItems.clear();
	mvNav.clear();
	mbFoundFocus = false;
	mlWrapMode = 3;
	mlGroupFlags = 1;
	if (mlMouseLock > 0)
		--mlMouseLock;

	for (auto it = mmapFades.begin(); it != mmapFades.end(); ++it)
	{
		cFade &f = it->second;
		f.mfCount = std::min(f.mfCount + afTimeStep, f.mfTime);
		float t = f.mfTime > 0 ? SomaEasing(f.mlEasing, f.mfCount / f.mfTime) : 1;
		float v[4];
		for (int i = 0; i < 4; ++i)
			v[i] = f.mvStart[i] + (f.mvGoal[i] - f.mvStart[i]) * t;
		cState &st = mmapStates[it->first];
		if (f.mlType == 0)
			st.mfFloat = v[0], st.mbSetFloat = true;
		else if (f.mlType == 1)
			st.mvVec = cVector3f(v[0], v[1], v[2]), st.mbSetVec = true;
		else
			st.mCol = cColor(v[0], v[1], v[2], v[3]), st.mbSetCol = true;
	}
	for (cTimer &t : mvTimers)
		t.mfTime -= afTimeStep, t.mbTouched = false;
}

void cSomaImGui::End()
{
	std::erase_if(mvTimers, [](const cTimer &t) { return t.mfTime <= 0 || (t.mbRepeat && !t.mbTouched); });
	UpdateUIMovement();
	mbFirstRun = false;
	for (int i = 0; i < 10; ++i)
	{
		mvActionTriggered[i] = false;
		mvActionUsed[i] = false;
		if (i >= 4)
			mvActionDown[i] = false;
	}
	mvMouseRel = 0;
	if (mbShowMouse && this == GetInputFocus())
	{
		void *pMouse = GetDefault("__mouse");
		if (StrAt(pMouse, kGfxFile) != "")
			DrawGfx(pMouse, cVector3f(mvMousePos.x, mvMousePos.y, 100), -1, cColor(1, 1));
	}
	mvDrawn.swap(mvBuilding);
	mvBuilding.clear();
}

tString cSomaImGui::DebugOps(size_t alMax)
{
	tString s = cString::ToString((int)mvDrawn.size()) + ": ";
	for (size_t i = 0; i < mvDrawn.size() && i < alMax; ++i)
	{
		const cOp &op = mvDrawn[i];
		char sBuf[160];
		snprintf(sBuf, sizeof(sBuf), "%.1f,%.1f,%.1f %.1fx%.1f c=%.2f,%.2f,%.2f,%.2f m=%d", op.mvPos.x, op.mvPos.y, op.mvPos.z, op.mvSize.x, op.mvSize.y, op.mColor.r,
				 op.mColor.g, op.mColor.b, op.mColor.a, op.mlMaterial);
		s += (op.mpGfx ? tString("gfx ") : op.mpFont ? "text '" + cString::To8Char(op.msText.substr(0, 64)) + "' " : tString("clip ")) + sBuf + "; ";
	}
	return s;
}

void cSomaImGui::DrawAll()
{
	std::vector<cGuiClipRegion *> vClip(1, mpSet->GetCurrentClipRegion());
	if (mScreenClear.a > 0)
	{
		static cGuiGfxElement *pWhite = gpSomaBase->mpEngine->GetGui()->CreateGfxFilledRect(cColor(1, 1), eGuiMaterial_Alpha);
		mpSet->DrawGfx(pWhite, cVector3f(0, 0, -1), mpSet->GetVirtualSize(), mScreenClear, eGuiMaterial_Alpha);
	}
	for (cOp op : mvDrawn)
	{
		if (op.mpGfx == NULL && op.mpFont == NULL)
		{
			if (op.mlAlign == kClipBegin)
				vClip.push_back(vClip.back()->CreateChild(op.mvPos, op.mvSize));
			else if (vClip.size() > 1)
				vClip.pop_back();
			mpSet->SetCurrentClipRegion(vClip.back());
		}
		else if (op.mpGfx)
			mpSet->DrawGfx(op.mpGfx, op.mvPos, op.mvSize, op.mColor, (eGuiMaterial)op.mlMaterial, op.mfAngle);
		else if (op.mpFont)
			mpSet->DrawFont(op.msText, op.mpFont, op.mvPos, op.mvSize, op.mColor, (eFontAlign)op.mlAlign);
	}
	mpSet->SetCurrentClipRegion(vClip.front());
}

void cSomaImGui::SendAction(int alAction, bool abDown, bool abTriggered)
{
	if (alAction < 0 || alAction >= 10)
		return;
	mvActionDown[alAction] = abDown;
	mvActionTriggered[alAction] = mvActionTriggered[alAction] || abTriggered;
}

void cSomaImGui::SendMouseVirtualPosition(const cVector2f &avPos, const cVector2f &avRel)
{
	mvMousePos = avPos;
	mvMouseRel += avRel;
}

void cSomaImGui::SendMousePosition(const cVector2l &avPos, const cVector2l &avRel)
{
	cVector2f vScreen = gpSomaBase->mpEngine->GetGraphics()->GetLowLevel()->GetScreenSizeFloat();
	cVector2f vVirtual = mpSet->GetVirtualSize();
	if (mpSet->Is3D())
	{
		// Screens in the world: the captured mouse moves a cursor across the screen
		float fScale = vVirtual.y / vScreen.y * 1.5f;
		cVector2f vRel((float)avRel.x * fScale, (float)avRel.y * fScale);
		mvCursor3D.x = cMath::Clamp(mvCursor3D.x + vRel.x, 0.0f, vVirtual.x);
		mvCursor3D.y = cMath::Clamp(mvCursor3D.y + vRel.y, 0.0f, vVirtual.y);
		SendMouseVirtualPosition(mvCursor3D, vRel);
		return;
	}
	cVector2f vScale(vVirtual.x / vScreen.x, vVirtual.y / vScreen.y);
	SendMouseVirtualPosition(cVector2f((float)avPos.x * vScale.x, (float)avPos.y * vScale.y) - mpSet->GetVirtualSizeOffset(),
							 cVector2f((float)avRel.x * vScale.x, (float)avRel.y * vScale.y));
}

void cSomaImGui::Fade(uint64_t alId, int alType, const float *apGoal, float afTime, int alEasing, bool abReplace)
{
	auto it = mmapFades.find(alId);
	if (it != mmapFades.end() && abReplace == false && it->second.mfCount < it->second.mfTime)
		return;
	cState &st = mmapStates[alId];
	cFade f;
	f.mlType = alType;
	float vStart[4] = {st.mfFloat, 0, 0, 0};
	if (alType == 1)
	{
		vStart[0] = st.mvVec.x;
		vStart[1] = st.mvVec.y;
		vStart[2] = st.mvVec.z;
	}
	else if (alType == 2)
	{
		vStart[0] = st.mCol.r;
		vStart[1] = st.mCol.g;
		vStart[2] = st.mCol.b;
		vStart[3] = st.mCol.a;
	}
	memcpy(f.mvStart, vStart, sizeof(vStart));
	memcpy(f.mvGoal, apGoal, sizeof(f.mvGoal));
	f.mfTime = afTime;
	f.mfCount = 0;
	f.mlEasing = alEasing;
	mmapFades[alId] = f;
}

// cLuxGuiCameraTexture
struct cSomaCameraTexture : public iViewportCallback
{
	cCamera *mpCamera = NULL;
	iTexture *mpTexture = NULL;
	iFrameBuffer *mpBuffer = NULL;
	cViewport *mpViewport = NULL;
	cGuiGfxElement *mpGfx = NULL;
	unsigned long mlFrameMs = 33, mlLastDraw = 0, mlLastUsed = 0;
	cVector2l mvSize;
	unsigned mlFPS = 30;
	cSomaID mAttached;
	bool mbAttached = false;
	void OnPreWorldDraw() override { mlLastDraw = cPlatform::GetApplicationTime(); }
	void OnPostWorldDraw() override {}
};
static std::map<tString, cSomaCameraTexture *> gmapCameraTextures;

static void DestroyCameraTexture(const tString &asName)
{
	auto it = gmapCameraTextures.find(asName);
	if (it == gmapCameraTextures.end())
		return;
	cSomaCameraTexture *p = it->second;
	cEngine *pEngine = gpSomaBase->mpEngine;
	pEngine->GetScene()->DestroyViewport(p->mpViewport);
	pEngine->GetScene()->DestroyCamera(p->mpCamera);
	if (p->mpGfx)
		pEngine->GetGui()->DestroyGfx(p->mpGfx);
	pEngine->GetGraphics()->DestroyFrameBuffer(p->mpBuffer);
	pEngine->GetGraphics()->DestroyTexture(p->mpTexture);
	delete p;
	gmapCameraTextures.erase(it);
}

void SomaDestroyCameraTextures()
{
	while (!gmapCameraTextures.empty())
		DestroyCameraTexture(gmapCameraTextures.begin()->first);
}

static void CreateCameraTexture(const tString &asName, const cVector2l &avSize, unsigned alFPS, float afFOV, float afNear, float afFar)
{
	cSomaLuxMap *pMap = cSomaLuxMap::GetCurrent();
	if (pMap == NULL)
		return;
	DestroyCameraTexture(asName);
	cEngine *pEngine = gpSomaBase->mpEngine;
	cSomaCameraTexture *p = new cSomaCameraTexture();
	p->mlFrameMs = 1000 / (alFPS ? alFPS : 1);
	p->mvSize = avSize;
	p->mlFPS = alFPS;
	p->mlLastDraw = cPlatform::GetApplicationTime();
	p->mpCamera = pEngine->GetScene()->CreateCamera(eCameraMoveMode_Fly);
	p->mpCamera->SetAspect((float)avSize.x / (float)avSize.y);
	p->mpCamera->SetFOV(cMath::ToRad(afFOV));
	p->mpCamera->SetNearClipPlane(afNear);
	p->mpCamera->SetFarClipPlane(afFar);
	p->mpCamera->SetRotateMode(eCameraRotateMode_Matrix);
	p->mpTexture = pEngine->GetGraphics()->CreateTexture("CameraTexture_" + asName, eTextureType_2D, eTextureUsage_RenderTarget);
	p->mpTexture->CreateFromRawData(cVector3l(avSize.x, avSize.y, 0), ePixelFormat_RGBA, NULL);
	p->mpTexture->SetWrapSTR(eTextureWrap_ClampToEdge);
	p->mpTexture->SetFilter(eTextureFilter_Bilinear);
	p->mpBuffer = pEngine->GetGraphics()->CreateFrameBuffer("CameraTexture_" + asName);
	p->mpBuffer->SetTexture2D(0, p->mpTexture);
	p->mpBuffer->CompileAndValidate();
	p->mpViewport = pEngine->GetScene()->CreateViewport(p->mpCamera, pMap->GetWorld(), true);
	p->mpViewport->SetFrameBuffer(p->mpBuffer);
	p->mpViewport->SetVisible(false);
	cRenderSettings *pSettings = p->mpViewport->GetRenderSettings();
	pSettings->mbRenderShadows = false;
	pSettings->mbSSAOActive = false;
	pSettings->mbUseEdgeSmooth = false;
	pSettings->mbUseFxaa = false;
	pSettings->mbRenderWorldReflection = false;
	pSettings->mbUseDelayedOcclusionCulling = true;
	p->mpViewport->AddViewportCallback(p);
	gmapCameraTextures[asName] = p;
}

// cLuxGuiCameraTexture::VariableUpdate: render at most alFrameRate times a second, and only while a gui shows it
void SomaUpdateCameraTextures()
{
	unsigned long lNow = cPlatform::GetApplicationTime();
	cSomaLuxMap *pMap = cSomaLuxMap::GetCurrent();
	for (auto &it : gmapCameraTextures)
	{
		cSomaCameraTexture *p = it.second;
		p->mpViewport->SetVisible(lNow - p->mlLastDraw > p->mlFrameMs && p->mlLastUsed >= p->mlLastDraw);
		cSomaLuxEntity *pEnt = p->mbAttached && pMap ? pMap->GetEntity(p->mAttached) : NULL;
		if (pEnt == NULL)
			continue;
		cMatrixf mtx = pEnt->GetMatrix();
		p->mpCamera->SetRotationMatrix(cMath::MatrixMul(mtx.GetRotation(), cMath::MatrixRotateY(kPif)).GetTranspose());
		p->mpCamera->SetPosition(mtx.GetTranslation());
	}
}

std::vector<cSomaCameraTextureState> SomaGetCameraTextures()
{
	std::vector<cSomaCameraTextureState> v;
	cSomaLuxMap *pMap = cSomaLuxMap::GetCurrent();
	for (auto &it : gmapCameraTextures)
	{
		cSomaCameraTexture *p = it.second;
		cSomaLuxEntity *pEnt = p->mbAttached && pMap ? pMap->GetEntity(p->mAttached) : NULL;
		cCamera *c = p->mpCamera;
		v.push_back({it.first, pEnt ? pEnt->msName : "", p->mvSize, p->mlFPS, cMath::ToDeg(c->GetFOV()), c->GetNearClipPlane(), c->GetFarClipPlane(),
					 c->GetRotationMatrix(), c->GetPosition()});
	}
	return v;
}

iTexture *SomaGetCameraTexture(const tString &asName)
{
	auto it = gmapCameraTextures.find(asName);
	return it == gmapCameraTextures.end() ? NULL : it->second->mpTexture;
}

void SomaRestoreCameraTexture(const cSomaCameraTextureState &s)
{
	CreateCameraTexture(s.msName, s.mvSize, s.mlFPS, s.mfFOV, s.mfNear, s.mfFar);
	auto it = gmapCameraTextures.find(s.msName);
	if (it == gmapCameraTextures.end())
		return;
	cSomaCameraTexture *p = it->second;
	p->mpCamera->SetRotationMatrix(s.mtxRotation);
	p->mpCamera->SetPosition(s.mvPosition);
	cSomaLuxEntity *pEnt = s.msAttached.empty() ? NULL : cSomaLuxMap::GetCurrent()->GetEntity(s.msAttached);
	p->mbAttached = pEnt != NULL;
	if (pEnt)
		p->mAttached = pEnt->mID;
}

static cGuiGfxElement *GfxElement(const void *apGfx)
{
	const tString &sFile = StrAt(apGfx, kGfxFile);
	int lMaterial = F<int>(apGfx, kGfxMaterial);
	int lType = F<int>(apGfx, kGfxType);
	if (lType == 4)
	{
		auto it = gmapCameraTextures.find(sFile);
		if (it == gmapCameraTextures.end())
			return NULL;
		cSomaCameraTexture *p = it->second;
		if (p->mpGfx == NULL)
			p->mpGfx = gpSomaBase->mpEngine->GetGui()->CreateGfxTexture(p->mpTexture, false, (eGuiMaterial)lMaterial, cColor(1, 1), true);
		p->mlLastUsed = cPlatform::GetApplicationTime();
		return p->mpGfx;
	}
	static std::map<tString, cGuiGfxElement *> mapCache;
	if (sFile.empty())
	{
		static cGuiGfxElement *pWhite = NULL;
		if (pWhite == NULL)
			pWhite = gpSomaBase->mpEngine->GetGui()->CreateGfxFilledRect(cColor(1, 1), eGuiMaterial_Alpha);
		return pWhite;
	}
	const cVector2f &vUVMin = F<cVector2f>(apGfx, kGfxUVMin);
	const cVector2f &vUVMax = F<cVector2f>(apGfx, kGfxUVMax);
	bool bSubRect = vUVMin != cVector2f(0) || vUVMax != cVector2f(1);
	tString sKey = sFile + "#" + cString::ToString(lType);
	if (bSubRect)
		sKey += "#" + vUVMin.ToString() + vUVMax.ToString();
	auto it = mapCache.find(sKey);
	if (it != mapCache.end())
		return it->second;
	cGui *pGui = gpSomaBase->mpEngine->GetGui();
	cGuiGfxElement *pGfx = NULL;
	if (bSubRect)
	{
		if (iTexture *pTex = gpSomaBase->mpEngine->GetResources()->GetTextureManager()->Create2D(sFile, true))
			pGfx = pGui->CreateGfxTexture(pTex, false, (eGuiMaterial)lMaterial, cColor(1, 1), true, vUVMin, vUVMax);
	}
	else if (lType == 2 || lType == 3)
		pGfx = pGui->CreateGfxTexture(sFile, (eGuiMaterial)lMaterial, eTextureType_2D, cColor(1, 1), true);
	else
		pGfx = pGui->CreateGfxImage(sFile, (eGuiMaterial)lMaterial);
	mapCache[sKey] = pGfx;
	return pGfx;
}

// Full texture size; the official engine ignores mvUVMin/mvUVMax here
cVector2f cSomaImGui::GetGfxSize(const void *apGfx)
{
	cGuiGfxElement *pGfx = GfxElement(apGfx);
	if (pGfx == NULL)
		return 0;
	cVector2f vUV = F<cVector2f>(apGfx, kGfxUVMax) - F<cVector2f>(apGfx, kGfxUVMin);
	cVector2f vSize = pGfx->GetImageSize();
	return cVector2f(vUV.x > 0 ? vSize.x / vUV.x : vSize.x, vUV.y > 0 ? vSize.y / vUV.y : vSize.y);
}

void cSomaImGui::DrawGfx(const void *apGfx, const cVector3f &avPos, cVector2f avSize, const cColor &aColor)
{
	cGuiGfxElement *pGfx = GfxElement(apGfx);
	if (pGfx == NULL)
		return;
	if (avSize.x < 0 || avSize.y < 0)
		avSize = GetGfxSize(apGfx);
	cOp op;
	op.mpGfx = pGfx;
	op.mpFont = NULL;
	op.mvPos = avPos + F<cVector3f>(apGfx, 28);
	op.mvSize = avSize;
	op.mColor = Mul(Mul(aColor, F<cColor>(apGfx, kGfxColor)), mMods.mColorMul);
	op.mlMaterial = F<int>(apGfx, kGfxMaterial);
	op.mlAlign = 0;
	op.mfAngle = mMods.mfRotateAngle;
	Record(op);
}

// cImGuiFrameGfx: background, then corners at native size and borders stretched between them
void cSomaImGui::DrawFrame(const void *apFrame, const cVector3f &avPos, const cVector2f &avSize, const cColor &aColor)
{
	const char *p = (const char *)apFrame;
	enum { kBg = 16, kTR = 184, kBR = 352, kBL = 520, kTL = 688, kTop = 856, kRight = 1024, kBottom = 1192, kLeft = 1360 };
	auto Size = [&](int off) { return StrAt(p + off, kGfxFile).empty() ? cVector2f(0) : GetGfxSize(p + off); };
	cVector2f vTL = Size(kTL), vTR = Size(kTR), vBL = Size(kBL), vBR = Size(kBR);
	float fTop = Size(kTop).y, fBottom = Size(kBottom).y, fLeft = Size(kLeft).x, fRight = Size(kRight).x;
	float x0 = avPos.x, y0 = avPos.y, x1 = avPos.x + avSize.x, y1 = avPos.y + avSize.y, z = avPos.z;
	DrawGfx(p + kBg, cVector3f(x0 + fLeft, y0 + fTop, z), cVector2f(avSize.x - fLeft - fRight, avSize.y - fTop - fBottom), aColor);
	auto Part = [&](int off, float x, float y, float w, float h) {
		if (w > 0 && h > 0 && StrAt(p + off, kGfxFile).empty() == false)
			DrawGfx(p + off, cVector3f(x, y, z), cVector2f(w, h), aColor);
	};
	Part(kTop, x0 + vTL.x, y0, avSize.x - vTL.x - vTR.x, fTop);
	Part(kBottom, x0 + vBL.x, y1 - fBottom, avSize.x - vBL.x - vBR.x, fBottom);
	Part(kLeft, x0, y0 + vTL.y, fLeft, avSize.y - vTL.y - vBL.y);
	Part(kRight, x1 - fRight, y0 + vTR.y, fRight, avSize.y - vTR.y - vBR.y);
	Part(kTL, x0, y0, vTL.x, vTL.y);
	Part(kTR, x1 - vTR.x, y0, vTR.x, vTR.y);
	Part(kBL, x0, y1 - vBL.y, vBL.x, vBL.y);
	Part(kBR, x1 - vBR.x, y1 - vBR.y, vBR.x, vBR.y);
}

iFontData *cSomaImGui::GetFont(const void *apFont)
{
	tString sFile = apFont ? StrAt(apFont, kFontFile) : "";
	if (sFile.empty())
	{
		void *pDefault = GetDefault("__font");
		sFile = StrAt(pDefault, kFontFile);
		if (sFile.empty())
			sFile = "sansation_medium_bold.fnt";
	}
	static std::map<tString, iFontData *> mapFonts;
	iFontData *&pFont = mapFonts[sFile];
	if (pFont == NULL)
		pFont = gpSomaBase->mpEngine->GetResources()->GetFontManager()->CreateFontData(sFile);
	return pFont;
}

cVector2f cSomaImGui::FontSize(const void *apFont, float afMul)
{
	cVector2f vSize = apFont ? F<cVector2f>(apFont, kFontSize) : cVector2f(16);
	if (vSize.x <= 0 || vSize.y <= 0)
		vSize = F<cVector2f>(GetDefault("__font"), kFontSize);
	return vSize * afMul;
}

float cSomaImGui::GetFontLength(const void *apFont, float afMul, const tWString &asText)
{
	iFontData *pFont = GetFont(apFont);
	return pFont ? pFont->GetLength(FontSize(apFont, afMul), asText.c_str()) : 0;
}

void cSomaImGui::DrawFont(const tWString &asText, const void *apFont, const cVector3f &avPos, int alAlign, const cVector2f &avSizeMul, const cColor &aColor)
{
	iFontData *pFont = GetFont(apFont);
	if (pFont == NULL || asText.empty())
		return;
	cVector2f vSize = FontSize(apFont, 1);
	cOp op;
	op.mpGfx = NULL;
	op.mpFont = pFont;
	op.msText = asText;
	op.mvPos = avPos;
	op.mvSize = cVector2f(vSize.x * avSizeMul.x, vSize.y * avSizeMul.y);
	op.mColor = Mul(Mul(aColor, apFont ? F<cColor>(apFont, kFontColor) : cColor(1, 1)), mMods.mTextColorMul);
	op.mColor = Mul(op.mColor, mMods.mColorMul);
	op.mlMaterial = eGuiMaterial_FontNormal;
	op.mlAlign = alAlign;
	op.mfAngle = 0;
	Record(op);
}

void cSomaImGui::DrawText(const tWString &asText, const void *apFont, const cColor &aColor, int alAlign, const cVector3f &avPos, const cVector2f &avSize,
						  float afSizeMul)
{
	cVector2f vFont = FontSize(apFont, afSizeMul);
	float fX = alAlign == eFontAlign_Center ? avPos.x + avSize.x * 0.5f : alAlign == eFontAlign_Right ? avPos.x + avSize.x : avPos.x;
	float fY = avPos.y + std::max(0.0f, (avSize.y - vFont.y) * 0.5f);
	DrawFont(asText, apFont, cVector3f(fX, fY, avPos.z + 0.2f), alAlign, cVector2f(afSizeMul), aColor);
}

void cSomaImGui::Layout(cVector3f &avPos, cVector2f &avSize, const cVector2f &avDefaultSize)
{
	if (avSize.x < 0)
		avSize.x = avDefaultSize.x;
	if (avSize.y < 0)
		avSize.y = avDefaultSize.y;
	avPos += mvLayouts.empty() ? GroupPos() : mvLayouts.back().mvCursor;
	avPos = Align(avPos, avSize, mlAlign);
}

// The position names the given corner/centre of the widget
cVector3f cSomaImGui::Align(const cVector3f &avPos, const cVector2f &avSize, int alAlign)
{
	static const float vAlign[9][2] = {{0, 0}, {0, 1}, {0, 0.5f}, {1, 0}, {1, 1}, {1, 0.5f}, {0.5f, 0.5f}, {0.5f, 0}, {0.5f, 1}};
	if (alAlign <= 0 || alAlign >= 9)
		return avPos;
	return cVector3f(avPos.x - avSize.x * vAlign[alAlign][0], avPos.y - avSize.y * vAlign[alAlign][1], avPos.z);
}

void cSomaImGui::Advance(const cVector3f &avPos, const cVector2f &avSize, bool abUpdated)
{
	mPrev.mvPos = avPos;
	mPrev.mvSize = avSize;
	mPrev.mbUpdated = abUpdated;
	if (mvLayouts.empty())
		return;
	cLayout &l = mvLayouts.back();
	if (l.mlType == 3)
		l.mvCursor.y = avPos.y + avSize.y + l.mvSpacing.y;
	else if (l.mlType == 1)
		l.mvCursor.x = avPos.x + avSize.x + l.mvSpacing.x;
	else if (l.mlType == 2)
	{
		if (l.mvSpacing.x > 0.1f)
			l.mvCursor.x += std::ceil(avSize.x / l.mvSpacing.x) * l.mvSpacing.x;
	}
	else
	{
		l.mvCursor.x = avPos.x + avSize.x + l.mvSpacing.x;
		l.mfLineMax = std::max(l.mfLineMax, avSize.y);
		if (l.mvSize.x > 0 && l.mvCursor.x > l.mvStart.x + l.mvSize.x)
		{
			l.mvCursor.x = l.mvStart.x;
			l.mvCursor.y += l.mfLineMax + l.mvSpacing.y;
			l.mfLineMax = 0;
		}
	}
}

bool cSomaImGui::MouseOver(const cVector3f &avPos, const cVector2f &avSize)
{
	if (mMods.mbUseInput == false || this != GetInputFocus())
		return false;
	return mvMousePos.x >= avPos.x && mvMousePos.y >= avPos.y && mvMousePos.x < avPos.x + avSize.x && mvMousePos.y < avPos.y + avSize.y;
}

void cSomaImGui::DrawWidgetBase(const void *apData, const cVector3f &avPos, const cVector2f &avSize, bool abInFocus, bool abTriggered, int alInFocusGfx,
								int alTriggeredGfx)
{
	cColor col = F<cColor>(apData, kWColorBase);
	if (F<bool>(apData, kWUseBg))
		DrawGfx((char *)apData + kWGfxBg, avPos, avSize, col);
	if (abInFocus && alInFocusGfx >= 0 && F<bool>(apData, alInFocusGfx - 8))
		DrawGfx((char *)apData + alInFocusGfx, avPos + cVector3f(0, 0, 0.05f), avSize, cColor(1, 1));
	if (abTriggered && alTriggeredGfx >= 0 && F<bool>(apData, alTriggeredGfx - 4))
		DrawGfx((char *)apData + alTriggeredGfx, avPos + cVector3f(0, 0, 0.06f), avSize, cColor(1, 1));
}

// Per widget across frames: the previous widget is a different one each call
void cSomaImGui::SetPrevFocus(cState &aState, bool abOver)
{
	mPrev.mbWasInFocus = aState.mbInFocus && abOver == false;
	mPrev.mbBecameInFocus = abOver && aState.mbInFocus == false;
	mPrev.mbInFocus = abOver;
	aState.mbInFocus = abOver;
}

void cSomaImGui::SetFocus(const tString &asName)
{
	mlFocus = mlPrevFocus = asName.empty() ? 0 : Id(asName);
	mvLastDir = 0;
	mlLastCount = 0;
}

// cImGui::DoWidgetBase: the mouse takes focus only when it moves or clicks, the earliest drawn widget wins
bool cSomaImGui::WidgetBase(uint64_t alId, const cVector3f &avPos, const cVector2f &avSize, cState &aState)
{
	bool bOver = false;
	if (mMods.mbUseInput)
	{
		if (mbShowMouse == false || mlMouseLock > 0 || this != GetInputFocus())
		{
			if (mlFocus == alId)
				mbFoundFocus = true;
		}
		else
		{
			if (mlFocus == 0 && mbFirstRun && mbFoundFocus == false)
			{
				mlFocus = alId;
				mbFoundFocus = true;
			}
			bOver = MouseOver(avPos, avSize);
			bool bAct = mvMouseRel.x != 0 || mvMouseRel.y != 0 || mvActionTriggered[1] || mvActionTriggered[0];
			if (bOver && bAct && mbFoundFocus == false)
			{
				mlFocus = alId;
				mbFoundFocus = true;
			}
			else if (bOver == false && bAct && mlFocus == alId)
				mlFocus = 0;
		}
		if (mMods.mbUseUIPos)
		{
			const cVector2f &h = mMods.mvExpHori, &v = mMods.mvExpVert;
			cNavEntry e;
			e.mlId = alId;
			e.mvPos = cVector2l((int)(avPos.x - h.x * avSize.x), (int)(avPos.y - v.x * avSize.y));
			e.mvSize = cVector2l((int)((h.x + h.y) * avSize.x + avSize.x), (int)((v.x + v.y) * avSize.y + avSize.y));
			e.mlWrap = mlWrapMode;
			e.mlGroup = mlGroupFlags;
			mvNav.push_back(e);
		}
	}
	bool bIn = mMods.mbUseInput && mlFocus == alId;
	SetPrevFocus(aState, bIn);
	mPrev.mbMouseOver = bOver;
	return bIn;
}

bool cSomaImGui::BecamePressed(bool abKeys, bool abMouse)
{
	bool b = false;
	if (abKeys)
		b = ActionTriggered(2);
	if (abMouse)
		b = ActionTriggered(1) || b;
	return b;
}

const cSomaImGui::cNavEntry *cSomaImGui::FindNav(uint64_t alId)
{
	for (const cNavEntry &e : mvNav)
		if (e.mlId == alId)
			return &e;
	return NULL;
}

// cImGui::GetClosestUIPos: distance along the move axis between top-left corners, lanes must overlap
uint64_t cSomaImGui::NavClosest(const cNavEntry &aCur, const cVector2l &avDir, bool abAhead, bool abLoose)
{
	int mv = avDir.x == 0 ? 1 : 0, perp = 1 - mv;
	const cNavEntry *pBest = NULL;
	int lBest = 0;
	for (const cNavEntry &e : mvNav)
	{
		if (e.mlId == mlFocus || (e.mlGroup & aCur.mlGroup) == 0)
			continue;
		int lo = abLoose ? e.mvPos.v[perp] - e.mvSize.v[perp] / 2 : e.mvPos.v[perp];
		int ext = abLoose ? 2 * e.mvSize.v[perp] : e.mvSize.v[perp];
		if ((lo < aCur.mvPos.v[perp] + aCur.mvSize.v[perp] && aCur.mvPos.v[perp] < lo + ext) == false)
			continue;
		int lDelta = e.mvPos.v[mv] - aCur.mvPos.v[mv];
		if ((lDelta * avDir.v[mv] > 0) != abAhead)
			continue;
		int lDist = std::abs(lDelta);
		if (pBest == NULL || (abAhead ? lDist < lBest : lDist > lBest))
		{
			pBest = &e;
			lBest = lDist;
		}
	}
	return pBest ? pBest->mlId : 0;
}

void cSomaImGui::UpdateUIMovement()
{
	static const int kDirs[4][3] = {{4, 0, -1}, {5, 0, 1}, {6, 1, 0}, {7, -1, 0}};
	cVector2l vDir(0, 0);
	for (const auto &d : kDirs)
		if (ActionTriggered(d[0], true) && vDir.x == 0 && vDir.y == 0)
			vDir = cVector2l(d[1], d[2]);
	bool bConfirm = ActionTriggered(2, true);
	bool bMove = vDir.x != 0 || vDir.y != 0;
	if ((bMove == false && bConfirm == false) || mvNav.empty())
		return;
	const cNavEntry *pCur = FindNav(mlFocus);
	if (pCur == NULL)
	{
		mlFocus = mlPrevFocus = mvNav[0].mlId;
		mvLastDir = 0;
		mlLastCount = 0;
		return;
	}
	if (mvNav.size() < 2 || bMove == false)
		return;
	uint64_t lTarget = 0;
	if ((int)mvNav.size() == mlLastCount && mvLastDir.x == -vDir.x && mvLastDir.y == -vDir.y && FindNav(mlPrevFocus))
		lTarget = mlPrevFocus;
	else
	{
		bool bHori = vDir.x != 0;
		bool bWrap = pCur->mlWrap == 3 || (pCur->mlWrap == 1 && bHori) || (pCur->mlWrap == 2 && bHori == false);
		lTarget = NavClosest(*pCur, vDir, true, false);
		if (lTarget == 0 && bWrap)
			lTarget = NavClosest(*pCur, vDir, false, false);
		if (lTarget == 0)
			lTarget = NavClosest(*pCur, vDir, true, true);
		if (lTarget == 0 && bWrap)
			lTarget = NavClosest(*pCur, vDir, false, true);
	}
	if (lTarget)
	{
		mlPrevFocus = mlFocus;
		mlFocus = lTarget;
		mvLastDir = vDir;
		mlLastCount = (int)mvNav.size();
	}
}

bool cSomaImGui::DoButton(const tString &asName, const tWString &asText, const void *apData, cVector3f avPos, cVector2f avSize, int alMode)
{
	Layout(avPos, avSize, F<cVector2f>(apData, kWDefaultSize));
	uint64_t lId = asName.empty() ? Id(avPos.ToString()) : Id(asName);
	cState &st = State(lId);
	bool bOver = WidgetBase(lId, avPos, avSize, st);
	bool bMouseOver = mPrev.mbMouseOver;
	bool bClicked = bOver && BecamePressed(true, true);
	bool bDown = bOver && (mvActionDown[2] || (bMouseOver && mvActionDown[1]));
	bool bResult = bClicked;
	if (alMode == 1) // toggle
	{
		if (st.mbSetInt == false)
		{
			st.mlInt = 0;
			st.mbSetInt = true;
		}
		if (bClicked)
			st.mlInt = !st.mlInt;
		bResult = st.mlInt != 0;
	}
	else if (alMode == 2) // repeat
	{
		if (bDown)
		{
			st.mfFloat -= mfTimeStep;
			bResult = bClicked || st.mfFloat <= 0;
			if (st.mfFloat <= 0)
				st.mfFloat = 0.1f;
		}
		else
			st.mfFloat = 0.4f;
	}
	// cImGui::DoButtonBase: state colours replace the base, disabled multiplies frame and text
	bool bTrig = alMode == 1 ? bResult : alMode == 3 || (alMode != 4 && bDown && bOver);
	bool bUseTrigGfx = F<bool>(apData, kBUseTrigGfx), bUseFocusGfx = F<bool>(apData, kBUseInFocusGfx);
	bool bFocus = bOver && (alMode == 2 && bTrig && (bUseTrigGfx || F<bool>(apData, kBUseTrigColor))) == false;
	cColor col = F<cColor>(apData, kWColorBase);
	if (bTrig && bFocus && F<bool>(apData, kBUseTrigFocusColor) && bUseTrigGfx == false)
		col = F<cColor>(apData, kBColorTrigFocus);
	else if (bTrig && F<bool>(apData, kBUseTrigColor) && bUseTrigGfx == false)
		col = F<cColor>(apData, kBColorTrig);
	else if (bFocus && F<bool>(apData, kBUseInFocusColor) && bUseFocusGfx == false)
		col = F<cColor>(apData, kBColorInFocus);
	cColor textCol = F<cColor>(apData, kWColorText);
	if (mMods.mbUseInput == false && F<bool>(apData, kWUseDisabledColor))
	{
		col = Mul(col, F<cColor>(apData, kWColorDisabled));
		textCol = Mul(textCol, F<cColor>(apData, kWColorDisabled));
	}
	if (F<bool>(apData, kBUseFrame))
		DrawFrame((char *)apData + kBFrame, avPos, avSize, col);
	else if (F<bool>(apData, kWUseBg))
		DrawGfx((char *)apData + kWGfxBg, avPos, avSize, col);
	DrawText(asText, (char *)apData + kWFont, textCol, F<int>(apData, kWFontAlign), avPos, avSize, 1);
	if (bTrig && bUseTrigGfx)
	{
		cColor c(1, 1);
		if (bFocus && F<bool>(apData, kBUseTrigFocusColor))
			c = F<cColor>(apData, kBColorTrigFocus);
		else if (F<bool>(apData, kBUseTrigColor))
			c = F<cColor>(apData, kBColorTrig);
		DrawGfx((char *)apData + kBGfxTrig, avPos + cVector3f(0, 0, 0.15f), avSize, c);
	}
	if (bFocus && bUseFocusGfx)
		DrawGfx((char *)apData + kBGfxInFocus, avPos + cVector3f(0, 0, 0.1f), avSize,
				F<bool>(apData, kBUseInFocusColor) ? F<cColor>(apData, kBColorInFocus) : cColor(1, 1));
	mPrev.mbMouseOver = bMouseOver;
	mPrev.mbPressed = bDown;
	mPrev.mbBecamePressed = bClicked;
	Advance(avPos, avSize, bResult);
	return bResult;
}

void cSomaImGui::DoLabel(const tWString &asText, const void *apData, cVector3f avPos, cVector2f avSize, float afSizeMul)
{
	const void *pFont = (char *)apData + kWFont;
	cVector2f vDefault = F<cVector2f>(apData, kWDefaultSize);
	if (vDefault.x < 0)
		vDefault.x = GetFontLength(pFont, afSizeMul, asText);
	if (vDefault.y < 0)
		vDefault.y = FontSize(pFont, afSizeMul).y;
	Layout(avPos, avSize, vDefault);
	DrawWidgetBase(apData, avPos, avSize, false, false, -1, -1);
	DrawText(asText, pFont, F<cColor>(apData, kWColorText), F<int>(apData, kWFontAlign), avPos, avSize, afSizeMul);
	mPrev.mbMouseOver = MouseOver(avPos, avSize);
	Advance(avPos, avSize);
}

void cSomaImGui::DoImage(const void *apGfx, cVector3f avPos, cVector2f avSize)
{
	Layout(avPos, avSize, GetGfxSize(apGfx));
	DrawGfx(apGfx, avPos, avSize, cColor(1, 1));
	mPrev.mbMouseOver = MouseOver(avPos, avSize);
	mPrev.mbInFocus = mPrev.mbMouseOver;
	mPrev.mbBecameInFocus = false;
	mPrev.mbBecamePressed = mPrev.mbMouseOver && mvActionTriggered[1];
	mPrev.mbPressed = mPrev.mbMouseOver && mvActionDown[1];
	Advance(avPos, avSize);
}

float cSomaImGui::DoTextFrame(const tWString &asText, const cVector2f &avEdge, float afRowSpace, float afStartRow, const void *apData, cVector3f avPos,
							  cVector2f avSize)
{
	Layout(avPos, avSize, F<cVector2f>(apData, kWDefaultSize));
	DrawWidgetBase(apData, avPos, avSize, false, false, -1, -1);
	const void *pFont = (char *)apData + kWFont;
	iFontData *pFontData = GetFont(pFont);
	cVector2f vFont = FontSize(pFont, 1);
	tWStringVec vRows;
	if (pFontData)
		pFontData->GetWordWrapRows(avSize.x - avEdge.x * 2, vFont.y + afRowSpace, vFont, asText, &vRows);
	float fRowH = vFont.y + afRowSpace;
	float fY = avPos.y + avEdge.y - afStartRow * fRowH;
	for (size_t i = 0; i < vRows.size(); ++i)
	{
		if (fY >= avPos.y - 0.5f && fY + vFont.y <= avPos.y + avSize.y + 0.5f)
			DrawText(vRows[i], pFont, F<cColor>(apData, kWColorText), F<int>(apData, kWFontAlign), cVector3f(avPos.x + avEdge.x, fY, avPos.z),
					 cVector2f(avSize.x - avEdge.x * 2, vFont.y), 1);
		fY += fRowH;
	}
	Advance(avPos, avSize);
	if (fRowH <= 0)
		return 0;
	return cMath::Max((float)vRows.size() - (avSize.y - avEdge.y * 2 + afRowSpace) / fRowH, 0.0f);
}

void cSomaImGui::DoFrame(const void *apData, cVector3f avPos, cVector2f avSize)
{
	Layout(avPos, avSize, F<cVector2f>(apData, kWDefaultSize));
	DrawWidgetBase(apData, avPos, avSize, false, false, -1, -1);
	DrawFrame((char *)apData + 320, avPos, avSize, F<cColor>(apData, kWColorBase));
	Advance(avPos, avSize);
}

float cSomaImGui::DoSlider(const tString &asName, float afDefault, float afMin, float afMax, float afStep, const void *apData, cVector3f avPos,
						   cVector2f avSize, bool abVertical)
{
	Layout(avPos, avSize, F<cVector2f>(apData, kWDefaultSize));
	cState &st = State(Id(asName));
	DefaultOrCurrent(st, afDefault);
	bool bIn = WidgetBase(Id(asName), avPos, avSize, st);
	bool bOver = mPrev.mbMouseOver;
	float fOld = st.mfFloat;
	if (bIn)
	{
		float fStep = afStep > 0 ? afStep : (afMax - afMin) * 0.05f;
		int lInc = abVertical ? 5 : 6, lDec = abVertical ? 4 : 7;
		float d = (ActionTriggered(lInc) ? fStep : 0) - (ActionTriggered(lDec) ? fStep : 0);
		d += (ActionTriggered(8) ? fStep : 0) - (ActionTriggered(9) ? fStep : 0);
		if (abVertical)
			d = -d;
		if (d != 0)
			st.mfFloat = cMath::Clamp(st.mfFloat + d, cMath::Min(afMin, afMax), cMath::Max(afMin, afMax));
		if (bOver && ActionTriggered(1))
			st.mlInt = 1;
	}
	if (mvActionDown[1] == false)
		st.mlInt = 0;
	if (st.mlInt && avSize.x > 0 && avSize.y > 0)
	{
		mlMouseLock = 2;
		float t = abVertical ? 1 - (mvMousePos.y - avPos.y) / avSize.y : (mvMousePos.x - avPos.x) / avSize.x;
		float v = afMin + cMath::Clamp(t, 0.0f, 1.0f) * (afMax - afMin);
		if (afStep > 0)
			v = afMin + std::round((v - afMin) / afStep) * afStep;
		st.mfFloat = v;
	}
	DrawWidgetBase(apData, avPos, avSize, bIn, false, -1, -1);
	float t = afMax > afMin ? (st.mfFloat - afMin) / (afMax - afMin) : 0;
	cVector2f vButton = F<cVector2f>(apData, kSliderButtonSize);
	cVector3f vButtonPos = abVertical ? cVector3f(avPos.x, avPos.y + (1 - t) * (avSize.y - vButton.y), avPos.z + 0.1f)
									  : cVector3f(avPos.x + t * (avSize.x - vButton.x), avPos.y, avPos.z + 0.1f);
	if (F<bool>(apData, kSliderUseButton))
		DrawGfx((char *)apData + kSliderGfxButton, vButtonPos, vButton, cColor(1, 1));
	Advance(avPos, avSize, st.mfFloat != fOld);
	return st.mfFloat;
}

bool cSomaImGui::DoCheckBox(const tString &asName, const tWString &asText, bool abDefault, const void *apData, cVector3f avPos, cVector2f avSize)
{
	Layout(avPos, avSize, F<cVector2f>(apData, kWDefaultSize));
	cState &st = State(Id(asName));
	DefaultOrCurrent(st, (int)abDefault);
	bool bOver = WidgetBase(Id(asName), avPos, avSize, st);
	bool bToggled = bOver && BecamePressed(true, true);
	if (bToggled)
		st.mlInt = !st.mlInt;
	DrawWidgetBase(apData, avPos, avSize, bOver, false, -1, -1);
	cVector2f vBox = F<cVector2f>(apData, kCheckBoxSize);
	DrawGfx((char *)apData + kCheckGfxBox, avPos + cVector3f(0, 0, 0.1f), vBox, cColor(1, 1));
	if (st.mlInt)
		DrawGfx((char *)apData + kCheckGfxOverlay, avPos + cVector3f(0, 0, 0.2f), F<cVector2f>(apData, kCheckOverlaySize), cColor(1, 1));
	DrawText(asText, (char *)apData + kWFont, F<cColor>(apData, kWColorText), eFontAlign_Left, avPos + cVector3f(vBox.x + 4, 0, 0), avSize - cVector2f(vBox.x + 4, 0), 1);
	Advance(avPos, avSize, bToggled);
	return st.mlInt != 0;
}

int cSomaImGui::DoMultiSelect(const tString &asName, int alDefault, const void *apData, cVector3f avPos, cVector2f avSize)
{
	Layout(avPos, avSize, F<cVector2f>(apData, kWDefaultSize));
	cState &st = State(Id(asName));
	DefaultOrCurrent(st, alDefault);
	int lNum = (int)mvItems.size();
	int lOld = st.mlInt;
	cVector2f vArrow = F<cVector2f>(apData, kMultiArrowSize);
	bool bOver = WidgetBase(Id(asName), avPos, avSize, st);
	if (bOver && lNum > 0)
	{
		int d = ActionTriggered(2) + ActionTriggered(6) - ActionTriggered(7);
		if (ActionTriggered(1) && mPrev.mbMouseOver)
			d += mvMousePos.x < avPos.x + avSize.x * 0.5f ? -1 : 1;
		st.mlInt = ((st.mlInt + d) % lNum + lNum) % lNum;
	}
	if (lNum > 0)
		st.mlInt = cMath::Clamp(st.mlInt, 0, lNum - 1);
	DrawWidgetBase(apData, avPos, avSize, bOver, false, -1, -1);
	DrawGfx((char *)apData + kMultiGfxArrowLeft, avPos + cVector3f(0, (avSize.y - vArrow.y) * 0.5f, 0.1f), vArrow, cColor(1, 1));
	DrawGfx((char *)apData + kMultiGfxArrowRight, avPos + cVector3f(avSize.x - vArrow.x, (avSize.y - vArrow.y) * 0.5f, 0.1f), vArrow, cColor(1, 1));
	if (st.mlInt >= 0 && st.mlInt < lNum)
		DrawText(mvItems[st.mlInt], (char *)apData + kWFont, F<cColor>(apData, kWColorText), eFontAlign_Center, avPos, avSize, 1);
	mvItems.clear();
	Advance(avPos, avSize, st.mlInt != lOld);
	return st.mlInt;
}

void cSomaImGui::DoGauge(const void *apData, float afFill, cVector3f avPos, cVector2f avSize)
{
	Layout(avPos, avSize, F<cVector2f>(apData, kWDefaultSize));
	if (F<bool>(apData, kGaugeUseFrame))
		DrawFrame((char *)apData + kGaugeFrame, avPos, avSize, F<cColor>(apData, kWColorBase));
	cVector2f vPad = F<cVector2f>(apData, kGaugePadding);
	cVector2f vInner = avSize - vPad * 2;
	float fFill = cMath::Clamp(afFill, 0.0f, 1.0f);
	bool bVert = F<int>(apData, kGaugeOrient) == 1;
	cVector2f vFill = bVert ? cVector2f(vInner.x, vInner.y * fFill) : cVector2f(vInner.x * fFill, vInner.y);
	cVector3f vFillPos = avPos + cVector3f(vPad.x, vPad.y + (bVert ? vInner.y - vFill.y : 0), 0.1f);
	DrawGfx((char *)apData + kGaugeFill, vFillPos, vFill, F<cColor>(apData, kWColorBase));
	Advance(avPos, avSize);
}

void cSomaImGui::DoWindowStart(const tWString &asCaption, const void *apData, cVector3f avPos, cVector2f avSize, bool abClip)
{
	Layout(avPos, avSize, F<cVector2f>(apData, kWDefaultSize));
	cColor colBase = F<cColor>(apData, kWColorBase);
	DrawFrame((char *)apData + kWindowFrame, avPos, avSize, colBase);
	float fHeader = 0;
	if (F<bool>(apData, kWUseHeader))
	{
		const void *pFont = (char *)apData + kWFont;
		const float *pLabelPad = &F<float>(apData, kWindowLabelPadTop);
		float fPadT = pLabelPad[0], fPadR = pLabelPad[1], fPadB = pLabelPad[2], fPadL = pLabelPad[3];
		cVector3f vOffset = F<cVector3f>(apData, kWLabelOffset);
		float fMul = F<float>(apData, kWCaptionSizeMul);
		cVector2f vFont = FontSize(pFont, fMul);
		cVector3f vPos = avPos + cVector3f(vOffset.x, vOffset.y, 0.1f);
		cVector2f vSize(vFont.x, fPadT + fPadB + vFont.y);
		int lType = F<int>(apData, kWHeaderType);
		if (lType == 1)
			vSize.x = std::max(GetFontLength(pFont, fMul, asCaption) + fPadR + fPadL, F<float>(apData, kWindowLabelMinWidth));
		else if (lType == 0)
			vSize.x = avSize.x - 2 * vOffset.x;
		DrawFrame((char *)apData + kWLabelFrame, vPos, vSize, colBase);
		cVector3f vText = vPos + cVector3f(fPadL, fPadT, 0);
		int lAlign = F<int>(apData, kWFontAlign);
		float fFree = vSize.x - (2 * vOffset.x + fPadR + fPadL);
		if (fFree > 0)
		{
			vText.y += (vSize.y - (fPadT + fPadB + vOffset.y) - vFont.y) * 0.5f;
			vText.z += 0.2f;
			if (lAlign == eFontAlign_Right)
				vText.x += fFree;
			else if (lAlign == eFontAlign_Center)
				vText.x += fFree * 0.5f;
		}
		DrawFont(asCaption, pFont, vText, lAlign, cVector2f(fMul), F<cColor>(apData, kWColorText));
		fHeader = vSize.y;
	}
	float fTop = F<float>(apData, kWindowPadTop), fLeft = F<float>(apData, kWindowPadLeft);
	float fRight = F<float>(apData, kWindowPadRight), fBottom = F<float>(apData, kWindowPadBottom);
	cGroup g;
	g.mvPos = avPos + cVector3f(fLeft, fHeader + fTop, 0.3f);
	g.mvSize = avSize - cVector2f(fLeft + fRight, fTop + fBottom + fHeader);
	g.mbClip = abClip;
	PushGroup(g);
	mPrev.mvPos = avPos;
	mPrev.mvSize = avSize;
}

void cSomaImGui::DoWindowEnd()
{
	PopGroup();
}

void cSomaImGui::DoMouse(const void *apGfx, const cVector3f &avOffset, cVector2f avSize)
{
	DrawGfx(apGfx, cVector3f(mvMousePos.x, mvMousePos.y, 100) + avOffset, avSize, cColor(1, 1));
}

typedef const S_ &D; // a script struct passed by reference
typedef const tString &Str;
typedef const tWString &WStr;
typedef const cVector3f &V3;
typedef const cVector2f &V2;
typedef cSomaImGui I;

static const void *P(D d) { return &d; }

static void DrawLine(I *p, V2 a, V2 b, float z, float t, const cColor &c, const void *g)
{
	cVector2f d = b - a;
	float fLen = d.Length(), fAngle = p->mMods.mfRotateAngle;
	p->mMods.mfRotateAngle += atan2f(d.y, d.x);
	p->DrawGfx(g, cVector3f((a.x + b.x - fLen) * 0.5f, (a.y + b.y - t) * 0.5f, z), cVector2f(fLen, t), c);
	p->mMods.mfRotateAngle = fAngle;
}

// cLuxInputHandler glyph layouts and cLuxScreenTextIcon (HPL3 FetchKeyboardInputLayout, ParseStringIntoScreenText, Icon::Draw)
namespace
{
	struct cKeyLayout
	{
		int mlType, mlMin, mlMax;
		char mGfx[kGfxSize], mLabel[336];
	};
	std::vector<cKeyLayout> gvKeyLayouts;
	cKeyLayout gKeyDefault{-1, 0, 0, {}, {}};
	tStringVec gvMouseLayout;

	struct cIcon
	{
		char mHeader[16];
		float mfTimer, mfX, mfW, mfH;
		int mlCol, mlChars, mlLayers;
		char mGfx[3][kGfxSize], mLabel[336];
		wchar_t msText[32];
	};
	static_assert(sizeof(cIcon) <= 4096, "script struct block");
}

static cIcon *NewIcon(iSubAction *apSub, const void *apParams)
{
	cIcon *pIcon = NULL;
	if (apSub->GetInputType() == "Keyboard")
	{
		eKey key = static_cast<cActionKeyboard *>(apSub)->GetKey();
		const cKeyLayout *pLayout = &gKeyDefault;
		for (const cKeyLayout &l : gvKeyLayouts)
			if (l.mlMin <= key && key <= l.mlMax)
			{
				pLayout = &l;
				break;
			}
		if (pLayout->mlType < 0)
			return NULL;
		pIcon = (cIcon *)SomaNewOwnedScriptStruct("cLuxScreenTextIcon");
		memcpy(pIcon->mGfx[0], pLayout->mGfx, kGfxSize);
		memcpy(pIcon->mLabel, pLayout->mLabel, sizeof(pIcon->mLabel));
		if (pLayout->mlType != 0)
		{
			tString sKey = gpSomaBase->mpEngine->GetInput()->GetKeyboard()->KeyToString(key);
			tWString sName = gpSomaBase->mpEngine->GetResources()->Translate("ButtonNames", sKey);
			wcsncpy(pIcon->msText, (sName.empty() ? cString::To16Char(sKey) : sName).c_str(), 31);
		}
		pIcon->mfH = F<float>(apParams, 364);
	}
	else if (apSub->GetInputType() == "MouseButton")
	{
		size_t lButton = static_cast<cActionMouseButton *>(apSub)->GetButton();
		tStringVec vFiles;
		tString sSep = ",";
		if (lButton < gvMouseLayout.size())
			cString::GetStringVec(gvMouseLayout[lButton], vFiles, &sSep);
		if (vFiles.empty())
			return NULL;
		pIcon = (cIcon *)SomaNewOwnedScriptStruct("cLuxScreenTextIcon");
		void *pDefault = SomaNewScriptStruct("cImGuiGfx");
		pIcon->mlLayers = std::min<int>(vFiles.size(), 3) - 1;
		for (int i = 0; i <= pIcon->mlLayers; ++i)
		{
			memcpy(pIcon->mGfx[i], pDefault, kGfxSize);
			F<const tString *>(pIcon->mGfx[i], kGfxFile) = SomaIntern(vFiles[i]);
			F<int>(pIcon->mGfx[i], kGfxMaterial) = eGuiMaterial_Alpha;
			F<int>(pIcon->mGfx[i], kGfxType) = 0;
		}
		free(pDefault);
		pIcon->mfH = F<float>(apParams, 368);
	}
	return pIcon;
}

// "$Input{Action}" -> one '.' per glyph icon when apIcons is set, else translated button names
static tWString ParseString(const tWString &asText, std::vector<cIcon *> *apIcons, const void *apParams)
{
	tWString sOut;
	size_t lPos = 0;
	while (true)
	{
		size_t lStart = asText.find(_W("$Input{"), lPos);
		size_t lEnd = lStart == tWString::npos ? tWString::npos : asText.find(_W('}'), lStart);
		if (lEnd == tWString::npos)
			break;
		sOut += asText.substr(lPos, lStart - lPos);
		lPos = lEnd + 1;
		cAction *pAction = gpSomaBase->mpEngine->GetInput()->GetAction(cString::To8Char(asText.substr(lStart + 7, lEnd - lStart - 7)));
		if (pAction == NULL || pAction->GetSubActionNum() == 0)
		{
			sOut += _W("BADACTION");
			continue;
		}
		for (int i = 0; i < pAction->GetSubActionNum(); ++i)
		{
			if (i > 0)
				sOut += _W(" / ");
			iSubAction *pSub = pAction->GetSubAction(i);
			if (cIcon *pIcon = apIcons ? NewIcon(pSub, apParams) : NULL)
			{
				pIcon->mlCol = sOut.size();
				sOut += _W('.');
				apIcons->push_back(pIcon);
				continue;
			}
			tWString sName = gpSomaBase->mpEngine->GetResources()->Translate("ButtonNames", pSub->GetInputName());
			sOut += sName.empty() ? cString::To16Char(pSub->GetInputName()) : sName;
		}
	}
	return sOut + asText.substr(lPos);
}

tWString SomaParseString(const tWString &asText) { return ParseString(asText, NULL, NULL); }

static void ParseScreenText(WStr asInput, I *p, D aParams, CScriptArray &aLines, CScriptArray &aIcons, CScriptArray &aIconsPerLine, float &afLineHeight,
							float &afTime, bool)
{
	const void *pFont = (char *)P(aParams) + 24 + kWFont;
	std::vector<cIcon *> vIcons;
	tWString sText = ParseString(asInput, &vIcons, P(aParams));
	cVector2f vSize = p->FontSize(pFont, 1);
	float fDot = std::max(p->GetFontLength(pFont, 1, _W(".")), 1e-4f), fSpace = std::max(p->GetFontLength(pFont, 1, _W(" ")), 1e-4f);
	afLineHeight = vSize.y;
	int lInserted = 0;
	for (cIcon *pIcon : vIcons)
	{
		pIcon->mlCol += lInserted;
		cVector2f vGfx = p->GetGfxSize(pIcon->mGfx[0]);
		if (vGfx.x <= 0)
			vGfx.x = vSize.x;
		if (vGfx.y <= 0)
			vGfx.y = vSize.y;
		if (pIcon->mfH < 0 || pIcon->mfH > vGfx.y)
			pIcon->mfH = vGfx.y;
		pIcon->mfW = vGfx.x / vGfx.y * pIcon->mfH;
		if (pIcon->msText[0])
			pIcon->mfW = std::max(pIcon->mfW, p->GetFontLength(pIcon->mLabel + kWFont, 1, pIcon->msText) + 4 * fSpace);
		pIcon->mlChars = std::max(1, (int)ceilf(pIcon->mfW / fDot));
		sText.insert(pIcon->mlCol, pIcon->mlChars - 1, _W('.'));
		lInserted += pIcon->mlChars - 1;
		afLineHeight = std::max(afLineHeight, pIcon->mfH);
	}
	afTime = SomaStringDuration(sText);
	float fWidth = F<float>(P(aParams), 16);
	tWStringVec vRows;
	iFontData *pFontData = p->GetFont(pFont);
	if (fWidth > 0 && pFontData)
		pFontData->GetWordWrapRows(fWidth, vSize.y, vSize, sText, &vRows);
	else
		vRows.push_back(sText);
	aIconsPerLine.Resize(vRows.size());
	size_t lStart = 0, k = 0;
	for (size_t r = 0; r < vRows.size(); ++r)
	{
		tWString &sRow = vRows[r];
		size_t lEnd = lStart + sRow.size();
		int lShift = 0;
		for (; k < vIcons.size() && vIcons[k]->mlCol < (int)lEnd; ++k)
		{
			cIcon *pIcon = vIcons[k];
			int lCol = pIcon->mlCol - lStart + lShift, lSpaces = (int)ceilf(pIcon->mlChars * fDot / fSpace);
			sRow.replace(lCol, pIcon->mlChars, lSpaces, _W(' '));
			lShift += lSpaces - pIcon->mlChars;
			pIcon->mfX = p->GetFontLength(pFont, 1, sRow.substr(0, lCol)) + (lSpaces * fSpace - pIcon->mfW) * 0.5f;
			int lIndex = k;
			((CScriptArray *)aIconsPerLine.At(r))->InsertLast(&lIndex);
		}
		lStart = lEnd < sText.size() && (sText[lEnd] == _W(' ') || sText[lEnd] == _W('\n')) ? lEnd + 1 : lEnd;
		aLines.InsertLast(&sRow);
	}
	aIcons.Resize(vIcons.size());
	for (size_t i = 0; i < vIcons.size(); ++i)
		*(cIcon **)aIcons.At(i) = vIcons[i];
}

static void DrawIcon(I *p, cIcon *apIcon, float afTimeStep, const cVector3f &avBase)
{
	apIcon->mfTimer += afTimeStep;
	cVector3f vPos(avBase.x + apIcon->mfX, avBase.y - apIcon->mfH * 0.5f, avBase.z);
	cVector2f vSize(apIcon->mfW, apIcon->mfH);
	p->DoImage(apIcon->mGfx[0], vPos, vSize);
	float t = apIcon->mlLayers > 0 ? fmodf(apIcon->mfTimer, apIcon->mlLayers) : 0;
	for (int i = 0; i < apIcon->mlLayers; ++i)
	{
		F<cColor>(apIcon->mGfx[i + 1], kGfxColor) = cColor(1, t > i && t < i + 1 ? sinf((t - i) * kPif) : 0);
		p->DoImage(apIcon->mGfx[i + 1], vPos + cVector3f(0, 0, 0.01f), vSize);
	}
	if (apIcon->msText[0])
		p->DoLabel(apIcon->msText, apIcon->mLabel, vPos, vSize, 1);
}

// HPL3 DrawHint: first line centred, the rest left-aligned under its start
static void DrawScreenText(I *p, float afTimeStep, D aLabel, V3 avPos, float afLineWidth, float afLineSpacing, const CScriptArray &avLines,
						   const CScriptArray &avIcons, const CScriptArray &avIconsPerLine, bool abHint)
{
	char vLabel[336];
	memcpy(vLabel, P(aLabel), sizeof(vLabel));
	const void *pFont = vLabel + kWFont;
	float fFontH = p->FontSize(pFont, 1).y;
	int lAlign = F<int>(vLabel, kWFontAlign);
	float fHintX = avLines.GetSize() ? avPos.x + (afLineWidth - p->GetFontLength(pFont, 1, *(const tWString *)avLines.At(0))) * 0.5f : 0;
	for (asUINT i = 0; i < avLines.GetSize(); ++i)
	{
		const tWString &sLine = *(const tWString *)avLines.At(i);
		cVector3f vPos(abHint && i > 0 ? fHintX : avPos.x, avPos.y + i * afLineSpacing, avPos.z);
		if (abHint)
			F<int>(vLabel, kWFontAlign) = i == 0 ? eFontAlign_Center : eFontAlign_Left;
		p->DoLabel(sLine, vLabel, vPos, cVector2f(afLineWidth, fFontH), 1);
		if (i >= avIconsPerLine.GetSize())
			continue;
		float fLen = p->GetFontLength(pFont, 1, sLine);
		float fX = abHint ? fHintX : lAlign == eFontAlign_Right ? avPos.x + afLineWidth - fLen : lAlign == eFontAlign_Center ? avPos.x + (afLineWidth - fLen) * 0.5f : avPos.x;
		const CScriptArray &vIndices = *(const CScriptArray *)avIconsPerLine.At(i);
		for (asUINT j = 0; j < vIndices.GetSize(); ++j)
		{
			asUINT lIdx = *(const int *)vIndices.At(j);
			if (lIdx < avIcons.GetSize())
				DrawIcon(p, *(cIcon *const *)avIcons.At(lIdx), afTimeStep, cVector3f(fX, vPos.y + fFontH * 0.5f, avPos.z));
		}
	}
}

static int SomaMultiToggle(I *p, Str n, int def, asUINT alCols, V2 spacing, const void *apData, V3 pos, V2 size)
{
	auto &st = p->State(Id(n));
	cSomaImGui::DefaultOrCurrent(st, def);
	std::vector<tWString> vItems = p->mvItems;
	p->mvItems.clear();
	bool bUpdated = false;
	if (vItems.empty() == false)
	{
		size_t cols = alCols ? std::min<size_t>(alCols, vItems.size()) : vItems.size();
		size_t rows = (vItems.size() + cols - 1) / cols;
		cVector2f vItem((size.x - (cols - 1) * spacing.x) / cols, (size.y - (rows - 1) * spacing.y) / rows);
		p->mvGroups.push_back({p->GroupPos() + pos, size});
		for (size_t i = 0; i < vItems.size(); ++i)
		{
			cVector3f vPos((vItem.x + spacing.x) * (float)(i % cols), (vItem.y + spacing.y) * (float)(i / cols), 0);
			if (p->DoButton(n + "_" + cString::ToString((int)i), vItems[i], apData, vPos, vItem, st.mlInt == (int)i ? 3 : 4))
			{
				bUpdated = st.mlInt != (int)i;
				st.mlInt = (int)i;
			}
		}
		p->mvGroups.pop_back();
	}
	p->mPrev.mbUpdated = bUpdated;
	return st.mlInt;
}

static void GfxFactory(asIScriptGeneric *g)
{
	void *p = SomaNewOwnedScriptStruct("cImGuiGfx");
	int n = g->GetArgCount();
	for (int i = 0; i < n; ++i)
	{
		int lType = g->GetArgTypeId(i);
		asITypeInfo *pInfo = g->GetEngine()->GetTypeInfoById(lType);
		if (pInfo && strcmp(pInfo->GetName(), "cImGuiGfx") == 0)
			memcpy((char *)p + 16, (char *)g->GetArgObject(i) + 16, kGfxSize - 16);
		else if (pInfo && strcmp(pInfo->GetName(), "tString") == 0)
			F<const tString *>(p, kGfxFile) = SomaIntern(*(tString *)g->GetArgObject(i));
		else if (pInfo && strcmp(pInfo->GetName(), "eGuiMaterial") == 0)
			F<int>(p, kGfxMaterial) = *(int *)g->GetAddressOfArg(i);
		else if (pInfo && strcmp(pInfo->GetName(), "eImGuiGfx") == 0)
			F<int>(p, kGfxType) = *(int *)g->GetAddressOfArg(i);
	}
	*(void **)g->GetAddressOfReturnLocation() = p;
}

static void FontFactory(asIScriptGeneric *g)
{
	void *p = SomaNewOwnedScriptStruct("cImGuiFont");
	if (g->GetArgCount() == 2)
	{
		F<const tString *>(p, kFontFile) = SomaIntern(*(tString *)g->GetArgObject(0));
		F<cVector2f>(p, kFontSize) = *(cVector2f *)g->GetArgObject(1);
	}
	*(void **)g->GetAddressOfReturnLocation() = p;
}

// cLuxScriptImGui: a script-owned ImGui drawn by the GUI handler
struct cSomaScriptImGui
{
	void *mvPad[2] = {};
	cSomaImGui *mpImGui;
	void *mpProp = NULL;
};
static_assert(offsetof(cSomaScriptImGui, mpImGui) == 16, "script property mpImGui");

static cSomaImGui *gpHudImGui = NULL;

// Matches the official engine at 4:3, 16:10 and 16:9: a 1024x768 centre area, widened by 4/3 of the extra width
static cVector2f HudVirtualSize()
{
	cVector2f vScreen = gpSomaBase->mpEngine->GetGraphics()->GetLowLevel()->GetScreenSizeFloat();
	return cVector2f((4.0f * 768.0f * vScreen.x / vScreen.y - 1024.0f) / 3.0f, 768.0f);
}

static cVector2f gvHudSize, gvHudOffset, gvHudCenter(1024, 768);
static cVector3f gvHudStart;

static void Hud()
{
	gvHudSize = HudVirtualSize();
	gvHudOffset = cVector2f((gvHudSize.x - 1024.0f) * 0.5f, 0);
	gvHudStart = cVector3f(-gvHudOffset.x, 0, 0);
}

static void SetHudVirtualSize(cGuiSet *apSet)
{
	cVector2f vSize = HudVirtualSize();
	apSet->SetVirtualSize(vSize, -1000, 1000, cVector2f((vSize.x - 1024.0f) * 0.5f, 0));
}
static std::vector<cSomaScriptImGui *> gvScriptImGuis;
static std::vector<cGuiSet *> gvHudSets;

cSomaImGui *SomaHudImGui()
{
	if (gpHudImGui == NULL)
	{
		cGui *pGui = gpSomaBase->mpEngine->GetGui();
		cGuiSet *pSet = pGui->CreateSet("GameHud", pGui->CreateSkin("gui_default.skin"));
		SetHudVirtualSize(pSet);
		gvHudSets.push_back(pSet);
		cViewport *pViewport = gpSomaBase->mpEngine->GetScene()->CreateViewport(NULL, NULL, false);
		pViewport->AddGuiSet(pSet);
		gpHudImGui = new cSomaImGui("GameHud", pSet);
	}
	return gpHudImGui;
}

void SomaDrawImGuis()
{
	static cVector2l vLastScreen(0, 0);
	cVector2l vScreen = gpSomaBase->mpEngine->GetGraphics()->GetLowLevel()->GetScreenSizeInt();
	if (vScreen != vLastScreen)
	{
		vLastScreen = vScreen;
		for (cGuiSet *pSet : gvHudSets)
			if (pSet->Is3D() == false)
				SetHudVirtualSize(pSet);
	}
	if (gpHudImGui)
		gpHudImGui->DrawAll();
	for (cSomaScriptImGui *p : gvScriptImGuis)
		p->mpImGui->DrawAll();
}

void cSomaImGui::RegisterNatives(asIScriptEngine *e)
{
	SOMA_METHOD(e, "cLuxGuiHandler", "void CreateCameraTexture(const tString&in asName, const cVector2l&in avSize, uint alFrameRate, float afFOV, float afNearPlane, float afFarPlane)",
				+[](void *, Str s, const cVector2l &v, unsigned f, float fov, float n, float fa) { CreateCameraTexture(s, v, f, fov, n, fa); });
	SOMA_METHOD(e, "cLuxGuiHandler", "void DestroyCameraTexture(const tString&in asName)", +[](void *, Str s) { DestroyCameraTexture(s); });
	SOMA_METHOD(e, "cLuxGuiHandler", "void SetCameraTextureSettings(const tString&in asName, float afFOV, float afNearPlane, float afFarPlane)",
				+[](void *, Str s, float fov, float n, float f) {
					auto it = gmapCameraTextures.find(s);
					if (it == gmapCameraTextures.end())
						return;
					it->second->mpCamera->SetFOV(cMath::ToRad(fov));
					it->second->mpCamera->SetNearClipPlane(n);
					it->second->mpCamera->SetFarClipPlane(f);
				});
	SOMA_METHOD(e, "cLuxGuiHandler", "void SetCameraTextureMatrix(const tString&in asName, const cMatrixf&in a_mtxCamera)", +[](void *, Str s, const cMatrixf &m) {
		auto it = gmapCameraTextures.find(s);
		if (it == gmapCameraTextures.end())
			return;
		it->second->mpCamera->SetRotationMatrix(m.GetRotation());
		it->second->mpCamera->SetPosition(m.GetTranslation());
		it->second->mbAttached = false;
	});
	SOMA_METHOD(e, "cLuxGuiHandler", "void AttachCameraTextureToEntity(const tString&in asName, iLuxEntity@ apEnt)", +[](void *, Str s, cSomaLuxEntity *pEnt) {
		auto it = gmapCameraTextures.find(s);
		if (it == gmapCameraTextures.end() || pEnt == NULL)
			return;
		it->second->mAttached = pEnt->mID;
		it->second->mbAttached = true;
	});
	const char *vGfxFactories[] = {"cImGuiGfx@ f()", "cImGuiGfx@ f(const tString &in asFile)", "cImGuiGfx@ f(const tString &in asFile, eGuiMaterial aMat)",
								   "cImGuiGfx@ f(const tString &in asFile, eGuiMaterial aMat, eImGuiGfx aType)",
								   "cImGuiGfx@ f(const tString &in asFile, eImGuiGfx aType)", "cImGuiGfx@ f(const cImGuiGfx &in aGfx)"};
	for (const char *pDecl : vGfxFactories)
		e->RegisterObjectBehaviour("cImGuiGfx", asBEHAVE_FACTORY, pDecl, asFUNCTION(GfxFactory), asCALL_GENERIC);
	e->RegisterObjectBehaviour("cImGuiFont", asBEHAVE_FACTORY, "cImGuiFont@ f()", asFUNCTION(FontFactory), asCALL_GENERIC);
	e->RegisterObjectBehaviour("cImGuiFont", asBEHAVE_FACTORY, "cImGuiFont@ f(const tString&in asFile, const cVector2f &in avSize)", asFUNCTION(FontFactory),
							   asCALL_GENERIC);
	SOMA_METHOD(e, "cImGuiGfx", "void SetFile(const tString&in asFile)", +[](S_ &g, Str s) { F<const tString *>(&g, kGfxFile) = SomaIntern(s); });
	SOMA_METHOD(e, "cImGuiGfx", "const tString& GetFile()const", +[](S_ &g) -> const tString & { return StrAt(&g, kGfxFile); });
	SOMA_METHOD(e, "cImGuiGfx", "void CopyFrom(const cImGuiGfx &in aGfx)", +[](S_ &g, D o) { memcpy((char *)&g + 16, (char *)&o + 16, kGfxSize - 16); });
	SOMA_METHOD(e, "cImGuiGfx", "uint64 GetId()", +[](S_ &g) { return (asQWORD)SomaHash64(StrAt(&g, kGfxFile)); });
	SOMA_METHOD(e, "cImGuiWindowData", "void SetLabelPadding(float afTop, float afRight, float afBottom, float afLeft)", +[](S_ &g, float t, float r, float b, float l) {
		F<float>(&g, kWindowLabelPadTop) = t, F<float>(&g, kWindowLabelPadTop + 4) = r, F<float>(&g, kWindowLabelPadTop + 8) = b, F<float>(&g, kWindowLabelPadTop + 12) = l;
	});
	SOMA_METHOD(e, "cImGuiWindowData", "void SetPadding(float afTop, float afRight, float afBottom, float afLeft)", +[](S_ &g, float t, float r, float b, float l) {
		F<float>(&g, kWindowPadTop) = t, F<float>(&g, kWindowPadRight) = r, F<float>(&g, kWindowPadBottom) = b, F<float>(&g, kWindowPadLeft) = l;
	});
	SOMA_METHOD(e, "cImGuiFont", "void SetFile(const tString&in asFile)", +[](S_ &g, Str s) { F<const tString *>(&g, kFontFile) = SomaIntern(s); });
	SOMA_METHOD(e, "cImGuiFont", "const tString& GetFile()const", +[](S_ &g) -> const tString & { return StrAt(&g, kFontFile); });
	SOMA_METHOD(e, "cImGuiFont", "uint64 GetId()", +[](S_ &g) { return (asQWORD)SomaHash64(StrAt(&g, kFontFile)); });
	SOMA_METHOD(e, "cImGuiFrameGfx", "void CopyFrom(const cImGuiFrameGfx &in aFrame)", +[](S_ &g, D o) { memcpy((char *)&g + 16, (char *)&o + 16, 1592 - 16); });

	SOMA_FUNC(e, "cImGui@ cLux_GetCurrentImGui()", +[]() { return cSomaImGui::GetCurrent() ? cSomaImGui::GetCurrent() : SomaHudImGui(); });
	SOMA_FUNC(e, "cImGui@ cLux_GetGameHudImGui()", +[]() { return SomaHudImGui(); });
	SOMA_FUNC(e, "cGuiSet@ cLux_GetGameHudSet()", +[]() { return SomaHudImGui()->GetSet(); });
	SOMA_FUNC(e, "const cVector2f& cLux_GetHudVirtualCenterSize()", +[]() -> const cVector2f & { return gvHudCenter; });
	SOMA_FUNC(e, "const cVector2f& cLux_GetHudVirtualSize()", +[]() -> const cVector2f & { Hud(); return gvHudSize; });
	SOMA_FUNC(e, "const cVector2f& cLux_GetHudVirtualOffset()", +[]() -> const cVector2f & { Hud(); return gvHudOffset; });
	SOMA_FUNC(e, "const cVector3f& cLux_GetHudVirtualStartPos()", +[]() -> const cVector3f & { Hud(); return gvHudStart; });
	SOMA_FUNC(e, "const cVector2f& cLux_GetHudVirtualCenterScreenSize()", +[]() -> const cVector2f & { Hud(); return gvHudSize; });
	SOMA_FUNC(e, "const cVector3f& cLux_GetHudVirtualCenterScreenStartPos()", +[]() -> const cVector3f & { Hud(); return gvHudStart; });
	SOMA_FUNC(e, "void cLux_SetImGuiInputFocus(cImGui@ apImGui, bool abShowMouse)", +[](I *p, bool b) { cSomaImGui::SetInputFocus(p, b); });
	SOMA_FUNC(e, "void cLux_ParseStringIntoScreenText(const tWString &in asInput, cImGui @apImGui, const cLuxScreenTextFormatParameters &aFormatParams, array<tWString> &out aOutLines, array<cLuxScreenTextIcon@> &out aIconArray, array<array<int>> &out aOutIconsPerLine, float &out afMaxLineHeight, float &out afDisplayTime, bool abTriggeredByGamepad)",
			  ParseScreenText);
	SOMA_FUNC(e, "void cLux_DrawScreenText(cImGui @apImGui, float afTimeStep, const cImGuiLabelData &in aLabel, const cVector3f &in avPosition, float afLineWidth, float afLineSpacing, const array<tWString> &in avTextLines, const array<cLuxScreenTextIcon@> &in avIcons, const array<array<int>> &in avIconsPerLine)",
			  +[](I *p, float dt, D l, V3 pos, float w, float sp, const CScriptArray &lines, const CScriptArray &icons, const CScriptArray &perLine) {
				  DrawScreenText(p, dt, l, pos, w, sp, lines, icons, perLine, false);
			  });
	SOMA_FUNC(e, "void cLux_DrawHint(cImGui @apImGui, float afTimeStep, const cImGuiLabelData &in aLabel, const cVector3f &in avPosition, float afLineWidth, float afLineSpacing, const array<tWString> &in avTextLines, const array<cLuxScreenTextIcon@> &in avIcons, const array<array<int>> &in avIconsPerLine)",
			  +[](I *p, float dt, D l, V3 pos, float w, float sp, const CScriptArray &lines, const CScriptArray &icons, const CScriptArray &perLine) {
				  DrawScreenText(p, dt, l, pos, w, sp, lines, icons, perLine, true);
			  });
	const char *IH = "cLuxInputHandler";
	SOMA_METHOD(e, IH, "void ClearKeyboardLayout()", +[](void *) { gvKeyLayouts.clear(); });
	SOMA_METHOD(e, IH, "void AddKeyboardLayoutRange(eKey aFirstKey, eKey aLastKey, eLuxKeyboardLayoutType aType, const cImGuiGfx &in aGfxKey, const cImGuiLabelData &in aLabelKey)",
				+[](void *, int a, int b, int t, D g, D l) {
					gvKeyLayouts.push_back({t, a, b, {}, {}});
					memcpy(gvKeyLayouts.back().mGfx, P(g), kGfxSize);
					memcpy(gvKeyLayouts.back().mLabel, P(l), sizeof(cKeyLayout::mLabel));
				});
	SOMA_METHOD(e, IH, "void AddKeyboardLayoutKey(eKey aKey, eLuxKeyboardLayoutType aType, const cImGuiGfx &in aGfxKey, const cImGuiLabelData &in aLabelKey)",
				+[](void *, int k, int t, D g, D l) {
					gvKeyLayouts.push_back({t, k, k, {}, {}});
					memcpy(gvKeyLayouts.back().mGfx, P(g), kGfxSize);
					memcpy(gvKeyLayouts.back().mLabel, P(l), sizeof(cKeyLayout::mLabel));
				});
	SOMA_METHOD(e, IH, "void SetKeyboardLayoutDefaults(const cImGuiGfx &in aGfxKey, const cImGuiLabelData &in aLabelKey)", +[](void *, D g, D l) {
		gKeyDefault.mlType = 1;
		memcpy(gKeyDefault.mGfx, P(g), kGfxSize);
		memcpy(gKeyDefault.mLabel, P(l), sizeof(gKeyDefault.mLabel));
	});
	SOMA_METHOD(e, IH, "void SetMouseLayout(array<tString> &in avButtons)", +[](void *, const CScriptArray &v) {
		gvMouseLayout.clear();
		for (asUINT i = 0; i < v.GetSize(); ++i)
			gvMouseLayout.push_back(*(const tString *)v.At(i));
	});
	SOMA_FUNC(e, "cImGui@ cLux_GetInputFocusImGui()", +[]() { return cSomaImGui::GetScriptInputFocus(); });
	SOMA_FUNC(e, "cImGui@ cLux_GetPrevInputFocusImGui()", +[]() { return cSomaImGui::GetPrevInputFocus(); });
	SOMA_FUNC(e, "void cLux_PreloadGuiGfx(const tString &in asFile, eImGuiGfx aType)", +[](Str, int) {});
	SOMA_FUNC(e, "cLuxScriptImGui@ cLux_CreateScriptImGui(const tString &in asName, bool abRegisterForDrawing, bool abSkipResetOnRegistration=true)",
			  +[](Str n, bool bDraw, bool) {
				  cGui *pGui = gpSomaBase->mpEngine->GetGui();
				  cGuiSet *pSet = pGui->CreateSet(n, pGui->CreateSkin("gui_default.skin"));
				  SetHudVirtualSize(pSet);
				  gvHudSets.push_back(pSet);
				  cViewport *pViewport = gpSomaBase->mpEngine->GetScene()->CreateViewport(NULL, NULL, false);
				  pViewport->AddGuiSet(pSet);
				  cSomaScriptImGui *p = new cSomaScriptImGui;
				  p->mpImGui = new cSomaImGui(n, pSet);
				  if (bDraw)
					  gvScriptImGuis.push_back(p);
				  return p;
			  });
	const char *SI = "cLuxScriptImGui";
	SOMA_METHOD(e, SI, "cImGui@ GetImGui()", +[](cSomaScriptImGui *p) { return p->mpImGui; });
	SOMA_METHOD(e, SI, "cGuiSet@ GetSet()", +[](cSomaScriptImGui *p) { return p->mpImGui->GetSet(); });
	SOMA_METHOD(e, SI, "void DrawAll()", +[](cSomaScriptImGui *p) { p->mpImGui->DrawAll(); });

	const char *T = "cImGui";
	SOMA_METHOD(e, T, "void Begin(float afTimeStep)", +[](I *p, float t) { p->Begin(t); cSomaImGui::SetCurrent(p); });
	SOMA_METHOD(e, T, "void End()", +[](I *p) { p->End(); });
	SOMA_METHOD(e, T, "void DrawAll()", +[](I *p) { p->DrawAll(); });
	SOMA_METHOD(e, T, "void ClearStates()", +[](I *p) { p->ClearStates(); });
	SOMA_METHOD(e, T, "void DestroyAssets()", +[](I *) {});
	SOMA_METHOD(e, T, "cGuiSet@ GetSet()", +[](I *p) { return p->GetSet(); });
	SOMA_METHOD(e, T, "const tString& GetName()", +[](I *p) -> const tString & { return p->GetName(); });
	SOMA_METHOD(e, T, "bool IsFirstRun()", +[](I *p) { return p->IsFirstRun(); });
	SOMA_METHOD(e, T, "float GetTimeStep()", +[](I *p) { return p->GetTimeStep(); });
	SOMA_METHOD(e, T, "float GetTimeCount()", +[](I *p) { return p->GetTimeCount(); });
	SOMA_METHOD(e, T, "void SetShowMouse(bool abX)", +[](I *p, bool b) { p->mbShowMouse = b; });
	SOMA_METHOD(e, T, "bool GetShowMouse()", +[](I *p) { return p->mbShowMouse; });
	SOMA_METHOD(e, T, "void SetShowMouseAutomatically(bool abX)", +[](I *p, bool b) { p->mbShowMouseAutomatically = b; });
	SOMA_METHOD(e, T, "bool GetShowMouseAutomatically()", +[](I *p) { return p->mbShowMouseAutomatically; });
	SOMA_METHOD(e, T, "void SendMousePosition(const cVector2l&in avPos, const cVector2l&in avRel)", +[](I *p, const cVector2l &a, const cVector2l &r) { p->SendMousePosition(a, r); });
	SOMA_METHOD(e, T, "void SendMouseVirtualPosition(const cVector2f&in avPos, const cVector2f&in avRel)", +[](I *p, V2 a, V2 r) { p->SendMouseVirtualPosition(a, r); });
	SOMA_METHOD(e, T, "void SendAction(eImGuiAction aAction, bool abDown, bool abTriggered)", +[](I *p, int a, bool d, bool t) { p->SendAction(a, d, t); });
	SOMA_METHOD(e, T, "bool ActionTriggered(eImGuiAction aAction, bool abCheckIfUsed=false)", +[](I *p, int a, bool c) { return p->ActionTriggered(a, c); });
	SOMA_METHOD(e, T, "bool ActionIsDown(eImGuiAction aAction, bool abCheckIfUsed=false)", +[](I *p, int a, bool c) { return p->ActionIsDown(a, c); });
	SOMA_METHOD(e, T, "const cVector2f& GetMouseRel()", +[](I *p) -> const cVector2f & { return p->GetMouseRel(); });
	SOMA_METHOD(e, T, "const cVector2f& GetMousePosition()", +[](I *p) -> const cVector2f & { return p->GetMousePosition(); });
	SOMA_METHOD(e, T, "cVector3f GetMousePosition3D()", +[](I *p) { return cVector3f(p->GetMousePosition().x, p->GetMousePosition().y, 0); });
	SOMA_METHOD(e, T, "cVector3f GetMouseRel3D()", +[](I *p) { return cVector3f(p->GetMouseRel().x, p->GetMouseRel().y, 0); });
	SOMA_METHOD(e, T, "bool CheckMouseHasMoved()", +[](I *p) { return p->GetMouseRel().x != 0 || p->GetMouseRel().y != 0; });
	SOMA_METHOD(e, T, "void SetAlignment(eImGuiAlign aAlign)", +[](I *p, int a) { p->mlAlign = a; });
	SOMA_METHOD(e, T, "void SetFocus(const tString&in asWidgetName)", +[](I *p, Str s) { p->SetFocus(s); });
	SOMA_METHOD(e, T, "void SetUIMoveGroupFlags(int alGroupFlags)", +[](I *p, int f) { p->mlGroupFlags = f; });
	SOMA_METHOD(e, T, "void SetUIMoveWrapMode(eImGuiWrap aWrap)", +[](I *p, int w) { p->mlWrapMode = w; });
	SOMA_METHOD(e, T, "void LockMouseFocus()", +[](I *p) { p->mlMouseLock = 2; });
	SOMA_METHOD(e, T, "bool MouseFocusIsLocked()", +[](I *p) { return p->mlMouseLock > 0; });
	SOMA_METHOD(e, T, "void SetDrawUIDebugBoxes(bool abX)", +[](I *, bool) {});
	SOMA_METHOD(e, T, "bool CheckMouseOver(const cVector3f&in avPos, const cVector2f &in avSize)", +[](I *p, V3 a, V2 s) { return p->MouseOver(a + p->GroupPos(), s); });

#define STATE(TYPE, RET, ARG, FIELD, FLAG, INTYPE, DEF1, DEF2, CRET)                                                                                        \
	SOMA_METHOD(e, T, RET " GetState" TYPE "(uint64 alId, " ARG " aDefault" DEF1 ")", +[](I *p, asQWORD id, INTYPE d) -> CRET {                           \
		return p->GetOrSetState(id, &cState::FIELD, &cState::FLAG, d);                                                                                 \
	});                                                                                                                                                  \
	SOMA_METHOD(e, T, RET " GetState" TYPE "(const tString&in asVarName, " ARG " aDefault" DEF2 ")", +[](I *p, Str n, INTYPE d) -> CRET {                 \
		return p->GetOrSetState(Id(n), &cState::FIELD, &cState::FLAG, d);                                                                              \
	});                                                                                                                                                  \
	SOMA_METHOD(e, T, "void SetState" TYPE "(uint64 alId, " ARG " aVal)", +[](I *p, asQWORD id, INTYPE v) { auto &s = p->State(id); s.FIELD = v; s.FLAG = true; }); \
	SOMA_METHOD(e, T, "void SetState" TYPE "(const tString&in asVarName, " ARG " aVal)", +[](I *p, Str n, INTYPE v) { auto &s = p->State(Id(n)); s.FIELD = v; s.FLAG = true; }); \
	SOMA_METHOD(e, T, "void IncState" TYPE "(uint64 alId, " ARG " aVal)", +[](I *p, asQWORD id, INTYPE v) { auto &s = p->State(id); s.FIELD = s.FIELD + v; s.FLAG = true; }); \
	SOMA_METHOD(e, T, "void IncState" TYPE "(const tString&in asVarName, " ARG " aVal)", +[](I *p, Str n, INTYPE v) { auto &s = p->State(Id(n)); s.FIELD = s.FIELD + v; s.FLAG = true; });
	STATE("Int", "int", "int", mlInt, mbSetInt, int, "=0", "=0", int)
	STATE("Float", "float", "float", mfFloat, mbSetFloat, float, "=0.0f", "=0.0f", float)
	STATE("Vector3f", "cVector3f", "const cVector3f&in", mvVec, mbSetVec, V3, "=0.0f", "=cVector3f(0.0f)", cVector3f)
	STATE("Color", "cColor", "const cColor&in", mCol, mbSetCol, const cColor &, "=cColor(1,1)", "=cColor(1,1)", cColor)
#undef STATE

	SOMA_METHOD(e, T, "void FadeStateFloat(const tString&in asVarName, float afGoalVal,float afTime, eEasing aType=eEasing_QuadInOut, bool abReplaceIfExist=true)",
				+[](I *p, Str n, float g, float t, int ease, bool r) { float v[4] = {g, 0, 0, 0}; p->Fade(Id(n), 0, v, t, ease, r); });
	SOMA_METHOD(e, T, "void FadeStateVector3f(const tString&in asVarName, const cVector3f&in avGoalVal,float afTime, eEasing aType=eEasing_QuadInOut, bool abReplaceIfExist=true)",
				+[](I *p, Str n, V3 g, float t, int ease, bool r) { float v[4] = {g.x, g.y, g.z, 0}; p->Fade(Id(n), 1, v, t, ease, r); });
	SOMA_METHOD(e, T, "void FadeStateColor(const tString&in asVarName, const cColor&in aGoalVal,float afTime, eEasing aType=eEasing_QuadInOut, bool abReplaceIfExist=true)",
				+[](I *p, Str n, const cColor &g, float t, int ease, bool r) { float v[4] = {g.r, g.g, g.b, g.a}; p->Fade(Id(n), 2, v, t, ease, r); });
	SOMA_METHOD(e, T, "void StopFade(const tString&in asVarName)", +[](I *p, Str n) { p->mmapFades.erase(Id(n)); });
	SOMA_METHOD(e, T, "bool FadeOver(const tString&in asVarName)", +[](I *p, Str n) {
		auto it = p->mmapFades.find(Id(n));
		return it == p->mmapFades.end() || it->second.mfCount >= it->second.mfTime;
	});
	SOMA_METHOD(e, T, "bool IsFading(const tString&in asVarName)", +[](I *p, Str n) {
		auto it = p->mmapFades.find(Id(n));
		return it != p->mmapFades.end() && it->second.mfCount < it->second.mfTime;
	});
	SOMA_METHOD(e, T, "float FadeOscillateFloat(const tString&in asVarName, float afStart, float afGoal, float afTime, eEasing aType=eEasing_QuadInOut)",
				+[](I *p, Str, float a, float b, float t, int ease) {
					float ph = t > 0 ? std::fmod(p->GetTimeCount(), 2 * t) / t : 0;
					float x = SomaEasing(ease, ph <= 1 ? ph : 2 - ph);
					return a + (b - a) * x;
				});
	SOMA_METHOD(e, T, "cColor FadeOscillateColor(const tString&in asVarName, const cColor&in aStart, const cColor&in aGoal, float afTime, eEasing aType=eEasing_QuadInOut)",
				+[](I *p, Str, const cColor &a, const cColor &b, float t, int ease) {
					float ph = t > 0 ? std::fmod(p->GetTimeCount(), 2 * t) / t : 0;
					float x = SomaEasing(ease, ph <= 1 ? ph : 2 - ph);
					return cColor(a.r + (b.r - a.r) * x, a.g + (b.g - a.g) * x, a.b + (b.b - a.b) * x, a.a + (b.a - a.a) * x);
				});
	SOMA_METHOD(e, T, "cVector3f FadeOscillateVector3f(const tString&in asVarName, const cVector3f&in avStart, const cVector3f&in avGoal, float afTime, eEasing aType=eEasing_QuadInOut)",
				+[](I *p, Str, V3 a, V3 b, float t, int ease) {
					float ph = t > 0 ? std::fmod(p->GetTimeCount(), 2 * t) / t : 0;
					return a + (b - a) * SomaEasing(ease, ph <= 1 ? ph : 2 - ph);
				});

	// Duplicates append; lookups see the oldest, expired entries drop in End()
	SOMA_METHOD(e, T, "void AddTimer(const tString&in asName, float afTime)", +[](I *p, Str n, float t) {
		p->mvTimers.push_back({Id(n), t, false, true});
	});
	SOMA_METHOD(e, T, "bool RepeatTimer(const tString&in asName, float afTime)", +[](I *p, Str n, float t) {
		cTimer *pT = p->FindTimer(Id(n));
		if (pT && pT->mfTime > 0)
			return pT->mbTouched = true, false;
		p->mvTimers.push_back({Id(n), t, true, true});
		return pT != nullptr;
	});
	SOMA_METHOD(e, T, "void StopTimer(const tString&in asName)", +[](I *p, Str n) {
		std::erase_if(p->mvTimers, [&](const cTimer &t) { return t.mlId == Id(n); });
	});
	SOMA_METHOD(e, T, "bool TimerOver(const tString&in asName)", +[](I *p, Str n) {
		cTimer *pT = p->FindTimer(Id(n));
		return pT && pT->mfTime <= 0;
	});
	SOMA_METHOD(e, T, "bool TimerExists(const tString&in asName)", +[](I *p, Str n) { return p->FindTimer(Id(n)) != nullptr; });

	SOMA_METHOD(e, T, "void SetModColorMul(const cColor&in aCol)", +[](I *p, const cColor &c) { p->mMods.mColorMul = c; });
	SOMA_METHOD(e, T, "void SetModTextColorMul(const cColor&in aCol)", +[](I *p, const cColor &c) { p->mMods.mTextColorMul = c; });
	SOMA_METHOD(e, T, "void SetModUseUIPos(bool abX)", +[](I *p, bool b) { p->mMods.mbUseUIPos = b; });
	SOMA_METHOD(e, T, "void SetModUseInput(bool abX)", +[](I *p, bool b) { p->mMods.mbUseInput = b; });
	SOMA_METHOD(e, T, "void SetModUISizeHoriExpansion(float afNeg, float afPos)", +[](I *p, float a, float b) { p->mMods.mvExpHori = cVector2f(a, b); });
	SOMA_METHOD(e, T, "void SetModUISizeVertExpansion(float afNeg, float afPos)", +[](I *p, float a, float b) { p->mMods.mvExpVert = cVector2f(a, b); });
	SOMA_METHOD(e, T, "void SetModRotateAngle(float afX)", +[](I *p, float f) { p->mMods.mfRotateAngle = f; });
	SOMA_METHOD(e, T, "void SetModRotateCustomPivot(bool abX)", +[](I *p, bool b) { p->mMods.mbRotateCustomPivot = b; });
	SOMA_METHOD(e, T, "void SetModRotatePivot(const cVector2f&in avPivot)", +[](I *p, V2 v) { p->mMods.mvRotatePivot = v; });
	SOMA_METHOD(e, T, "void ResetModifiers()", +[](I *p) { p->mMods = cSomaImGui::cModifiers(); });
	SOMA_METHOD(e, T, "void PushModifiers()", +[](I *p) { p->mvModStack.push_back(p->mMods); });
	SOMA_METHOD(e, T, "void PopModifiers()", +[](I *p) {
		if (p->mvModStack.empty() == false)
		{
			p->mMods = p->mvModStack.back();
			p->mvModStack.pop_back();
		}
	});

	SOMA_METHOD(e, T, "bool PrevPressed()", +[](I *p) { return p->mPrev.mbPressed; });
	SOMA_METHOD(e, T, "bool PrevBecamePressed()", +[](I *p) { return p->mPrev.mbBecamePressed; });
	SOMA_METHOD(e, T, "bool PrevInFocus()", +[](I *p) { return p->mPrev.mbInFocus; });
	SOMA_METHOD(e, T, "bool PrevBecameInFocus()", +[](I *p) { return p->mPrev.mbBecameInFocus; });
	SOMA_METHOD(e, T, "bool PrevWasInFocus()", +[](I *p) { return p->mPrev.mbWasInFocus; });
	SOMA_METHOD(e, T, "bool PrevMouseOver()", +[](I *p) { return p->mPrev.mbMouseOver; });
	SOMA_METHOD(e, T, "bool PrevUpdated()", +[](I *p) { return p->mPrev.mbUpdated; });
	SOMA_METHOD(e, T, "const cVector3f& PrevPosition()", +[](I *p) -> const cVector3f & { return p->mPrev.mvPos; });
	SOMA_METHOD(e, T, "const cVector2f& PrevSize()", +[](I *p) -> const cVector2f & { return p->mPrev.mvSize; });
	SOMA_METHOD(e, T, "void ClearPrevData()", +[](I *p) { p->mPrev = cSomaImGui::cPrev(); });

	// Groups, layouts, items
	SOMA_METHOD(e, T, "void GroupBegin(const cVector3f&in avPos, const cVector2f&in avSize=0, bool abClip=false)", +[](I *p, V3 pos, V2 size, bool clip) {
		cSomaImGui::cGroup g;
		g.mvPos = p->GroupPos() + pos;
		g.mvSize = size.x > 0 || size.y > 0 ? size : p->GroupSize();
		g.mbClip = clip;
		p->PushGroup(g);
	});
	SOMA_METHOD(e, T, "void GroupEnd()", +[](I *p) { p->PopGroup(); });
	SOMA_METHOD(e, T, "const cVector3f &GetCurrentGroupPos()", +[](I *p) -> const cVector3f & { static cVector3f v; v = p->GroupPos(); return v; });
	SOMA_METHOD(e, T, "const cVector2f &GetCurrentGroupSize()", +[](I *p) -> const cVector2f & { static cVector2f v; v = p->GroupSize(); return v; });
	SOMA_METHOD(e, T, "void ClipAreaBegin(const cVector3f&in avPos, const cVector2f&in avSize)", +[](I *p, V3 pos, V2 size) {
		p->Record({NULL, NULL, L"", pos, size, cColor(1, 1), 0, cSomaImGui::kClipBegin, 0});
	});
	SOMA_METHOD(e, T, "void ClipAreaEnd()", +[](I *p) { p->Record({NULL, NULL, L"", 0, 0, cColor(1, 1), 0, cSomaImGui::kClipEnd, 0}); });
	SOMA_METHOD(e, T, "void LayoutBegin(eImGuiLayout aType, const cVector3f&in avPos=0, const cVector2f&in avSize=-1, const cVector2f&in avSpacing=0)",
				+[](I *p, int t, V3 pos, V2 size, V2 spacing) {
					cSomaImGui::cLayout l;
					l.mlType = t;
					l.mvStart = l.mvCursor = p->GroupPos() + pos;
					l.mvSize = size;
					l.mvSpacing = spacing;
					l.mfLineMax = 0;
					p->mvLayouts.push_back(l);
				});
	SOMA_METHOD(e, T, "void LayoutEnd()", +[](I *p) { if (p->mvLayouts.empty() == false) p->mvLayouts.pop_back(); });
	SOMA_METHOD(e, T, "void AddLayoutHorizontalSpace(float afWidth, float afHeight=0)", +[](I *p, float w, float) { if (p->mvLayouts.empty() == false) p->mvLayouts.back().mvCursor.x += w; });
	SOMA_METHOD(e, T, "void AddLayoutVerticalSpace(float afHeight)", +[](I *p, float h) { if (p->mvLayouts.empty() == false) p->mvLayouts.back().mvCursor.y += h; });
	SOMA_METHOD(e, T, "void AddItemString(const tWString&in asStr)", +[](I *p, WStr s) { p->mvItems.push_back(s); });
	SOMA_METHOD(e, T, "void AddItemStringList(const tWString&in asStrList)", +[](I *p, WStr s) {
		tWStringVec v;
		tWString sSep = _W(";");
		cString::GetStringVecW(s, v, &sSep);
		for (const tWString &x : v)
			p->mvItems.push_back(x);
	});
	SOMA_METHOD(e, T, "void ClearItems()", +[](I *p) { p->mvItems.clear(); });

#define DEFAULT(NAME, TYPE)                                                                                                            \
	SOMA_METHOD(e, T, "void SetDefault" NAME "(const " TYPE " &in aData)", +[](I *p, D d) { p->SetDefault(TYPE, P(d)); });                \
	SOMA_METHOD(e, T, "const " TYPE "& GetDefault" NAME "()", +[](I *p) -> const S_ & { return *(const S_ *)p->GetDefault(TYPE); });
	DEFAULT("Button", "cImGuiButtonData")
	DEFAULT("SliderHorizontal", "cImGuiSliderData")
	DEFAULT("Label", "cImGuiLabelData")
	DEFAULT("CheckBox", "cImGuiCheckBoxData")
	DEFAULT("TextFrame", "cImGuiTextFrameData")
	DEFAULT("MultiSelect", "cImGuiMultiSelectData")
	DEFAULT("Frame", "cImGuiFrameData")
	DEFAULT("Window", "cImGuiWindowData")
	DEFAULT("Gauge", "cImGuiGaugeData")
#undef DEFAULT
	SOMA_METHOD(e, T, "void SetDefaultSliderVertical(const cImGuiSliderData &in aData)", +[](I *p, D d) { p->SetDefault("cImGuiSliderDataV", P(d)); });
	SOMA_METHOD(e, T, "const cImGuiSliderData& GetDefaultSliderVertical()", +[](I *p) -> const S_ & { return *(const S_ *)p->GetDefault("cImGuiSliderDataV"); });
	SOMA_METHOD(e, T, "void SetDefaultMouse(const cImGuiGfx&in aGfx)", +[](I *p, D d) { memcpy(p->GetDefault("__mouse"), P(d), kGfxSize); });
	SOMA_METHOD(e, T, "void SetDefaultFont(const cImGuiFont&in aFont)", +[](I *p, D d) {
		memcpy(p->GetDefault("__font"), P(d), 64);
		// Widgets constructed afterwards inherit the default font
		for (const char *pType : {"cImGuiButtonData", "cImGuiSliderData", "cImGuiSliderDataV", "cImGuiLabelData", "cImGuiCheckBoxData",
								  "cImGuiTextFrameData", "cImGuiMultiSelectData", "cImGuiFrameData", "cImGuiWindowData", "cImGuiGaugeData"})
			if (StrAt((char *)p->GetDefault(pType) + kWFont, kFontFile).empty())
				memcpy((char *)p->GetDefault(pType) + kWFont + 16, (const char *)P(d) + 16, 48);
	});

	SOMA_METHOD(e, T, "bool DoButton(const tString&in asName,const tWString&in asText, const cImGuiButtonData &in aData, const cVector3f&in avPos=0, const cVector2f&in avSize=-1)",
				+[](I *p, Str n, WStr t, D d, V3 pos, V2 size) { return p->DoButton(n, t, P(d), pos, size, 0); });
	SOMA_METHOD(e, T, "bool DoButton(const tString&in asName,const tWString&in asText, const cVector3f&in avPos=0, const cVector2f&in avSize=-1)",
				+[](I *p, Str n, WStr t, V3 pos, V2 size) { return p->DoButton(n, t, p->GetDefault("cImGuiButtonData"), pos, size, 0); });
	SOMA_METHOD(e, T, "bool DoRepeatButton(const tString&in asName,const tWString&in asText, const cImGuiButtonData &in aData, const cVector3f&in avPos=0, const cVector2f&in avSize=-1)",
				+[](I *p, Str n, WStr t, D d, V3 pos, V2 size) { return p->DoButton(n, t, P(d), pos, size, 2); });
	SOMA_METHOD(e, T, "bool DoRepeatButton(const tString&in asName,const tWString&in asText, const cVector3f&in avPos=0, const cVector2f&in avSize=-1)",
				+[](I *p, Str n, WStr t, V3 pos, V2 size) { return p->DoButton(n, t, p->GetDefault("cImGuiButtonData"), pos, size, 2); });
	SOMA_METHOD(e, T, "bool DoToggleButton(const tString&in asName,const tWString&in asText, bool abDefaultChecked, const cImGuiButtonData &in aData, const cVector3f&in avPos=0, const cVector2f&in avSize=-1)",
				+[](I *p, Str n, WStr t, bool def, D d, V3 pos, V2 size) {
					auto &st = p->State(Id(n));
					cSomaImGui::DefaultOrCurrent(st, (int)def);
					return p->DoButton(n, t, P(d), pos, size, 1);
				});
	SOMA_METHOD(e, T, "bool DoToggleButton(const tString&in asName,const tWString&in asText, bool abDefaultChecked, const cVector3f&in avPos=0, const cVector2f&in avSize=-1)",
				+[](I *p, Str n, WStr t, bool def, V3 pos, V2 size) {
					auto &st = p->State(Id(n));
					cSomaImGui::DefaultOrCurrent(st, (int)def);
					return p->DoButton(n, t, p->GetDefault("cImGuiButtonData"), pos, size, 1);
				});
	SOMA_METHOD(e, T, "float DoSliderHorizontal(const tString&in asName, float afDefaultValue, float afMin, float afMax, float afStepSize, const cImGuiSliderData &in aData, const cVector3f&in avPos=0, const cVector2f&in avSize=-1)",
				+[](I *p, Str n, float def, float mn, float mx, float st, D d, V3 pos, V2 size) { return p->DoSlider(n, def, mn, mx, st, P(d), pos, size, false); });
	SOMA_METHOD(e, T, "float DoSliderHorizontal(const tString&in asName, float afDefaultValue, float afMin, float afMax, float afStepSize=-1, const cVector3f&in avPos=0, const cVector2f&in avSize=-1)",
				+[](I *p, Str n, float def, float mn, float mx, float st, V3 pos, V2 size) { return p->DoSlider(n, def, mn, mx, st, p->GetDefault("cImGuiSliderData"), pos, size, false); });
	SOMA_METHOD(e, T, "float DoSliderVertical(const tString&in asName, float afDefaultValue, float afMin, float afMax, float afStepSize, const cImGuiSliderData &in aData, const cVector3f&in avPos=0, const cVector2f&in avSize=-1)",
				+[](I *p, Str n, float def, float mn, float mx, float st, D d, V3 pos, V2 size) { return p->DoSlider(n, def, mn, mx, st, P(d), pos, size, true); });
	SOMA_METHOD(e, T, "float DoSliderVertical(const tString&in asName, float afDefaultValue, float afMin, float afMax, float afStepSize=-1, const cVector3f&in avPos=0, const cVector2f&in avSize=-1)",
				+[](I *p, Str n, float def, float mn, float mx, float st, V3 pos, V2 size) { return p->DoSlider(n, def, mn, mx, st, p->GetDefault("cImGuiSliderDataV"), pos, size, true); });
	SOMA_METHOD(e, T, "void DoLabel(const tWString&in asText, const cImGuiLabelData &in aData, const cVector3f&in avPos=0, const cVector2f&in avSize=-1, float afFontSizeMul=1)",
				+[](I *p, WStr t, D d, V3 pos, V2 size, float mul) { p->DoLabel(t, P(d), pos, size, mul); });
	SOMA_METHOD(e, T, "void DoLabel(const tWString&in asText, const cVector3f&in avPos=0, const cVector2f&in avSize=-1, float afFontSizeMul=1)",
				+[](I *p, WStr t, V3 pos, V2 size, float mul) { p->DoLabel(t, p->GetDefault("cImGuiLabelData"), pos, size, mul); });
	SOMA_METHOD(e, T, "void DoImage(const cImGuiGfx&in aGfxImage, const cVector3f&in avPos=0, const cVector2f&in avSize=-1)", +[](I *p, D g, V3 pos, V2 size) { p->DoImage(P(g), pos, size); });
	SOMA_METHOD(e, T, "bool DoCheckBox(const tString&in asName, const tWString&in asText, bool abDefaultChecked, const cImGuiCheckBoxData &in aData, const cVector3f&in avPos=0, const cVector2f&in avSize=-1)",
				+[](I *p, Str n, WStr t, bool def, D d, V3 pos, V2 size) { return p->DoCheckBox(n, t, def, P(d), pos, size); });
	SOMA_METHOD(e, T, "bool DoCheckBox(const tString&in asName, const tWString&in asText, bool abDefaultChecked, const cVector3f&in avPos=0, const cVector2f&in avSize=-1)",
				+[](I *p, Str n, WStr t, bool def, V3 pos, V2 size) { return p->DoCheckBox(n, t, def, p->GetDefault("cImGuiCheckBoxData"), pos, size); });
	SOMA_METHOD(e, T, "float DoTextFrame(const tWString&in asText,const cVector2f&in avEdgeSpacing, float afRowSpace, float afStartRowOffset, const cImGuiTextFrameData &in aData, const cVector3f&in avPos, const cVector2f&in avSize)",
				+[](I *p, WStr t, V2 edge, float row, float start, D d, V3 pos, V2 size) { return p->DoTextFrame(t, edge, row, start, P(d), pos, size); });
	SOMA_METHOD(e, T, "float DoTextFrame(const tWString&in asText,const cVector2f&in avEdgeSpacing, float afRowSpace, float afStartRowOffset, const cVector3f&in avPos, const cVector2f&in avSize)",
				+[](I *p, WStr t, V2 edge, float row, float start, V3 pos, V2 size) { return p->DoTextFrame(t, edge, row, start, p->GetDefault("cImGuiTextFrameData"), pos, size); });
	SOMA_METHOD(e, T, "int DoMultiSelect(const tString&in asName, int alDefaultSelectedItem, const cImGuiMultiSelectData &in aData, const cVector3f&in avPos=0, const cVector2f&in avSize=-1)",
				+[](I *p, Str n, int def, D d, V3 pos, V2 size) { return p->DoMultiSelect(n, def, P(d), pos, size); });
	SOMA_METHOD(e, T, "int DoMultiSelect(const tString&in asName, int alDefaultSelectedItem, const cVector3f&in avPos=0, const cVector2f&in avSize=-1)",
				+[](I *p, Str n, int def, V3 pos, V2 size) { return p->DoMultiSelect(n, def, p->GetDefault("cImGuiMultiSelectData"), pos, size); });
	SOMA_METHOD(e, T, "int DoMultiToggle(const tString&in asName, int alDefaultSelectedItem, uint alColumnNum, const cVector2f&in avSpacing, const cImGuiButtonData &in aData, const cVector3f&in avPos, const cVector2f&in avSize)",
				+[](I *p, Str n, int def, asUINT cols, V2 spacing, D d, V3 pos, V2 size) { return SomaMultiToggle(p, n, def, cols, spacing, P(d), pos, size); });
	SOMA_METHOD(e, T, "int DoMultiToggle(const tString&in asName, int alDefaultSelectedItem, uint alColumnNum, const cVector2f&in avSpacing, const cVector3f&in avPos, const cVector2f&in avSize)",
				+[](I *p, Str n, int def, asUINT cols, V2 spacing, V3 pos, V2 size) { return SomaMultiToggle(p, n, def, cols, spacing, p->GetDefault("cImGuiButtonData"), pos, size); });
	SOMA_METHOD(e, T, "void DoFrame(const cImGuiFrameData &in aData, const cVector3f &in avPos=0, const cVector2f &in avSize=-1)", +[](I *p, D d, V3 pos, V2 size) { p->DoFrame(P(d), pos, size); });
	SOMA_METHOD(e, T, "void DoFrame(const cVector3f &in avPos=0, const cVector2f &in avSize=-1)", +[](I *p, V3 pos, V2 size) { p->DoFrame(p->GetDefault("cImGuiFrameData"), pos, size); });
	SOMA_METHOD(e, T, "void DoWindowStart(const tWString &in asCaption, const cImGuiWindowData &in aData, const cVector3f &in avPos=0, const cVector2f &in avSize=-1, bool abClip=true)",
				+[](I *p, WStr c, D d, V3 pos, V2 size, bool clip) { p->DoWindowStart(c, P(d), pos, size, clip); });
	SOMA_METHOD(e, T, "void DoWindowStart(const tWString &in asCaption, const cVector3f &in avPos=0, const cVector2f &in avSize=-1, bool abClip=true)",
				+[](I *p, WStr c, V3 pos, V2 size, bool clip) { p->DoWindowStart(c, p->GetDefault("cImGuiWindowData"), pos, size, clip); });
	SOMA_METHOD(e, T, "void DoWindowEnd()", +[](I *p) { p->DoWindowEnd(); });
	SOMA_METHOD(e, T, "void DoGauge(const cImGuiGaugeData &in aData, float afFillAmount, const cVector3f &in avPos=0, const cVector2f &in avSize=-1)",
				+[](I *p, D d, float f, V3 pos, V2 size) { p->DoGauge(P(d), f, pos, size); });
	SOMA_METHOD(e, T, "void DoGauge(float afFillAmount, const cVector3f &in avPos=0, const cVector2f &in avSize=-1)",
				+[](I *p, float f, V3 pos, V2 size) { p->DoGauge(p->GetDefault("cImGuiGaugeData"), f, pos, size); });
	SOMA_METHOD(e, T, "void DoMouse(const cImGuiGfx &in aGfx, const cVector3f&in avOffset=0, const cVector2f&in avSize=-1)", +[](I *p, D g, V3 off, V2 size) { p->DoMouse(P(g), off, size); });

	SOMA_METHOD(e, T, "void DrawGfx(const cImGuiGfx &in aGfx, const cVector3f&in avPos, const cVector2f&in avSize=-1, const cColor&in aCol=cColor(1,1), const cColor&in aColTopLeft=cColor(1,1), const cColor&in aColTopRight=cColor(1,1), const cColor&in aColBotRight=cColor(1,1), const cColor&in aColBotLeft=cColor(1,1))",
				+[](I *p, D g, V3 pos, V2 size, const cColor &c, const cColor &, const cColor &, const cColor &, const cColor &) { p->DrawGfx(P(g), pos, size, c); });
	SOMA_METHOD(e, T, "void DrawLine(const cVector2f&in avStart, const cVector2f&in avEnd,float afZ, float afThickness=1.0f, const cColor&in aCol=cColor(1,1), const cImGuiGfx &in aGfx=cImGuiGfx())",
				+[](I *p, V2 a, V2 b, float z, float t, const cColor &c, D g) { DrawLine(p, a, b, z, t, c, P(g)); });
	SOMA_METHOD(e, T, "void AddLineStripVertex(const cVector2f &in avVertex)", +[](I *p, V2 v) { p->mvLineStrip.push_back(v); });
	SOMA_METHOD(e, T, "void DrawAndClearLineStrip(float afZ, float afThickness, const cColor &in aCol=cColor_White, const cImGuiGfx &in aGfx=cImGuiGfx())",
				+[](I *p, float z, float t, const cColor &c, D g) {
					for (size_t i = 1; i < p->mvLineStrip.size(); ++i)
						DrawLine(p, p->mvLineStrip[i - 1], p->mvLineStrip[i], z, t, c, P(g));
					p->mvLineStrip.clear();
				});
	SOMA_METHOD(e, T, "void DrawAlignedGfx(const cImGuiGfx &in aGfx, const cVector3f &in avPos, eImGuiAlign aAlignment, const cVector2f&in avSize=-1, const cColor &in aCol=cColor(1,1), const cColor&in aColTopLeft=cColor(1,1), const cColor&in aColTopRight=cColor(1,1), const cColor&in aColBotRight=cColor(1,1), const cColor&in aColBotLeft=cColor(1,1))",
				+[](I *p, D g, V3 pos, int align, V2 size, const cColor &c, const cColor &, const cColor &, const cColor &, const cColor &) {
					cVector2f vSize = size.x < 0 || size.y < 0 ? p->GetGfxSize(P(g)) : size;
					p->DrawGfx(P(g), p->Align(pos, vSize, align), vSize, c);
				});
	SOMA_METHOD(e, T, "void DrawFont(const tWString&in asText, const cImGuiFont &in aFont, const cVector3f&in avPos, eFontAlign aAlign, const cVector2f&in avSizeMul=1, const cColor&in aColMul=cColor(1,1))",
				+[](I *p, WStr t, D f, V3 pos, int align, V2 mul, const cColor &c) { p->DrawFont(t, P(f), pos, align, mul, c); });
	SOMA_METHOD(e, T, "void DrawFrame(const cImGuiFrameGfx &in aGfx, const cVector3f&in avPos, const cVector2f&in avSize=-1, const cColor&in aCol=cColor(1,1))",
				+[](I *p, D g, V3 pos, V2 size, const cColor &c) { p->DrawFrame(P(g), pos, size, c); });
	SOMA_METHOD(e, T, "cVector2f GetGfxSize(const cImGuiGfx&in aGfx)", +[](I *p, D g) { return p->GetGfxSize(P(g)); });
	SOMA_METHOD(e, T, "cVector2f GetUsedGfxSize(const cImGuiGfx&in aGfx, const cVector2f&in avCustomSize)",
				+[](I *p, D g, V2 s) { return s.x < 0 || s.y < 0 ? p->GetGfxSize(P(g)) : s; });
	SOMA_METHOD(e, T, "float GetFontLength(const cImGuiFont&in aFont, float afSizeMul, const tWString&in asText)", +[](I *p, D f, float m, WStr t) { return p->GetFontLength(P(f), m, t); });
	SOMA_METHOD(e, T, "const cVector2f& GetUsedFontSize(const cImGuiFont&in aFont)", +[](I *p, D f) -> const cVector2f & { static cVector2f v; v = p->FontSize(P(f), 1); return v; });
	SOMA_METHOD(e, T, "cVector2f CalcWidgetSize(const cVector2f&in avArgSize, const cVector2f&in avDefaultSize)",
				+[](I *, V2 a, V2 d) { return cVector2f(a.x < 0 ? d.x : a.x, a.y < 0 ? d.y : a.y); });
	SOMA_METHOD(e, T, "void GetFontWordWrapRows(const cImGuiFont&in aFont, float afSizeMul, const tWString&in asText, float afLineWidth, array<tWString> &out avLines, array<bool> &out avRowEndedWithNewLine=void)",
				+[](I *p, D f, float m, WStr t, float w, CScriptArray &lines, CScriptArray &ends) {
					iFontData *pFont = p->GetFont(P(f));
					cVector2f vSize = p->FontSize(P(f), m);
					tWStringVec vRows;
					if (pFont)
						pFont->GetWordWrapRows(w, vSize.y, vSize, t, &vRows);
					for (tWString &s : vRows)
					{
						lines.InsertLast(&s);
						bool b = false;
						ends.InsertLast(&b);
					}
				});
}
