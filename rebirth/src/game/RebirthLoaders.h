
#ifndef REBIRTH_LOADERS_H
#define REBIRTH_LOADERS_H

#include "hpl.h"

using namespace hpl;

class cRebirthGenericEntityLoader : public cEntityLoader_Object
{
public:
	cRebirthGenericEntityLoader(const tString &asName);
	virtual ~cRebirthGenericEntityLoader(){}

protected:
	void BeforeLoad(cXmlElement *apRootElem, const cMatrixf &a_mtxTransform, cWorld *apWorld, cResourceVarsObject *apInstanceVars);
	void AfterLoad(cXmlElement *apRootElem, const cMatrixf &a_mtxTransform, cWorld *apWorld, cResourceVarsObject *apInstanceVars);
};

class cRebirthAreaLoader_PlayerStart : public iAreaLoader
{
public:
	cRebirthAreaLoader_PlayerStart(const tString &asName);
	virtual ~cRebirthAreaLoader_PlayerStart(){}

	void Load(const tString &asName, int alID, bool abActive, const cVector3f &avSize, const cMatrixf &a_mtxTransform, cWorld *apWorld);
};

class cRebirthAreaLoader_Noop : public iAreaLoader
{
public:
	cRebirthAreaLoader_Noop(const tString &asName);
	virtual ~cRebirthAreaLoader_Noop(){}

	void Load(const tString &asName, int alID, bool abActive, const cVector3f &avSize, const cMatrixf &a_mtxTransform, cWorld *apWorld);
};

void RegisterRebirthLoaders(cResources *apResources);

#endif // REBIRTH_LOADERS_H
