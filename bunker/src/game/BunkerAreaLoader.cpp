
#include "BunkerAreaLoader.h"

std::map<tString, cMatrixf> cBunkerAreaLoader_PlayerStart::mmapPlayerStarts;

cBunkerAreaLoader_PlayerStart::cBunkerAreaLoader_PlayerStart(const tString &asName) : iAreaLoader(asName)
{
}

void cBunkerAreaLoader_PlayerStart::Load(const tString &asName, int alID, bool abActive, const cVector3f &avSize,
										  const cMatrixf &a_mtxTransform, cWorld *apWorld)
{
	// inactive areas are not spawn candidates
	if (abActive == false)
		return;

	mmapPlayerStarts[asName] = a_mtxTransform;
}

bool cBunkerAreaLoader_PlayerStart::GetStartTransform(const tString &asMapStartName, cMatrixf &aMtxOut)
{
	std::map<tString, cMatrixf>::iterator it = mmapPlayerStarts.find(asMapStartName);
	if (it == mmapPlayerStarts.end())
		return false;

	aMtxOut = it->second;
	return true;
}

void cBunkerAreaLoader_PlayerStart::Clear()
{
	mmapPlayerStarts.clear();
}
