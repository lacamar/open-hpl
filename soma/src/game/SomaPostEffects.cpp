#include "SomaPostEffects.h"
#include "RustSomaS8b.h"
#include "SomaBase.h"
#include "SomaS8Host.h"
#include "SomaToneMapping.h"

#include <cstring>

namespace
{
	cSomaPostEffect *gpCur = NULL;
	cPostEffectComposite *gpViewportComposite = NULL;
	bool gbRnd = false; // the game's scripts draw every video distortion parameter at random
	tString gsOut;

	template <class T> T *Ptr(uint64_t a) { return (T *)a; }
	cGraphics *Graphics() { return gpSomaBase->mpEngine->GetGraphics(); }
	cResources *Resources() { return gpSomaBase->mpEngine->GetResources(); }
	tString Str(const OhplS8bIo *io) { return tString((const char *)io->s, io->n); }

	std::string Hx(float afX) { return OhplS8bHex(afX); }
	float FrameTime(cPostEffectComposite *c) { return OhplS8cDump() ? 1.0f / 60.0f : c->GetCurrentFrameTime(); }

	void UF(iGpuProgram *p, const char *n, float x)
	{
		OhplS8cAdd("%s=%s", n, gbRnd ? "rnd" : Hx(x).c_str());
		p->SetFloat(p->GetVariableId(n), x);
	}
	void U2(iGpuProgram *p, const char *n, float x, float y)
	{
		if (gbRnd && strcmp(n, "avScreenSize"))
			OhplS8cAdd("%s=rnd", n);
		else
			OhplS8cAdd("%s=%s,%s", n, Hx(x).c_str(), Hx(y).c_str());
		p->SetVec2f(p->GetVariableId(n), x, y);
	}
	void U3(iGpuProgram *p, const char *n, float x, float y, float z)
	{
		OhplS8cAdd("%s=%s,%s,%s", n, Hx(x).c_str(), Hx(y).c_str(), Hx(z).c_str());
		p->SetVec3f(p->GetVariableId(n), x, y, z);
	}
	void U4(iGpuProgram *p, const char *n, float x, float y, float z, float w)
	{
		OhplS8cAdd("%s=%s,%s,%s,%s", n, Hx(x).c_str(), Hx(y).c_str(), Hx(z).c_str(), Hx(w).c_str());
		p->SetVec4f(p->GetVariableId(n), x, y, z, w);
	}
	void UArr(iGpuProgram *p, const char *n, const float *v, int lN)
	{
		std::string s;
		for (int i = 0; i < lN; ++i) s += Hx(v[i]) + ",";
		OhplS8cAdd("%s=%s", n, s.c_str());
		p->SetFloatArray(p->GetVariableId(n), v, lN);
	}
	void UMat(iGpuProgram *p, const char *n, const cMatrixf &m)
	{
		std::string s;
		for (int i = 0; i < 16; ++i) s += Hx(m.v[i]) + ",";
		OhplS8cAdd("%s=%s", n, s.c_str());
		p->SetMatrixf(p->GetVariableId(n), m);
	}
	void CTex(cPostEffectComposite *c, int i, iTexture *t)
	{
		OhplS8cAdd("tex%d=%s:%dx%d", i, t ? t->GetName().c_str() : "-", t ? t->GetWidth() : 0, t ? t->GetHeight() : 0);
		c->SetTexture(i, t);
	}
	void CProg(cPostEffectComposite *c, iGpuProgram *p)
	{
		OhplS8cAdd("prog=%s", p ? p->GetName().c_str() : "-");
		c->SetProgram(p);
	}

	void Text(OhplS8bIo *io, const tString &asText)
	{
		gsOut = asText;
		io->s = (const uint8_t *)gsOut.data();
		io->n = gsOut.size();
	}

	std::vector<tString> Lines(const tString &asText, char acSep)
	{
		std::vector<tString> v;
		size_t lPos = 0;
		for (size_t lEnd; (lEnd = asText.find(acSep, lPos)) != tString::npos; lPos = lEnd + 1)
			v.push_back(asText.substr(lPos, lEnd - lPos));
		v.push_back(asText.substr(lPos));
		return v;
	}

	void Target(OhplS8bIo *io)
	{
		tString sName = Str(io);
		iTexture *pTex = Graphics()->CreateTexture(sName, eTextureType_2D, eTextureUsage_RenderTarget);
		pTex->CreateFromRawData(cVector3l(io->i[0], io->i[1], 0), ePixelFormat_RGBA, NULL);
		pTex->SetWrapSTR(eTextureWrap_ClampToEdge);
		pTex->SetFilter(eTextureFilter_Bilinear);
		iFrameBuffer *pFb = Graphics()->CreateFrameBuffer(sName);
		pFb->SetTexture2D(0, pTex);
		pFb->CompileAndValidate();
		io->r = (uint64_t)pTex;
		io->p[0] = (uint64_t)pFb;
	}

	void TentacleDef(OhplS8bIo *io)
	{
		cResources *pRes = Resources();
		iXmlDocument *pDoc = pRes->LoadXmlDocument(Str(io));
		cXmlElement *pModel = pDoc ? pDoc->GetFirstElement("ModelData") : NULL;
		cXmlElement *pMeshElem = pModel ? pModel->GetFirstElement("Mesh") : NULL;
		cXmlElement *pAnims = pModel ? pModel->GetFirstElement("Animations") : NULL;
		tString sOut = (pMeshElem ? "1\t" + pMeshElem->GetAttributeString("Filename", "") : tString("0\t")) + "\n";
		if (pAnims)
		{
			cXmlNodeListIterator it = pAnims->GetChildIterator();
			while (it.HasNext())
			{
				cXmlElement *pAnimElem = it.Next()->ToElement();
				sOut += pAnimElem->GetAttributeString("Name", "") + "\t" + pAnimElem->GetAttributeString("File", "") + "\t" +
						Hx(pAnimElem->GetAttributeFloat("Speed", 1)) + "\n";
			}
		}
		if (pDoc)
			pRes->DestroyXmlDocument(pDoc);
		Text(io, sOut);
	}

	void Entity(uint32_t aOp, OhplS8bIo *io)
	{
		cResources *pRes = Resources();
		cMeshEntity *pEnt = Ptr<cMeshEntity>(io->p[0]);
		switch (aOp)
		{
		case kS8c_EntNew:
		{
			cMesh *pMesh = pRes->GetMeshManager()->CreateMesh(Str(io));
			io->r = pMesh ? (uint64_t)hplNew(cMeshEntity, ("FearTentacle", pMesh, pRes->GetMaterialManager(), pRes->GetMeshManager(),
														   pRes->GetAnimationManager()))
						  : 0;
			break;
		}
		case kS8c_EntAnim:
		{
			std::vector<tString> v = Lines(Str(io), '\x01');
			if (cAnimation *pAnim = pRes->GetAnimationManager()->CreateAnimation(v[0]))
				pEnt->AddAnimation(pAnim, v[1], io->f[0]);
			break;
		}
		case kS8c_EntPlay: pEnt->Play(io->i[0], io->i[1] != 0, io->i[2] != 0); break;
		case kS8c_EntAnimIndex: io->i[0] = pEnt->GetAnimationStateIndex(Str(io)); break;
		case kS8c_EntBaseSpeed: pEnt->GetAnimationState(io->i[0])->SetBaseSpeed(io->f[0]); break;
		case kS8c_EntState:
		{
			cAnimationState *pState = pEnt->GetAnimationState(io->i[0]);
			pState->SetSpeed(io->f[0]);
			pState->SetWeight(io->f[1]);
			break;
		}
		case kS8c_EntUpdate:
			pEnt->UpdateLogic(io->f[0]);
			pEnt->GetSubMeshEntity(0)->UpdateGraphicsForFrame(io->f[0]);
			break;
		case kS8c_EntVb: io->r = (uint64_t)pEnt->GetSubMeshEntity(0)->GetVertexBuffer(); break;
		case kS8c_EntDelete: hplDelete(pEnt); break;
		}
	}

	void Shaders(uint32_t aOp, OhplS8bIo *io)
	{
		switch (aOp)
		{
		case kS8c_ProgramFromShaders:
		{
			std::vector<tString> v = Lines(Str(io), '\n');
			cParserVarContainer vars;
			for (size_t i = 3; i < v.size(); ++i)
			{
				size_t lEq = v[i].find('=');
				if (lEq == tString::npos)
					vars.Add(v[i]);
				else
					vars.Add(v[i].substr(0, lEq), atoi(v[i].c_str() + lEq + 1));
			}
			io->r = (uint64_t)Graphics()->CreateGpuProgramFromShaders(v[0], v[1], v[2], &vars);
			break;
		}
		case kS8c_ShaderNew:
		{
			std::vector<tString> v = Lines(Str(io), '\x01');
			iGpuShader *pShader = Graphics()->GetLowLevel()->CreateGpuShader(v[0], io->i[0] ? eGpuShaderType_Fragment : eGpuShaderType_Vertex);
			io->r = pShader->CreateFromString(v[1].c_str()) ? (uint64_t)pShader : 0;
			break;
		}
		case kS8c_ShaderSampler: Ptr<iGpuShader>(io->p[0])->AddSamplerUnit(Str(io), io->i[0]); break;
		case kS8c_ProgramNew: io->r = (uint64_t)Graphics()->CreateGpuProgram(Str(io)); break;
		case kS8c_ProgramShader:
			Ptr<iGpuProgram>(io->p[0])->SetShader(io->i[0] ? eGpuShaderType_Fragment : eGpuShaderType_Vertex, Ptr<iGpuShader>(io->p[1]));
			break;
		case kS8c_ProgramLink: io->r = Ptr<iGpuProgram>(io->p[0])->Link(); break;
		}
	}
}

cSomaPostEffect::cSomaPostEffect() : iPostEffect(gpSomaBase->mpEngine->GetGraphics(), gpSomaBase->mpEngine->GetResources(), NULL) {}

void cSomaPostEffect::Reset() { ohpl_s8c_reset((size_t)this); }

iTexture *cSomaPostEffect::RenderEffect(iTexture *apIn, iFrameBuffer *apOut)
{
	gpCur = this;
	return Ptr<iTexture>(ohpl_s8c_render((size_t)this, (size_t)apIn, (size_t)apOut));
}

void OhplS8cCall(uint32_t aOp, OhplS8bIo *io)
{
	cPostEffectComposite *c = gpCur ? gpCur->Composite() : NULL;
	cPostEffectComposite *pComp = Ptr<cPostEffectComposite>(io->p[0]);
	iLowLevelGraphics *pLow = Graphics()->GetLowLevel();
	switch (aOp)
	{
	case kS8c_NewShell: io->r = (uint64_t) new cSomaPostEffect(); break;
	case kS8c_DeleteShell: delete Ptr<cSomaPostEffect>(io->p[0]); break;
	case kS8c_IsActive: io->r = Ptr<cSomaPostEffect>(io->p[0])->IsActive(); break;
	case kS8c_SetActive: Ptr<cSomaPostEffect>(io->p[0])->SetActive(io->i[0] != 0); break;
	case kS8c_SetDisabled: Ptr<cSomaPostEffect>(io->p[0])->SetDisabled(io->i[0] != 0); break;
	case kS8c_IsDisabled: io->r = Ptr<cSomaPostEffect>(io->p[0])->IsDisabled(); break;
	case kS8c_CompAdd:
		pComp->RemovePostEffect(Ptr<cSomaPostEffect>(io->p[1]));
		pComp->AddPostEffect(Ptr<cSomaPostEffect>(io->p[1]), -io->i[0]);
		break;
	case kS8c_CompRemove: pComp->RemovePostEffect(Ptr<cSomaPostEffect>(io->p[1])); break;
	case kS8c_CompNum: io->r = pComp->GetPostEffectNum(); break;
	case kS8c_CompGet: io->r = (uint64_t)(void *)pComp->GetPostEffect(io->i[0]); break;
	case kS8c_ViewportComp: io->r = (uint64_t)cSomaPostEffects::GetViewportComposite(); break;
	case kS8c_FrameTime: io->f[0] = FrameTime(c); break;
	case kS8c_TrailAlpha: cRendererDeferred::SetImageTrailAlpha(io->f[0]); break;
	case kS8c_ProgramFromShaders:
	case kS8c_ShaderNew:
	case kS8c_ShaderSampler:
	case kS8c_ProgramNew:
	case kS8c_ProgramShader:
	case kS8c_ProgramLink: Shaders(aOp, io); break;
	case kS8c_UniF: UF(Ptr<iGpuProgram>(io->p[0]), Str(io).c_str(), io->f[0]); break;
	case kS8c_UniV2: U2(Ptr<iGpuProgram>(io->p[0]), Str(io).c_str(), io->f[0], io->f[1]); break;
	case kS8c_UniV3: U3(Ptr<iGpuProgram>(io->p[0]), Str(io).c_str(), io->f[0], io->f[1], io->f[2]); break;
	case kS8c_UniV4: U4(Ptr<iGpuProgram>(io->p[0]), Str(io).c_str(), io->f[0], io->f[1], io->f[2], io->f[3]); break;
	case kS8c_UniArr: UArr(Ptr<iGpuProgram>(io->p[0]), Str(io).c_str(), io->f, io->i[0]); break;
	case kS8c_UniMat:
	{
		cMatrixf mtx;
		memcpy(mtx.v, io->f, sizeof(float) * 16);
		UMat(Ptr<iGpuProgram>(io->p[0]), Str(io).c_str(), mtx);
		break;
	}
	case kS8c_SetFlat:
		OhplS8cAdd("flat");
		c->SetFlatProjection();
		break;
	case kS8c_SetBlend:
	{
		eMaterialBlendMode mode = io->i[0] ? eMaterialBlendMode_MulX2 : eMaterialBlendMode_None;
		OhplS8cAdd("blend=%d", (int)mode);
		c->SetBlendMode(mode);
		break;
	}
	case kS8c_SetChannel:
		OhplS8cAdd("chan=%d", (int)eMaterialChannelMode_RGBA);
		c->SetChannelMode(eMaterialChannelMode_RGBA);
		break;
	case kS8c_SetRange:
		OhplS8cAdd("range=%d", io->i[0]);
		c->SetTextureRange(NULL, io->i[0]);
		break;
	case kS8c_SetTexture: CTex(c, io->i[0], Ptr<iTexture>(io->p[0])); break;
	case kS8c_SetProgram: CProg(c, Ptr<iGpuProgram>(io->p[0])); break;
	case kS8c_SetFinalFb:
		OhplS8cAdd("final");
		gpCur->Final(Ptr<iFrameBuffer>(io->p[0]));
		break;
	case kS8c_SetFb:
		OhplS8cAdd("fb=%dx%d", Ptr<iTexture>(io->p[1])->GetWidth(), Ptr<iTexture>(io->p[1])->GetHeight());
		gpCur->Buffer(Ptr<iFrameBuffer>(io->p[0]));
		break;
	case kS8c_DrawQuad:
		OhplS8cAdd("quad");
		gpCur->Quad(Ptr<iTexture>(io->p[0]), io->i[0] != 0);
		break;
	case kS8c_FbResult: io->r = (uint64_t)Ptr<iFrameBuffer>(io->p[0])->GetColorBuffer(0)->ToTexture(); break;
	case kS8c_TexSize:
	{
		iTexture *pTex = Ptr<iTexture>(io->p[0]);
		io->i[0] = pTex->GetWidth();
		io->i[1] = pTex->GetHeight();
		cVector2f vSize = pTex->GetSizeFloat2D();
		io->f[0] = vSize.x;
		io->f[1] = vSize.y;
		break;
	}
	case kS8c_TexBilinear: Ptr<iTexture>(io->p[0])->SetFilter(eTextureFilter_Bilinear); break;
	case kS8c_ScreenSize:
	{
		cVector2f vF = pLow->GetScreenSizeFloat();
		cVector2l vI = pLow->GetScreenSizeInt();
		io->f[0] = vF.x;
		io->f[1] = vF.y;
		io->i[0] = vI.x;
		io->i[1] = vI.y;
		break;
	}
	case kS8c_CreateTarget: Target(io); break;
	case kS8c_DestroyTex: Graphics()->DestroyTexture(Ptr<iTexture>(io->p[0])); break;
	case kS8c_DestroyFb: Graphics()->DestroyFrameBuffer(Ptr<iFrameBuffer>(io->p[0])); break;
	case kS8c_CompSetFb: c->SetFrameBuffer(Ptr<iFrameBuffer>(io->p[0]), false, false); break;
	case kS8c_ClearWhite:
		pLow->SetClearColor(cColor(1, 1));
		c->ClearFrameBuffer(eClearFrameBufferFlag_Color, false);
		pLow->SetClearColor(cColor(0, 0));
		break;
	case kS8c_SetDepthTest: c->SetDepthTest(io->i[0] != 0); break;
	case kS8c_SetDepthWrite: c->SetDepthWrite(io->i[0] != 0); break;
	case kS8c_SetCull: c->SetCullActive(io->i[0] != 0); break;
	case kS8c_SetMatrixNull: c->SetMatrix(NULL); break;
	case kS8c_SetVb: c->SetVertexBuffer(Ptr<iVertexBuffer>(io->p[0])); break;
	case kS8c_DrawCurrent:
		OhplS8cAdd("draw");
		c->DrawCurrent();
		break;
	case kS8c_MatCreate: io->r = (uint64_t)Resources()->GetMaterialManager()->CreateMaterial(Str(io)); break;
	case kS8c_MatDestroy: Resources()->GetMaterialManager()->Destroy(Ptr<cMaterial>(io->p[0])); break;
	case kS8c_MatTex:
		io->r = (uint64_t)Ptr<cMaterial>(io->p[0])->GetTexture(io->i[0] ? eMaterialTexture_NMap : eMaterialTexture_Diffuse);
		break;
	case kS8c_XmlTentacle: TentacleDef(io); break;
	case kS8c_EntNew:
	case kS8c_EntAnim:
	case kS8c_EntPlay:
	case kS8c_EntAnimIndex:
	case kS8c_EntBaseSpeed:
	case kS8c_EntState:
	case kS8c_EntUpdate:
	case kS8c_EntVb:
	case kS8c_EntDelete: Entity(aOp, io); break;
	case kS8c_DumpOn: io->r = OhplS8cDump(); break;
	case kS8c_Log:
		gbRnd = io->i[0] != 0;
		OhplS8cAdd("%s", Str(io).c_str());
		break;
	case kS8c_LogEnd:
		gbRnd = false;
		OhplS8cEnd();
		break;
	}
}

cPostEffectComposite *cSomaPostEffects::GetViewportComposite()
{
	if (gpViewportComposite == NULL)
		gpViewportComposite = Graphics()->CreatePostEffectComposite();
	return gpViewportComposite;
}

void cSomaPostEffects::RegisterNatives(asIScriptEngine *e)
{
	cSomaToneMapping::InstallHost();
	ohpl_s8c_register(e);
}
