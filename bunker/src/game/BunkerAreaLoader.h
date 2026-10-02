
#ifndef BUNKER_AREA_LOADER_H
#define BUNKER_AREA_LOADER_H

#include "hpl.h"

using namespace hpl;

class cBunkerAreaLoader_PlayerStart : public iAreaLoader
{
public:
	cBunkerAreaLoader_PlayerStart(const tString &asName);

	void Load(const tString &asName, int alID, bool abActive, const cVector3f &avSize,
			  const cMatrixf &a_mtxTransform, cWorld *apWorld);

	static bool GetStartTransform(const tString &asMapStartName, cMatrixf &aMtxOut);

	static void Clear();

private:
	static std::map<tString, cMatrixf> mmapPlayerStarts;
};

#endif // BUNKER_AREA_LOADER_H
