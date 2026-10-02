#ifndef SOMA_LUX_PLAYER_H
#define SOMA_LUX_PLAYER_H

#include "SomaLuxGame.h"

#include <map>

class cSomaLuxPlayer;
class cSomaLuxEntity;

class cSomaLuxPlayerState : public cSomaLuxScriptable
{
public:
	tString msName;
	int mlId = -1;
	cSomaLuxPlayer *mpPlayer = NULL;
};

class cSomaLuxMoveState : public cSomaLuxScriptable
{
public:
	tString msName;
	int mlId = -1;
	cSomaLuxPlayer *mpPlayer = NULL;
	bool mbAutoUpdateSpeedSettings = true;

	float mfMaxForwardSpeed = 0;
	float mfMaxBackwardSpeed = 0;
	float mfMaxSidwaySpeed = 0;
	float mfForwardAcc = 0;
	float mfSidewayAcc = 0;
	float mfForwardDeacc = 0;
	float mfSidewayDeacc = 0;
	float mfForwardOppositeDirAccMul = 1;
	float mfSidewayOppositeDirAccMul = 1;
	float mfMaxForwardSpeedMul = 1;
	float mfMaxBackwardSpeedMul = 1;
	float mfMaxSidwaySpeedMul = 1;

	// cLuxMoveState::Update
	void OnUpdate(float afTimeStep) override;
};

class cSomaLuxPlayer : public cSomaLuxHandler
{
public:
	cSomaLuxPlayer();
	~cSomaLuxPlayer();

	static cSomaLuxPlayer *Get() { return mpInstance; }

	void SetCamera(cCamera *apCamera);
	cCamera *GetCamera() { return mpCamera; }
	iCharacterBody *GetCharacterBody() { return mpCharBody; }
	void SetCharacterBody(iCharacterBody *apBody);

	bool IsActive() { return mbActive; }
	void SetActive(bool abX) { mbActive = abX; }

	void AddState(const tString &asName, int alId, const tString &asFile, const tString &asClass);
	void AddMoveState(const tString &asName, int alId, const tString &asFile, const tString &asClass);
	void ChangeState(int alId);
	void ChangeMoveState(int alId);
	cSomaLuxPlayerState *GetState() { return mpState; }
	cSomaLuxMoveState *GetMoveState() { return mpMoveState; }

	// cLuxMapHandler::SetCurrentMap: after CreateWorldEntities/OnMapEnter
	void PlaceAtStart(const cVector3f &avFeetPos, float afYaw, bool abCrouching = false);

	void OnMessage(const char *apDecl) override;
	void OnUpdate(float afTimeStep) override;
	void OnPostUpdate(float afTimeStep) override;
	void OnMapMessage(const char *apDecl, void *apMap) override;
	void OnAction(int alAction, bool abPressed) override;
	void OnAnalogInput(int alAnalogId, const cVector3f &avAmount) override;

	float GetDefaultFOV() { return mfDefaultFOV; }

	static void RegisterNatives(asIScriptEngine *apEngine);

	struct cMoveVector
	{
		cVector3f mvValue = 0;
		cVector3f mvGoal = 0;
		float mfSpeed = 0;
		float mfAcc = 0;
		float mfMaxSpeed = 0;
		float mfSlowdownDist = 0;
		bool mbMoving = false;
	};
	struct cFadeValue
	{
		float mfValue = 0;
		float mfGoal = 0;
		float mfSpeed = 0;
		float mfMaxSpeed = 0;
		float mfSpeedMul = 0;
	};

	cVector3f mvBaseCameraPosAdd = 0;
	std::map<int, cMoveVector> mmapCameraPosAdd;
	std::map<int, cFadeValue> mmapCameraRoll;
	cFadeValue mFOVMul, mAspectMul, mFOV;

	float mfHealth = 1;
	float mfMaxHealth = 1;

	bool mbCameraRotateActive = false;
	cVector3f mvCameraRotateTarget = 0;
	bool mbCameraRotateLocal = false;
	float mfCameraRotateSpeed = 0, mfCameraRotateAcc = 0, mfCameraRotateSpeedMul = 0, mfCameraRotateMaxSpeed = 0;

	bool mbAutomoveActive = false;
	cVector3f mvAutomoveTarget = 0;
	float mfAutomoveSpeedMul = 0;

	std::map<int, float> mmapVisibilityRangeMul;
	std::map<int, float> mmapVisibilityMaxRange;

	float mfTimeSincePhysicsInteraction = 0;
	float mfAverageMoveSpeed = 0;
	cVector3f mvAverageMoveDirection = 0;
	cVector2f mvCameraTrackingAvg = 0;

private:
	void UpdateCamera(float afTimeStep);

	static cSomaLuxPlayer *mpInstance;

	cCamera *mpCamera = NULL;
	iCharacterBody *mpCharBody = NULL;
	bool mbActive = true;
	float mfDefaultFOV = 70;

	std::map<int, cSomaLuxPlayerState *> mmapStates;
	std::map<int, cSomaLuxMoveState *> mmapMoveStates;
	cSomaLuxPlayerState *mpState = NULL;
	cSomaLuxMoveState *mpMoveState = NULL;
};

class cSomaLuxInputHandler : public cSomaLuxHandler
{
public:
	cSomaLuxInputHandler();

	static cSomaLuxInputHandler *Get() { return mpInstance; }

	// cLuxInputHandler::LoadKeyConfig: script CreateActions(), then LoadKeyConfig(cfg) binds them
	void LoadKeyConfig();
	// cLuxInputHandler::LoadScript
	void LoadScript();
	void LoadUserConfig();
	// cLuxInputHandler::Update: action edges and analog sums to every updateable, then mouse look
	void UpdateInput(float afTimeStep, bool abGameInput);

	static void RegisterNatives(asIScriptEngine *apEngine);

	struct cLuxAction
	{
		tString msName;
		int mlId;
		int mlAxis = -1;
		float mfMul = 0;
		int mlAnalogId = -1;
		bool mbGamepad = false;
	};
	std::vector<cLuxAction> mvActions;

	void CreateAction(const cLuxAction &aAction);
	void CreateActionInput(const tString &asInput, int alActionId);
	tString GetActionName(int alId, bool abGamepad = false);

	struct cGamepadPreset
	{
		tString msName;
		std::vector<int> mvActions;
		tStringVec mvBindings;
		std::vector<bool> mvAnalog;
	};
	struct cGamepadProfile
	{
		tString msName, msPrefix;
		tStringVec mvButtons, mvAxes;
		std::vector<unsigned> mvDPad;
		std::vector<cGamepadPreset> mvPresets;
	};
	std::vector<cGamepadProfile> mvGamepadProfiles;
	tString msGamepadProfile, msGamepadPreset;
	cGamepadProfile *GetGamepadProfile(const tString &asName);
	cGamepadPreset *GetGamepadPreset();
	void GetActionsAssociatedToGamepadControl(const tString &asProfile, const tString &asPreset, const tString &asControl, tString &asActions);
	bool FetchGamepadInputLayoutString(const tString &asInput, tString &asPrefix, tString &asLayout);

	// Per-frame action edges; HPL2's cAction::BecameTriggerd consumes the edge on the first call
	void LatchActions();
	bool IsDown(int alId) { return alId >= 0 && alId < kMaxActions && mvDown[alId]; }
	bool BecameDown(int alId) { return IsDown(alId) && mvPrevDown[alId] == false; }
	bool BecameUp(int alId) { return alId >= 0 && alId < kMaxActions && mvDown[alId] == false && mvPrevDown[alId]; }

	bool mbInvertMouse = false;
	bool mbSmoothMouse = true;
	float mfMouseSensitivity = 1;
	float mfGamepadSensitivity = 2;

private:
	static const int kMaxActions = 256;
	bool mvDown[kMaxActions] = {};
	bool mvPrevDown[kMaxActions] = {};
	static cSomaLuxInputHandler *mpInstance;
};

#endif // SOMA_LUX_PLAYER_H
