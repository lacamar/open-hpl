#include "gui/RustGuiG1.h"

#include "graphics/Graphics.h"
#include "graphics/FontData.h"
#include "graphics/FrameBuffer.h"
#include "graphics/GPUProgram.h"
#include "gui/GuiGfxElement.h"
#include "gui/GuiMaterial.h"
#include "gui/GuiPopUp.h"
#include "gui/Widget.h"
#include "gui/WidgetFrame.h"
#include "gui/WidgetLabel.h"
#include "input/Input.h"
#include "input/Keyboard.h"
#include "math/Math.h"
#include "scene/Camera.h"
#include "scene/Scene.h"
#include <stdarg.h>

#include "graphics/LowLevelGraphics.h"
#include "gui/Gui.h"
#include "gui/GuiMaterialBasicTypes.h"
#include "gui/GuiSet.h"
#include "gui/GuiSkin.h"
#include "resources/FileSearcher.h"
#include "resources/FontManager.h"
#include "system/LowLevelSystem.h"
#include "system/Platform.h"

#include "graphics/FrameSubImage.h"
#include "graphics/Texture.h"
#include "resources/ImageManager.h"
#include "resources/Resources.h"
#include "resources/TextureManager.h"

namespace hpl {

	static void TexInfo(uintptr_t aTex, OhplG1TexInfo *apOut)
	{
		iTexture *pTex = (iTexture*)aTex;
		apOut->width = (float)pTex->GetWidth();
		apOut->height = (float)pTex->GetHeight();
		apOut->rect = pTex->GetType() == eTextureType_Rect;
	}

	static void ImageInfo(uintptr_t aImage, OhplG1ImageInfo *apOut)
	{
		cFrameSubImage *pImage = (cFrameSubImage*)aImage;
		apOut->texture = (uintptr_t)pImage->GetTexture();
		const tVertexVec &vtx = pImage->GetVertexVec();
		for(int i=0; i<4; ++i)
		{
			apOut->tex[i][0] = vtx[i].tex.x;
			apOut->tex[i][1] = vtx[i].tex.y;
			apOut->tex[i][2] = vtx[i].tex.z;
		}
		apOut->width = pImage->GetWidth();
		apOut->height = pImage->GetHeight();
		apOut->updateCount = pImage->GetUpdateCount();
	}

	static void ImageFlush(uintptr_t aImage) { ((cFrameSubImage*)aImage)->Flush(); }
	static void ImageDestroy(void *apResources, uintptr_t aImage) { ((cResources*)apResources)->GetImageManager()->Destroy((cFrameSubImage*)aImage); }
	static void TextureDestroy(void *apResources, uintptr_t aTex) { ((cResources*)apResources)->GetTextureManager()->Destroy((iTexture*)aTex); }

	static void HostLog(int alKind, const char *apText, size_t alLen)
	{
		tString sText(apText, alLen);
		if(alKind == 0) Log("%s", sText.c_str());
		else if(alKind == 1) Warning("%s", sText.c_str());
		else Error("%s", sText.c_str());
	}

	static uintptr_t MaterialNew(int alKind, void *apGraphics)
	{
		iLowLevelGraphics *pLow = ((cGraphics*)apGraphics)->GetLowLevel();
		switch(alKind)
		{
		case eGuiMaterial_Diffuse: return (uintptr_t)(iGuiMaterial*)hplNew( cGuiMaterial_Diffuse,(pLow) );
		case eGuiMaterial_Alpha: return (uintptr_t)(iGuiMaterial*)hplNew( cGuiMaterial_Alpha,(pLow) );
		case eGuiMaterial_FontNormal: return (uintptr_t)(iGuiMaterial*)hplNew( cGuiMaterial_FontNormal,(pLow) );
		case eGuiMaterial_Additive: return (uintptr_t)(iGuiMaterial*)hplNew( cGuiMaterial_Additive,(pLow) );
		case eGuiMaterial_Modulative: return (uintptr_t)(iGuiMaterial*)hplNew( cGuiMaterial_Modulative,(pLow) );
		case eGuiMaterial_PremulAlpha: return (uintptr_t)(iGuiMaterial*)hplNew( cGuiMaterial_PremulAlpha,(pLow) );
		}
		return 0;
	}

	static void MaterialDelete(uintptr_t aMaterial) { iGuiMaterial *p = (iGuiMaterial*)aMaterial; hplDelete(p); }

	static uintptr_t ImageCreate(void *apRes, const char *apFile, size_t alLen)
	{
		return (uintptr_t)((cResources*)apRes)->GetImageManager()->CreateImage(tString(apFile, alLen));
	}

	static uintptr_t TextureCreate2D(void *apRes, const char *apFile, size_t alLen, bool abMips, int alType)
	{
		return (uintptr_t)((cResources*)apRes)->GetTextureManager()->Create2D(tString(apFile, alLen), abMips, (eTextureType)alType);
	}

	static bool TextureRenderTarget(uintptr_t aTex) { return ((iTexture*)aTex)->GetUsage() == eTextureUsage_RenderTarget; }

	static bool FileFound(void *apRes, const char *apFile, size_t alLen)
	{
		return ((cResources*)apRes)->GetFileSearcher()->GetFilePath(tString(apFile, alLen)) != _W("");
	}

	static uintptr_t FontCreate(void *apRes, const char *apFile, size_t alLen)
	{
		return (uintptr_t)((cResources*)apRes)->GetFontManager()->CreateFontData(tString(apFile, alLen));
	}

	static void FindFile(void *apRes, const char *apFile, size_t alLen, void (*apSink)(void*, const uint32_t*, size_t), void *apCtx)
	{
		tWString sPath = ((cResources*)apRes)->GetFileSearcher()->GetFilePath(tString(apFile, alLen));
		apSink(apCtx, (const uint32_t*)sPath.data(), sPath.size());
	}

	static int ReadFile(const uint32_t *apPath, size_t alLen, void (*apSink)(void*, const char*, size_t), void *apCtx)
	{
		FILE *pFile = cPlatform::OpenFile(tWString((const wchar_t*)apPath, alLen), _W("rb"));
		if(pFile == NULL) return 0;
		fseek(pFile, 0, SEEK_END);
		long lLength = ftell(pFile);
		fseek(pFile, 0, SEEK_SET);
		std::vector<char> vData(lLength > 0 ? lLength : 0);
		bool bReadOk = lLength <= 0 || fread(vData.data(), lLength, 1, pFile) == 1;
		fclose(pFile);
		apSink(apCtx, vData.data(), vData.size());
		return bReadOk ? 1 : 2;
	}

	static void ScreenSize(void *apGraphics, float *apOut)
	{
		cVector2f vSize = ((cGraphics*)apGraphics)->GetLowLevel()->GetScreenSizeFloat();
		apOut[0] = vSize.x;
		apOut[1] = vSize.y;
	}

	static OhplG1Host gG1Host = { HostLog, MaterialNew, MaterialDelete, ImageCreate, TextureCreate2D, TextureRenderTarget, FileFound, FontCreate,
								  FindFile, ReadFile, ScreenSize };

	static OhplG1Engine gG1Engine = { TexInfo, ImageInfo, ImageFlush, ImageDestroy, TextureDestroy };


	//////////////////////////////////////////////////////////////////////////
	// cGuiSet: text drawing, rendering and the trace hook
	//////////////////////////////////////////////////////////////////////////

	void (*gfpOhplGuiTrace)(cGuiSet*, const cGuiGfxElement*, const tWString*, const cVector3f&, const cVector2f&, const cColor&, int) = NULL;

	static void TraceDraw(void *apSet, void *apGfx, const cVector3f *apPos, const cVector2f *apSize, const cColor *apColor, int alMaterial)
	{
		if(gfpOhplGuiTrace) gfpOhplGuiTrace((cGuiSet*)apSet, (const cGuiGfxElement*)apGfx, NULL, *apPos, *apSize, *apColor, alMaterial);
	}

	static void DrawTextFromCharArray(cGuiSet *apSet, const wchar_t* apString, iFontData *apFont,
										const cVector2f& avSize, const cVector3f& avPosition,
										const cColor& aColor, eGuiMaterial aMaterial, eFontAlign aAlign)
	{
		const tGlyphVec &vGlyphs = apFont->GetGlyphs();
		OhplGuiGlyphTable table = {	(const void* const*)vGlyphs.data(), vGlyphs.size(), apFont->GetFirstChar(), apFont->GetLastChar(),
									offsetof(cGlyph, mpGuiGfx), offsetof(cGlyph, mvOffset), offsetof(cGlyph, mvSize), offsetof(cGlyph, mfAdvance) };
		ohpl_g1_set_draw_text(apSet, &table, (const uint32_t*)apString, wcslen(apString), &avSize, &avPosition, &aColor, aMaterial, aAlign);
	}

	void cGuiSet::DrawFont(	const tWString &asText,
							iFontData *apFont, const cVector3f &avPos,
							const cVector2f &avSize, const cColor& aColor,
							eFontAlign aAlign, eGuiMaterial aMaterial)
	{
		if(gfpOhplGuiTrace) gfpOhplGuiTrace(this, NULL, &asText, avPos, avSize, aColor, (int)aAlign);
		DrawTextFromCharArray(this, asText.c_str(), apFont,avSize,avPos,aColor,aMaterial,aAlign);
	}

	static wchar_t gsTempTextArray[1024];
	void cGuiSet::DrawFont (iFontData *apFont, const cVector3f &avPos,
							const cVector2f &avSize, const cColor& aColor,
							eFontAlign aAlign,eGuiMaterial aMaterial,
							const wchar_t* fmt,...)
	{
		va_list ap;
		if (fmt == NULL) return;
		va_start(ap, fmt);
		vswprintf(gsTempTextArray, 1023, fmt, ap);
		va_end(ap);

		DrawTextFromCharArray(this, gsTempTextArray, apFont,avSize,avPos,aColor,aMaterial,aAlign);
	}

	void cGuiSet::DrawFont (	iFontData *apFont, const cVector3f &avPos,
								const cVector2f &avSize, const cColor& aColor,
								const wchar_t* fmt,...)
	{
		va_list ap;
		if (fmt == NULL) return;
		va_start(ap, fmt);
		vswprintf(gsTempTextArray, 1023, fmt, ap);
		va_end(ap);

		DrawTextFromCharArray(this, gsTempTextArray, apFont,avSize,avPos,aColor,eGuiMaterial_FontNormal,eFontAlign_Left);
	}

	static void SetClipArea(cGuiSet *apSet, iLowLevelGraphics *apLowLevel, const cGuiClipRegion *apRegion)
	{
		cRect2f clipRect;
		ohpl_g1_clip_rect(apRegion, &clipRect);
		if(clipRect.w <= 0) return;

		// HPL3: scissor; clip planes need gl_ClipVertex, which shader-drawn sets never write
		if(apSet->Is3D() == false)
		{
			iFrameBuffer *pTarget = apLowLevel->GetCurrentFrameBuffer();
			cVector2l vTarget = pTarget ? pTarget->GetSize() : apLowLevel->GetScreenSizeInt();
			cVector2l vPos, vSize;
			ohpl_g1_set_scissor(apSet, apRegion, &vTarget, &vPos, &vSize);
			apLowLevel->SetScissorActive(true);
			apLowLevel->SetScissorRect(vPos, vSize);
			return;
		}

		cPlanef plane;
		plane.FromNormalPoint(cVector3f(0,-1,0),cVector3f(0,clipRect.y+clipRect.h,0));
		apLowLevel->SetClipPlane(0, plane);
		plane.FromNormalPoint(cVector3f(0,1,0),cVector3f(0,clipRect.y,0));
		apLowLevel->SetClipPlane(1, plane);
		plane.FromNormalPoint(cVector3f(1,0,0),cVector3f(clipRect.x,0,0));
		apLowLevel->SetClipPlane(2, plane);
		plane.FromNormalPoint(cVector3f(-1,0,0),cVector3f(clipRect.x+clipRect.w,0,0));
		apLowLevel->SetClipPlane(3, plane);
		for(int i=0; i<4; ++i) apLowLevel->SetClipPlaneActive(i, true);
	}

	static void RenderClipRegion(cGuiSet *apSet, iLowLevelGraphics *pLowLevelGraphics, iGpuProgram *const *apPrograms)
	{
		OhplGuiBatches batches;
		ohpl_gui_build(ohpl_g1_set_list(apSet), &batches);
		if(batches.batch_count == 0) return;

		pLowLevelGraphics->SetTexture(0,NULL);

		for(size_t b=0; b<batches.batch_count; ++b)
		{
			const OhplGuiBatch &batch = batches.batches[b];
			iGuiMaterial *pMaterial = (iGuiMaterial*)batch.material;
			iTexture *pTexture = (iTexture*)batch.texture;
			const cGuiClipRegion *pClipRegion = (const cGuiClipRegion*)batch.clip;

			if(batch.flags & kOhplGuiBeginMaterial) pMaterial->BeforeRender();
			if(batch.flags & kOhplGuiSetClip) SetClipArea(apSet, pLowLevelGraphics,pClipRegion);

			pLowLevelGraphics->SetTexture(0,pTexture);
			if(apPrograms[0] && apPrograms[1]) apPrograms[pTexture ? 0 : 1]->Bind();

			const OhplGuiVertex *pVtx = batches.vertices + batch.first;
			for(size_t i=0; i<batch.count; i+=4)
			{
				for(size_t j=0; j<4; ++j)
					pLowLevelGraphics->AddVertexToBatch_Raw(pVtx[i+j].pos, pVtx[i+j].col, pVtx[i+j].tex);
				for(size_t j=0; j<4; ++j)
					pLowLevelGraphics->AddIndexToBatch((int)(i+j));
			}

			pLowLevelGraphics->FlushQuadBatch(	eVtxBatchFlag_Position | eVtxBatchFlag_Texture0 |
												eVtxBatchFlag_Color0,false);
			pLowLevelGraphics->ClearBatch();

			cRect2f clipRect;
			ohpl_g1_clip_rect(pClipRegion, &clipRect);
			if((batch.flags & kOhplGuiEndClip) && clipRect.w >0)
			{
				if(apSet->Is3D()) for(int i=0; i<4; ++i) pLowLevelGraphics->SetClipPlaneActive(i, false);
				else pLowLevelGraphics->SetScissorActive(false);
			}

			if(batch.flags & kOhplGuiEndMaterial) pMaterial->AfterRender();
		}

		if(apPrograms[0]) apPrograms[0]->UnBind();
	}

	void cGuiSet::Render(cFrustum *apFrustum)
	{
		iLowLevelGraphics *pLowLevelGraphics = ((cGraphics*)ohpl_g1_set_graphics(this))->GetLowLevel();
		OhplG1SetPub &p = P();
		float fMinZ = ohpl_g1_set_virtual_min_z(this);
		float fMaxZ = ohpl_g1_set_virtual_max_z(this);

		///////////////////////////////////
		// Init rendering

		//3D projection
		if(p.is3D)
		{
			pLowLevelGraphics->SetDepthTestActive(true);
			pLowLevelGraphics->SetDepthWriteActive(false);

			//Invert the y coordinate: = -y, this also get the gui into the correct position.
			//Also scale to size
			cVector3f vPreScale = cVector3f(p.size3D.x / p.virtualSize.x,
											-p.size3D.y / p.virtualSize.y,
											p.size3D.z / (fMaxZ - fMinZ));
			cMatrixf mtxPreMul = cMath::MatrixScale(vPreScale);
			//note: Offset needs to be converted to shape coords (done by multiplying with pre scale)
			mtxPreMul.SetTranslation(cVector3f(p.virtualOffset.x*vPreScale.x, p.virtualOffset.y*vPreScale.y, 0));

			//Create the final model matrix
			cMatrixf mtxModel = cMath::MatrixMul(Get3DTransform(), mtxPreMul);
			mtxModel = cMath::MatrixMul(apFrustum->GetViewMatrix(), mtxModel);

			pLowLevelGraphics->SetMatrix(eMatrix_ModelView, mtxModel);

			//No need for projection matrix, should be setup, right? :)

			pLowLevelGraphics->SetCullActive(p.cullBackface);
		}
		//Screen projection
		else
		{
			pLowLevelGraphics->SetDepthTestActive(false);
			pLowLevelGraphics->SetDepthWriteActive(false);
			pLowLevelGraphics->SetIdentityMatrix(eMatrix_ModelView);

			//Set up min and max for orth projection
			cVector3f vProjMin(-p.virtualOffset.x, -p.virtualOffset.y, fMinZ);
			cVector3f vProjMax(p.virtualSize.x-p.virtualOffset.x, p.virtualSize.y-p.virtualOffset.y, fMaxZ);
			if(p.flipY) std::swap(vProjMin.y, vProjMax.y);

			pLowLevelGraphics->SetOrthoProjection(vProjMin,vProjMax);
		}

		///////////////////////////////
		// Render all clip regions

		RenderClipRegion(this, pLowLevelGraphics, (iGpuProgram *const *)p.programs);

		///////////////////////////////
		//Clear the render object set
		ohpl_g1_set_clear_base_clip(this);

		if(p.is3D)
		{
			if(p.cullBackface==false) pLowLevelGraphics->SetCullActive(true);
		}
	}

	void OhplG2RegisterPopupOps();
	void OhplGwRegisterHost();
	static struct OhplG1Register { OhplG1Register() { ohpl_g1_set_engine(&gG1Engine); ohpl_g1_set_host(&gG1Host); OhplG2RegisterPopupOps(); OhplGwRegisterHost(); ohpl_g1_set_trace_fn(TraceDraw); } } gG1Register;
}
