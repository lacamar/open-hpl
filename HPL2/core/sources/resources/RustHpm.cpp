
#include "resources/RustHpm.h"

#include <vector>

#include "resources/XmlDocument.h"

namespace hpl {

	static_assert(sizeof(wchar_t) == 4, "the Rust parser takes 32-bit wide paths");

	namespace
	{
		iHpmBuilder& B(void* apCtx) { return *static_cast<iHpmBuilder*>(apCtx); }

		struct cElCtx
		{
			cXmlElement* mpRoot;
			std::vector<cXmlElement*> mvStack;
		};

		void ElBegin(void* apCtx, const char* asName, size_t alLen)
		{
			cElCtx* pCtx = static_cast<cElCtx*>(apCtx);
			cXmlElement* pElem = pCtx->mvStack.empty() ? pCtx->mpRoot : pCtx->mvStack.back()->CreateChildElement();
			pElem->SetValue(tString(asName, alLen));
			pCtx->mvStack.push_back(pElem);
		}

		void ElAttr(void* apCtx, const char* asName, size_t alNameLen, const char* asVal, size_t alValLen)
		{
			static_cast<cElCtx*>(apCtx)->mvStack.back()->SetAttribute(tString(asName, alNameLen), tString(asVal, alValLen).c_str());
		}

		void ElEnd(void* apCtx)
		{
			static_cast<cElCtx*>(apCtx)->mvStack.pop_back();
		}

		// A Rust element as a cXmlElement tree for the duration of one builder call.
		class cTempElement
		{
		public:
			cTempElement(const OhplHpmEl* apEl) : mRoot("", NULL), mbValid(apEl != NULL)
			{
				if (apEl == NULL) return;
				cElCtx ctx = {&mRoot, std::vector<cXmlElement*>()};
				OhplHpmTreeSink sink = {&ctx, ElBegin, ElAttr, ElEnd};
				ohpl_hpm_el_walk(apEl, &sink);
			}
			cXmlElement* Get() { return mbValid ? &mRoot : NULL; }

		private:
			cXmlElement mRoot;
			bool mbValid;
		};

		void Collect(void* apCtx, const char* apData, size_t alLen)
		{
			static_cast<std::vector<char>*>(apCtx)->assign(apData, apData + alLen);
		}

		uint64_t NowMs(void* c) { return B(c).NowMs(); }
		void ObjectDone(void* c) { B(c).ObjectDone(); }
		void Warn(void* c, const char* s) { B(c).Warn(s); }
		void LogMsg(void* c, const char* s) { B(c).Log(s); }

		int ReadFile(void* c, const wchar_t* apPath, size_t alLen, void (*apSink)(void*, const char*, size_t), void* apSinkCtx)
		{
			std::vector<char> vData;
			int lStatus = B(c).ReadFile(tWString(apPath, alLen), vData);
			apSink(apSinkCtx, vData.data(), vData.size());
			return lStatus;
		}

		void GlobalFog(void* c, const OhplHpmFog* p) { B(c).GlobalFog(*p); }
		void DistanceCull(void* c, const OhplHpmDistanceCull* p) { B(c).DistanceCull(*p); }
		void DirLight(void* c, const OhplHpmDirLight* p) { B(c).DirLight(*p); }
		void EnvParticles(void* c, const OhplHpmEnvParticles* p) { B(c).EnvParticles(*p); }
		void PostEffects(void* c, const OhplHpmPostEffects* p) { B(c).PostEffects(*p); }
		void SkyBox(void* c, const OhplHpmSkyBox* p) { B(c).SkyBox(*p); }
		bool StaticObject(void* c, const OhplHpmStaticObject* p) { return B(c).StaticObject(*p); }
		bool Plane(void* c, const OhplHpmPlane* p) { return B(c).Plane(*p); }
		bool Entity(void* c, const OhplHpmEntity* p, const OhplHpmEl* e) { cTempElement el(e); return B(c).Entity(*p, el.Get()); }
		bool Light(void* c, const OhplHpmEl* e) { cTempElement el(e); return B(c).Light(el.Get()); }
		bool Area(void* c, const OhplHpmArea* p, const OhplHpmEl* e) { cTempElement el(e); return B(c).Area(*p, el.Get()); }
		bool Sound(void* c, const OhplHpmEl* e, bool abActive) { cTempElement el(e); return B(c).Sound(el.Get(), abActive); }
		bool Decal(void* c, const OhplHpmDecal* p, const OhplHpmEl* e) { cTempElement el(e); return B(c).Decal(*p, el.Get()); }
		bool Billboard(void* c, const OhplHpmEl* e) { cTempElement el(e); return B(c).Billboard(el.Get()); }
		bool ParticleSystem(void* c, const OhplHpmEl* e, const char* asLight) { cTempElement el(e); return B(c).ParticleSystem(el.Get(), asLight); }
		bool FogArea(void* c, const OhplHpmEl* e) { cTempElement el(e); return B(c).FogArea(el.Get()); }
		bool LensFlare(void* c, const OhplHpmEl* e) { cTempElement el(e); return B(c).LensFlare(el.Get()); }
		void LightMaskBox(void* c, const OhplHpmLightMask* p) { B(c).LightMaskBox(*p); }
		void ConnectLights(void* c, int* apBillboards, int* apParticles) { B(c).ConnectLights(apBillboards, apParticles); }
		void ConnectLightMasks(void* c) { B(c).ConnectLightMasks(); }
		int DetailMeshes(void* c, const OhplHpmDetailMeshes* p) { return B(c).DetailMeshes(*p); }
		void ExposureArea(void* c, const OhplHpmExposureArea* p) { B(c).ExposureArea(*p); }
		void Terrain(void* c, const OhplHpmTerrain* p) { B(c).Terrain(*p); }

		void TrackReport(void* c, const char* asTrack, int alInXml, int alCreated, int alTimeMs, bool abFileMissing,
						 const OhplHpmSkip* apSkipped, size_t alNumSkipped)
		{
			B(c).TrackReport(asTrack, alInXml, alCreated, alTimeMs, abFileMissing, apSkipped, alNumSkipped);
		}
	}

	void ParseHpmMap(iHpmBuilder& aBuilder, const tWString& asFile)
	{
		OhplHpmVt vt = {&aBuilder,	  NowMs,		   ObjectDone,	  Warn,			 LogMsg,	  ReadFile,		  GlobalFog,   DistanceCull,
						DirLight,	  EnvParticles,	   PostEffects,	  SkyBox,		 StaticObject, Plane,		  Entity,	   Light,
						Area,		  Sound,		   Decal,		  Billboard,	 ParticleSystem, FogArea,		  LensFlare,   LightMaskBox,
						ConnectLights, ConnectLightMasks, DetailMeshes, ExposureArea, Terrain,	  TrackReport};
		ohpl_hpm_parse(&vt, asFile.data(), asFile.size());
	}

};
