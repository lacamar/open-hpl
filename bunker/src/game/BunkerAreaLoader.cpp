/*
 * See BunkerAreaLoader.h for the rationale.
 */

#include "BunkerAreaLoader.h"

//---------------------------------------

std::map<tString, cMatrixf> cBunkerAreaLoader_PlayerStart::mmapPlayerStarts;

//---------------------------------------

cBunkerAreaLoader_PlayerStart::cBunkerAreaLoader_PlayerStart(const tString &asName) : iAreaLoader(asName)
{
}

//-----------------------------------------------------------------------

void cBunkerAreaLoader_PlayerStart::Load(const tString &asName, int alID, bool abActive, const cVector3f &avSize,
										  const cMatrixf &a_mtxTransform, cWorld *apWorld)
{
	// An inactive PlayerStart Area is presumably disabled for some
	// story-state reason this Phase 0 scaffold has no concept of - don't
	// offer it up as a valid spawn candidate.
	if (abActive == false)
		return;

	mmapPlayerStarts[asName] = a_mtxTransform;
}

//-----------------------------------------------------------------------

bool cBunkerAreaLoader_PlayerStart::GetStartTransform(const tString &asMapStartName, cMatrixf &aMtxOut)
{
	std::map<tString, cMatrixf>::iterator it = mmapPlayerStarts.find(asMapStartName);
	if (it == mmapPlayerStarts.end())
		return false;

	aMtxOut = it->second;
	return true;
}

//-----------------------------------------------------------------------

void cBunkerAreaLoader_PlayerStart::Clear()
{
	mmapPlayerStarts.clear();
}

//-----------------------------------------------------------------------
