// Engine side of the ImGui port (rust/src/somaui/s8d*.rs): gui sets, gui images, fonts, input
// actions, camera textures and script arrays behind the OhplS8dHost table.
#include "SomaImGui.h"
#include "SomaBase.h"
#include "SomaLux.h"
#include "SomaLuxEntity.h"
#include "SomaLuxGame.h"

#include "impl/scriptarray.h"
#include "input/ActionKeyboard.h"
#include "input/ActionMouseButton.h"

#include <cmath>
#include <cstring>

static std::map<cGuiGfxElement *, tString> gmapGfxNames;
static std::map<iFontData *, tString> gmapFontNames;

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

static cGuiGfxElement *GfxElement(const tString &sFile, int lType, int lMaterial, const cVector2f &vUVMin, const cVector2f &vUVMax);

// Rebirth eGuiMaterial_Custom0 (cGuiMaterial_MemorySketch)
class cSomaGuiMaterial_MemorySketch : public iGuiMaterial
{
public:
	cSomaGuiMaterial_MemorySketch(iLowLevelGraphics *apLowLevelGraphics) : iGuiMaterial("MemorySketch", apLowLevelGraphics), mvAlphaLimit(1, 0)
	{
		static iGpuProgram *pProgram = gpSomaBase->mpEngine->GetGraphics()->CreateGpuProgramFromShaders(
			"MemorySketch", "game_gui_memorysketch_vtx.glsl", "game_gui_memorysketch_frag.glsl", NULL);
		mpProgram = pProgram;
	}

	void BeforeRender()
	{
		mpLowLevelGraphics->SetBlendActive(true);
		mpLowLevelGraphics->SetBlendFunc(eBlendFunc_DestColor, eBlendFunc_Zero);
		if (mpProgram == NULL)
			return;
		mpProgram->Bind();
		mpProgram->SetVec2f(mpProgram->GetVariableId("avAlphaLimit"), mvAlphaLimit);
		mpProgram->SetFloat(mpProgram->GetVariableId("afTime"), fmodf(cPlatform::GetApplicationTime() / 1000.0f, 3000.0f));
	}

	void AfterRender()
	{
		if (mpProgram)
			mpProgram->UnBind();
	}

	cVector2f mvAlphaLimit;
	iGpuProgram *mpProgram;
};
static std::map<cGuiGfxElement *, cSomaGuiMaterial_MemorySketch *> gmapSketchMaterials;

static cGuiGfxElement *WhiteGfx()
{
	static cGuiGfxElement *pWhite = NULL;
	if (pWhite == NULL)
		pWhite = gpSomaBase->mpEngine->GetGui()->CreateGfxFilledRect(cColor(1, 1), eGuiMaterial_Alpha);
	return pWhite;
}

static cGuiGfxElement *HostGfx(const OhplS8dGfx *g)
{
	tString sFile((const char *)g->file, g->file_n);
	int lMaterial = g->material, lType = g->type;
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
	if (sFile.empty())
		return WhiteGfx();
	cGuiGfxElement *pGfx = GfxElement(sFile, lType, lMaterial, cVector2f(g->uv_min[0], g->uv_min[1]), cVector2f(g->uv_max[0], g->uv_max[1]));
	// ponytail: one limit per image and material, the last draw of a frame wins
	if (pGfx && lMaterial >= eGuiMaterial_LastEnum)
		gmapSketchMaterials[pGfx]->mvAlphaLimit = cVector2f(g->alpha_limit[0], g->alpha_limit[1]);
	return pGfx;
}

static cGuiGfxElement *GfxElement(const tString &sFile, int lType, int lMaterial, const cVector2f &vUVMin, const cVector2f &vUVMax)
{
	static std::map<tString, cGuiGfxElement *> mapCache;
	bool bSubRect = vUVMin != cVector2f(0) || vUVMax != cVector2f(1);
	bool bSketch = lMaterial >= eGuiMaterial_LastEnum;
	tString sKey = sFile + "#" + cString::ToString(lType) + (bSketch ? "#sketch" : "");
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
	if (pGfx && bSketch)
		pGfx->SetMaterial(gmapSketchMaterials[pGfx] = new cSomaGuiMaterial_MemorySketch(gpSomaBase->mpEngine->GetGraphics()->GetLowLevel()));
	mapCache[sKey] = pGfx;
	gmapGfxNames[pGfx] = sKey;
	return pGfx;
}


namespace
{
	cGuiSet *Set(size_t a) { return (cGuiSet *)a; }
	cGuiClipRegion *Clip(size_t a) { return (cGuiClipRegion *)a; }
	cVector3f V3(const float *p) { return cVector3f(p[0], p[1], p[2]); }
	cVector2f V2(const float *p) { return cVector2f(p[0], p[1]); }
	cColor Col(const float *p) { return cColor(p[0], p[1], p[2], p[3]); }
	void Bytes(OhplS8dBytes f, void *c, const tString &s) { f(c, (const unsigned char *)s.data(), s.size()); }
	void Words(OhplS8dWords f, void *c, const tWString &s) { f(c, (const uint32_t *)s.data(), s.size()); }
	tString Str(const unsigned char *p, size_t n) { return tString((const char *)p, n); }
	tWString WStr(const uint32_t *p, size_t n) { return tWString((const wchar_t *)p, n); }

	void HostString(size_t p, OhplS8dBytes f, void *c) { Bytes(f, c, *(const tString *)p); }
	size_t HostIntern(const unsigned char *p, size_t n) { return (size_t)SomaIntern(Str(p, n)); }
	size_t HostGfxElement(const OhplS8dGfx *g) { return (size_t)HostGfx(g); }
	void HostGfxImageSize(size_t g, float *o)
	{
		cVector2f v = ((cGuiGfxElement *)g)->GetImageSize();
		o[0] = v.x, o[1] = v.y;
	}
	void HostGfxName(size_t g, OhplS8dBytes f, void *c)
	{
		auto it = gmapGfxNames.find((cGuiGfxElement *)g);
		Bytes(f, c, it == gmapGfxNames.end() ? tString() : it->second);
	}
	size_t HostWhite() { return (size_t)WhiteGfx(); }
	size_t HostFont(const unsigned char *p, size_t n)
	{
		static std::map<tString, iFontData *> mapFonts;
		tString sFile = Str(p, n);
		iFontData *&pFont = mapFonts[sFile];
		if (pFont == NULL)
			pFont = gpSomaBase->mpEngine->GetResources()->GetFontManager()->CreateFontData(sFile);
		if (pFont)
			gmapFontNames[pFont] = sFile;
		return (size_t)pFont;
	}
	void HostFontName(size_t f, OhplS8dBytes fn, void *c)
	{
		auto it = gmapFontNames.find((iFontData *)f);
		Bytes(fn, c, it == gmapFontNames.end() ? tString() : it->second);
	}
	float HostFontLength(size_t f, const float *size, const uint32_t *t, size_t n) { return ((iFontData *)f)->GetLength(V2(size), WStr(t, n).c_str()); }
	void HostFontWrap(size_t f, float w, float rowH, const float *size, const uint32_t *t, size_t n, OhplS8dWords fn, void *c)
	{
		tWStringVec vRows;
		((iFontData *)f)->GetWordWrapRows(w, rowH, V2(size), WStr(t, n), &vRows);
		for (const tWString &s : vRows)
			Words(fn, c, s);
	}
	void HostDrawGfx(size_t set, size_t gfx, const OhplS8dDraw *d)
	{
		Set(set)->DrawGfx((cGuiGfxElement *)gfx, V3(d->pos), V2(d->size), Col(d->color), (eGuiMaterial)d->material, d->angle);
	}
	void HostDrawFont(size_t set, size_t font, const OhplS8dDraw *d)
	{
		Set(set)->DrawFont(WStr(d->text, d->text_n), (iFontData *)font, V3(d->pos), V2(d->size), Col(d->color), (eFontAlign)d->align);
	}
	size_t HostClipCurrent(size_t set) { return (size_t)Set(set)->GetCurrentClipRegion(); }
	size_t HostClipChild(size_t parent, const float *pos, const float *size) { return (size_t)Clip(parent)->CreateChild(V3(pos), V2(size)); }
	void HostClipSet(size_t set, size_t clip) { Set(set)->SetCurrentClipRegion(Clip(clip)); }
	void HostSetInfo(size_t set, float *size, float *offset, bool *is3D)
	{
		cVector2f v = Set(set)->GetVirtualSize(), o = Set(set)->GetVirtualSizeOffset();
		size[0] = v.x, size[1] = v.y, offset[0] = o.x, offset[1] = o.y;
		*is3D = Set(set)->Is3D();
	}
	void HostScreenSize(float *f, int *i)
	{
		iLowLevelGraphics *pLow = gpSomaBase->mpEngine->GetGraphics()->GetLowLevel();
		cVector2f vf = pLow->GetScreenSizeFloat();
		cVector2l vi = pLow->GetScreenSizeInt();
		f[0] = vf.x, f[1] = vf.y, i[0] = vi.x, i[1] = vi.y;
	}
	size_t HostCreateSet(const unsigned char *p, size_t n)
	{
		cGui *pGui = gpSomaBase->mpEngine->GetGui();
		cGuiSet *pSet = pGui->CreateSet(Str(p, n), pGui->CreateSkin("gui_default.skin"));
		cViewport *pViewport = gpSomaBase->mpEngine->GetScene()->CreateViewport(NULL, NULL, false);
		pViewport->AddGuiSet(pSet);
		return (size_t)pSet;
	}
	void HostSetHudVirtual(size_t set, const float *size, const float *offset) { Set(set)->SetVirtualSize(V2(size), -1000, 1000, V2(offset)); }
	void HostSetRender(size_t set)
	{
		Set(set)->Render(NULL);
		Set(set)->ClearRenderObjects();
	}
	void HostWstr(size_t p, OhplS8dWords f, void *c) { Words(f, c, *(const tWString *)p); }
	CScriptArray *Arr(size_t a) { return (CScriptArray *)a; }
	size_t HostArraySize(size_t a) { return Arr(a)->GetSize(); }
	size_t HostArrayAt(size_t a, size_t i) { return (size_t)Arr(a)->At((asUINT)i); }
	void HostArrayResize(size_t a, size_t n) { Arr(a)->Resize((asUINT)n); }
	void HostArrayPushWstring(size_t a, const uint32_t *p, size_t n)
	{
		tWString s = WStr(p, n);
		Arr(a)->InsertLast(&s);
	}
	void HostArrayPushBool(size_t a, bool b) { Arr(a)->InsertLast(&b); }
	void HostArrayPushI32(size_t a, int v) { Arr(a)->InsertLast(&v); }
	int HostActionSubs(const unsigned char *p, size_t n, OhplS8dSub f, void *c)
	{
		cAction *pAction = gpSomaBase->mpEngine->GetInput()->GetAction(Str(p, n));
		if (pAction == NULL)
			return -1;
		for (int i = 0; i < pAction->GetSubActionNum(); ++i)
		{
			iSubAction *pSub = pAction->GetSubAction(i);
			int lKind = 0, lCode = 0;
			if (pSub->GetInputType() == "Keyboard")
				lKind = 1, lCode = (int)static_cast<cActionKeyboard *>(pSub)->GetKey();
			else if (pSub->GetInputType() == "MouseButton")
				lKind = 2, lCode = (int)static_cast<cActionMouseButton *>(pSub)->GetButton();
			tString sName = pSub->GetInputName();
			f(c, lKind, lCode, (const unsigned char *)sName.data(), sName.size());
		}
		return pAction->GetSubActionNum();
	}
	void HostTranslateButton(const unsigned char *p, size_t n, OhplS8dWords f, void *c)
	{
		Words(f, c, gpSomaBase->mpEngine->GetResources()->Translate("ButtonNames", Str(p, n)));
	}
	void HostKeyName(int key, OhplS8dBytes f, void *c) { Bytes(f, c, gpSomaBase->mpEngine->GetInput()->GetKeyboard()->KeyToString((eKey)key)); }
	float HostStringDuration(const uint32_t *p, size_t n) { return SomaStringDuration(WStr(p, n)); }
	void HostCamTexCreate(const unsigned char *p, size_t n, int w, int h, unsigned fps, float fov, float near, float far)
	{
		CreateCameraTexture(Str(p, n), cVector2l(w, h), fps, fov, near, far);
	}
	void HostCamTexDestroy(const unsigned char *p, size_t n) { DestroyCameraTexture(Str(p, n)); }
	void HostCamTexSettings(const unsigned char *p, size_t n, float fov, float near, float far)
	{
		auto it = gmapCameraTextures.find(Str(p, n));
		if (it == gmapCameraTextures.end())
			return;
		it->second->mpCamera->SetFOV(cMath::ToRad(fov));
		it->second->mpCamera->SetNearClipPlane(near);
		it->second->mpCamera->SetFarClipPlane(far);
	}
	void HostCamTexMatrix(const unsigned char *p, size_t n, size_t m)
	{
		auto it = gmapCameraTextures.find(Str(p, n));
		if (it == gmapCameraTextures.end())
			return;
		const cMatrixf &mtx = *(const cMatrixf *)m;
		it->second->mpCamera->SetRotationMatrix(mtx.GetRotation());
		it->second->mpCamera->SetPosition(mtx.GetTranslation());
		it->second->mbAttached = false;
	}
	void HostCamTexAttach(const unsigned char *p, size_t n, size_t ent)
	{
		auto it = gmapCameraTextures.find(Str(p, n));
		cSomaLuxEntity *pEnt = (cSomaLuxEntity *)ent;
		if (it == gmapCameraTextures.end() || pEnt == NULL)
			return;
		it->second->mAttached = pEnt->mID;
		it->second->mbAttached = true;
	}

	const OhplS8dHost gHost = {HostString, HostIntern, HostGfxElement, HostGfxImageSize, HostGfxName, HostWhite, HostFont, HostFontName, HostFontLength, HostFontWrap,
							   HostDrawGfx, HostDrawFont, HostClipCurrent, HostClipChild, HostClipSet, HostSetInfo, HostScreenSize, HostCreateSet, HostSetHudVirtual,
							   HostSetRender, HostWstr, HostArraySize, HostArrayAt, HostArrayResize, HostArrayPushWstring, HostArrayPushBool, HostArrayPushI32,
							   HostActionSubs, HostTranslateButton, HostKeyName, HostStringDuration, HostCamTexCreate, HostCamTexDestroy, HostCamTexSettings,
							   HostCamTexMatrix, HostCamTexAttach};
}

void SomaInstallImGuiHost() { ohpl_s8d_install(&gHost); }

tWString SomaParseString(const tWString &asText)
{
	tWString sOut;
	ohpl_s8d_parse_string((const uint32_t *)asText.data(), asText.size(), +[](void *c, const uint32_t *p, size_t n) { ((tWString *)c)->assign((const wchar_t *)p, n); }, &sOut);
	return sOut;
}
