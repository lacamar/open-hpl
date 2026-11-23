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

#ifndef LUX_AREA_H
#define LUX_AREA_H

//----------------------------------------------

#include "LuxEntity.h"
#include "RustLuxMapA7.h"

//----------------------------------------------

class iLuxArea;

class iLuxArea_SaveData : public iLuxEntity_SaveData
{
	kSerializableClassInit(iLuxArea_SaveData)
public:
	virtual ~iLuxArea_SaveData() {}

	////////////////
	//Properties
	cVector3f mvSize;
	cMatrixf m_mtxTransform;
	
	cVector3f mvRelativeOffset; 
	tString msPropName;
	int mlBodyId;

	////////////////
	//Methods
	iLuxEntity* CreateEntity(cLuxMap *apMap);
	
	virtual iLuxArea* CreateArea(cLuxMap *apMap)=0;
};

//----------------------------------------------

class iLuxArea : public iLuxEntity
{
typedef iLuxEntity super_class;
friend class iLuxAreaLoader;
friend class iLuxArea_SaveData;
friend struct OhplA7bDump;
friend struct OhplAreaHost;
public:	
	iLuxArea(const tString &asName, int alID, cLuxMap *apMap, eLuxAreaType aAreaType);
	virtual ~iLuxArea();

	//////////////////////
	//General
	virtual void OnRenderSolid(cRendererCallbackFunctions* apFunctions);
	
	virtual bool CanInteract(iPhysicsBody *apBody);
	virtual bool OnInteract(iPhysicsBody *apBody, const cVector3f &avPos);

	//////////////////////
	//Actions
	void GiveDamage(float afAmount, int alStrength){}

	//////////////////////
	//Properties
	void AttachToBody( const tString& asPropName, int alBodyId);
	void DetachBody();

	eLuxAreaType GetAreaType(){ return mAreaType;}

	iPhysicsBody* GetBody() { return mpBody;}
	cVector3f GetPosition() { return mpBody->GetLocalPosition(); }

	int GetBodyNum(){ return 1; }
	iPhysicsBody* GetBody(int alIdx){ return mpBody;}

	eLuxFocusCrosshair GetFocusCrosshair(iPhysicsBody *apBody, const cVector3f &avPos);

	virtual iEntity3D* GetAttachEntity();

	//////////////////////
	//Save data stuff
	virtual void SaveToSaveData(iLuxEntity_SaveData* apSaveData);
	virtual void LoadFromSaveData(iLuxEntity_SaveData* apSaveData);
	virtual void SetupSaveData(iLuxEntity_SaveData *apSaveData);
protected:
	virtual void SetupAfterLoad(cWorld *apWorld){}
	void OnSetActive(bool abX);
	void OnUpdate(float afTimeStep);

	template<class T> T& Pod(){ return *static_cast<T*>(mpA7Pod); }

	eLuxAreaType mAreaType;

	OhplA7Area *mpA7Area;
	OhplA7Base *mpA7Base;
	void *mpA7Pod;

	iPhysicsBody *mpBody;
	cVector3f &mvSize;
	cMatrixf &m_mtxTransform;

	iPhysicsBody *mpParentBody;
	cVector3f &mvRelativeOffset;
	tString msPropName;
	int &mlBodyId;
};

//----------------------------------------------

class iLuxAreaLoader : public iAreaLoader
{
public:
	iLuxAreaLoader(const tString& asName) : iAreaLoader(asName){}
	virtual ~iLuxAreaLoader(){}

	void Load(const tString &asName, int alID, bool abActive, const cVector3f &avSize, const cMatrixf &a_mtxTransform,cWorld *apWorld);

	virtual iLuxArea *CreateArea(const tString& asName, int alID, cLuxMap *apMap)=0;
	virtual void LoadVariables(iLuxArea *apArea, cWorld *apWorld)=0;
	virtual void SetupArea(iLuxArea *apAre, cWorld *apWorld)=0;
};

//----------------------------------------------

// Every area type but rope: all state and logic is Rust (A7Area).
class cLuxAreaRs : public iLuxArea
{
typedef iLuxArea super_class;
friend class cLuxAreaLoaderRs;
public:
	cLuxAreaRs(const tString &asName, int alID, cLuxMap *apMap, eLuxAreaType aAreaType);
	virtual ~cLuxAreaRs();

	void SetupAfterLoad(cWorld *apWorld);
	void OnUpdate(float afTimeStep);
	void OnConnectionStateChange(iLuxEntity *apEntity, int alState){}

	bool CanInteract(iPhysicsBody *apBody);
	bool OnInteract(iPhysicsBody *apBody, const cVector3f &avPos);
	eLuxFocusCrosshair GetFocusCrosshair(iPhysicsBody *apBody, const cVector3f &avPos);
	tWString GetFocusText();

	const cVector3f& GetForward(){ return *reinterpret_cast<cVector3f*>(&Pod<OhplA7Ladder>().forward);}
	float GetMaxY(){ return Pod<OhplA7Ladder>().maxY;}
	float GetMinY(){ return Pod<OhplA7Ladder>().minY;}
	tString GetMaterial(){ return Str(kOhplA7sLadderMaterial);}
	cVector3f GetStartRotation();

	void StickyAttach(iPhysicsBody *apBody){ ohpl_a7_area_sticky_attach_body(mpA7Area, apBody);}
	void StickyDetach();
	iPhysicsBody *GetAttachedBody(){ return reinterpret_cast<iPhysicsBody*>(ohpl_a7_area_sticky_attached(mpA7Area));}
	bool CanDetach(){ return ohpl_a7_area_sticky_can_detach(mpA7Area);}

	iLuxEntity_SaveData* CreateSaveData();
	virtual void SaveToSaveData(iLuxEntity_SaveData* apSaveData);
	virtual void LoadFromSaveData(iLuxEntity_SaveData* apSaveData);

private:
	tString Str(size_t alIdx);
	bool LadderInteract();
	void LoadVars(iAreaLoader *apLoader, cWorld *apWorld);
};

class cLuxAreaLoaderRs : public iLuxAreaLoader
{
public:
	cLuxAreaLoaderRs(const tString& asName, eLuxAreaType aAreaType) : iLuxAreaLoader(asName), mAreaType(aAreaType){}

	iLuxArea *CreateArea(const tString& asName, int alID, cLuxMap *apMap);
	void LoadVariables(iLuxArea *apArea, cWorld *apWorld);
	void SetupArea(iLuxArea *apArea, cWorld *apWorld){}

private:
	eLuxAreaType mAreaType;
};

//----------------------------------------------

// Rope entity: settings, loader and save data are Rust (A7Area); this shell creates the physics and graphic rope.
class cLuxRope_SaveData : public iLuxEntity_SaveData
{
	kSerializableClassInit(cLuxRope_SaveData)
public:
	bool mbRopeCreated;
	cVector3f mvStartPos;
	cVector3f mvEndPos;
	tString msEndPosNode;
	tString msStartBody;
	tString msEndBody;
	float mfMinTotalLength;
	float mfMaxTotalLength;
	float mfSegmentLength;
	float mfDamping;
	float mfStrength;
	float mfStiffness;
	tString msMaterial;
	float mfRadius;
	float mfLengthTileAmount;
	float mfLengthTileSize;
	tString msSound;
	float mfSoundStartSpeed;
	float mfSoundStopSpeed;
	bool mbAutoMove;
	float mfAutoMoveAcc;
	float mfAutoMoveMaxSpeed;

	iLuxEntity* CreateEntity(cLuxMap *apMap);
};

class cLuxRope : public iLuxEntity
{
typedef iLuxEntity super_class;
friend struct OhplA7bDump;
friend class cLuxAreaRopeLoader;
public:
	cLuxRope(const tString& asName, int alID, cLuxMap *apMap);
	~cLuxRope();

	void AfterWorldLoad();

	void OnSetActive(bool abX){}
	void OnUpdate(float afTimeStep){}
	void OnConnectionStateChange(iLuxEntity *apEntity, int alState){}

	eLuxFocusCrosshair GetFocusCrosshair(iPhysicsBody *apBody, const cVector3f &avPos){ return eLuxFocusCrosshair_Default; }
	iEntity3D* GetAttachEntity(){ return NULL;}
	void GiveDamage(float afAmount, int alStrength){}
	bool CanInteract(iPhysicsBody *apBody){ return false; }
	bool OnInteract(iPhysicsBody *apBody, const cVector3f &avPos){ return false;}
	int GetBodyNum(){ return 0;}
	iPhysicsBody* GetBody(int alIdx){ return NULL;}

	iLuxEntity_SaveData* CreateSaveData(){ return hplNew(cLuxRope_SaveData, ());}
	void SaveToSaveData(iLuxEntity_SaveData* apSaveData);
	void LoadFromSaveData(iLuxEntity_SaveData* apSaveData);
	void SetupSaveData(iLuxEntity_SaveData *apSaveData){ super_class::SetupSaveData(apSaveData);}
private:
	iPhysicsBody* GetBody(const tString& asName);
	tString Str(size_t alIdx);

	OhplA7Area *mpA7Area;
	OhplA7Rope &mR;

	iPhysicsRope *mpRope;
	cRopeEntity *mpRopeGfx;
};

class cLuxAreaRopeLoader : public iAreaLoader
{
public:
	cLuxAreaRopeLoader(const tString& asName) : iAreaLoader(asName){}
	virtual ~cLuxAreaRopeLoader(){}

	void Load(const tString &asName, int alID, bool abActive, const cVector3f &avSize, const cMatrixf &a_mtxTransform,cWorld *apWorld);
};

//----------------------------------------------


#endif // LUX_AREA_H
