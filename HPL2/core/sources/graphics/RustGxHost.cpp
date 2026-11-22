#include "graphics/RustGx.h"
#include "graphics/Graphics.h"
#include "graphics/MeshCreator.h"
#include "graphics/TextureCreator.h"
#include "graphics/DecalCreator.h"
#include "graphics/FrameBuffer.h"
#include "graphics/PostEffectComposite.h"
#include "graphics/PostEffect.h"
#include "graphics/PostEffect_Bloom.h"
#include "graphics/PostEffect_ColorConvTex.h"
#include "graphics/PostEffect_ImageTrail.h"
#include "graphics/PostEffect_RadialBlur.h"
#include "graphics/MaterialType.h"
#include "graphics/Texture.h"
#include "graphics/GPUProgram.h"
#include "graphics/GPUShader.h"
#include "graphics/RendererDeferred.h"
#include "graphics/RendererWireFrame.h"
#include "graphics/RendererSimple.h"
#include "resources/LowLevelResources.h"
#include "resources/GpuShaderManager.h"
#include "resources/FileSearcher.h"
#include "system/LowLevelSystem.h"
#include "system/String.h"
#include "system/Platform.h"

#include "graphics/LowLevelGraphics.h"
#include "graphics/VertexBuffer.h"
#include "graphics/Mesh.h"
#include "graphics/SubMesh.h"
#include "graphics/Material.h"
#include "scene/SubMeshEntity.h"
#include "resources/Resources.h"
#include "resources/MaterialManager.h"
#include "resources/AnimationManager.h"
#include "math/BoundingVolume.h"

namespace hpl {

	namespace {
		void* VbCreate(void *apLl)
		{
			return static_cast<iLowLevelGraphics*>(apLl)->CreateVertexBuffer(eVertexBufferType_Hardware, eVertexBufferDrawType_Tri, eVertexBufferUsageType_Static);
		}
		bool VbCompile(void *apVb, int32_t alFlags) { return static_cast<iVertexBuffer*>(apVb)->Compile(alFlags); }

		void SubView(void *apSub, OhplGxSubView *apOut)
		{
			cSubMeshEntity *pSub = static_cast<cSubMeshEntity*>(apSub);
			iVertexBuffer *pVb = pSub->GetVertexBuffer();
			const int lVtx = pVb->GetVertexNum();
			const int lPos = pVb->GetElementNum(eVertexBufferElement_Position);
			const int lNrm = pVb->GetElementNum(eVertexBufferElement_Normal);
			apOut->world = pSub->GetWorldMatrix();
			apOut->bv = pSub->GetBoundingVolume()->Rs();
			apOut->pos = pVb->GetFloatArray(eVertexBufferElement_Position);
			apOut->posLen = (size_t)lVtx * lPos;
			apOut->posStride = (uint32_t)lPos;
			apOut->nrm = pVb->GetFloatArray(eVertexBufferElement_Normal);
			apOut->nrmLen = (size_t)lVtx * lNrm;
			apOut->nrmStride = (uint32_t)lNrm;
			apOut->idx = pVb->GetIndices();
			apOut->idxNum = (uint32_t)pVb->GetIndexNum();
		}

		void* MakeMesh(void *apRes, void *apVb, void *apMat)
		{
			cResources *pRes = static_cast<cResources*>(apRes);
			iVertexBuffer *pVb = static_cast<iVertexBuffer*>(apVb);
			cMesh *pMesh = hplNew( cMesh, ("", _W(""), pRes->GetMaterialManager(), pRes->GetAnimationManager()) );
			cSubMesh *pSubMesh = pMesh->CreateSubMesh("Decal");
			pSubMesh->SetVertexBuffer(pVb->CreateCopy(eVertexBufferType_Hardware, eVertexBufferUsageType_Static, pVb->GetVertexElementFlags()));
			pSubMesh->SetMaterial(static_cast<cMaterial*>(apMat));
			return pMesh;
		}

		cResources* R(void *p) { return static_cast<cResources*>(p); }
		iLowLevelGraphics* L(void *p) { return static_cast<iLowLevelGraphics*>(p); }

		void DeleteObj(int alKind, void *p)
		{
			switch(alKind)
			{
			case 0: hplDelete(static_cast<iTexture*>(p)); break;
			case 1: hplDelete(static_cast<iFrameBuffer*>(p)); break;
			case 2: hplDelete(static_cast<iDepthStencilBuffer*>(p)); break;
			case 3: hplDelete(static_cast<iGpuProgram*>(p)); break;
			case 4: hplDelete(static_cast<iPostEffect*>(p)); break;
			case 5: hplDelete(static_cast<cPostEffectComposite*>(p)); break;
			case 6: hplDelete(static_cast<iPostEffectType*>(p)); break;
			case 7: hplDelete(static_cast<iRenderer*>(p)); break;
			case 8: hplDelete(static_cast<cMeshCreator*>(p)); break;
			case 9: hplDelete(static_cast<cTextureCreator*>(p)); break;
			case 10: hplDelete(static_cast<cDecalCreator*>(p)); break;
			}
		}
		void Fatal(const char *s, size_t l) { FatalError("%s", tString(s, l).c_str()); }
		void LlInit(void *ll, const OhplGxLlInit *i)
		{
			L(ll)->Init(i->width, i->height, i->display, i->bpp, i->fullscreen, i->multisampling, (eGpuProgramFormat)i->gpuFormat, tString(i->caption, i->captionLen), i->pos);
		}
		bool LlCheckScreen(void *ll) { return L(ll)->CheckAndUpdateScreenSize(); }
		void* LlCreateTexture(void *ll, const char *s, size_t l, int k, int u) { return static_cast<iTexture*>(L(ll)->CreateTexture(tString(s, l), (eTextureType)k, (eTextureUsage)u)); }
		void* LlCreateFb(void *ll, const char *s, size_t l) { return static_cast<iFrameBuffer*>(L(ll)->CreateFrameBuffer(tString(s, l))); }
		void* LlCreateDs(void *ll, const cVector2l *v, int d, int s) { return static_cast<iDepthStencilBuffer*>(L(ll)->CreateDepthStencilBuffer(*v, d, s)); }
		void* LlCreateProg(void *ll, const char *s, size_t l) { return static_cast<iGpuProgram*>(L(ll)->CreateGpuProgram(tString(s, l))); }
		void* CreatorNew(int k, void *ll, void *res)
		{
			if(k == 0) return hplNew( cMeshCreator,(L(ll), R(res)));
			if(k == 1) return hplNew( cTextureCreator,(L(ll), R(res)));
			return hplNew( cDecalCreator,(L(ll), R(res)));
		}
		void* CompositeNew(void *g) { return hplNew( cPostEffectComposite,(static_cast<cGraphics*>(g)) ); }
		void ResAddDir(void *r, const char *s, size_t l, bool ab) { R(r)->AddResourceDir(cString::To16Char(tString(s, l)), ab); }
		void ResAddCompatDir(void *r)
		{
			// HPL3 data ships these core meshes only as HPL3 .msh
			tWString sCompatDir = cString::To16Char(cPlatform::GetDataDir()) + _W("/compat");
			if(cPlatform::FolderExists(sCompatDir)) R(r)->AddResourceDir(sCompatDir, false);
		}
		void ResCheckFeatures(void *r) { R(r)->GetGpuShaderManager()->CheckFeatureSupport(); }
		bool ResHasFile(void *r, const char *s, size_t l) { return R(r)->GetFileSearcher()->GetFilePath(tString(s, l)) != _W(""); }
		void ResCompileTextureLookups(void *r) { R(r)->GetMaterialManager()->CompileTextureLookups(); }
		void* RendererNew(int k, void *g, void *r)
		{
			cGraphics *pGfx = static_cast<cGraphics*>(g);
			if(k == 0) return static_cast<iRenderer*>(hplNew(cRendererDeferred, (pGfx, R(r))));
			if(k == 1) return static_cast<iRenderer*>(hplNew(cRendererWireFrame, (pGfx, R(r))));
			return static_cast<iRenderer*>(hplNew(cRendererSimple, (pGfx, R(r))));
		}
		bool RendererLoad(void *r) { return static_cast<iRenderer*>(r)->LoadData(); }
		void RendererDestroyData(void *r) { static_cast<iRenderer*>(r)->DestroyData(); }
		void RendererUpdate(void *r, float af) { static_cast<iRenderer*>(r)->Update(af); }
		void RendererSetupFuncs(void *r, void *ll) { static_cast<iRenderer*>(r)->SetupRenderFunctions(L(ll)); }
		void* MatTypeNew(int c, void *g, void *r) { return static_cast<iMaterialType*>(iMaterialType::Create(c, static_cast<cGraphics*>(g), R(r))); }
		void MatTypeSetName(void *t, const char *s, size_t l) { static_cast<iMaterialType*>(t)->SetName(tString(s, l)); }
		void MatTypeLoad(void *t) { static_cast<iMaterialType*>(t)->LoadData(); }
		void MatTypeDestroyData(void *t) { static_cast<iMaterialType*>(t)->DestroyData(); }
		void MatTypeDestroy(void *t) { static_cast<iMaterialType*>(t)->Destroy(); }
		void MatTypeReload(void *t) { static_cast<iMaterialType*>(t)->Reload(); }
		void MatDestroyTypeSpecifics(bool ab) { cMaterial::SetDestroyTypeSpecifics(ab); }
		void* PeTypeNew(int k, void *g, void *r)
		{
			cGraphics *pGfx = static_cast<cGraphics*>(g);
			if(k == 0) return static_cast<iPostEffectType*>(hplNew( cPostEffectType_Bloom, (pGfx, R(r))));
			if(k == 1) return static_cast<iPostEffectType*>(hplNew( cPostEffectType_ColorConvTex, (pGfx, R(r))));
			if(k == 2) return static_cast<iPostEffectType*>(hplNew( cPostEffectType_ImageTrail, (pGfx, R(r))));
			return static_cast<iPostEffectType*>(hplNew( cPostEffectType_RadialBlur, (pGfx, R(r))));
		}
		const char* PeTypeName(void *t, size_t *apLen) { const tString &s = static_cast<iPostEffectType*>(t)->GetName(); *apLen = s.size(); return s.data(); }
		void* PeTypeCreate(void *t, void *p) { return static_cast<iPostEffect*>(static_cast<iPostEffectType*>(t)->CreatePostEffect(static_cast<iPostEffectParams*>(p))); }
		void PeSetParams(void *pe, void *p) { static_cast<iPostEffect*>(pe)->SetParams(static_cast<iPostEffectParams*>(p)); }
		const char* ParamsName(void *p, size_t *apLen) { const tString &s = static_cast<iPostEffectParams*>(p)->GetName(); *apLen = s.size(); return s.data(); }
		void TexSetup(void *t, const cVector2l *v, int f)
		{
			iTexture *pTexture = static_cast<iTexture*>(t);
			pTexture->CreateFromRawData(cVector3l(v->x, v->y, 0), (ePixelFormat)f, NULL);
			pTexture->SetWrapSTR(eTextureWrap_ClampToEdge);
		}
		bool FbAttach(void *fb, void *t)
		{
			iFrameBuffer *pFb = static_cast<iFrameBuffer*>(fb);
			pFb->SetTexture2D(0, static_cast<iTexture*>(t));
			return pFb->CompileAndValidate();
		}
		void DsInfo(void *d, OhplGxDsInfo *o)
		{
			iDepthStencilBuffer *p = static_cast<iDepthStencilBuffer*>(d);
			o->size = p->GetSize();
			o->depthBits = p->GetDepthBits();
			o->stencilBits = p->GetStencilBits();
		}
		void DsInc(void *d) { static_cast<iDepthStencilBuffer*>(d)->IncUserCount(); }
		bool DsDecHasUsers(void *d) { iDepthStencilBuffer *p = static_cast<iDepthStencilBuffer*>(d); p->DecUserCount(); return p->HasUsers(); }
		void ProgSetRes(void *p, void *r) { static_cast<iGpuProgram*>(p)->SetResources(R(r)); }
		void* ShaderCreate(void *r, const char *s, size_t l, int k, void *v)
		{
			return static_cast<iGpuShader*>(R(r)->GetGpuShaderManager()->CreateShader(tString(s, l), k == 0 ? eGpuShaderType_Vertex : eGpuShaderType_Fragment, static_cast<cParserVarContainer*>(v)));
		}
		void ShaderDestroy(void *r, void *s) { R(r)->GetGpuShaderManager()->Destroy(static_cast<iGpuShader*>(s)); }
		void ProgSetShader(void *p, int k, void *s) { static_cast<iGpuProgram*>(p)->SetShader(k == 0 ? eGpuShaderType_Vertex : eGpuShaderType_Fragment, static_cast<iGpuShader*>(s)); }
		void ProgLink(void *p) { static_cast<iGpuProgram*>(p)->Link(); }
		void TexFill(void *t, const cVector3l *v, unsigned char *d)
		{
			iTexture *pTexture = static_cast<iTexture*>(t);
			pTexture->CreateFromRawData(*v, ePixelFormat_RGBA, d);
			pTexture->SetWrapSTR(eTextureWrap_Repeat);
			pTexture->SetFilter(eTextureFilter_Nearest);
		}

		struct cRegister
		{
			cRegister()
			{
				static const OhplGxHost h = { VbCreate, VbCompile, SubView, MakeMesh };
				ohpl_gx_set_host(&h);
				static const OhplGxGfxHost g = { DeleteObj, Fatal, LlInit, LlCheckScreen, LlCreateTexture, LlCreateFb, LlCreateDs, LlCreateProg,
												 CreatorNew, CompositeNew, ResAddDir, ResAddCompatDir, ResCheckFeatures, ResHasFile, ResCompileTextureLookups,
												 RendererNew, RendererLoad, RendererDestroyData, RendererUpdate, RendererSetupFuncs,
												 MatTypeNew, MatTypeSetName, MatTypeLoad, MatTypeDestroyData, MatTypeDestroy, MatTypeReload, MatDestroyTypeSpecifics,
												 PeTypeNew, PeTypeName, PeTypeCreate, PeSetParams, ParamsName, TexSetup, FbAttach, DsInfo, DsInc, DsDecHasUsers,
												 ProgSetRes, ShaderCreate, ShaderDestroy, ProgSetShader, ProgLink, TexFill };
				ohpl_gx_set_gfx_host(&g);
			}
		} gRegister;
	}

	cGraphics::cGraphics(iLowLevelGraphics *apLowLevelGraphics, iLowLevelResources *apLowLevelResources) : iUpdateable("HPL_Graphics"),
		mpLowLevelGraphics(apLowLevelGraphics), mpRs(ohpl_gx_gfx_new(this, apLowLevelGraphics))
	{
	}

	cGraphics::~cGraphics() { ohpl_gx_gfx_free(mpRs); }

	void cGraphics::Update(float afTimeStep) { ohpl_gx_gfx_update(mpRs, afTimeStep); }
}
