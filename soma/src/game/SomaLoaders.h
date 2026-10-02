#ifndef SOMA_LOADERS_H
#define SOMA_LOADERS_H

#include "hpl.h"

using namespace hpl;

class cSomaGenericEntityLoader : public cEntityLoader_Object
{
public:
	cSomaGenericEntityLoader(const tString &asName);
	virtual ~cSomaGenericEntityLoader(){}

protected:
	void BeforeLoad(cXmlElement *apRootElem, const cMatrixf &a_mtxTransform, cWorld *apWorld, cResourceVarsObject *apInstanceVars);
	void AfterLoad(cXmlElement *apRootElem, const cMatrixf &a_mtxTransform, cWorld *apWorld, cResourceVarsObject *apInstanceVars);
	void LoadPose();
};

class cSomaAreaLoader_PlayerStart : public iAreaLoader
{
public:
	cSomaAreaLoader_PlayerStart(const tString &asName);
	virtual ~cSomaAreaLoader_PlayerStart(){}

	void Load(const tString &asName, int alID, bool abActive, const cVector3f &avSize, const cMatrixf &a_mtxTransform, cWorld *apWorld);
};

class cSomaAreaLoader_Noop : public iAreaLoader
{
public:
	cSomaAreaLoader_Noop(const tString &asName);
	virtual ~cSomaAreaLoader_Noop(){}

	void Load(const tString &asName, int alID, bool abActive, const cVector3f &avSize, const cMatrixf &a_mtxTransform, cWorld *apWorld);
};

void RegisterSomaLoaders(cResources *apResources);

#endif // SOMA_LOADERS_H
