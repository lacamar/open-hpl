/*
 * Copyright © 2009-2020 Frictional Games
 * 
 * This file is part of Amnesia: The Dark Descent.
 * 
 * Amnesia: The Dark Descent is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version. 

 * Amnesia: The Dark Descent is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with Amnesia: The Dark Descent.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "engine/SaveGame.h"

#include "system/LowLevelSystem.h"

namespace hpl {
	
	
	
	//////////////////////////////////////////////////////////////////////////
	// SAVE GAME DATA
	//////////////////////////////////////////////////////////////////////////
	
	//------------------------------------------------------------------------
	
	//Serialize iSaveGame
	kBeginSerializeBaseVirtual(iSaveData)
	kSerializeVar(mlSaveDataId, eSerializeType_Int32)
	kEndSerialize()

	//------------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// SAVE GAME OBJECT
	//////////////////////////////////////////////////////////////////////////
	
	int iSaveObject::_mlGlobalIdCount =0;

	//------------------------------------------------------------------------

	iSaveObject::iSaveObject()
	{
		mlSaveObjectId = ohpl_en_save_next_id(&_mlGlobalIdCount);

		mbIsSaved = true;
	}
	
	iSaveObject::~iSaveObject()
	{

	}

	//------------------------------------------------------------------------
	
	void iSaveObject::SaveToSaveData(iSaveData *apSaveData)
	{
		apSaveData->mlSaveDataId = mlSaveObjectId;	
	}

	//------------------------------------------------------------------------
	
	void iSaveObject::LoadFromSaveData(iSaveData *apSaveData)
	{
		mlSaveObjectId = apSaveData->mlSaveDataId;
		mpSaveData = apSaveData;
	}

	//------------------------------------------------------------------------

	void iSaveObject::SaveDataSetup(cSaveObjectHandler *apSaveObjectHandler, cEngine *apGame)
	{

	}
	
	//------------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// SAVE GAME CONTAINER
	//////////////////////////////////////////////////////////////////////////

	//------------------------------------------------------------------------

	cSaveObjectHandler::cSaveObjectHandler()
	{
		mpReg = ohpl_en_reg_new();
	}
	
	cSaveObjectHandler::~cSaveObjectHandler()
	{
		ohpl_en_reg_free(mpReg);
	}

	//------------------------------------------------------------------------

	void cSaveObjectHandler::Add(iSaveObject *pObject)
	{
		ohpl_en_reg_add(mpReg, pObject->GetSaveObjectId(), reinterpret_cast<size_t>(pObject));
	}
	
	//------------------------------------------------------------------------

	iSaveObject* cSaveObjectHandler::Get(int alId)
	{
		iSaveObject *pObject = reinterpret_cast<iSaveObject*>(ohpl_en_reg_get(mpReg, alId));
		if(pObject == NULL){
			Warning("Couldn't find save object with id %d\n",alId);
			return NULL;
		}

		return pObject;
	}

	//------------------------------------------------------------------------

	cSaveObjectIterator cSaveObjectHandler::GetIterator()
	{
		return cSaveObjectIterator(mpReg);
	}
	
	//------------------------------------------------------------------------

	void cSaveObjectHandler::SetUpAll(cEngine *apGame)
	{
		struct tCtx { cSaveObjectHandler *mpHandler; cEngine *mpGame; } ctx = { this, apGame };
		iSaveObject::_mlGlobalIdCount = ohpl_en_reg_set_up_all(mpReg,
			[](void *apCtx, size_t alObject) -> int
			{
				tCtx *pCtx = static_cast<tCtx*>(apCtx);
				iSaveObject *pObject = reinterpret_cast<iSaveObject*>(alObject);
				int lId = pObject->GetSaveObjectId();
				pObject->SaveDataSetup(pCtx->mpHandler, pCtx->mpGame);
				return lId;
			}, &ctx);
	}
	
	//------------------------------------------------------------------------

	void cSaveObjectHandler::Clear()
	{
		ohpl_en_reg_clear(mpReg);
	}

	//------------------------------------------------------------------------

	size_t cSaveObjectHandler::Size()
	{
		return ohpl_en_reg_size(mpReg);
	}

	//------------------------------------------------------------------------
	

	//////////////////////////////////////////////////////////////////////////
	// SAVE DATA CONTAINER
	//////////////////////////////////////////////////////////////////////////

	//------------------------------------------------------------------------
	
	cSaveDataHandler::cSaveDataHandler()
	{
		mpReg = ohpl_en_reg_new();
	}

	cSaveDataHandler::~cSaveDataHandler()
	{
		ohpl_en_reg_free(mpReg);
	}
	
	//------------------------------------------------------------------------

	void cSaveDataHandler::Add(iSaveData *pData)
	{
		ohpl_en_reg_add(mpReg, pData->GetSaveCreatePrio(), reinterpret_cast<size_t>(pData));
	}

	cSaveDataIterator cSaveDataHandler::GetIterator()
	{
		return cSaveDataIterator(mpReg);
	}

	void cSaveDataHandler::Clear()
	{
		ohpl_en_reg_clear(mpReg);
	}

	size_t cSaveDataHandler::Size()
	{
		return ohpl_en_reg_size(mpReg);
	}
	
	//------------------------------------------------------------------------

	void cSaveDataHandler::AddVoidPtr(void **apPtr)
	{
		iSaveData** pDataPtr = (iSaveData**)apPtr;
		Add(*pDataPtr);
	}
	void cSaveDataHandler::AddVoidClass(void *apClass)
	{
		iSaveData* pData = (iSaveData*)(apClass);
		Add(pData);
	}

	iContainerIterator* cSaveDataHandler::CreateIteratorPtr()
	{
		return hplNew( cSaveDataIterator, (mpReg) );
	}

	//------------------------------------------------------------------------

	
}
