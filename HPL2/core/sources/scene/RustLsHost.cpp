// Engine objects behind the Rust-owned scene shells (rust/src/scene/ls_host.rs) and the out-of-line
// destructors that anchor their vtables. Each thunk is deleted with the C++ class it wraps.

#include "scene/RustLs.h"
#include "scene/RopeEntity.h"
#include "scene/BillBoard.h"
#include "scene/Beam.h"
#include "scene/LensFlare.h"
#include "scene/Light.h"
#include "scene/LightPoint.h"
#include "scene/LightSpot.h"
#include "scene/LightBox.h"
#include "scene/FogArea.h"
#include "scene/World.h"
#include "scene/ParticleSystem.h"
#include "scene/SoundEntity.h"
#include "scene/RenderableContainer.h"
#include "resources/TextureManager.h"
#include "graphics/Texture.h"
#include "scene/SubMeshEntity.h"
#include "graphics/Renderer.h"
#include "graphics/RendererDeferred.h"
#include "graphics/Material.h"
#include "system/LowLevelSystem.h"
#include "impl/XmlDocumentTiny.h"
#include "resources/FileSearcher.h"
#include "system/Platform.h"
#include "scene/EnvironmentParticles.h"
#include "scene/LightSpot.h"
#include "resources/TextureManager.h"
#include "graphics/GPUProgram.h"
#include "system/PreprocessParser.h"
#include "scene/Entity3D.h"

#include "graphics/Graphics.h"
#include "graphics/LowLevelGraphics.h"
#include "graphics/VertexBuffer.h"
#include "resources/Resources.h"
#include "resources/MaterialManager.h"
#include "physics/PhysicsRope.h"
#include "physics/VerletParticle.h"

namespace hpl {

	namespace {
		iVertexBuffer* V(void *p){ return static_cast<iVertexBuffer*>(p); }

		void* VbCreate(void *apGfx, int32_t alUsage, int32_t alVerts, int32_t alIdx)
		{
			return static_cast<cGraphics*>(apGfx)->GetLowLevel()->CreateVertexBuffer(eVertexBufferType_Hardware,
								eVertexBufferDrawType_Tri, (eVertexBufferUsageType)alUsage, alVerts, alIdx);
		}
		void VbElem(void *p, int32_t e, int32_t n){ V(p)->CreateElementArray((eVertexBufferElement)e, eVertexBufferElementFormat_Float, n);}
		void VbAdd(void *p, int32_t e, int32_t n, const float *v)
		{
			if(e == eVertexBufferElement_Color0) V(p)->AddVertexColor(eVertexBufferElement_Color0, cColor(v[0], v[1], v[2], v[3]));
			else if(n == 4) V(p)->AddVertexVec4f((eVertexBufferElement)e, cVector3f(v[0], v[1], v[2]), v[3]);
			else V(p)->AddVertexVec3f((eVertexBufferElement)e, cVector3f(v[0], v[1], v[2]));
		}
		void VbIndex(void *p, int32_t i){ V(p)->AddIndex(i);}
		void VbCompile(void *p, int32_t f){ V(p)->Compile(f);}
		float* VbArray(void *p, int32_t e){ return V(p)->GetFloatArray((eVertexBufferElement)e);}
		void VbUpdate(void *p, int32_t f){ V(p)->UpdateData(f, false);}
		void VbCount(void *p, int32_t n){ V(p)->SetElementNum(n);}
		void VbFree(void *p){ hplDelete(V(p));}
		void MatDestroy(void *r, void *m){ static_cast<cResources*>(r)->GetMaterialManager()->Destroy(static_cast<cMaterial*>(m));}

		void RopeInfo(void *p, int32_t *apOut)
		{
			iPhysicsRope *pRope = static_cast<iPhysicsRope*>(p);
			apOut[0] = pRope->GetUpdateCount();
			apOut[1] = pRope->GetParticleNum();
			const float f[2] = {pRope->GetSegmentLength(), pRope->GetParticleRadius()};
			memcpy(&apOut[2], f, sizeof(f));
		}
		int32_t RopePoints(void *p, cVector3f *apOut, int32_t alTake, bool abSmooth)
		{
			cVerletParticleIterator it = static_cast<iPhysicsRope*>(p)->GetParticleIterator();
			int32_t lCount = 0;
			for(; lCount < alTake && it.HasNext(); ++lCount)
			{
				cVerletParticle *pPart = it.Next();
				apOut[lCount] = (abSmooth && lCount > 0) ? pPart->GetSmoothPosition() : pPart->GetPosition();
			}
			return lCount;
		}
		const cMatrixf* EntWorld(void *p){ return &static_cast<iEntity3D*>(p)->GetWorldMatrix();}
		void EntUpdated(void *p, bool abCallbacks){ static_cast<iEntity3D*>(p)->SetTransformUpdated(abCallbacks);}

		void TexDestroy(void *r, void *t){ static_cast<cResources*>(r)->GetTextureManager()->Destroy(static_cast<iTexture*>(t));}
		void* ProgCreate(void *g, const char *n, size_t l, const OhplLsVar *v, size_t c)
		{
			cParserVarContainer vars;
			for(size_t i=0; i<c; ++i)
			{
				if(v[i].hasValue) vars.Add(tString(v[i].name, v[i].len), v[i].value);
				else vars.Add(tString(v[i].name, v[i].len));
			}
			return static_cast<cGraphics*>(g)->CreateGpuProgramFromShaders(tString(n, l),
								"deferred_env_particles_vtx.glsl", "deferred_base_frag.glsl", &vars);
		}
		void ProgDestroy(void *g, void *p){ static_cast<cGraphics*>(g)->DestroyGpuProgram(static_cast<iGpuProgram*>(p));}
		void ProgVar(void *p, const char *n, size_t l, int32_t id){ static_cast<iGpuProgram*>(p)->GetVariableAsId(tString(n, l), id);}
		void ProgSet(void *p, int32_t id, int32_t n, const float *v)
		{
			iGpuProgram *pProg = static_cast<iGpuProgram*>(p);
			if(n == 1) pProg->SetFloat(id, v[0]);
			else if(n == 3) pProg->SetVec3f(id, v[0], v[1], v[2]);
			else if(n == 4) pProg->SetVec4f(id, v[0], v[1], v[2], v[3]);
			else pProg->SetMatrixf(id, *reinterpret_cast<const cMatrixf*>(v));
		}
		void EntClip(void *p, OhplLsClip *apOut)
		{
			iEntity3D *pEnt = static_cast<iEntity3D*>(p);
			apOut->active = pEnt->IsActive();
			apOut->bv = pEnt->GetBoundingVolume()->Rs();
		}
		void SpotInfo(void *p, OhplLsSpot *apOut)
		{
			cLightSpot *pSpot = static_cast<cLightSpot*>(p);
			apOut->color = pSpot->GetDiffuseColor();
			apOut->brightness = pSpot->GetBrightness();
			apOut->visible = pSpot->IsVisible();
			apOut->viewProj = pSpot->GetViewProjMatrix();
			apOut->pos = pSpot->GetWorldPosition();
			apOut->radius = pSpot->GetRadius();
			apOut->falloffPow = pSpot->GetFalloffPow();
			apOut->spotFalloffPow = pSpot->GetSpotFalloffPow();
		}
		bool LightLevel(const cVector3f *apPos, float *apOut)
		{
			if(cEnvironmentParticles::mpLightLevelFunc == NULL) return false;
			if(apPos) *apOut = cEnvironmentParticles::mpLightLevelFunc(*apPos);
			return true;
		}

		bool Hdr(){ return cRendererDeferred::GetHdr();}
		void EntPos(void *p, cVector3f *apOut){ *apOut = static_cast<iEntity3D*>(p)->GetWorldPosition();}
		void OccAssign(void *r, void *c, int32_t i, void *v, const cMatrixf *m, bool b)
		{
			iRenderer *pR = static_cast<iRenderer*>(r);
			pR->AssignOcclusionObject(static_cast<iRenderable*>(c), i, v ? static_cast<iVertexBuffer*>(v) : pR->GetShapeBoxVertexBuffer(),
									  const_cast<cMatrixf*>(m), b);
		}
		int32_t OccSamples(void *r, void *c, int32_t i){ return static_cast<iRenderer*>(r)->RetrieveOcclusionObjectSamples(static_cast<iRenderable*>(c), i);}
		const OhplFrustum* RendererFrustum(void *r){ return OhplLsFrustum(static_cast<iRenderer*>(r)->GetCurrentFrustum());}
		void Log(bool abError, const char *s, size_t l)
		{
			if(abError) Error("%.*s", (int)l, s);
			else Warning("%.*s", (int)l, s);
		}

		struct cXml
		{
			cXmlDocumentTiny doc;
			cXmlElement *main;
			cXml() : doc(""), main(NULL){}
		};
		void* XmlOpen(void *r, const char *s, size_t l, int32_t *apStatus)
		{
			tWString sPath = static_cast<cResources*>(r)->GetFileSearcher()->GetFilePath(tString(s, l));
			if(sPath == _W("")){ *apStatus = 1; return NULL;}
			FILE *pFile = cPlatform::OpenFile(sPath, _W("rb"));
			if(pFile == NULL){ *apStatus = 2; return NULL;}
			fclose(pFile);
			cXml *pXml = hplNew(cXml, ());
			if(pXml->doc.CreateFromFile(sPath) == false){ *apStatus = 3; hplDelete(pXml); return NULL;}
			pXml->main = pXml->doc.GetFirstElement("MAIN");
			if(pXml->main == NULL){ *apStatus = 4; hplDelete(pXml); return NULL;}
			*apStatus = 0;
			return pXml;
		}
		const char* XmlAttr(void *d, const char *s, size_t l){ return static_cast<cXml*>(d)->main->GetAttribute(tString(s, l));}
		void XmlClose(void *d){ hplDelete(static_cast<cXml*>(d));}
		void* MatCreate(void *r, const char *s, size_t l){ return static_cast<cResources*>(r)->GetMaterialManager()->CreateMaterial(tString(s, l));}

		OhplBv* EntBv(void *c){ return static_cast<iEntity3D*>(c)->GetBoundingVolume()->Rs();}
		void* FlareTypeNew(void *c, OhplSLFlare *apFlare, int32_t k){ return hplNew(cLensFlareType, (static_cast<cLensFlare*>(c), static_cast<cLensFlare*>(c)->GetName(), (eLensFlareType)k, apFlare));}
		void FlareTypeDelete(void *t){ hplDelete(static_cast<cLensFlareType*>(t));}
		bool FlareParentMesh(void *c, void **apVb, const cMatrixf **apMtx)
		{
			iEntity3D *pParent = static_cast<iEntity3D*>(c)->GetEntityParent();
			if(pParent == NULL || pParent->GetEntityType() != "SubMesh") return false;
			cSubMeshEntity *pSubMesh = static_cast<cSubMeshEntity*>(pParent);
			*apVb = pSubMesh->GetVertexBuffer();
			*apMtx = pSubMesh->GetModelMatrix(NULL);
			return true;
		}
		int32_t VbVertexNum(void *p){ return V(p)->GetVertexNum();}

		void* TexCreate(void *r, int32_t k, const char *n, size_t l)
		{
			cTextureManager *pMgr = static_cast<cResources*>(r)->GetTextureManager();
			const tString sName(n, l);
			if(k == 0) return pMgr->Create1D(sName, false);
			if(k == 1) return pMgr->Create2D(sName, true);
			return pMgr->CreateAnim(sName, true, eTextureType_2D);
		}
		void TexClamp(void *t, bool abStr)
		{
			iTexture *pTex = static_cast<iTexture*>(t);
			if(abStr) pTex->SetWrapSTR(eTextureWrap_ClampToEdge);
			else { pTex->SetWrapS(eTextureWrap_ClampToEdge); pTex->SetWrapT(eTextureWrap_ClampToEdge); }
		}
		void TexAnim(void *t, int32_t alMode, float afFrameTime)
		{
			iTexture *pTex = static_cast<iTexture*>(t);
			pTex->SetAnimMode((eTextureAnimMode)alMode);
			pTex->SetFrameTime(afFrameTime);
		}
		void* TrackerNew(){ return hplNew(cVisibleRCNodeTracker, ());}
		void TrackerDelete(void *t){ hplDelete(static_cast<cVisibleRCNodeTracker*>(t));}
		OhplFrustum* FrustumNew(){ return const_cast<OhplFrustum*>(OhplLsFrustum(hplNew(cFrustum, ())));}
		void FrustumDelete(OhplFrustum *f){ hplDelete(reinterpret_cast<cFrustum*>(f));}
		void BbVisible(void *b, bool v){ static_cast<cBillboard*>(b)->SetVisible(v);}
		void BbColor(void *b, const cColor *c){ static_cast<cBillboard*>(b)->SetColor(*c);}
		void PsVisible(void *p, bool v){ static_cast<cParticleSystem*>(p)->SetVisible(v);}
		void PsSetLight(void *p, void *l){ static_cast<cParticleSystem*>(p)->SetLight(static_cast<iLight*>(l));}
		void PsLeaveLight(void *p)
		{
			cParticleSystem *pPS = static_cast<cParticleSystem*>(p);
			if(pPS->GetLight()) pPS->GetLight()->RemoveParticleSystem(pPS);
		}
		void PsLightColor(void *p, const cColor *c){ static_cast<cParticleSystem*>(p)->SetLightColor(*c);}
		void PsLightBrightness(void *p, float f){ static_cast<cParticleSystem*>(p)->SetLightBrightness(f);}
		void VisibleChange(void *cb, void *c){ static_cast<iRenderableCallback*>(cb)->OnVisibleChange(static_cast<iRenderable*>(c));}
		float Ease(void *c, float t){ return static_cast<iLight*>(c)->Ease(t);}
		void WorldSound(void *w, void *c, bool abOn, const char *s, size_t l)
		{
			cSoundEntity *pSound = static_cast<cWorld*>(w)->CreateSoundEntity(abOn ? "FlickerOn" : "FlickerOff", tString(s, l), true);
			if(pSound)
			{
				pSound->SetIsSaved(false);
				pSound->SetPosition(static_cast<iLight*>(c)->GetWorldPosition());
			}
		}
		void WorldParticle(void *w, void *c, const char *s, size_t l)
		{
			iLight *pLight = static_cast<iLight*>(c);
			cParticleSystem *pPS = static_cast<cWorld*>(w)->CreateParticleSystem(pLight->GetName() + "_PS", tString(s, l), cVector3f(1,1,1));
			if(pPS) pPS->SetMatrix(pLight->GetWorldMatrix());
		}
		void RenderableInfo(void *o, int32_t *apCount, bool *apStatic)
		{
			iRenderable *pObj = static_cast<iRenderable*>(o);
			*apCount = pObj->GetTransformUpdateCount();
			*apStatic = pObj->IsStatic();
		}

		const char* ObjName(int32_t k, void *o, size_t *apLen)
		{
			const tString *psName = NULL;
			if(k == 0) psName = &static_cast<iTexture*>(o)->GetName();
			else if(k == 1) psName = &static_cast<cMaterial*>(o)->GetName();
			else { static tString sWorld; sWorld = static_cast<cWorld*>(o)->GetName(); psName = &sWorld; }
			*apLen = psName->size();
			return psName->data();
		}
		template<class T> void FillInfo(T *p, OhplLsDumpInfo *apOut)
		{
			apOut->name = p->GetName().data();
			apOut->nameLen = p->GetName().size();
			apOut->uid = p->GetUniqueID();
			apOut->visible = p->IsVisible();
			apOut->pod = (uintptr_t)p->Pod();
		}
		void DumpInfo(int32_t k, void *o, OhplLsDumpInfo *apOut)
		{
			switch(k)
			{
			case 0: FillInfo(static_cast<iLight*>(o), apOut); break;
			case 1: FillInfo(static_cast<cBillboard*>(o), apOut); break;
			case 2: FillInfo(static_cast<cBeam*>(o), apOut); break;
			case 3: FillInfo(static_cast<cFogArea*>(o), apOut); break;
			default: FillInfo(static_cast<cLensFlare*>(o), apOut); break;
			}
		}
		OhplBv* ObjBv(int32_t k, void *o){ return static_cast<iRenderable*>(o)->GetBoundingVolume()->Rs();}
		template<class It, class T> size_t Collect(It it, uintptr_t *apOut, size_t alCap)
		{
			size_t lNum = 0;
			while(it.HasNext() && lNum < alCap) apOut[lNum++] = (uintptr_t)static_cast<T*>(it.Next());
			return lNum;
		}
		size_t DumpList(void *w, int32_t k, uintptr_t *apOut, size_t alCap)
		{
			cWorld *pWorld = static_cast<cWorld*>(w);
			switch(k)
			{
			case 0: return Collect<cLightListIterator, iLight>(pWorld->GetLightIterator(), apOut, alCap);
			case 1: return Collect<cBillboardIterator, cBillboard>(pWorld->GetBillboardIterator(), apOut, alCap);
			case 2: return Collect<cBeamIterator, cBeam>(pWorld->GetBeamIterator(), apOut, alCap);
			case 3: return Collect<cFogAreaIterator, cFogArea>(pWorld->GetFogAreaIterator(), apOut, alCap);
			default: return Collect<cLensFlareIterator, cLensFlare>(pWorld->GetLensFlareIterator(), apOut, alCap);
			}
		}

		struct cRegister
		{
			cRegister()
			{
				static const OhplLsHost h = { VbCreate, VbElem, VbAdd, VbIndex, VbCompile, VbArray, VbUpdate, VbCount, VbFree,
											  MatDestroy, RopeInfo, RopePoints, EntWorld, EntUpdated,
											  TexDestroy, ProgCreate, ProgDestroy, ProgVar, ProgSet, EntClip, SpotInfo, LightLevel,
											  Hdr, EntPos, OccAssign, OccSamples, RendererFrustum, Log, XmlOpen, XmlAttr, XmlClose, MatCreate,
											  EntBv, FlareTypeNew, FlareTypeDelete, FlareParentMesh, VbVertexNum,
											  TexCreate, TexClamp, TexAnim, TrackerNew, TrackerDelete, FrustumNew, FrustumDelete, BbVisible, BbColor,
											  PsVisible, PsSetLight, PsLeaveLight, PsLightColor, PsLightBrightness, VisibleChange, Ease, WorldSound,
											  WorldParticle, RenderableInfo, ObjName, DumpInfo, ObjBv, DumpList };
				ohpl_ls_set_host(&h);
			}
		} gRegister;
	}

	cRopeEntity::~cRopeEntity(){ ohpl_ls_rope_free(mpL);}
	cEnvironmentParticles::~cEnvironmentParticles(){ ohpl_ls_env_free(mpL);}
	cBillboard::~cBillboard(){ ohpl_ls_bb_free(mpS);}
	iLight::~iLight(){ ohpl_ls_light_free(mpS);}
	cLightPoint::~cLightPoint(){}
	cLightSpot::~cLightSpot(){}
	cLightBox::~cLightBox(){}
	cLensFlareType::~cLensFlareType(){ ohpl_ls_ft_free(mpS);}
	void cBeamEnd_UpdateCallback::OnTransformUpdate(iEntity3D *apEntity)
	{
		((cBeam*)static_cast<cBeamEnd*>(apEntity)->mpS->beam)->SetTransformUpdated(true);
	}
	cLensFlare::~cLensFlare(){ ohpl_ls_flare_free(mpS);}
	cBeam::~cBeam(){ hplDelete(GetEnd()); ohpl_ls_beam_free(mpS);}
}
