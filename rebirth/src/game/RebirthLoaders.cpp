
#include "RebirthLoaders.h"

cRebirthGenericEntityLoader::cRebirthGenericEntityLoader(const tString &asName) : cEntityLoader_Object(asName)
{
	mbLoadAsStatic = true;
	mbCreatesStaticEntity = true;
}

void cRebirthGenericEntityLoader::BeforeLoad(cXmlElement *apRootElem, const cMatrixf &a_mtxTransform, cWorld *apWorld, cResourceVarsObject *apInstanceVars)
{
}

void cRebirthGenericEntityLoader::AfterLoad(cXmlElement *apRootElem, const cMatrixf &a_mtxTransform, cWorld *apWorld, cResourceVarsObject *apInstanceVars)
{
	if (apInstanceVars && mpEntity)
	{
		mpEntity->SetRenderFlagBit(eRenderableFlag_ShadowCaster, apInstanceVars->GetVarBool("CastShadows", true));
	}
}

cRebirthAreaLoader_PlayerStart::cRebirthAreaLoader_PlayerStart(const tString &asName) : iAreaLoader(asName)
{
	mbCreatesStaticArea = true;
}

void cRebirthAreaLoader_PlayerStart::Load(const tString &asName, int alID, bool abActive, const cVector3f &avSize, const cMatrixf &a_mtxTransform, cWorld *apWorld)
{
	cStartPosEntity *pStartPos = apWorld->CreateStartPos(asName);
	pStartPos->SetMatrix(a_mtxTransform);
}

cRebirthAreaLoader_Noop::cRebirthAreaLoader_Noop(const tString &asName) : iAreaLoader(asName)
{
	mbCreatesStaticArea = true;
}

void cRebirthAreaLoader_Noop::Load(const tString &asName, int alID, bool abActive, const cVector3f &avSize, const cMatrixf &a_mtxTransform, cWorld *apWorld)
{
}

void RegisterRebirthLoaders(cResources *apResources)
{
	static const char* apEntityTypeNames[] = {
		"StaticProp",
		"Prop_Rigid",
		"Prop_Grab",
		"Prop_SwingDoor",
		"Prop_Readable",
		"Prop_SketchbookReadable",
	};
	for (size_t i = 0; i < sizeof(apEntityTypeNames) / sizeof(apEntityTypeNames[0]); ++i)
	{
		apResources->AddEntityLoader(hplNew(cRebirthGenericEntityLoader, (apEntityTypeNames[i])));
	}

	apResources->AddAreaLoader(hplNew(cRebirthAreaLoader_PlayerStart, ("PlayerStart")));
	apResources->AddAreaLoader(hplNew(cRebirthAreaLoader_Noop, ("Trigger")));
	apResources->AddAreaLoader(hplNew(cRebirthAreaLoader_Noop, ("Soundscape")));
}
