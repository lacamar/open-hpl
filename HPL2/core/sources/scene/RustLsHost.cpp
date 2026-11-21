// Engine objects behind the Rust-owned scene shells (rust/src/scene/ls_host.rs) and the out-of-line
// destructors that anchor their vtables. Each thunk is deleted with the C++ class it wraps.

#include "scene/RustLs.h"
#include "scene/RopeEntity.h"
#include "scene/BillBoard.h"
#include "scene/Beam.h"
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
		void OccAssign(void *r, void *c, int32_t i, const cMatrixf *m, bool b)
		{
			iRenderer *pR = static_cast<iRenderer*>(r);
			pR->AssignOcclusionObject(static_cast<iRenderable*>(c), i, pR->GetShapeBoxVertexBuffer(), const_cast<cMatrixf*>(m), b);
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

		struct cRegister
		{
			cRegister()
			{
				static const OhplLsHost h = { VbCreate, VbElem, VbAdd, VbIndex, VbCompile, VbArray, VbUpdate, VbCount, VbFree,
											  MatDestroy, RopeInfo, RopePoints, EntWorld, EntUpdated,
											  TexDestroy, ProgCreate, ProgDestroy, ProgVar, ProgSet, EntClip, SpotInfo, LightLevel,
											  Hdr, EntPos, OccAssign, OccSamples, RendererFrustum, Log, XmlOpen, XmlAttr, XmlClose, MatCreate };
				ohpl_ls_set_host(&h);
			}
		} gRegister;
	}

	cRopeEntity::~cRopeEntity(){ ohpl_ls_rope_free(mpL);}
	cEnvironmentParticles::~cEnvironmentParticles(){ ohpl_ls_env_free(mpL);}
	cBillboard::~cBillboard(){ ohpl_ls_bb_free(mpS);}
	cBeam::~cBeam(){ hplDelete(GetEnd()); ohpl_ls_beam_free(mpS);}
}
