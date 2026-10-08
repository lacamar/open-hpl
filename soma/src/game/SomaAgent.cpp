#include "SomaAgent.h"
#include "SomaBase.h"
#include "SomaLux.h"
#include "SomaLuxEntity.h"
#include "SomaLuxPlayer.h"
#include "SomaLuxScriptable.h"
#include "SomaLuxVoice.h"
#include "SomaScriptApi.h"
#include "SomaScriptBind.h"
#include "SomaScriptRuntime.h"

#include "impl/scriptarray.h"
#include "impl/tinyXML/tinyxml.h"

#include <deque>

#include <algorithm>
#include <angelscript.h>
#include <cmath>
#include <cstring>
#include <map>
#include <memory>
#include <new>
#include <unordered_map>

namespace
{
	typedef cSomaLuxEntity E;

	enum
	{
		eMsg_StuckCounterIsAtMax = 1,
		eMsg_EndOfPath = 2,
		eMsg_AnimationOver = 3,
		eMsg_SoundHeard = 4,
		eMsg_TurningDone = 5,
		eMsg_PlayerDetected = 8,
		eMsg_PlayerUndetected = 9,
		eMsg_AtTrackNode = 11,
		eMsg_EndOfTrack = 12,
		eMsg_SensesDeactivated = 17,
	};

	enum
	{
		eComp_User,
		eComp_Pathfinder,
		eComp_CharMover,
		eComp_SoundListener,
		eComp_StateMachine,
		eComp_HeadTracker,
		eComp_ForceEmitter,
		eComp_BarkMachine,
		eComp_BackboneTail,
		eComp_LightSensor,
		eComp_EdgeGlow,
	};

	uint64_t Hash64(const tString &s) { return SomaHash64(s); }

	float Wrap(float a)
	{
		while (a > kPif) a -= k2Pif;
		while (a < -kPif) a += k2Pif;
		return a;
	}

	float YawTo(const cVector3f &avDir) { return std::atan2(-avDir.x, -avDir.z); }

	void SetYawNear(iCharacterBody *apBody, float afYaw) { apBody->SetYaw(apBody->GetYaw() + Wrap(afYaw - apBody->GetYaw())); }

	iCharacterBody *PlayerBody() { return cSomaLuxPlayer::Get() ? cSomaLuxPlayer::Get()->GetCharacterBody() : NULL; }

	// cLuxEntityMessageData in the official layout: mvX@12 mvY@24 mlX@36 mlY@40 mID@44 msX@56
	struct cAgentMessageData
	{
		alignas(8) char mBlock[128] = {};
		cAgentMessageData() { new (mBlock + 56) std::string(); }
		~cAgentMessageData() { ((std::string *)(mBlock + 56))->~basic_string(); }
		void Set(const cVector3f &avX, int alX)
		{
			memcpy(mBlock + 12, &avX, sizeof(cVector3f));
			memcpy(mBlock + 36, &alX, sizeof(int));
		}
	};

	struct cAgentComponent
	{
		cSomaLuxEntity *mpEntity;
		int mlType;
		tString msName;
		bool mbActive = true;
		cAgentComponent(E *apEnt, int alType) : mpEntity(apEnt), mlType(alType) {}
		virtual ~cAgentComponent() {}
		virtual void Update(float) {}
		virtual void OnMessage(int) {}
		virtual void OnSetActive() {}
		virtual void OnDestroy() {}
	};

	struct cAgent;
	cAgent *Agent(E *apEnt);

	struct cAgentStateMachine : cAgentComponent
	{
		std::map<int, tString> mapStates, mapSubStates;
		int mlCur = -1, mlPrev = -1, mlNext = -1;
		int mlSubCur = -1, mlSubPrev = -1, mlSubNext = -1;
		bool mbInSub = false;
		struct cTimer { uint64_t mlId; float mfTime; };
		std::vector<cTimer> mvTimers[2];

		cAgentStateMachine(E *p) : cAgentComponent(p, eComp_StateMachine) {}

		tString Name(int alId) { auto it = mapStates.find(alId); return it == mapStates.end() ? tString() : it->second; }
		tString SubName(int alId) { auto it = mapSubStates.find(alId); return it == mapSubStates.end() ? tString() : it->second; }

		// the state's own function, else State_Default_ (also when a Message handler returns false)
		void Run(bool abSub, int alId, const char *apFunc, const std::function<void(asIScriptContext *)> &aArgs = nullptr)
		{
			bool bMsg = strncmp(apFunc, "Message", 7) == 0;
			tString sPre = tString(bMsg ? "bool " : "void ") + (abSub ? "SubState_" : "State_");
			auto call = [&](const tString &asState) { tString d = sPre + asState + "_" + apFunc; return bMsg ? mpEntity->CallBool(d, aArgs, false) : mpEntity->Call(d, aArgs); };
			mbInSub = abSub;
			if (call(abSub ? SubName(alId) : Name(alId)) == false)
				call("Default");
			mbInSub = false;
		}

		void ChangeState(int alState)
		{
			if (mbInSub)
				Error("Entity '%s' cannot change to state %d inside a sub state ('%s').\n", mpEntity->msName.c_str(), alState, SubName(mlSubCur).c_str());
			else if (mapStates.count(alState) == 0)
				Error("State %d does not exist in Entity '%s'\n", alState, mpEntity->msName.c_str());
			else
				mlNext = alState;
		}

		void ChangeSubState(int alState)
		{
			if (alState > 0 && mapSubStates.count(alState) == 0)
				Error("State %d does not exist in Entity '%s'\n", alState, mpEntity->msName.c_str());
			else
				mlSubNext = alState;
		}

		void Reset()
		{
			mlCur = mlPrev = mlSubCur = mlSubPrev = mlSubNext = -1;
			mlNext = mapStates.empty() ? -1 : 0;
			mbInSub = false;
			mvTimers[0].clear();
			mvTimers[1].clear();
		}

		void OnSetActive() override { Reset(); }

		void AddTimer(uint64_t alId, float afTime) { mvTimers[mbInSub].push_back(cTimer{alId, afTime}); }
		void StopTimer(uint64_t alId) { std::erase_if(mvTimers[mbInSub], [alId](const cTimer &t) { return t.mlId == alId; }); }
		bool TimerExists(uint64_t alId) { return std::any_of(mvTimers[mbInSub].begin(), mvTimers[mbInSub].end(), [alId](const cTimer &t) { return t.mlId == alId; }); }

		void UpdateTimers(bool abSub, int alState, float afTimeStep)
		{
			std::vector<uint64_t> vDue;
			std::erase_if(mvTimers[abSub], [&](cTimer &t) { t.mfTime -= afTimeStep; return t.mfTime <= 0 && (vDue.push_back(t.mlId), true); });
			for (uint64_t lId : vDue)
				Run(abSub, alState, "TimerUp(uint64)", [lId](asIScriptContext *c) { c->SetArgQWord(0, lId); });
		}

		// cLuxStateMachine::OnUpdate: state changes requested last frame are applied here
		void Update(float afTimeStep) override
		{
			auto dt = [afTimeStep](asIScriptContext *c) { c->SetArgFloat(0, afTimeStep); };
			int lNext = mlNext;
			if (lNext >= 0 ? mlCur != lNext : mlCur >= 0)
			{
				mlPrev = mlCur;
				if (mlCur >= 0)
					Run(false, mlCur, "Leave()");
				mlCur = mapStates.count(lNext) ? lNext : -1;
				mlSubNext = -1;
				mvTimers[0].clear();
				if (mlCur >= 0)
					Run(false, mlCur, "Enter()");
			}
			if (mlCur >= 0)
			{
				Run(false, mlCur, "Update(float)", dt);
				UpdateTimers(false, mlCur, afTimeStep);
			}
			lNext = mlSubNext;
			if (lNext >= 0 ? mlSubCur != lNext : mlSubCur >= 0)
			{
				int lOld = mlSubCur;
				mlSubPrev = mlSubCur;
				if (lOld >= 0)
					Run(true, lOld, "Leave()");
				mlSubCur = mapSubStates.count(lNext) ? lNext : -1;
				if (lOld >= 0 && mlCur >= 0)
					Run(false, mlCur, "SubStateOver(int)", [lOld](asIScriptContext *c) { c->SetArgDWord(0, lOld); });
				mvTimers[1].clear();
				if (mlSubCur >= 0)
					Run(true, mlSubCur, "Enter()");
			}
			if (mlSubCur >= 0)
			{
				Run(true, mlSubCur, "Update(float)", dt);
				UpdateTimers(true, mlSubCur, afTimeStep);
			}
		}

		void OnMessage(int alMessage) override
		{
			auto msg = [alMessage](asIScriptContext *c) { c->SetArgDWord(0, alMessage); };
			if (mlCur >= 0)
				Run(false, mlCur, "Message(int)", msg);
			if (mlSubCur >= 0)
				Run(true, mlSubCur, "Message(int)", msg);
		}
	};

	struct cAgentSpeedState
	{
		float mfForward = 1, mfBackward = 1, mfSideways = 1;
		float mfTurnBreakMul = -1, mfTurnSpeedMul = -1, mfTurnMaxSpeed = -1;
		float mfForwardAcc = -1, mfForwardDeacc = -1, mfSidewayAcc = -1, mfSidewayDeacc = -1;
	};

	struct cAgentCharMover : cAgentComponent
	{
		iCharacterBody *mpBody;
		float mfMaxForward = 1, mfMaxBackward = 1;
		float mfTurnMinBreakAngle = cMath::ToRad(40), mfTurnBreakMul = 0, mfTurnSpeedMul = 3, mfTurnMaxSpeed = 4;
		float mfStoppedToWalk = 0.05f, mfWalkToRun = 3, mfWalkToStopped = 0.025f, mfRunToWalk = 0.8f;
		float mfMoveSpeedAnimMul = 1;
		bool mbUseMoveStateAnims = true;
		tString msIdleAnim = "Idle", msWalkAnim = "Walk", msRunAnim = "Run", msBackwardAnim;
		std::map<int, cAgentSpeedState> mapSpeedStates;
		int mlEditState = -1, mlSpeedState = -1;
		bool mbMoving = false, mbMoveRequested = false, mbSlowDownAtGoal = false, mb3D = false;
		cVector3f mvGoal = 0;
		bool mbTurning = false;
		float mfTurnGoal = 0;
		tString msTurnedCallback;
		int mlAnimState = -1;
		bool mbAnimPlaying = false;
		float mfStuck = 0;
		static constexpr int kStuckFrames = 30;
		static constexpr float kMaxStuck = 2.0f;
		cVector3f mvStuckDisp[kStuckFrames];
		int mlStuckIdx = 0, mlStuckCount = 0;

		cAgentCharMover(E *p, iCharacterBody *apBody) : cAgentComponent(p, eComp_CharMover), mpBody(apBody) {}

		cAgentSpeedState *Edit() { return mlEditState >= 0 ? &mapSpeedStates[mlEditState] : NULL; }
		cAgentSpeedState *Current()
		{
			auto it = mapSpeedStates.find(mlSpeedState);
			return it == mapSpeedStates.end() ? NULL : &it->second;
		}

		void LoadFromVariables(cResourceVarsObject *apVars)
		{
			if (apVars == NULL)
				return;
			msIdleAnim = apVars->GetVarString("CharMover_IdleAnim", msIdleAnim);
			msWalkAnim = apVars->GetVarString("CharMover_WalkAnim", msWalkAnim);
			msRunAnim = apVars->GetVarString("CharMover_RunAnim", msRunAnim);
			msBackwardAnim = apVars->GetVarString("CharMover_BackwardAnim", msBackwardAnim);
		}

		void MoveToPos(const cVector3f &avPos, bool abSlowDown)
		{
			mvGoal = avPos;
			mbMoveRequested = true;
			mbSlowDownAtGoal = abSlowDown;
			mbTurning = false;
		}

		void Stop() { mbMoving = mbMoveRequested = false; }

		void TurnTo(float afYaw)
		{
			mbMoving = mbMoveRequested = false;
			mbTurning = true;
			mfTurnGoal = afYaw;
		}

		float ForwardSpeed()
		{
			cAgentSpeedState *pState = Current();
			return pState ? pState->mfForward : mfMaxForward;
		}

		int PlayAnimation(const tString &asName, float afFade, bool abLoop, const tString &asCallback)
		{
			int lIdx = mpEntity->PlayAnimation(asName, afFade, abLoop, asCallback);
			if (lIdx >= 0)
			{
				mbAnimPlaying = true;
				mbUseMoveStateAnims = false;
			}
			return lIdx;
		}

		void SetUseMoveStateAnims(bool abX)
		{
			if (abX == mbUseMoveStateAnims)
				return;
			mbUseMoveStateAnims = abX;
			if (abX)
			{
				mbAnimPlaying = false;
				mlAnimState = -1;
			}
		}

		void PlayMoveAnim(int alState, float afSpeed)
		{
			if (mbAnimPlaying && mpEntity->mlCurrentAnim < 0)
				SetUseMoveStateAnims(true);
			if (mbUseMoveStateAnims == false)
			{
				mlAnimState = -1;
				return;
			}
			const tString &sAnim = alState == 0 ? msIdleAnim : alState == 1 ? msWalkAnim : msRunAnim;
			if (alState != mlAnimState && sAnim != "")
			{
				mlAnimState = alState;
				mpEntity->PlayAnimation(sAnim, 0.3f, true, "");
			}
			if (alState > 0 && mpEntity->mpMesh && mpEntity->mlCurrentAnim >= 0)
				if (cAnimationState *pAnim = mpEntity->mpMesh->GetAnimationState(mpEntity->mlCurrentAnim))
					pAnim->SetSpeed(cMath::Max(afSpeed * mfMoveSpeedAnimMul / cMath::Max(alState == 1 ? 1.0f : mfWalkToRun, 0.1f), 0.2f));
		}

		void UpdateStuck(float afTimeStep, float afSpeed)
		{
			cVector3f vDisp = mpBody->GetPosition() - mpBody->GetLastPosition();
			mvStuckDisp[mlStuckIdx] = vDisp;
			mlStuckIdx = (mlStuckIdx + 1) % kStuckFrames;
			mlStuckCount = cMath::Min(mlStuckCount + 1, kStuckFrames);
			if (mbAnimPlaying || afSpeed < 1e-5f)
			{
				mfStuck = 0;
				return;
			}
			cVector3f vSum = 0;
			for (int i = 0; i < mlStuckCount; ++i)
				vSum += mvStuckDisp[i];
			float fDist = vDisp.Length();
			float fDot = cMath::Vector3Dot(mpBody->GetForward(), fDist > 1e-8f ? vDisp / fDist : vDisp);
			if (vSum.Length() < 0.1f * afSpeed || fDist / afTimeStep / afSpeed < 0.3f || (std::fabs(fDot) < 0.3f && afSpeed > 0.001f))
				mfStuck = cMath::Min(mfStuck + afTimeStep, kMaxStuck);
			else
				mfStuck = cMath::Max(mfStuck - 0.8f * afTimeStep, 0.0f);
			if (mfStuck >= kMaxStuck)
			{
				mfStuck = 0;
				SomaAgentSendMessage(mpEntity, eMsg_StuckCounterIsAtMax);
			}
		}

		void Update(float afTimeStep) override
		{
			if (mpBody == NULL)
				return;
			// the real MoveToPos moves the body for one frame only; callers re-send it every frame
			mbMoving = std::exchange(mbMoveRequested, false);
			cAgentSpeedState *pState = Current();
			float fTurnSpeedMul = pState && pState->mfTurnSpeedMul >= 0 ? pState->mfTurnSpeedMul : mfTurnSpeedMul;
			float fTurnMax = pState && pState->mfTurnMaxSpeed >= 0 ? pState->mfTurnMaxSpeed : mfTurnMaxSpeed;
			float fBreakMul = pState && pState->mfTurnBreakMul >= 0 ? pState->mfTurnBreakMul : mfTurnBreakMul;

			float fYaw = mpBody->GetYaw();
			float fGoalYaw = fYaw;
			bool bRotate = false;
			float fDist = 0;
			if (mbMoving)
			{
				cVector3f vDelta = mvGoal - mpBody->GetPosition();
				float fDY = mb3D ? vDelta.y : 0;
				vDelta.y = 0;
				float fDistXZ = vDelta.Length();
				fDist = std::sqrt(fDistXZ * fDistXZ + fDY * fDY);
				if (fDistXZ > 0.05f)
				{
					fGoalYaw = YawTo(vDelta);
					bRotate = true;
				}
				if (mb3D && fDist > 0.05f)
				{
					float fPitchDiff = std::atan2(fDY, fDistXZ) - mpBody->GetPitch();
					mpBody->AddPitch(cMath::Clamp(fPitchDiff, -mfTurnMaxSpeed * afTimeStep, mfTurnMaxSpeed * afTimeStep));
				}
			}
			else if (mbTurning)
			{
				fGoalYaw = mfTurnGoal;
				bRotate = true;
			}
			float fDiff = Wrap(fGoalYaw - fYaw);
			if (bRotate)
			{
				float fStep = cMath::Min(std::fabs(fDiff) * fTurnSpeedMul, fTurnMax) * afTimeStep;
				fStep = cMath::Min(fStep, std::fabs(fDiff));
				mpBody->SetYaw(fYaw + (fDiff < 0 ? -fStep : fStep));
				if (mbTurning && std::fabs(fDiff) < cMath::ToRad(2))
				{
					mbTurning = false;
					SetYawNear(mpBody, fGoalYaw);
					SomaAgentSendMessage(mpEntity, eMsg_TurningDone);
					if (msTurnedCallback != "")
						SomaMapScriptCall("void " + msTurnedCallback + "(const tString &in)", [&](asIScriptContext *c) { c->SetArgObject(0, &mpEntity->msName); });
				}
			}

			float fWanted = 0;
			if (mbMoving)
			{
				fWanted = ForwardSpeed();
				if (std::fabs(fDiff) > mfTurnMinBreakAngle)
					fWanted *= fBreakMul;
				if (mbSlowDownAtGoal && fDist < 1.0f)
					fWanted *= cMath::Max(fDist, 0.2f);
			}
			mpBody->SetMaxPositiveMoveSpeed(eCharDir_Forward, cMath::Max(fWanted, 0.001f));
			if (pState && pState->mfForwardAcc > 0)
				mpBody->SetMoveAcc(eCharDir_Forward, pState->mfForwardAcc);
			if (pState && pState->mfForwardDeacc > 0)
				mpBody->SetMoveDeacc(eCharDir_Forward, pState->mfForwardDeacc);
			if (fWanted > 0)
				mpBody->Move(eCharDir_Forward, 1);

			float fSpeed = mpBody->GetMoveSpeed(eCharDir_Forward);
			UpdateStuck(afTimeStep, fSpeed);

			int lAnim = mlAnimState < 0 ? 0 : mlAnimState;
			if (lAnim == 0 && fSpeed > mfStoppedToWalk)
				lAnim = 1;
			else if (lAnim == 1 && fSpeed > mfWalkToRun)
				lAnim = 2;
			else if (lAnim == 2 && fSpeed < mfRunToWalk)
				lAnim = 1;
			else if (lAnim == 1 && fSpeed < mfWalkToStopped)
				lAnim = 0;
			PlayMoveAnim(lAnim, fSpeed);
		}
	};

	struct cNodeData
	{
		std::unique_ptr<cAINodeContainer> mpContainer;
		std::unique_ptr<cAStarHandler> mpAStar;
	};
	std::map<std::pair<cWorld *, tString>, cNodeData> gmapContainers;

	cNodeData *GetContainer(cSomaLuxMap *apMap, const tString &asName, const cVector3f &avSize, float afMaxHeight, float afMaxEdgeDist = 5, bool abAtCenter = false)
	{
		cWorld *pWorld = apMap->GetWorld();
		auto key = std::make_pair(pWorld, asName);
		auto it = gmapContainers.find(key);
		if (it != gmapContainers.end())
			return &it->second;
		for (auto i = gmapContainers.begin(); i != gmapContainers.end();)
			i = i->first.first != pWorld ? gmapContainers.erase(i) : std::next(i);

		tWString sFile = cString::SetFileExtW(pWorld->GetFilePath(), _W("")) + _W("_") + cString::To16Char(asName) + _W(".nodes");
		std::map<tString, int> mapIds;
		TiXmlDocument doc;
		FILE *pFile = cPlatform::OpenFile(sFile, _W("rb"));
		bool bHasFile = pFile && doc.LoadFile(pFile);
		if (pFile)
			fclose(pFile);
		if (bHasFile && doc.RootElement())
			for (TiXmlElement *p = doc.RootElement()->FirstChildElement("Node"); p; p = p->NextSiblingElement("Node"))
				mapIds[cString::ToString(p->Attribute("Name"), "")] = cString::ToInt(p->Attribute("ID"), -1);

		cNodeData &data = gmapContainers[key];
		data.mpContainer.reset(new cAINodeContainer(asName, "PathNode", pWorld, avSize));
		cAINodeContainer *pCont = data.mpContainer.get();
		pCont->SetMinEdges(2);
		pCont->SetMaxEdges(5);
		pCont->SetMaxEdgeDistance(afMaxEdgeDist);
		pCont->SetMaxHeight(afMaxHeight);
		pCont->SetNodeIsAtCenter(abAtCenter);
		int lNextId = 1 << 30;
		for (cSomaLuxEntity *pEnt : apMap->GetEntities())
			if (pEnt->msClassName == "PathNode")
			{
				auto idIt = mapIds.find(pEnt->msName);
				pCont->AddNode(pEnt->msName, idIt != mapIds.end() ? idIt->second : lNextId++, pEnt->GetPosition(), NULL);
			}
		if (bHasFile)
			pCont->LoadFromFile(sFile);
		else
			pCont->Compile();
		data.mpAStar.reset(new cAStarHandler(pCont));
		data.mpAStar->SetMaxIterations(2000);
		Log("SOMA agent: node container '%s' %d nodes%s\n", asName.c_str(), pCont->GetNodeNum(), bHasFile ? "" : " (compiled)");
		return &data;
	}

	struct cAgentTrackNode
	{
		tString msNode;
		float mfMinWait, mfMaxWait;
		tString msAnim;
		bool mbLoopAnim;
	};

	struct cAgentPathfinder : cAgentComponent
	{
		tString msContainer;
		cNodeData *mpNodes = NULL;
		float mfMaxHeight = 1.0f, mfMaxEdgeDist = 5;
		bool mbAtCenter = false;
		std::vector<cVector3f> mvPath;
		size_t mlPathIdx = 0;
		bool mbMoving = false, mbExact = false, mbGoalPending = false;
		cVector3f mvGoal = 0;
		std::deque<float> mvDistHistory;
		tString msResultCallback, msEndOfPathCallback;
		bool mbCallbackInMap = false;
		std::vector<cAINode *> mvNodeArray;
		std::vector<float> mvNodeArrayDist;

		std::vector<cAgentTrackNode> mvTrack;
		int mlTrackIdx = -1;
		bool mbTrackActive = false, mbTrackPaused = false, mbTrackLoop = false;
		bool mbAtTrackNode = false;
		float mfTrackWait = 0, mfTrackFreq = 1;
		tString msTrackCallback;

		cAgentPathfinder(E *p) : cAgentComponent(p, eComp_Pathfinder) {}

		cAgentCharMover *Mover();

		cNodeData *Nodes()
		{
			cSomaLuxMap *pMap = cSomaLuxMap::GetCurrent();
			if (mpNodes == NULL && pMap)
				mpNodes = GetContainer(pMap, msContainer != "" ? msContainer : cString::SetFileExt(cString::GetFileName(mpEntity->msFileName), ""),
									   cVector3f(0.6f, 1.5f, 0.6f), mfMaxHeight, mfMaxEdgeDist, mbAtCenter);
			return mpNodes;
		}

		cVector3f Feet();

		bool BuildPath(const cVector3f &avGoal, std::vector<cVector3f> &avOut)
		{
			avOut.clear();
			cNodeData *pNodes = Nodes();
			if (pNodes == NULL)
			{
				avOut.push_back(avGoal);
				return true;
			}
			tAINodeList lstNodes;
			if (pNodes->mpAStar->GetPath(Feet(), avGoal, &lstNodes) == false)
				return false;
			for (auto it = lstNodes.rbegin(); it != lstNodes.rend(); ++it)
				avOut.push_back((*it)->GetPosition());
			avOut.push_back(avGoal);
			return true;
		}

		void MoveTo(const cVector3f &avGoal, bool abExact, const tString &asCallback, bool abInMap)
		{
			mvGoal = avGoal;
			mbExact = abExact;
			msResultCallback = asCallback;
			mbCallbackInMap = abInMap;
			if (BuildPath(avGoal, mvPath) == false)
				mvPath.assign(1, avGoal);
			mlPathIdx = 0;
			mbMoving = true;
			mvDistHistory.clear();
			if (cAgentCharMover *pMover = Mover())
				pMover->MoveToPos(mvPath[0], abExact && mvPath.size() == 1);
		}

		void Stop()
		{
			mbMoving = false;
			if (cAgentCharMover *pMover = Mover())
				pMover->Stop();
		}

		void RunResultCallback(bool abOk)
		{
			if (msResultCallback == "")
				return;
			tString sFunc = msResultCallback;
			msResultCallback = "";
			cSomaLuxMap *pMap = cSomaLuxMap::GetCurrent();
			if (mbCallbackInMap && pMap && pMap->GetScript())
				cSomaScriptRuntime::Get()->Call(pMap->GetScript(), "void " + sFunc + "(const tString &in, bool)", [&](asIScriptContext *c) {
					c->SetArgObject(0, &mpEntity->msName);
					c->SetArgByte(1, abOk);
				});
			else
				mpEntity->Call("void " + sFunc + "(bool)", [abOk](asIScriptContext *c) { c->SetArgByte(0, abOk); });
		}

		void ArriveEnd(bool abOk = true)
		{
			mbMoving = false;
			if (cAgentCharMover *pMover = Mover())
				pMover->Stop();
			SomaAgentSendMessage(mpEntity, eMsg_EndOfPath, 0, abOk);
			if (msEndOfPathCallback != "")
			{
				tString sFunc = msEndOfPathCallback;
				msEndOfPathCallback = "";
				SomaMapScriptCall("void " + sFunc + "(const tString &in, bool)", [&](asIScriptContext *c) {
					c->SetArgObject(0, &mpEntity->msName);
					c->SetArgByte(1, abOk);
				});
			}
			if (mbTrackActive && mlTrackIdx >= 0 && mlTrackIdx < (int)mvTrack.size())
			{
				mbAtTrackNode = true;
				mfTrackWait = 0;
				if (abOk == false)
					return;
				const cAgentTrackNode &node = mvTrack[mlTrackIdx];
				mfTrackWait = cMath::RandRectf(node.mfMinWait, node.mfMaxWait);
				if (node.msAnim != "" && Mover())
					Mover()->PlayAnimation(node.msAnim, 0.3f, node.mbLoopAnim, "");
				SomaAgentSendMessage(mpEntity, eMsg_AtTrackNode);
			}
			else
				RunResultCallback(abOk);
		}

		void OnMessage(int alMessage) override
		{
			if (alMessage == eMsg_StuckCounterIsAtMax)
				ArriveEnd(false);
		}

		void StartTrackNode()
		{
			if (mlTrackIdx < 0 || mlTrackIdx >= (int)mvTrack.size())
				return;
			cNodeData *pNodes = Nodes();
			cAINode *pNode = pNodes ? pNodes->mpContainer->GetNodeFromName(mvTrack[mlTrackIdx].msNode) : NULL;
			if (pNode == NULL)
			{
				Warning("SOMA agent '%s': track node '%s' not found\n", mpEntity->msName.c_str(), mvTrack[mlTrackIdx].msNode.c_str());
				mbTrackActive = false;
				return;
			}
			mbAtTrackNode = false;
			tString sKeep = msResultCallback;
			MoveTo(pNode->GetPosition(), false, "", false);
			msResultCallback = sKeep;
		}

		void GoToNextTrackNode()
		{
			if (mvTrack.empty())
				return;
			++mlTrackIdx;
			if (mlTrackIdx >= (int)mvTrack.size())
			{
				SomaAgentSendMessage(mpEntity, eMsg_EndOfTrack);
				if (msTrackCallback != "")
					SomaMapScriptCall("void " + msTrackCallback + "(const tString &in)", [&](asIScriptContext *c) { c->SetArgObject(0, &mpEntity->msName); });
				if (mbTrackLoop == false)
				{
					mbTrackActive = false;
					mlTrackIdx = (int)mvTrack.size() - 1;
					return;
				}
				mlTrackIdx = 0;
			}
			StartTrackNode();
		}

		void Update(float afTimeStep) override
		{
			if (mbTrackActive && mbTrackPaused == false && mbAtTrackNode && mbMoving == false)
			{
				mfTrackWait -= afTimeStep;
				if (mfTrackWait <= 0)
					GoToNextTrackNode();
			}
			if (mbMoving == false || mvPath.empty())
				return;
			bool bLast = mlPathIdx + 1 >= mvPath.size();
			if (Arrived(mvPath[mlPathIdx], bLast))
			{
				mvDistHistory.clear();
				if (bLast)
				{
					ArriveEnd();
					return;
				}
				++mlPathIdx;
			}
			mbGoalPending = true;
		}

		bool Arrived(const cVector3f &avNode, bool abLast);

		// queued like the real goal message, so it lands after the state machine's MoveToPos
		void SendGoal()
		{
			if (std::exchange(mbGoalPending, false) && mbMoving && mlPathIdx < mvPath.size() && Mover())
				Mover()->MoveToPos(mvPath[mlPathIdx], mbExact && mlPathIdx + 1 >= mvPath.size());
		}

		cAINode *NodeAtPos(const cVector3f &avPos, float afMin, float afMax, bool abClosest, bool abLOS, cAINode *apSkip)
		{
			cNodeData *pNodes = Nodes();
			if (pNodes == NULL)
				return NULL;
			cAINode *pBest = NULL;
			float fBest = 1e30f;
			std::vector<cAINode *> vCandidates;
			for (int i = 0; i < pNodes->mpContainer->GetNodeNum(); ++i)
			{
				cAINode *pNode = pNodes->mpContainer->GetNode(i);
				if (pNode == apSkip)
					continue;
				float fDist = cMath::Vector3Dist(pNode->GetPosition(), avPos);
				if (fDist < afMin || fDist > afMax)
					continue;
				if (abLOS && pNodes->mpContainer->FreePath(avPos + cVector3f(0, 0.5f, 0), pNode->GetPosition() + cVector3f(0, 0.5f, 0), 1, eAIFreePathFlag_SkipDynamic) == false)
					continue;
				vCandidates.push_back(pNode);
				if (fDist < fBest)
				{
					fBest = fDist;
					pBest = pNode;
				}
			}
			if (abClosest || vCandidates.empty())
				return pBest;
			return vCandidates[cMath::RandRectl(0, (int)vCandidates.size() - 1)];
		}
	};

	struct cAgentBarkMachine : cAgentComponent
	{
		struct cState
		{
			tString msSound;
			float mfMin = 0, mfMax = 0;
		};
		std::map<int, cState> mapStates;
		int mlEdit = -1, mlCur = -1;
		float mfCount = 0;

		cAgentBarkMachine(E *p) : cAgentComponent(p, eComp_BarkMachine) {}

		void OnSetActive() override
		{
			mlCur = mapStates.empty() ? -1 : 0;
			mfCount = 0;
		}

		void Update(float afTimeStep) override
		{
			auto it = mapStates.find(mlCur);
			if (mbActive == false || it == mapStates.end() || it->second.msSound == "")
				return;
			mfCount -= afTimeStep;
			if (mfCount > 0)
				return;
			mfCount = cMath::RandRectf(it->second.mfMin, it->second.mfMax);
			if (cSomaLuxMap *pMap = cSomaLuxMap::GetCurrent())
				if (cSoundEntity *pSound = pMap->GetWorld()->CreateSoundEntity(mpEntity->msName + "_Bark", it->second.msSound, true))
					pSound->SetPosition(mpEntity->GetPosition());
		}
	};

	struct cAgentSoundListener : cAgentComponent
	{
		float mfHearRadius = 30, mfRadiusMul = 1, mfMinRadius = 0, mfMaxRadius = 1000, mfIgnoreRadius = 0, mfMaxPlayerInteractTime = 0;
		int mlMinPrio = 0;
		cAgentSoundListener(E *p) : cAgentComponent(p, eComp_SoundListener) {}
	};

	struct cAgentHeadTracker : cAgentComponent
	{
		cSomaLuxEntity *mpTrack = NULL;
		float mfMaxAngle = cMath::ToRad(70);
		cAgentHeadTracker(E *p) : cAgentComponent(p, eComp_HeadTracker) { mbActive = false; }
	};

	struct cAgentForceEmitter : cAgentComponent
	{
		cForceField *mpField;
		iCharacterBody *mpBody = NULL;
		cVector3f mvOffset = 0;
		bool mbAtFoot = false;
		float mfMinSpeed = 0, mfMaxSpeed = 0;
		cAgentForceEmitter(E *p) : cAgentComponent(p, eComp_ForceEmitter)
		{
			mpField = p->mpMap->GetWorld()->CreateForceField(p->msName + "_ForceField", false, false);
			mpField->SetForce(3);
			mpField->SetRadius(1.1f);
			mpField->SetFreq(2.4f);
			mpField->FadeOut(0.001f);
		}
		void OnDestroy() { mpEntity->mpMap->GetWorld()->DestroyForceField(mpField); }
		void SetActive(bool abX)
		{
			mbActive = abX;
			OnSetActive();
		}
		void OnSetActive() { mpField->SetActive(mpEntity->mbActive && mbActive); }
		void Update(float afTimeStep)
		{
			if (mpBody == NULL)
				return;
			if (mfMaxSpeed > 0)
			{
				float fSpeed = mpBody->GetVelocity(afTimeStep).Length();
				if (fSpeed > mfMinSpeed)
					mpField->FadeTo(cMath::Clamp(fSpeed / (mfMaxSpeed - mfMinSpeed), 0.0f, 1.0f), 0.25f);
				else if (fSpeed < mfMinSpeed * 0.66f && !mpField->IsDead())
					mpField->FadeOut(1);
			}
			if (mpField->IsDead())
				return;
			cVector3f vPos = mbAtFoot ? mpBody->GetFeetPosition() : mpBody->GetPosition();
			if (mvOffset != 0)
				vPos += cMath::MatrixMul(cMath::MatrixRotate(cVector3f(mpBody->GetPitch(), mpBody->GetYaw(), 0), eEulerRotationOrder_ZXY), mvOffset);
			mpField->SetPosition(vPos);
		}
	};

	struct cAgentBackboneTail : cAgentComponent
	{
		struct cBone
		{
			cBoneState *mpBone;
			float mfDist;
		};
		struct cTrail
		{
			cQuaternion mqRot;
			float mfDist;
		};
		std::vector<cBone> mvBones;
		std::vector<cTrail> mvTrail = std::vector<cTrail>(1);
		size_t mlCount = 0, mlHead = 0;
		float mfLength = 0, mfTimer = 0, mfInterval = 0.1f, mfStiffness = 1;
		cVector3f mvLastPos = 0;
		cQuaternion mqLast;
		cAgentBackboneTail(E *p) : cAgentComponent(p, eComp_BackboneTail) {}

		cTrail &Get(size_t i) { return mvTrail[(mlHead + mvTrail.size() - 1 - i) % mvTrail.size()]; }
		void SetMaxTrailSize(int alX)
		{
			if (alX <= 0)
				return;
			mvTrail.resize(alX);
			mlCount = mlHead = 0;
		}
		cQuaternion Rotation()
		{
			cMatrixf m = mpEntity->mpMesh->GetWorldMatrix().GetRotation();
			cVector3f vR = m.GetRight(), vU = m.GetUp(), vF = m.GetForward();
			vR.Normalize();
			vU.Normalize();
			vF.Normalize();
			m.SetRight(vR);
			m.SetUp(vU);
			m.SetForward(vF);
			cQuaternion q(m);
			q.Normalize();
			return q;
		}
		void Setup(const tStringVec &avNames)
		{
			mvBones.clear();
			cMeshEntity *pMesh = mpEntity->mpMesh;
			if (pMesh == NULL)
				return;
			for (const tString &sName : avNames)
			{
				cBoneState *pBone = pMesh->GetBoneStateFromName(sName);
				if (pBone == NULL)
					Error("Could not find bone '%s' in '%s' to be used as backbone!\n", sName.c_str(), mpEntity->msName.c_str());
				else
					mvBones.push_back({pBone, 0});
			}
			mvLastPos = pMesh->GetWorldPosition();
			mqLast = Rotation();
			mfLength = 0;
			for (size_t i = 0; i < mvBones.size(); ++i)
			{
				mvBones[i].mpBone->SetUsePreTransform(true);
				if (i > 0)
					mfLength += cMath::Vector3Dist(mvBones[i - 1].mpBone->GetWorldPosition(), mvBones[i].mpBone->GetWorldPosition());
				mvBones[i].mfDist = mfLength;
			}
		}
		void Update(float afTimeStep)
		{
			cMeshEntity *pMesh = mpEntity->mpMesh;
			if (pMesh == NULL || mvBones.empty())
				return;
			cVector3f vPos = pMesh->GetWorldPosition();
			cQuaternion qRot = Rotation();
			if (mlCount > 0)
			{
				float fAngle = 2 * std::acos(std::min(std::fabs(cMath::QuaternionDot(mqLast, qRot)), 1.0f));
				float fAdd = (cMath::Vector3Dist(mvLastPos, vPos) + fAngle / k2Pif * mfLength) * mfStiffness;
				for (size_t i = 0; i < mlCount; ++i)
					Get(i).mfDist += fAdd;
			}
			mvLastPos = vPos;
			mqLast = qRot;

			mfTimer += afTimeStep;
			if (mfTimer >= mfInterval || mlCount == 0)
			{
				mfTimer = 0;
				mvTrail[mlHead] = {qRot, 0};
				mlCount = std::min(mlCount + 1, mvTrail.size());
				mlHead = (mlHead + 1) % mvTrail.size();
			}
			if (pMesh->IsVisible() == false)
				return;

			cQuaternion qInv(qRot.w, -qRot.v.x, -qRot.v.y, -qRot.v.z);
			for (cBone &bone : mvBones)
			{
				int lPrev = -1;
				while (lPrev + 1 < (int)mlCount && Get(lPrev + 1).mfDist < bone.mfDist)
					++lPrev;
				cQuaternion qA = lPrev < 0 ? qRot : Get(lPrev).mqRot;
				float fPrevDist = lPrev < 0 ? 0 : Get(lPrev).mfDist;
				qA = cMath::QuaternionMul(qInv, qA);
				cQuaternion qB = qA;
				if (lPrev + 1 < (int)mlCount)
				{
					cTrail &next = Get(lPrev + 1);
					float fSeg = next.mfDist - fPrevDist;
					if (fSeg >= 1e-5f)
						qB = cMath::QuaternionSlerp((bone.mfDist - fPrevDist) / fSeg, qA, cMath::QuaternionMul(qInv, next.mqRot), true);
				}
				bone.mpBone->SetPreTransform(cMath::MatrixQuaternion(qB));
			}
		}
	};

	struct cGenericComponent : cAgentComponent
	{
		cGenericComponent(E *p, int alType) : cAgentComponent(p, alType) {}
	};

	struct cAgentLightSensor : cAgentComponent
	{
		float mfSensitivity = 1;
		cAgentLightSensor(E *p) : cAgentComponent(p, eComp_LightSensor) {}
	};

	struct cComponentIterator
	{
		std::vector<cAgentComponent *> mvComps;
		size_t mlPos = 0;
		cAgentComponent *Next() { return mlPos < mvComps.size() ? mvComps[mlPos++] : NULL; }
		cAgentComponent *PeekNext() { return mlPos < mvComps.size() ? mvComps[mlPos] : NULL; }
	};

	struct cAgentEdgeGlow : cAgentComponent
	{
		cColor mColor = cColor(0, 0, 1, 1);
		float mfThickness = 0, mfAlpha = 1, mfLightLimit = 1;
		cAgentEdgeGlow(E *p) : cAgentComponent(p, eComp_EdgeGlow) {}
	};

	struct cAgent
	{
		cSomaLuxEntity *mpEnt;
		iCharacterBody *mpBody = NULL;
		cMatrixf mtxMeshOffset = cMatrixf::Identity;
		std::vector<std::unique_ptr<cAgentComponent>> mvComponents;
		cAgentMessageData mMessage;
		tString msMessageCallback;

		bool mbSensesActive = true, mbUpdateDetection = true;
		float mfFOV = cMath::ToRad(90), mfFOVMul = 1, mfSightRange = 30, mfSightRangeMul = 1, mfEyeHeight = 0.9f;
		float mfDetectMinTime = 0, mfDetectCount = 0, mfDetectTimer = 0, mfCurrentSightDist = 0;
		int mlSeenCount = 0;
		bool mbSeen = false, mbDetected = false, mbSightRangeAffectedByModifiers = true;
		cVector3f mvLastKnownPlayerPos = 0;
		bool mbStaticCollider = false, mbCheckForDoors = true, mbAlignGround = false;
		float mfAlignRayStart = 0.5f, mfAlignRayMax = 0.5f, mfAlignTimer = 0, mfAlignY = 0, mvAlignDist[3] = {};
		int mlAlignCount = 0, mlAlignIdx = 0;
		cVector3f mvGroundAlignPos = 0;
		cBoneState *mpPosBone = NULL;
		bool mbPosBoneIsFeet = true, mbGlobalSpace = false, mbGravityBeforeGlobal = true, mbCollisionBeforeGlobal = true;
		float mfPosBoneYOffset = 0;
		float mfMaxDoorDist = 1, mfCheckDoorsCount = 0, mfDoorCheckTimer = 0;
		bool mbAutoDisable = false;
		float mfAutoDisableMinDist = 0, mfAutoDisableTimer = 0, mfAutoDisableCount = 0;
		tString msAutoDisableCallback;

		template <class T> T *Find(int alType)
		{
			for (auto &p : mvComponents)
				if (p->mlType == alType)
					return static_cast<T *>(p.get());
			return NULL;
		}

		cVector3f Eye() { return mpBody ? mpBody->GetFeetPosition() + cVector3f(0, mpBody->GetSize().y * mfEyeHeight, 0) : mpEnt->GetPosition(); }
		float PlayerDist() { return cMath::Vector3Dist(mpBody ? mpBody->GetFeetPosition() : mpEnt->GetPosition(), PlayerBody()->GetFeetPosition()); }
		cVector3f Forward() { return mpBody ? mpBody->GetForward() : cVector3f(0, 0, -1); }

		bool PlayerInSight(float afFOV)
		{
			iCharacterBody *pPlayer = PlayerBody();
			cVector3f vEye = Eye();
			cVector3f vHead = pPlayer->GetPosition() + cVector3f(0, pPlayer->GetSize().y * 0.4f, 0);
			cVector3f vDir = vHead - vEye;
			return cMath::Vector3Angle(cMath::Vector3Normalize(vDir), Forward()) < afFOV * 0.5f && SomaLineOfSight(vEye, vHead, mpEnt);
		}

		void ResetPlayerDetectionState()
		{
			mbDetected = mbSeen = false;
			mfDetectCount = 0;
		}

		void SetSensesActive(bool abX)
		{
			if (mbSensesActive == abX)
				return;
			mbSensesActive = abX;
			if (abX == false)
				SomaAgentSendMessage(mpEnt, eMsg_SensesDeactivated, 0);
			ResetPlayerDetectionState();
		}

		void SetUndetected()
		{
			if (mbDetected)
				SomaAgentSendMessage(mpEnt, eMsg_PlayerUndetected, 0);
			mbDetected = false;
		}

		void DetectedCallback(bool abX)
		{
			if (abX == false || mfDetectMinTime > mfDetectCount)
			{
				if (abX)
					mfDetectCount += 0.15f;
				else if (mfDetectCount > 0)
					mfDetectCount -= 0.15f;
				SetUndetected();
				return;
			}
			mvLastKnownPlayerPos = PlayerBody()->GetFeetPosition();
			if (mbDetected == false)
			{
				mbDetected = true;
				SomaAgentSendMessage(mpEnt, eMsg_PlayerDetected, 0);
			}
		}

		void SeenCallback(bool abX)
		{
			mbSeen = abX && ++mlSeenCount >= 4;
			if (abX == false)
				mlSeenCount = 0;
			if (mbSeen)
				return DetectedCallback(true);
			if (mpBody && mpBody->GetSize().x > PlayerDist())
				return DetectedCallback(PlayerInSight(mfFOV * mfFOVMul));
			if (mfDetectCount > 0)
				mfDetectCount -= 0.15f;
			SetUndetected();
		}

		void UpdateSenses(float afTimeStep)
		{
			cSomaLuxPlayer *pPlayer = cSomaLuxPlayer::Get();
			if (mbSensesActive == false || pPlayer == NULL || pPlayer->GetCharacterBody() == NULL)
				return;
			if (pPlayer->mfHealth <= 0)
			{
				SetUndetected();
				mbSeen = false;
				return;
			}
			mfDetectTimer += afTimeStep;
			if (mfDetectTimer < 0.15f)
				return;
			mfDetectTimer = 0;
			if (mbUpdateDetection == false)
			{
				mbSeen = false;
				SetUndetected();
				return;
			}
			float fRange = mfSightRange * mfSightRangeMul;
			float fMax = -1;
			if (mbSightRangeAffectedByModifiers)
			{
				fRange *= pPlayer->GetVisibilityRangeMul();
				fMax = pPlayer->GetVisibilityMaxRange();
			}
			mfCurrentSightDist = fMax >= 0 ? std::min(fRange, fMax) : fRange;
			SeenCallback(PlayerDist() <= mfCurrentSightDist && PlayerInSight(mfFOV * mfFOVMul));
		}

		void UpdateAutoDisable(float afTimeStep)
		{
			if (mbAutoDisable == false || (mfAutoDisableTimer -= afTimeStep) > 0)
				return;
			mfAutoDisableTimer = 0.3f;
			if (PlayerDist() < mfAutoDisableMinDist)
				return;
			if (SomaEntityInPlayerLOS(mpEnt, true) || cSomaLuxPlayer::Get()->mfHealth <= 0)
			{
				mfAutoDisableCount = 0;
				return;
			}
			if ((mfAutoDisableCount += 0.3f) <= 3)
				return;
			mbAutoDisable = false;
			mpEnt->SetActive(false);
			cSomaLuxMap *pMap = cSomaLuxMap::GetCurrent();
			if (msAutoDisableCallback != "" && pMap && pMap->GetScript())
				cSomaScriptRuntime::Get()->Call(pMap->GetScript(), "void " + msAutoDisableCallback + "(const tString &in)",
												[this](asIScriptContext *c) { c->SetArgObject(0, &mpEnt->msName); });
		}

		void CheckForDoors(float afTimeStep)
		{
			mfCheckDoorsCount -= afTimeStep;
			mfDoorCheckTimer -= afTimeStep;
			if (mbCheckForDoors == false || mpBody == NULL || mfCheckDoorsCount > 0 || mfDoorCheckTimer > 0 || mpEnt->mpMap == NULL)
				return;
			mfDoorCheckTimer = 0.1f;
			if (mpBody->GetMoveSpeed(eCharDir_Forward) < 0.05f)
				return;
			float fMax = mpBody->GetSize().x * 0.5f + mfMaxDoorDist;
			for (cSomaLuxEntity *pDoor : mpEnt->mpMap->GetEntities())
			{
				float fDist;
				if (pDoor->mbIsDoor && pDoor->mbIsClosedDoor && pDoor->mbActive &&
					SomaRayHitsEntity(pDoor, mpBody->GetPosition(), Forward(), fMax, fDist))
				{
					memcpy(mMessage.mBlock + 44, &pDoor->mID, sizeof(cSomaID));
					SomaAgentSendMessage(mpEnt, 18, pDoor->GetPosition());
					return;
				}
			}
		}

		float FloorDist(const cVector3f &avStart, const cVector3f &avEnd)
		{
			struct cFloor : iPhysicsRayCallback
			{
				float mfDist = 1e9f;
				bool OnIntersect(iPhysicsBody *b, cPhysicsRayParams *p) override
				{
					if (b->IsCharacter() == false && b->GetCollideCharacter() && p->mfDist < mfDist)
						mfDist = p->mfDist;
					return true;
				}
			} ray;
			mpEnt->mpMap->GetWorld()->GetPhysicsWorld()->CastRay(&ray, avStart, avEnd, true, false, false);
			return ray.mfDist;
		}

		float GroundDist(float afMax, bool abDynamic, int alRays, float afRadius, bool abShortest)
		{
			struct cGround : iPhysicsRayCallback
			{
				bool mbDynamic;
				float mfDist;
				bool OnIntersect(iPhysicsBody *b, cPhysicsRayParams *p) override
				{
					if (b->IsCharacter() == false && (mbDynamic || b->GetMass() == 0) && p->mfDist < mfDist)
						mfDist = p->mfDist;
					return true;
				}
			};
			if (mpBody == NULL || mpEnt->mpMap == NULL)
				return afMax;
			cVector3f vFeet = mpBody->GetFeetPosition();
			float fResult = abShortest ? afMax : 0;
			int lRays = std::max(alRays, 1);
			for (int i = 0; i < lRays; ++i)
			{
				float fAngle = k2Pif * i / lRays;
				cVector3f vStart = vFeet + (i == 0 ? cVector3f(0) : cVector3f(std::cos(fAngle), 0, std::sin(fAngle)) * afRadius);
				cGround ray;
				ray.mbDynamic = abDynamic;
				ray.mfDist = afMax;
				mpEnt->mpMap->GetWorld()->GetPhysicsWorld()->CastRay(&ray, vStart, vStart - cVector3f(0, afMax, 0), true, false, false);
				fResult = abShortest ? std::min(fResult, ray.mfDist) : fResult + ray.mfDist / lRays;
			}
			return fResult;
		}

		// curbs without risers never trigger the body's side-collision step climb
		void StepUp()
		{
			if (mpBody == NULL || mbGlobalSpace || mbStaticCollider || mpBody->GetTestCollision() == false || mpEnt->mpMap == NULL)
				return;
			cVector3f vFeet = mpBody->GetFeetPosition();
			float fStep = mpBody->GetMaxStepSize();
			float fDist = FloorDist(vFeet + cVector3f(0, fStep, 0), vFeet);
			if (fDist < fStep - 0.01f)
				mpBody->SetFeetPosition(vFeet + cVector3f(0, fStep - fDist, 0));
		}

		void AlignWithGround(float afTimeStep)
		{
			if (mbAlignGround == false || mpBody == NULL || mpEnt->mpMap == NULL || mpBody->IsOnGround() == false || (mfAlignTimer -= afTimeStep) > 0)
				return;
			mfAlignTimer = 0.02f;
			cVector3f vAhead = cMath::MatrixMul(cMath::MatrixRotateY(mpBody->GetYaw()), cVector3f(0, 0, -1)) * mpBody->GetSize().x * mfAlignRayStart;
			cVector3f vStart = mpBody->GetFeetPosition() + vAhead + cVector3f(0, 0.05f, 0);
			float fDist = FloorDist(vStart, vStart - cVector3f(0, mfAlignRayMax, 0));
			if (fDist > mfAlignRayMax)
				return;
			mvAlignDist[mlAlignIdx] = fDist - 0.05f;
			mlAlignIdx = (mlAlignIdx + 1) % 3;
			mlAlignCount = std::min(mlAlignCount + 1, 3);
			mfAlignY = 0;
			for (int i = 0; i < mlAlignCount; ++i)
				mfAlignY += mvAlignDist[i] / mlAlignCount;
			mvGroundAlignPos = mpBody->GetFeetPosition() + vAhead - cVector3f(0, mfAlignY, 0);
		}

		void SyncMesh()
		{
			if (mpBody == NULL || mpEnt->mpMesh == NULL)
				return;
			if (mbGlobalSpace != mpEnt->mbGlobalSpaceAnim)
			{
				mbGlobalSpace = mpEnt->mbGlobalSpaceAnim;
				if (mbGlobalSpace)
					mbGravityBeforeGlobal = mpBody->GravityIsActive(), mbCollisionBeforeGlobal = mpBody->GetTestCollision();
				mpBody->SetGravityActive(mbGlobalSpace == false && mbGravityBeforeGlobal);
				mpBody->SetTestCollision(mbGlobalSpace == false && mbCollisionBeforeGlobal);
				if (mbGlobalSpace)
					mpBody->SetYaw(0);
			}
			if (mbGlobalSpace)
			{
				if (mpPosBone == NULL)
					return;
				cVector3f vPos = mpPosBone->GetWorldPosition() + cVector3f(0, mfPosBoneYOffset * mpEnt->mvScale.y, 0);
				if (mbPosBoneIsFeet)
					mpBody->SetFeetPosition(vPos);
				else
					mpBody->SetPosition(vPos);
				return;
			}
			cMatrixf mtx = cMath::MatrixMul(cMath::MatrixRotateY(mpBody->GetYaw() + kPif), cMath::MatrixRotateX(-mpBody->GetPitch() * mpBody->GetEntityPitchAmount()));
			mtx.SetTranslation(mpBody->GetFeetPosition());
			cMatrixf mtxOffset = mtxMeshOffset;
			mtxOffset.m[1][3] -= mfAlignY;
			mpEnt->mpMesh->SetMatrix(cMath::MatrixMul(mtx, mtxOffset));
		}
	};

	std::unordered_map<cSomaLuxEntity *, std::unique_ptr<cAgent>> gmapAgents;

	cAgent *Agent(E *apEnt)
	{
		auto it = gmapAgents.find(apEnt);
		return it == gmapAgents.end() ? NULL : it->second.get();
	}

	// cLuxOutlineEffect::RenderEdgeGlow: additive, after translucents
	struct cEdgeGlowRenderer : iRendererCallback
	{
		struct cGlow { cSomaID mID; float mfAlpha, mfY; };
		cViewport *mpViewport = NULL;
		iGpuProgram *mpProgram = NULL;
		iGpuProgram *mpOutline = NULL;
		std::vector<cGlow> mvGlow;

		void Register()
		{
			cViewport *pViewport = gpSomaBase->GetCurrentViewport();
			if (pViewport == NULL || pViewport == mpViewport)
				return;
			pViewport->AddRendererCallback(this);
			mpViewport = pViewport;
		}

		void OnPostSolidDraw(cRendererCallbackFunctions *) override {}
		void OnPostTranslucentDraw(cRendererCallbackFunctions *f) override
		{
			cFrustum *pFrustum = f->GetFrustum();
			if (mpViewport == NULL || mpViewport->GetCamera() == NULL || pFrustum != mpViewport->GetCamera()->GetFrustum())
				return;
			if (static bool bTried = false; bTried == false)
			{
				bTried = true;
				cParserVarContainer vars;
				vars.Add("UseUv");
				vars.Add("UseNormals");
				vars.Add("UseVertexPosition");
				mpProgram = gpSomaBase->mpEngine->GetGraphics()->CreateGpuProgramFromShaders("SomaEdgeGlow", "deferred_base_vtx.glsl", "game_edge_glow.glsl", &vars);
				if (mpProgram)
				{
					mpProgram->GetVariableAsId("aColor", 0);
					mpProgram->GetVariableAsId("afAlpha", 1);
					mpProgram->GetVariableAsId("afLightLevel", 2);
					mpProgram->GetVariableAsId("afLightLimit", 3);
					mpProgram->GetVariableAsId("afEdgeThickness", 4);
				}
			}
			// cLuxEffectHandler "OutlineProgram"
			if (static bool bTriedOutline = false; bTriedOutline == false && mvGlow.empty() == false)
			{
				bTriedOutline = true;
				cParserVarContainer vars;
				vars.Add("UseUv");
				vars.Add("UseNormals");
				mpOutline = gpSomaBase->mpEngine->GetGraphics()->CreateGpuProgramFromShaders("SomaOutline", "deferred_base_vtx.glsl", "game_object_outline.glsl", &vars);
				if (mpOutline)
				{
					mpOutline->GetVariableAsId("avOutlineColor", 0);
					mpOutline->GetVariableAsId("avScreenSize", 1);
					mpOutline->GetVariableAsId("afEffectY", 2);
					mpOutline->GetVariableAsId("avObjectScreenMinAndSize", 3);
				}
			}
			iLowLevelGraphics *pLowLevel = gpSomaBase->mpEngine->GetGraphics()->GetLowLevel();
			bool bStarted = false;
			auto Begin = [&](iGpuProgram *apProgram) {
				if (bStarted == false)
				{
					bStarted = true;
					f->SetDepthTestFunc(eDepthTestFunc_LessOrEqual);
					f->SetDepthTest(true);
					f->SetDepthWrite(false);
					f->SetBlendMode(eMaterialBlendMode_Add);
					f->SetAlphaMode(eMaterialAlphaMode_Solid);
					f->SetChannelMode(eMaterialChannelMode_RGBA);
					f->SetNormalFrustumProjection();
					pLowLevel->SetPolygonOffsetActive(true);
					pLowLevel->SetPolygonOffset(-5, -2);
					f->SetTextureRange(NULL, 0);
					f->SetCullActive(true);
					f->SetCullMode(eCullMode_CounterClockwise);
				}
				f->SetProgram(apProgram);
			};
			auto DrawSub = [&](cSubMeshEntity *s) {
				cMaterial *pMat = s->GetMaterial();
				iTexture *pDiffuse = pMat ? pMat->GetTexture(eMaterialTexture_Diffuse) : NULL;
				if (pDiffuse == NULL || s->IsVisible() == false || pFrustum->CollideBoundingVolume(s->GetBoundingVolume()) == eCollision_Outside)
					return false;
				f->SetTexture(0, pDiffuse);
				f->SetMatrix(s->GetModelMatrixPtr());
				f->SetVertexBuffer(s->GetVertexBuffer());
				return true;
			};
			for (auto it = gmapAgents.begin(); mpProgram && it != gmapAgents.end(); ++it)
			{
				E *p = it->first;
				cAgentEdgeGlow *g = it->second->Find<cAgentEdgeGlow>(eComp_EdgeGlow);
				if (g == NULL || g->mbActive == false || g->mfAlpha <= 0.001f || g->mfLightLimit <= 0.001f || p->mpMesh == NULL || p->mbActive == false ||
					p->mpMesh->IsVisible() == false || pFrustum->CollideBoundingVolume(p->mpMesh->GetBoundingVolume()) == eCollision_Outside)
					continue;
				// ponytail: one light sample per entity, the original samples each submesh BV
				float fLevel = SomaLightLevelAtPos(p->mpMesh->GetBoundingVolume()->GetWorldCenter(), NULL, 0);
				if (fLevel >= g->mfLightLimit)
					continue;
				Begin(mpProgram);
				mpProgram->SetVec4f(0, g->mColor.r, g->mColor.g, g->mColor.b, g->mColor.a);
				mpProgram->SetFloat(1, g->mfAlpha);
				mpProgram->SetFloat(2, fLevel);
				mpProgram->SetFloat(3, g->mfLightLimit);
				mpProgram->SetFloat(4, g->mfThickness);
				for (int i = 0; i < p->mpMesh->GetSubMeshEntityNum(); ++i)
					if (DrawSub(p->mpMesh->GetSubMeshEntity(i)))
						f->DrawCurrent();
			}
			cSomaLuxMap *pMap = cSomaLuxMap::GetCurrent();
			const cVector2l &vScreen = pLowLevel->GetScreenSizeInt();
			float fTanHalfFov = tan(pFrustum->GetFOV() * 0.5f);
			for (cGlow &glow : mvGlow)
			{
				E *p = pMap && mpOutline ? pMap->GetEntity(glow.mID) : NULL;
				if (p == NULL || p->mpMesh == NULL || p->mbActive == false || p->mpMesh->IsVisible() == false)
					continue;
				Begin(mpOutline);
				mpOutline->SetVec4f(0, 0.2f, 0.2f, 1, glow.mfAlpha);
				mpOutline->SetVec2f(1, (float)vScreen.x, (float)vScreen.y);
				mpOutline->SetFloat(2, glow.mfY);
				for (int i = 0; i < p->mpMesh->GetSubMeshEntityNum(); ++i)
				{
					cSubMeshEntity *s = p->mpMesh->GetSubMeshEntity(i);
					if (DrawSub(s) == false)
						continue;
					cRect2l r;
					cMath::GetClipRectFromBV(r, *s->GetBoundingVolume(), pFrustum, vScreen, fTanHalfFov);
					mpOutline->SetVec2f(3, (float)(vScreen.y - (r.y + r.h)), (float)r.h);
					f->DrawCurrent();
				}
			}
			mvGlow.clear();
			if (bStarted == false)
				return;
			pLowLevel->SetPolygonOffsetActive(false);
			f->SetProgram(NULL);
			f->SetTexture(0, NULL);
			f->SetDepthWrite(true);
			f->SetBlendMode(eMaterialBlendMode_None);
		}
	} gEdgeGlowRenderer;

	cAgentCharMover *cAgentPathfinder::Mover()
	{
		cAgent *pAgent = Agent(mpEntity);
		return pAgent ? pAgent->Find<cAgentCharMover>(eComp_CharMover) : NULL;
	}

	cVector3f cAgentPathfinder::Feet()
	{
		cAgent *pAgent = Agent(mpEntity);
		if (pAgent == NULL || pAgent->mpBody == NULL)
			return mpEntity->GetPosition();
		return mbAtCenter ? pAgent->mpBody->GetPosition() : pAgent->mpBody->GetFeetPosition();
	}

	// cLuxPathfinder::UpdateMoving: node inside the enlarged body box, or no progress for 150 frames near it
	bool cAgentPathfinder::Arrived(const cVector3f &avNode, bool abLast)
	{
		cAgent *pAgent = Agent(mpEntity);
		if (pAgent == NULL || pAgent->mpBody == NULL)
		{
			cVector3f vDelta = avNode - Feet();
			float fHeight = std::fabs(vDelta.y);
			vDelta.y = 0;
			return vDelta.Length() < (abLast ? (mbExact ? 0.15f : 0.4f) : 0.6f) && fHeight < 2.0f;
		}
		cVector3f vSize = pAgent->mpBody->GetSize(), vPos = pAgent->mpBody->GetPosition();
		float fDistSqr = cMath::Vector3DistSqr(vPos, avNode);
		mvDistHistory.push_back(fDistSqr);
		if (mvDistHistory.size() > 150)
		{
			mvDistHistory.pop_front();
			float fGrowth = 0;
			for (float f : mvDistHistory)
				fGrowth += f - mvDistHistory.front();
			if (fGrowth > 0 && fDistSqr < vSize.y * 1.5f)
				return true;
		}
		cVector3f vD = avNode - vPos + cVector3f(0, 0.1f * vSize.y, 0);
		return std::fabs(vD.x) <= vSize.x * 0.65f && std::fabs(vD.y) <= vSize.y * 0.6f + 0.225f && std::fabs(vD.z) <= vSize.z * 0.65f;
	}

	cAgent *AgentOrNew(E *apEnt)
	{
		std::unique_ptr<cAgent> &p = gmapAgents[apEnt];
		if (p == NULL)
		{
			p.reset(new cAgent());
			p->mpEnt = apEnt;
		}
		return p.get();
	}

	template <class T> T *AddComponent(E *apEnt, T *apComp)
	{
		AgentOrNew(apEnt)->mvComponents.emplace_back(apComp);
		return apComp;
	}

	cVector3f PlayerFeet()
	{
		iCharacterBody *p = PlayerBody();
		return p ? p->GetFeetPosition() : cVector3f(0);
	}

	cVector3f AgentPos(E *p)
	{
		cAgent *pAgent = Agent(p);
		return pAgent && pAgent->mpBody ? pAgent->mpBody->GetFeetPosition() : p->GetPosition();
	}
}

void SomaCreateAgent(cSomaLuxEntity *apEnt)
{
	cSomaLuxMap *pMap = apEnt->mpMap;
	if (pMap == NULL || (Agent(apEnt) && Agent(apEnt)->mpBody))
		return;
	cAgent *pAgent = AgentOrNew(apEnt);

	cResourceVarsObject &v = apEnt->mVars;
	cVector3f vSize = v.GetVarVector3f("CharBodySize", cVector3f(0.9f, 1.9f, 0.9f)) * apEnt->mvScale;
	pAgent->mpBody = pMap->GetWorld()->GetPhysicsWorld()->CreateCharacterBody(apEnt->msName, vSize);
	iCharacterBody *pBody = pAgent->mpBody;
	pBody->SetUserData(apEnt);
	pBody->SetMass(80);
	pBody->SetCustomGravity(cVector3f(0, -12, 0));
	pBody->SetCustomGravityActive(true);
	pBody->SetGravityActive(true);
	pBody->SetMaxPositiveMoveSpeed(eCharDir_Forward, 1);
	pBody->SetMaxNegativeMoveSpeed(eCharDir_Forward, -1);

	cMatrixf mtx = apEnt->m_mtxOnLoad;
	pBody->SetFeetPosition(mtx.GetTranslation());
	pBody->SetYaw(std::atan2(mtx.m[0][2], mtx.m[2][2]) - kPif);

	cVector3f vRot = v.GetVarVector3f("MeshRotationOffset", 0);
	cMatrixf mtxOffset = cMath::MatrixRotate(cVector3f(cMath::ToRad(vRot.x), cMath::ToRad(vRot.y), cMath::ToRad(vRot.z)), eEulerRotationOrder_XYZ);
	mtxOffset = cMath::MatrixMul(mtxOffset, cMath::MatrixScale(v.GetVarVector3f("MeshScaleOffset", 1)));
	mtxOffset.SetTranslation(v.GetVarVector3f("MeshPositionOffset", 0));
	pAgent->mtxMeshOffset = cMath::MatrixMul(cMath::MatrixScale(apEnt->mvScale), mtxOffset);
	if (apEnt->mpMesh && apEnt->mpMesh->GetBoneStateNum() > 0)
	{
		tString sBone = v.GetVarString("CharBodyPosBone", "");
		pAgent->mpPosBone = sBone.empty() ? apEnt->mpMesh->GetBoneState(0) : apEnt->mpMesh->GetBoneStateFromName(sBone);
	}
	pAgent->mbPosBoneIsFeet = v.GetVarBool("CharBodyBoneIsFeet", true);
	pAgent->mfPosBoneYOffset = v.GetVarFloat("CharBodyBoneYOffset", 0);
	pAgent->mbSensesActive = apEnt->mInstanceVars.GetVarBool("SensesActive", true);
	pAgent->mbStaticCollider = apEnt->mInstanceVars.GetVarBool("StaticCollider", false);
	pBody->SetGravityActive(pAgent->mbStaticCollider == false);
	pBody->SetTestCollision(pAgent->mbStaticCollider == false);
	pBody->SetActive(apEnt->mbActive);
	pAgent->SyncMesh();
}

void SomaDestroyAgent(cSomaLuxEntity *apEnt)
{
	cAgent *pAgent = Agent(apEnt);
	if (pAgent == NULL)
		return;
	if (apEnt->mpMap)
	{
		for (auto &pComp : pAgent->mvComponents)
			pComp->OnDestroy();
		if (pAgent->mpBody)
			apEnt->mpMap->GetWorld()->GetPhysicsWorld()->DestroyCharacterBody(pAgent->mpBody);
	}
	gmapAgents.erase(apEnt);
	if (gmapAgents.empty())
		gmapContainers.clear();
}

void SomaUpdateComponents(cSomaLuxEntity *apEnt, float afTimeStep)
{
	cAgent *pAgent = Agent(apEnt);
	if (pAgent == NULL)
		return;
	for (size_t i = 0; i < pAgent->mvComponents.size(); ++i)
		if (pAgent->mvComponents[i]->mbActive)
			pAgent->mvComponents[i]->Update(afTimeStep);
}

void SomaAgentSetActive(cSomaLuxEntity *apEnt, bool abX)
{
	cAgent *pAgent = Agent(apEnt);
	if (pAgent == NULL)
		return;
	if (pAgent->mpBody)
		pAgent->mpBody->SetActive(abX);
	for (auto &pComp : pAgent->mvComponents)
		pComp->OnSetActive();
}

iCharacterBody *SomaAgentGetBody(cSomaLuxEntity *apEnt)
{
	cAgent *pAgent = Agent(apEnt);
	return pAgent ? pAgent->mpBody : NULL;
}

bool SomaAgentGetMatrix(cSomaLuxEntity *apEnt, cMatrixf &aMtx)
{
	cAgent *pAgent = Agent(apEnt);
	if (pAgent == NULL || pAgent->mpBody == NULL)
		return false;
	aMtx = cMatrixf::Identity;
	aMtx.SetTranslation(pAgent->mpBody->GetPosition());
	return true;
}

bool SomaAgentSetMatrix(cSomaLuxEntity *apEnt, const cMatrixf &aMtx)
{
	cAgent *pAgent = Agent(apEnt);
	if (pAgent == NULL || pAgent->mpBody == NULL)
		return false;
	pAgent->mpBody->SetPosition(aMtx.GetTranslation());
	pAgent->SyncMesh();
	return true;
}

int SomaAgentGetState(cSomaLuxEntity *apEnt)
{
	cAgent *pAgent = Agent(apEnt);
	cAgentStateMachine *pSM = pAgent ? pAgent->Find<cAgentStateMachine>(eComp_StateMachine) : NULL;
	return pSM ? pSM->mlCur : -1;
}

void SomaAgentSaveExtra(cSomaLuxEntity *apEnt, float &afYaw, bool &abSenses, bool &abDetection)
{
	cAgent *pAgent = Agent(apEnt);
	afYaw = pAgent->mpBody->GetYaw();
	abSenses = pAgent->mbSensesActive;
	abDetection = pAgent->mbUpdateDetection;
}

void SomaAgentLoadExtra(cSomaLuxEntity *apEnt, float afYaw, bool abSenses, bool abDetection)
{
	cAgent *pAgent = Agent(apEnt);
	pAgent->mpBody->SetYaw(afYaw);
	pAgent->mbSensesActive = abSenses;
	pAgent->mbUpdateDetection = abDetection;
	pAgent->SyncMesh();
}

// cLuxPathfinder::Scriptable_SaveToBuffer / SetupAfterLoadingSave
std::string SomaAgentSavePath(cSomaLuxEntity *apEnt)
{
	cAgent *pAgent = Agent(apEnt);
	cAgentPathfinder *pPF = pAgent ? pAgent->Find<cAgentPathfinder>(eComp_Pathfinder) : NULL;
	std::string s;
	if (pPF == NULL)
		return s;
	auto pod = [&](const auto &x) { s.append((const char *)&x, sizeof(x)); };
	auto str = [&](const tString &x) { pod((uint32_t)x.size()); s += x; };
	pod(pPF->mbMoving), pod(pPF->mvGoal), pod(pPF->mbExact), str(pPF->msResultCallback), pod(pPF->mbCallbackInMap), str(pPF->msEndOfPathCallback);
	pod((uint32_t)pPF->mvTrack.size());
	for (const cAgentTrackNode &n : pPF->mvTrack)
		str(n.msNode), pod(n.mfMinWait), pod(n.mfMaxWait), str(n.msAnim), pod(n.mbLoopAnim);
	pod(pPF->mlTrackIdx), pod(pPF->mbTrackActive), pod(pPF->mbTrackPaused), pod(pPF->mbTrackLoop), pod(pPF->mbAtTrackNode);
	pod(pPF->mfTrackWait), pod(pPF->mfTrackFreq), str(pPF->msTrackCallback);
	return s;
}

void SomaAgentLoadPath(cSomaLuxEntity *apEnt, const std::string &asData)
{
	cAgent *pAgent = Agent(apEnt);
	cAgentPathfinder *pPF = pAgent ? pAgent->Find<cAgentPathfinder>(eComp_Pathfinder) : NULL;
	if (pPF == NULL || asData.empty())
		return;
	size_t p = 0;
	auto pod = [&](auto &x) {
		if (p + sizeof(x) <= asData.size())
			memcpy(&x, asData.data() + p, sizeof(x));
		p += sizeof(x);
	};
	auto str = [&](tString &x) {
		uint32_t n = 0;
		pod(n);
		x = p + n <= asData.size() ? asData.substr(p, n) : "";
		p += n;
	};
	bool bMoving = false, bExact = false, bInMap = false;
	cVector3f vGoal = 0;
	tString sResult;
	pod(bMoving), pod(vGoal), pod(bExact), str(sResult), pod(bInMap), str(pPF->msEndOfPathCallback);
	uint32_t n = 0;
	pod(n);
	pPF->mvTrack.assign(std::min<uint32_t>(n, 4096), cAgentTrackNode{});
	for (cAgentTrackNode &t : pPF->mvTrack)
		str(t.msNode), pod(t.mfMinWait), pod(t.mfMaxWait), str(t.msAnim), pod(t.mbLoopAnim);
	pod(pPF->mlTrackIdx), pod(pPF->mbTrackActive), pod(pPF->mbTrackPaused), pod(pPF->mbTrackLoop), pod(pPF->mbAtTrackNode);
	pod(pPF->mfTrackWait), pod(pPF->mfTrackFreq), str(pPF->msTrackCallback);
	if (bMoving)
		pPF->MoveTo(vGoal, bExact, sResult, bInMap);
	else
		pPF->Stop();
}

void SomaAgentChangeState(cSomaLuxEntity *apEnt, int alState)
{
	cAgent *pAgent = Agent(apEnt);
	cAgentStateMachine *pSM = pAgent ? pAgent->Find<cAgentStateMachine>(eComp_StateMachine) : NULL;
	if (pSM && alState >= 0)
		pSM->ChangeState(alState);
}

void SomaAgentSendMessage(cSomaLuxEntity *apEnt, int alMessage, const cVector3f &avX, int alX)
{
	cAgent *pAgent = Agent(apEnt);
	if (pAgent == NULL)
		return;
	if (alMessage != 18)
		memset(pAgent->mMessage.mBlock + 44, 0, sizeof(cSomaID));
	pAgent->mMessage.Set(avX, alX);
	void *pData = pAgent->mMessage.mBlock;
	apEnt->Call("void OnRecieveMessage(int, cLuxEntityMessageData@)", [&](asIScriptContext *c) {
		c->SetArgDWord(0, alMessage);
		c->SetArgAddress(1, pData);
	});
	for (size_t i = 0; i < pAgent->mvComponents.size(); ++i)
		pAgent->mvComponents[i]->OnMessage(alMessage);
}

tString SomaNavPath(const cVector3f &avFrom, const cVector3f &avTo)
{
	cSomaLuxMap *pMap = cSomaLuxMap::GetCurrent();
	if (pMap && std::none_of(gmapContainers.begin(), gmapContainers.end(), [&](auto &it) { return it.first.first == pMap->GetWorld(); }))
	{
		tWString sBase = cString::GetFileNameW(cString::SetFileExtW(pMap->GetWorld()->GetFilePath(), _W(""))) + _W("_");
		tWStringList lstFiles;
		cPlatform::FindFilesInDir(lstFiles, cString::GetFilePathW(pMap->GetWorld()->GetFilePath()), sBase + _W("*.nodes"));
		for (const tWString &sFile : lstFiles)
			GetContainer(pMap, cString::To8Char(cString::SetFileExtW(sFile, _W("")).substr(sBase.size())), cVector3f(0.6f, 1.5f, 0.6f), 1.0f);
	}
	for (auto &it : gmapContainers)
	{
		tAINodeList lst;
		if (pMap == NULL || it.first.first != pMap->GetWorld() || it.second.mpAStar->GetPath(avFrom, avTo, &lst) == false)
			continue;
		tString s;
		for (auto n = lst.rbegin(); n != lst.rend(); ++n)
			s += cString::ToString((*n)->GetPosition().x) + " " + cString::ToString((*n)->GetPosition().y) + " " + cString::ToString((*n)->GetPosition().z) + "\n";
		return s;
	}
	return "";
}

tString SomaAgentDebug(cSomaLuxEntity *apEnt)
{
	cAgent *pAgent = Agent(apEnt);
	if (pAgent == NULL)
		return "";
	tString s;
	if (cAgentStateMachine *pSM = pAgent->Find<cAgentStateMachine>(eComp_StateMachine))
		s += "state=" + pSM->Name(pSM->mlCur) + " prev=" + pSM->Name(pSM->mlPrev) + " next=" + pSM->Name(pSM->mlNext) + " timers=" + cString::ToString((int)pSM->mvTimers[0].size());
	if (cAgentPathfinder *pPF = pAgent->Find<cAgentPathfinder>(eComp_Pathfinder))
		s += " pf_moving=" + cString::ToString(pPF->mbMoving) + " path=" + cString::ToString((int)pPF->mlPathIdx) + "/" + cString::ToString((int)pPF->mvPath.size()) +
			 " goal=" + pPF->mvGoal.ToString() + " track=" + cString::ToString(pPF->mlTrackIdx) + "/" + cString::ToString((int)pPF->mvTrack.size()) +
			 " track_active=" + cString::ToString(pPF->mbTrackActive) + " paused=" + cString::ToString(pPF->mbTrackPaused) + " at_node=" + cString::ToString(pPF->mbAtTrackNode) +
			 " wait=" + cString::ToString(pPF->mfTrackWait);
	if (cAgentCharMover *pM = pAgent->Find<cAgentCharMover>(eComp_CharMover))
		s += " cm_moving=" + cString::ToString(pM->mbMoving) + " cm_goal=" + pM->mvGoal.ToString() + " speed_state=" + cString::ToString(pM->mlSpeedState) +
			 " fwd=" + cString::ToString(pM->ForwardSpeed()) + " stuck=" + cString::ToString(pM->mfStuck) + " turning=" + cString::ToString(pM->mbTurning);
	return s;
}

void SomaUpdateAgent(cSomaLuxEntity *apEnt, float afTimeStep)
{
	cAgent *pAgent = Agent(apEnt);
	if (pAgent == NULL || apEnt->mbActive == false)
		return;
	pAgent->UpdateSenses(afTimeStep);
	pAgent->CheckForDoors(afTimeStep);
	pAgent->UpdateAutoDisable(afTimeStep);
	if (apEnt->mbActive == false)
		return;
	SomaUpdateComponents(apEnt, afTimeStep);
	if (cAgentPathfinder *pPF = pAgent->Find<cAgentPathfinder>(eComp_Pathfinder))
		pPF->SendGoal();
	pAgent->StepUp();
	pAgent->AlignWithGround(afTimeStep);
	pAgent->SyncMesh();
}

void SomaBroadcastSoundHeard(const cVector3f &avPos, float afRadius, int alPrio)
{
	for (auto &it : gmapAgents)
	{
		cAgent *pAgent = it.second.get();
		cAgentSoundListener *pListener = pAgent->Find<cAgentSoundListener>(eComp_SoundListener);
		if (pListener == NULL || pListener->mbActive == false || pAgent->mpEnt->mbActive == false || alPrio < pListener->mlMinPrio)
			continue;
		float fRadius = cMath::Clamp(afRadius * pListener->mfRadiusMul, pListener->mfMinRadius, pListener->mfMaxRadius);
		float fDist = cMath::Vector3Dist(AgentPos(pAgent->mpEnt), avPos);
		if (fDist < fRadius && fDist < pListener->mfHearRadius + fRadius && fDist >= pListener->mfIgnoreRadius)
			SomaAgentSendMessage(pAgent->mpEnt, eMsg_SoundHeard, avPos, alPrio);
	}
}

void SomaRegisterAgentNatives(asIScriptEngine *e)
{
	typedef const tString &S;
	typedef const cVector3f &V;

	SOMA_FUNC(e, "uint64 H64(const tString&in asStr)", +[](S s) -> asQWORD { return Hash64(s); });

	const char *A = "cLuxAgent";
	SOMA_METHOD(e, A, "iCharacterBody@ GetCharBody()", +[](E *p) { cAgent *a = Agent(p); return a ? a->mpBody : (iCharacterBody *)NULL; });
	SOMA_METHOD(e, A, "float GetDistanceToGround(float afMaxTestDistance, bool abCheckDynamic, int alNumOfRays=1, float afRadius=0.25, bool abGetShortest=true)", +[](E *p, float d, bool b, int n, float r, bool s) {
		cAgent *a = Agent(p);
		return a ? a->GroundDist(d, b, n, r, s) : d;
	});
	SOMA_METHOD(e, A, "void GetDistanceToGround(const tString&in asCallbackFunc, float afMaxTestDistance, bool abCheckDynamic, int alNumOfRays=1, float afRadius=0.25, bool abGetClosest=true)", +[](E *p, const tString &f, float d, bool b, int n, float r, bool s) {
		cAgent *a = Agent(p);
		float fDist = a ? a->GroundDist(d, b, n, r, s) : d;
		p->Call("void " + f + "(float)", [fDist](asIScriptContext *c) { c->SetArgFloat(0, fDist); });
	});
	SOMA_METHOD(e, A, "float GetDistanceToPlayer()", +[](E *p) { return cMath::Vector3Dist(AgentPos(p), PlayerFeet()); });
	SOMA_METHOD(e, A, "float GetDistanceToPlayer2D()", +[](E *p) { cVector3f d = AgentPos(p) - PlayerFeet(); d.y = 0; return d.Length(); });
	SOMA_METHOD(e, A, "float GetDistanceToPos(const cVector3f&in avPos)", +[](E *p, V v) { return cMath::Vector3Dist(AgentPos(p), v); });
	SOMA_METHOD(e, A, "float GetDistanceToPos2D(const cVector3f&in avPos)", +[](E *p, V v) { cVector3f d = AgentPos(p) - v; d.y = 0; return d.Length(); });
	SOMA_METHOD(e, A, "const cVector3f& GetPlayerPos()", +[](E *) -> const cVector3f & { static cVector3f v; iCharacterBody *b = PlayerBody(); v = b ? b->GetPosition() : cVector3f(0); return v; });
	SOMA_METHOD(e, A, "cVector3f GetPlayerFeetPos()", +[](E *) { return PlayerFeet(); });
	SOMA_METHOD(e, A, "cVector3f GetPlayerHeadPos()", +[](E *) { iCharacterBody *b = PlayerBody(); return b ? b->GetPosition() + cVector3f(0, b->GetSize().y * 0.4f, 0) : cVector3f(0); });
	SOMA_METHOD(e, A, "cVector3f GetEyePostion()", +[](E *p) { cAgent *a = Agent(p); return a ? a->Eye() : p->GetPosition(); });
	SOMA_METHOD(e, A, "void SetRelativeEyeHeight(float afX)", +[](E *p, float x) { if (cAgent *a = Agent(p)) a->mfEyeHeight = x; });
	SOMA_METHOD(e, A, "float GetRelativeEyeHeight()", +[](E *p) { cAgent *a = Agent(p); return a ? a->mfEyeHeight : 0.0f; });
	SOMA_METHOD(e, A, "bool GetPointIsInFOV(const cVector3f&in avPoint, float afFOV, const cVector3f &in avForward)", +[](E *p, V pt, float fov, V fwd) {
		cAgent *a = Agent(p);
		cVector3f vDir = cMath::Vector3Normalize(pt - (a ? a->Eye() : p->GetPosition()));
		return cMath::Vector3Angle(vDir, cMath::Vector3Normalize(fwd)) < fov * 0.5f;
	});
	SOMA_METHOD(e, A, "bool GetPlayerIsInFOV(float afFOV, const cVector3f &in avForward)", +[](E *p, float fov, V fwd) {
		cVector3f vDir = cMath::Vector3Normalize(PlayerFeet() - AgentPos(p));
		return cMath::Vector3Angle(vDir, cMath::Vector3Normalize(fwd)) < fov * 0.5f;
	});
	SOMA_METHOD(e, A, "bool GetPlayerIsInLineOfSight()", +[](E *p) {
		cAgent *a = Agent(p);
		iCharacterBody *b = PlayerBody();
		return a && b && SomaLineOfSight(a->Eye(), b->GetPosition(), p);
	});
	SOMA_METHOD(e, A, "bool GetPlayerIsInLineOfSight(float afFOV, const cVector3f &in avForward, bool abCheckFOV)", +[](E *p, float fov, V fwd, bool bFov) {
		cAgent *a = Agent(p);
		iCharacterBody *b = PlayerBody();
		if (a == NULL || b == NULL)
			return false;
		if (bFov && cMath::Vector3Angle(cMath::Vector3Normalize(b->GetPosition() - a->Eye()), cMath::Vector3Normalize(fwd)) > fov * 0.5f)
			return false;
		return SomaLineOfSight(a->Eye(), b->GetPosition(), p);
	});
	SOMA_METHOD(e, A, "float GetAngleToPos2D(const cVector3f&in avPos)", +[](E *p, V v) {
		cAgent *a = Agent(p);
		cVector3f d = v - AgentPos(p);
		d.y = 0;
		return a && a->mpBody ? std::fabs(Wrap(YawTo(d) - a->mpBody->GetYaw())) : 0.0f;
	});
	SOMA_METHOD(e, A, "float GetAngleToPlayer2D()", +[](E *p) {
		cAgent *a = Agent(p);
		cVector3f d = PlayerFeet() - AgentPos(p);
		d.y = 0;
		return a && a->mpBody ? std::fabs(Wrap(YawTo(d) - a->mpBody->GetYaw())) : 0.0f;
	});
	SOMA_METHOD(e, A, "void SetSensesActive(bool abX)", +[](E *p, bool b) { if (cAgent *a = Agent(p)) a->SetSensesActive(b); });
	SOMA_METHOD(e, A, "void ResetPlayerDetectionState()", +[](E *p) { if (cAgent *a = Agent(p)) a->ResetPlayerDetectionState(); });
	SOMA_METHOD(e, A, "bool GetSensesActive()", +[](E *p) { cAgent *a = Agent(p); return a && a->mbSensesActive; });
	SOMA_METHOD(e, A, "void SetPlayerDetectedMinTime(float afX)", +[](E *p, float x) { if (cAgent *a = Agent(p)) a->mfDetectMinTime = x; });
	SOMA_METHOD(e, A, "float GetPlayerDetectedCount()", +[](E *p) { cAgent *a = Agent(p); return a ? a->mfDetectCount : 0.0f; });
	SOMA_METHOD(e, A, "void SetUpdatePlayerDetection(bool abX)", +[](E *p, bool b) { if (cAgent *a = Agent(p)) a->mbUpdateDetection = b; });
	SOMA_METHOD(e, A, "bool SetUpdatePlayerDetection()", +[](E *p) { cAgent *a = Agent(p); return a && a->mbUpdateDetection; });
	SOMA_METHOD(e, A, "bool PlayerIsSeen()", +[](E *p) { cAgent *a = Agent(p); return a && a->mbSeen; });
	SOMA_METHOD(e, A, "bool PlayerIsDetected()", +[](E *p) { cAgent *a = Agent(p); return a && a->mbDetected; });
	SOMA_METHOD(e, A, "void SetFOV(float afX)", +[](E *p, float x) { if (cAgent *a = Agent(p)) a->mfFOV = x; });
	SOMA_METHOD(e, A, "float GetFOV()", +[](E *p) { cAgent *a = Agent(p); return a ? a->mfFOV : 0.0f; });
	SOMA_METHOD(e, A, "void SetFOVMul(float afX)", +[](E *p, float x) { if (cAgent *a = Agent(p)) a->mfFOVMul = x; });
	SOMA_METHOD(e, A, "float GetFOVMul()", +[](E *p) { cAgent *a = Agent(p); return a ? a->mfFOVMul : 1.0f; });
	SOMA_METHOD(e, A, "void SetSightRange(float afX)", +[](E *p, float x) { if (cAgent *a = Agent(p)) a->mfSightRange = x; });
	SOMA_METHOD(e, A, "float GetSightRange()", +[](E *p) { cAgent *a = Agent(p); return a ? a->mfSightRange : 0.0f; });
	SOMA_METHOD(e, A, "void SetSightRangeMul(float afX)", +[](E *p, float x) { if (cAgent *a = Agent(p)) a->mfSightRangeMul = x; });
	SOMA_METHOD(e, A, "float GetSightRangeMul()", +[](E *p) { cAgent *a = Agent(p); return a ? a->mfSightRangeMul : 1.0f; });
	SOMA_METHOD(e, A, "const cVector3f& GetLastKnownPlayerPos()", +[](E *p) -> const cVector3f & { static cVector3f z(0); cAgent *a = Agent(p); return a ? a->mvLastKnownPlayerPos : z; });
	SOMA_METHOD(e, A, "float GetDistFromLastKnownToActualPlayerPos()", +[](E *p) { cAgent *a = Agent(p); return a ? cMath::Vector3Dist(a->mvLastKnownPlayerPos, PlayerFeet()) : 0.0f; });
	SOMA_METHOD(e, A, "void RevealPlayerPos()", +[](E *p) {
		if (cAgent *a = Agent(p))
		{
			a->mvLastKnownPlayerPos = PlayerFeet();
			a->mbSeen = a->mbDetected = true;
			a->mfDetectCount = cMath::Max(a->mfDetectCount, a->mfDetectMinTime);
		}
	});
	SOMA_METHOD(e, A, "float GetCurrentPlayerSightDistance()", +[](E *p) { cAgent *a = Agent(p); return a ? a->mfCurrentSightDist : 0.0f; });
	SOMA_METHOD(e, A, "void SetSightRangeAffectedByModifiers(bool abX)", +[](E *p, bool b) { if (cAgent *a = Agent(p)) a->mbSightRangeAffectedByModifiers = b; });
	SOMA_METHOD(e, A, "bool GetSightRangeAffectedByModifiers()", +[](E *p) { cAgent *a = Agent(p); return a && a->mbSightRangeAffectedByModifiers; });
	for (const char *pType : {"iLuxEntity", "cLuxProp", "cLuxArea", "cLuxAgent", "cLuxCritter", "cLuxLiquidArea"})
	{
		SOMA_METHOD(e, pType, "bool CheckIsOnScreen(bool abUseRayCast)", +[](E *p, bool b) { return SomaEntityIsOnScreen(p, b); });
		SOMA_METHOD_NEW(e, pType, "cParticleSystem@ CreateParticleSystem(const tString&in asName, const tString&in asFile, bool abRemoveWhenDone, bool abAttach)",
						+[](E *p, S n, S f, bool bRemove, bool bAttach) -> cParticleSystem * {
							if (f == "" || p->mpMap == NULL)
								return NULL;
							cParticleSystem *pPS = p->mpMap->GetWorld()->CreateParticleSystem(n, f, 1, bRemove);
							if (pPS && p->mpMesh && bAttach)
								p->mpMesh->AddChild(pPS);
							else if (pPS)
								pPS->SetPosition(p->mpMesh ? p->mpMesh->GetWorldPosition() : p->GetPosition());
							return pPS;
						});
	}
	for (const char *pType : {"cLuxAgent", "cLuxCritter"})
	{
		SOMA_METHOD(e, pType, "bool GetEntityIsInPlayerFOV()", +[](E *p) { return SomaEntityIsOnScreen(p, false); });
		SOMA_METHOD(e, pType, "bool GetEntityIsInPlayerLineOfSight(bool abCheckFOV)", +[](E *p, bool b) { return SomaEntityInPlayerLOS(p, b); });
		SOMA_METHOD(e, pType, "void GetEntityIsInPlayerLineOfSight(const tString&in asCallbackFunc, bool abCheckFOV)", +[](E *p, const tString &f, bool b) {
			bool bLOS = SomaEntityInPlayerLOS(p, b);
			p->Call("void " + f + "(bool)", [bLOS](asIScriptContext *c) { c->SetArgByte(0, bLOS); });
		});
	}
	SOMA_FUNC(e, "bool Entity_IsInPlayerFOV(const tString &in asEntity)", +[](S n) {
		cSomaLuxEntity *p = cSomaLuxMap::GetCurrent() ? cSomaLuxMap::GetCurrent()->GetEntity(n) : NULL;
		return SomaEntityIsOnScreen(p, false);
	});
	SOMA_METHOD(e, A, "void SetCheckForDoorsCount(float afX)", +[](E *p, float x) { if (cAgent *a = Agent(p)) a->mfCheckDoorsCount = x; });
	for (const char *pType : {"iLuxEntity", "cLuxProp", "cLuxArea", "cLuxAgent", "cLuxCritter", "cLuxLiquidArea"})
	{
		SOMA_METHOD(e, pType, "void SetIsDoor(bool abX)", +[](E *p, bool b) { p->mbIsDoor = b; });
		SOMA_METHOD(e, pType, "bool GetIsDoor()", +[](E *p) { return p->mbIsDoor; });
		SOMA_METHOD(e, pType, "void SetIsClosedDoor(bool abX)", +[](E *p, bool b) { p->mbIsClosedDoor = b; });
		SOMA_METHOD(e, pType, "bool GetIsClosedDoor()", +[](E *p) { return p->mbIsClosedDoor; });
	}
	SOMA_METHOD(e, A, "void SetStaticCollider(bool abX)", +[](E *p, bool b) {
		cAgent *a = Agent(p);
		if (a == NULL)
			return;
		a->mbStaticCollider = b;
		if (a->mpBody)
		{
			a->mpBody->SetGravityActive(b == false);
			a->mpBody->SetTestCollision(b == false);
		}
	});
	SOMA_METHOD(e, A, "bool GetStaticCollider()", +[](E *p) { cAgent *a = Agent(p); return a && a->mbStaticCollider; });
	SOMA_METHOD(e, A, "void SetAutoDisableWhenOutOfSightActive(bool abX, float afMinDist)", +[](E *p, bool b, float d) {
		if (cAgent *a = Agent(p))
			a->mbAutoDisable = b, a->mfAutoDisableMinDist = d;
	});
	SOMA_METHOD(e, A, "void SetAutoDisableCallback(const tString&in asCallback)", +[](E *p, S f) { if (cAgent *a = Agent(p)) a->msAutoDisableCallback = f; });
	SOMA_METHOD(e, A, "void SetCheckForDoors(bool abX)", +[](E *p, bool b) { if (cAgent *a = Agent(p)) a->mbCheckForDoors = b; });
	SOMA_METHOD(e, A, "bool GetCheckForDoors()", +[](E *p) { cAgent *a = Agent(p); return a && a->mbCheckForDoors; });
	SOMA_METHOD(e, A, "void SetMaxCheckDoorDistance(float afX)", +[](E *p, float x) { if (cAgent *a = Agent(p)) a->mfMaxDoorDist = x; });
	SOMA_METHOD(e, A, "float GetMaxCheckDoorDistance()", +[](E *p) { cAgent *a = Agent(p); return a ? a->mfMaxDoorDist : 0.0f; });
	SOMA_METHOD(e, A, "void SetAlignEntityWithGroundRay(bool abX)", +[](E *p, bool b) { if (cAgent *a = Agent(p)) { a->mbAlignGround = b; a->mlAlignCount = a->mlAlignIdx = 0; } });
	SOMA_METHOD(e, A, "bool GetAlignEntityWithGroundRay()", +[](E *p) { cAgent *a = Agent(p); return a && a->mbAlignGround; });
	SOMA_METHOD(e, A, "const cVector3f& GetGroundAlignPosition()", +[](E *p) -> const cVector3f & { static cVector3f vZero = 0; cAgent *a = Agent(p); return a ? a->mvGroundAlignPos : vZero; });
	SOMA_METHOD(e, A, "void SetAlignEntityWithGroundRelativeRayStart(float afX)", +[](E *p, float f) { if (cAgent *a = Agent(p)) a->mfAlignRayStart = f; });
	SOMA_METHOD(e, A, "float GetAlignEntityWithGroundRelativeRayStart()", +[](E *p) { cAgent *a = Agent(p); return a ? a->mfAlignRayStart : 0.5f; });
	SOMA_METHOD(e, A, "void SetAlignEntityWithGroundMaxRayDistance(float afX)", +[](E *p, float f) { if (cAgent *a = Agent(p)) a->mfAlignRayMax = f; });
	SOMA_METHOD(e, A, "float GetAlignEntityWithGroundMaxRayDistance()", +[](E *p) { cAgent *a = Agent(p); return a ? a->mfAlignRayMax : 0.5f; });
	SOMA_METHOD(e, A, "void BroadcastMessage(int alMessageId, iLuxEntityComponent@ apSource, const cVector3f &in avData, int alData)",
				+[](E *p, int m, void *, V v, int l) { SomaAgentSendMessage(p, m, v, l); });
	SOMA_METHOD(e, A, "void SetRecieveMessageCallback(const tString&in asCallbackFunc)", +[](E *p, S f) { if (cAgent *a = Agent(p)) a->msMessageCallback = f; });

	SOMA_FUNC(e, "cLuxStateMachine@ cLux_CreateEntityComponent_StateMachine(iLuxEntity @apEntity)", +[](E *p) { return AddComponent(p, new cAgentStateMachine(p)); });
	SOMA_FUNC(e, "cLuxCharMover@ cLux_CreateEntityComponent_CharMover(iLuxEntity @apEntity, iCharacterBody @apCharBody)",
			  +[](E *p, iCharacterBody *b) { return AddComponent(p, new cAgentCharMover(p, b)); });
	SOMA_FUNC(e, "cLuxPathfinder@ cLux_CreateEntityComponent_Pathfinder(iLuxEntity @apEntity)", +[](E *p) { return AddComponent(p, new cAgentPathfinder(p)); });
	SOMA_FUNC(e, "cLuxBarkMachine@ cLux_CreateEntityComponent_BarkMachine(iLuxEntity @apEntity)", +[](E *p) { return AddComponent(p, new cAgentBarkMachine(p)); });
	SOMA_FUNC(e, "cLuxSoundListener@ cLux_CreateEntityComponent_SoundListener(iLuxEntity @apEntity)", +[](E *p) { return AddComponent(p, new cAgentSoundListener(p)); });
	SOMA_FUNC(e, "cLuxHeadTracker@ cLux_CreateEntityComponent_HeadTracker(iLuxEntity @apEntity)", +[](E *p) { return AddComponent(p, new cAgentHeadTracker(p)); });

	SOMA_METHOD(e, "cLuxEffectHandler", "void AddGlowObject(iLuxEntity@ apEntity, float afAlpha, float afY)", +[](void *, E *p, float a, float y) {
		if (p == NULL)
			return;
		gEdgeGlowRenderer.Register();
		for (auto &g : gEdgeGlowRenderer.mvGlow)
			if (g.mID == p->mID)
			{
				g.mfAlpha = a;
				g.mfY = y;
				return;
			}
		gEdgeGlowRenderer.mvGlow.push_back({p->mID, a, y});
	});
	SOMA_FUNC(e, "cLuxEdgeGlow@ cLux_CreateEntityComponent_EdgeGlow(iLuxEntity @apEntity)", +[](E *p) {
		gEdgeGlowRenderer.Register();
		return AddComponent(p, new cAgentEdgeGlow(p));
	});
	SOMA_FUNC(e, "cLuxForceEmitter@ cLux_CreateEntityComponent_ForceEmitter(iLuxEntity @apEntity)", +[](E *p) { return AddComponent(p, new cAgentForceEmitter(p)); });
	SOMA_FUNC(e, "cLuxLightSensor@ cLux_CreateEntityComponent_LightSensor(iLuxEntity @apEntity)", +[](E *p) { return AddComponent(p, new cAgentLightSensor(p)); });
	SOMA_METHOD(e, "cLuxLightSensor", "void LoadFromInstanceVariables(cResourceVarsObject@ apInstanceVars)", +[](cAgentLightSensor *, void *) {});
	SOMA_METHOD(e, "cLuxLightSensor", "void SetSensitivityLevel(float afX)", +[](cAgentLightSensor *c, float x) { c->mfSensitivity = x; });
	SOMA_METHOD(e, "cLuxLightSensor", "float GetSensitivityLevel()", +[](cAgentLightSensor *c) { return c->mfSensitivity; });
	SOMA_METHOD(e, "cLuxLightSensor", "bool IsSensoring()", +[](cAgentLightSensor *c) {
		cAgent *a = Agent(c->mpEntity);
		return c->mpEntity->mbActive && (a == NULL || a->mbSensesActive);
	});
	SOMA_METHOD(e, "cLuxMap", "cLuxEntityComponentIterator@ GetEntityComponentIterator(eLuxEntityComponentType aType)", +[](cSomaLuxMap *m, int t) {
		static cComponentIterator vPool[16];
		static size_t lNext = 0;
		cComponentIterator *pIt = &vPool[lNext++ % 16];
		pIt->mvComps.clear();
		pIt->mlPos = 0;
		for (auto &it : gmapAgents)
			if (it.first->mpMap == m)
				for (auto &c : it.second->mvComponents)
					if (c->mlType == t)
						pIt->mvComps.push_back(c.get());
		return pIt;
	});
	SOMA_METHOD(e, "cLuxEntityComponentIterator", "bool HasNext()", +[](cComponentIterator *p) { return p->mlPos < p->mvComps.size(); });
	SOMA_METHOD(e, "cLuxEntityComponentIterator", "iLuxEntityComponent@ Next()", +[](cComponentIterator *p) { return p->Next(); });
	SOMA_METHOD(e, "cLuxEntityComponentIterator", "iLuxEntityComponent@ PeekNext()", +[](cComponentIterator *p) { return p->PeekNext(); });
	SOMA_FUNC(e, "cLuxBackboneTail@ cLux_CreateEntityComponent_BackboneTail(iLuxEntity @apEntity)", +[](E *p) { return AddComponent(p, new cAgentBackboneTail(p)); });
	for (const char *pType : {"iLuxEntityComponent", "cLuxStateMachine", "cLuxCharMover", "cLuxPathfinder", "cLuxBarkMachine", "cLuxSoundListener", "cLuxHeadTracker",
							  "cLuxEdgeGlow", "cLuxForceEmitter", "cLuxLightSensor", "cLuxBackboneTail"})
	{
		SOMA_METHOD(e, pType, "iLuxEntity@ GetEntity()", +[](cAgentComponent *c) { return c->mpEntity; });
		SOMA_METHOD(e, pType, "eLuxEntityComponentType GetType()", +[](cAgentComponent *c) { return c->mlType; });
	}
	SOMA_METHOD(e, "cLuxMap", "iLuxEntityComponent@ GetEntityComponent(eLuxEntityComponentType aType, const tString&in asName)", +[](cSomaLuxMap *m, int t, S n) -> cAgentComponent * {
		cSomaLuxEntity *pEnt = m->GetEntity(n);
		cAgent *a = pEnt ? Agent(pEnt) : NULL;
		return a ? a->Find<cAgentComponent>(t) : NULL;
	});

	typedef cAgentEdgeGlow EG;
	SOMA_METHOD(e, "cLuxEdgeGlow", "void SetColor(const cColor&in aColor)", +[](EG *g, const cColor &c) { g->mColor = c; });
	SOMA_METHOD(e, "cLuxEdgeGlow", "cColor GetColor()", +[](EG *g) { return g->mColor; });
	SOMA_METHOD(e, "cLuxEdgeGlow", "void SetAlpha(float afX)", +[](EG *g, float x) { g->mfAlpha = x; });
	SOMA_METHOD(e, "cLuxEdgeGlow", "float GetAlpha()", +[](EG *g) { return g->mfAlpha; });
	SOMA_METHOD(e, "cLuxEdgeGlow", "void SetEdgeThickness(float afX)", +[](EG *g, float x) { g->mfThickness = x; });
	SOMA_METHOD(e, "cLuxEdgeGlow", "float GetEdgeThickness()", +[](EG *g) { return g->mfThickness; });
	SOMA_METHOD(e, "cLuxEdgeGlow", "void SetLightLimit(float afX)", +[](EG *g, float x) { g->mfLightLimit = x; });
	SOMA_METHOD(e, "cLuxEdgeGlow", "float GetLightLimit()", +[](EG *g) { return g->mfLightLimit; });
	SOMA_METHOD(e, "cLuxEdgeGlow", "void SetActive(bool abX)", +[](EG *g, bool b) { g->mbActive = b; });
	SOMA_METHOD(e, "cLuxEdgeGlow", "bool IsActive()", +[](EG *g) { return g->mbActive; });

	const char *T = "cLuxForceEmitter";
	typedef cAgentForceEmitter FE;
	SOMA_METHOD(e, T, "void SetActive(bool abX)", +[](FE *f, bool b) { f->SetActive(b); });
	SOMA_METHOD(e, T, "bool IsActive()", +[](FE *f) { return f->mbActive; });
	SOMA_METHOD(e, T, "void SetCharacterBody(iCharacterBody @apCharBody, const cVector3f &in avOffset, bool abAtFoot)",
				+[](FE *f, iCharacterBody *b, const cVector3f &o, bool a) { f->mpBody = b; f->mvOffset = o; f->mbAtFoot = a; });
	SOMA_METHOD(e, T, "void SetRadius(float afX)", +[](FE *f, float x) { f->mpField->SetRadius(x); });
	SOMA_METHOD(e, T, "void SetForce(float afX)", +[](FE *f, float x) { f->mpField->SetForce(x); });
	SOMA_METHOD(e, T, "void SetFreq(float afX)", +[](FE *f, float x) { f->mpField->SetFreq(x); });
	SOMA_METHOD(e, T, "void FadeIn(float afTime)", +[](FE *f, float x) { f->mpField->FadeIn(x); });
	SOMA_METHOD(e, T, "void FadeOut(float afTime)", +[](FE *f, float x) { f->mpField->FadeOut(x); });
	SOMA_METHOD(e, T, "void SetMinForceSpeed(float afX)", +[](FE *f, float x) { f->mfMinSpeed = x; });
	SOMA_METHOD(e, T, "void SetMaxForceSpeed(float afX)", +[](FE *f, float x) { f->mfMaxSpeed = x; });

	T = "cLuxBackboneTail";
	typedef cAgentBackboneTail BT;
	SOMA_METHOD(e, T, "void Setup(array<tString> &in avBoneNames)", +[](BT *b, const CScriptArray &a) {
		tStringVec vNames;
		for (asUINT i = 0; i < a.GetSize(); ++i)
			vNames.push_back(*(const tString *)a.At(i));
		b->Setup(vNames);
	});
	SOMA_METHOD(e, T, "void LoadFromVariables(cResourceVarsObject@ apVars)", +[](BT *b, cResourceVarsObject *v) {
		b->mfInterval = 1 / (v->GetVarFloat("Backbone_AddTrailFreq", 10) + 1e-5f);
		b->mfStiffness = v->GetVarFloat("Backbone_Stiffness", 1);
		b->SetMaxTrailSize(v->GetVarInt("Backbone_MaxTrailSize", 13));
		tStringVec vNames;
		cString::GetStringVec(v->GetVarString("Backbone_Bones", ""), vNames);
		b->Setup(vNames);
	});
	SOMA_METHOD(e, T, "void SetStiffness(float afX)", +[](BT *b, float x) { b->mfStiffness = x; });
	SOMA_METHOD(e, T, "void SetMaxTrailSize(int alX)", +[](BT *b, int x) { b->SetMaxTrailSize(x); });
	SOMA_METHOD(e, T, "void SetAddTrailFreq(float afX)", +[](BT *b, float x) { b->mfInterval = 1 / x; });

	T = "cLuxStateMachine";
	typedef cAgentStateMachine SM;
	SOMA_METHOD(e, T, "void AddState(const tString&in asName, int alId)", +[](SM *s, S n, int id) { s->mapStates[id] = n; if (s->mapStates.size() == 1) s->mlNext = 0; });
	SOMA_METHOD(e, T, "void AddSubState(const tString&in asName, int alId)", +[](SM *s, S n, int id) { s->mapSubStates[id] = n; });
	SOMA_METHOD(e, T, "void ChangeState(int alState)", +[](SM *s, int id) { s->ChangeState(id); });
	SOMA_METHOD(e, T, "void ChangeSubState(int alState)", +[](SM *s, int id) { s->ChangeSubState(id); });
	SOMA_METHOD(e, T, "int GetNextState()", +[](SM *s) { return s->mlNext; });
	SOMA_METHOD(e, T, "int GetPrevState()", +[](SM *s) { return s->mlPrev; });
	SOMA_METHOD(e, T, "int GetCurrentState()", +[](SM *s) { return s->mlCur; });
	SOMA_METHOD(e, T, "int GetNextSubState()", +[](SM *s) { return s->mlSubNext; });
	SOMA_METHOD(e, T, "int GetPrevSubState()", +[](SM *s) { return s->mlSubPrev; });
	SOMA_METHOD(e, T, "int GetCurrentSubState()", +[](SM *s) { return s->mlSubCur; });
	SOMA_METHOD(e, T, "void AddTimer(uint64 alId, float afTime)", +[](SM *s, asQWORD id, float t) { s->AddTimer(id, t); });
	SOMA_METHOD(e, T, "void StopTimer(uint64 alId)", +[](SM *s, asQWORD id) { s->StopTimer(id); });
	SOMA_METHOD(e, T, "bool TimerExists(uint64 alId)", +[](SM *s, asQWORD id) { return s->TimerExists(id); });
	SOMA_METHOD(e, T, "void AddTimer(const tString& in asId, float afTime)", +[](SM *s, S id, float t) { s->AddTimer(Hash64(id), t); });
	SOMA_METHOD(e, T, "void StopTimer(const tString& in asId)", +[](SM *s, S id) { s->StopTimer(Hash64(id)); });
	SOMA_METHOD(e, T, "bool TimerExists(const tString& in asId)", +[](SM *s, S id) { return s->TimerExists(Hash64(id)); });
	SOMA_METHOD(e, T, "cLuxEntityMessageData@ GetCurrentMessageData()", +[](SM *s) -> void * { cAgent *a = Agent(s->mpEntity); return a ? a->mMessage.mBlock : NULL; });

	T = "cLuxCharMover";
	typedef cAgentCharMover CM;
	SOMA_METHOD(e, T, "iCharacterBody@ GetCharBody()", +[](CM *m) { return m->mpBody; });
	SOMA_METHOD(e, T, "void LoadFromVariables(cResourceVarsObject@ apVars)", +[](CM *m, cResourceVarsObject *v) { m->LoadFromVariables(v); });
	SOMA_METHOD(e, T, "void MoveToPos(const cVector3f&in avFeetPos, bool abSlowDownAndStopAtGoal=false)", +[](CM *m, V v, bool b) { m->MoveToPos(v, b); });
	SOMA_METHOD(e, T, "void TurnToPos(const cVector3f&in avFeetPos)", +[](CM *m, V v) { cVector3f d = v - m->mpBody->GetFeetPosition(); d.y = 0; m->TurnTo(YawTo(d)); });
	SOMA_METHOD(e, T, "void TurnToAngle(float afAngle)", +[](CM *m, float a) { m->TurnTo(a); });
	SOMA_METHOD(e, T, "void TurnToAngles(float afYaw, float afPitch)", +[](CM *m, float a, float) { m->TurnTo(a); });
	SOMA_METHOD(e, T, "void TurnInstantlyToPos(const cVector3f&in avGoalPos)", +[](CM *m, V v) { cVector3f d = v - m->mpBody->GetFeetPosition(); d.y = 0; SetYawNear(m->mpBody, YawTo(d)); m->mbTurning = false; });
	SOMA_METHOD(e, T, "void TurnInstantlyToAngle(float afAngle)", +[](CM *m, float a) { SetYawNear(m->mpBody, a); m->mbTurning = false; });
	SOMA_METHOD(e, T, "void TurnInstantlyToAngle(float afYaw, float afPitch)", +[](CM *m, float a, float) { SetYawNear(m->mpBody, a); m->mbTurning = false; });
	SOMA_METHOD(e, T, "void StopTurning()", +[](CM *m) { m->mbTurning = false; });
	SOMA_METHOD(e, T, "int PlayAnimation(const tString&in asName, float afFadeTime=0.3f, bool abLoop=false, bool abPlayTransition=true, const tString&in asCallback=\"\")",
				+[](CM *m, S n, float f, bool l, bool, S cb) { return m->PlayAnimation(n, f, l, cb); });
	SOMA_METHOD(e, T, "void SetUseMoveStateAnimations(bool abX)", +[](CM *m, bool b) { m->SetUseMoveStateAnims(b); });
	SOMA_METHOD(e, T, "bool GetUseMoveStateAnimations()", +[](CM *m) { return m->mbUseMoveStateAnims; });
	SOMA_METHOD(e, T, "void SetTurnedToGoalCallbackFunc(const tString &in asFunc)", +[](CM *m, S f) { m->msTurnedCallback = f; });
	SOMA_METHOD(e, T, "float GetMoveSpeed()", +[](CM *m) { return m->mpBody ? m->mpBody->GetMoveSpeed(eCharDir_Forward) : 0.0f; });
	SOMA_METHOD(e, T, "float GetWantedSpeedAmount()", +[](CM *m) { return m->mbMoving ? 1.0f : 0.0f; });
	SOMA_METHOD(e, T, "float GetStuckCounter()", +[](CM *m) { return m->mfStuck; });
	SOMA_METHOD(e, T, "float GetMaxStuckCounter()", +[](CM *) { return CM::kMaxStuck; });
	SOMA_METHOD(e, T, "void ResetStuckCounter()", +[](CM *m) { m->mfStuck = 0; });
	SOMA_METHOD(e, T, "void SetMaxForwardSpeed(float afX)", +[](CM *m, float x) { m->mfMaxForward = x; });
	SOMA_METHOD(e, T, "void SetMaxBackwardSpeed(float afX)", +[](CM *m, float x) { m->mfMaxBackward = x; });
	SOMA_METHOD(e, T, "void SetTurnMinBreakAngle(float afX)", +[](CM *m, float x) { m->mfTurnMinBreakAngle = x; });
	SOMA_METHOD(e, T, "void SetTurnBreakMul(float afX)", +[](CM *m, float x) { m->mfTurnBreakMul = x; });
	SOMA_METHOD(e, T, "void SetTurnSpeedMul(float afX)", +[](CM *m, float x) { m->mfTurnSpeedMul = x; });
	SOMA_METHOD(e, T, "void SetTurnMaxSpeed(float afX)", +[](CM *m, float x) { m->mfTurnMaxSpeed = x; });
	SOMA_METHOD(e, T, "void SetStoppedToWalkSpeed(float afX)", +[](CM *m, float x) { m->mfStoppedToWalk = x; });
	SOMA_METHOD(e, T, "void SetWalkToRunSpeed(float afX)", +[](CM *m, float x) { m->mfWalkToRun = x; });
	SOMA_METHOD(e, T, "void SetWalkToStoppedSpeed(float afX)", +[](CM *m, float x) { m->mfWalkToStopped = x; });
	SOMA_METHOD(e, T, "void SetRunToWalkSpeed(float afX)", +[](CM *m, float x) { m->mfRunToWalk = x; });
	SOMA_METHOD(e, T, "void SetMoveSpeedAnimMul(float afX)", +[](CM *m, float x) { m->mfMoveSpeedAnimMul = x; });
	SOMA_METHOD(e, T, "void SetIdleAnimName(const tString&in asName)", +[](CM *m, S n) { m->msIdleAnim = n; m->mlAnimState = -1; });
	SOMA_METHOD(e, T, "void SetWalkAnimName(const tString&in asName)", +[](CM *m, S n) { m->msWalkAnim = n; m->mlAnimState = -1; });
	SOMA_METHOD(e, T, "void SetRunAnimName(const tString&in asName)", +[](CM *m, S n) { m->msRunAnim = n; m->mlAnimState = -1; });
	SOMA_METHOD(e, T, "void SetBackwardAnimName(const tString&in asName)", +[](CM *m, S n) { m->msBackwardAnim = n; });
	SOMA_METHOD(e, T, "void AddSpeedState(int alId)", +[](CM *m, int id) { m->mapSpeedStates[id]; m->mlEditState = id; });
	SOMA_METHOD(e, T, "void SetSpeedState(int alId)", +[](CM *m, int id) { m->mlSpeedState = id; });
	SOMA_METHOD(e, T, "void SetSpeedState_Forward(float afX)", +[](CM *m, float x) { if (cAgentSpeedState *s = m->Edit()) s->mfForward = x; });
	SOMA_METHOD(e, T, "void SetSpeedState_Backward(float afX)", +[](CM *m, float x) { if (cAgentSpeedState *s = m->Edit()) s->mfBackward = x; });
	SOMA_METHOD(e, T, "void SetSpeedState_Sideways(float afX)", +[](CM *m, float x) { if (cAgentSpeedState *s = m->Edit()) s->mfSideways = x; });
	SOMA_METHOD(e, T, "void SetSpeedState_TurnBreakMul(float afX)", +[](CM *m, float x) { if (cAgentSpeedState *s = m->Edit()) s->mfTurnBreakMul = x; });
	SOMA_METHOD(e, T, "void SetSpeedState_TurnSpeedMul(float afX)", +[](CM *m, float x) { if (cAgentSpeedState *s = m->Edit()) s->mfTurnSpeedMul = x; });
	SOMA_METHOD(e, T, "void SetSpeedState_TurnMaxSpeed(float afX)", +[](CM *m, float x) { if (cAgentSpeedState *s = m->Edit()) s->mfTurnMaxSpeed = x; });
	SOMA_METHOD(e, T, "void SetSpeedState_ForwardAcc(float afX)", +[](CM *m, float x) { if (cAgentSpeedState *s = m->Edit()) s->mfForwardAcc = x; });
	SOMA_METHOD(e, T, "void SetSpeedState_ForwardDeacc(float afX)", +[](CM *m, float x) { if (cAgentSpeedState *s = m->Edit()) s->mfForwardDeacc = x; });
	SOMA_METHOD(e, T, "void SetSpeedState_SidewayAcc(float afX)", +[](CM *m, float x) { if (cAgentSpeedState *s = m->Edit()) s->mfSidewayAcc = x; });
	SOMA_METHOD(e, T, "void SetSpeedState_SidewayDeacc(float afX)", +[](CM *m, float x) { if (cAgentSpeedState *s = m->Edit()) s->mfSidewayDeacc = x; });
	for (const char *pNoop : {"void SetWallAvoidanceActive(bool abX)", "void SetDynamicObjectAvoidanceActive(bool abX)", "void SetBankingActive(bool abX)",
							  "void SetIdleExtraAnimActive(bool abX)"})
		SOMA_METHOD(e, T, pNoop, +[](CM *, bool) {});
	SOMA_METHOD(e, T, "void SetUse3DMovement(bool abX)", +[](CM *m, bool b) { m->mb3D = b; });
	for (const char *pNoop : {"void SetTurnStoppedToWalkSpeed(float afX)", "void SetTurnWalkToStoppedSpeed(float afX)", "void SetVerticalMoveSpeedExtraAnimMul(float afX)",
							  "void SetBankingAngleMul(float afX)", "void SetBankingMaxAngle(float afX)", "void SetBankingSpeedMul(float afX)", "void SetBankingMaxSpeed(float afX)"})
		SOMA_METHOD(e, T, pNoop, +[](CM *, float) {});
	SOMA_METHOD(e, T, "void SetIdleExtraAnimName(const tString&in asName)", +[](CM *, S) {});
	SOMA_METHOD(e, T, "void SetupWallAvoidance(float afRadius, float afSteerAmount, int alSamples)", +[](CM *, float, float, int) {});
	SOMA_METHOD(e, T, "void SetupDynamicObjectAvoidance(float afMaxDistance, float afMinMass, float afSteerAmount)", +[](CM *, float, float, float) {});
	SOMA_METHOD(e, T, "void SetupIdleExtra(const tString&in asAnimName, float afMinWait, float afMaxWait, bool abPauseProceduralAnims)", +[](CM *, S, float, float, bool) {});
	SOMA_METHOD(e, T, "bool GetIdleExtraAnimActive()", +[](CM *) { return false; });

	T = "cLuxPathfinder";
	typedef cAgentPathfinder PF;
	SOMA_METHOD(e, T, "void MoveTo(const cVector3f&in avPos, float afUpdateFreq, bool abExactStopAtEnd, const tString&in asResultCallback=\"\", bool abCallbackInMap=false)",
				+[](PF *p, V v, float, bool x, S cb, bool m) { p->MoveTo(v, x, cb, m); });
	SOMA_METHOD(e, T, "void MoveToNode(const tString&in asNodeName, float afUpdateFreq, bool abExactStopAtEnd, const tString&in asResultCallback=\"\", bool abCallbackInMap=false)",
				+[](PF *p, S n, float, bool x, S cb, bool m) {
					cNodeData *pNodes = p->Nodes();
					cAINode *pNode = pNodes ? pNodes->mpContainer->GetNodeFromName(n) : NULL;
					if (pNode)
						p->MoveTo(pNode->GetPosition(), x, cb, m);
					else if (cSomaLuxEntity *pEnt = cSomaLuxMap::GetCurrent() ? cSomaLuxMap::GetCurrent()->GetEntity(n) : NULL)
						p->MoveTo(pEnt->GetPosition(), x, cb, m);
				});
	SOMA_METHOD(e, T, "void Stop()", +[](PF *p) { p->Stop(); });
	SOMA_METHOD(e, T, "void SetEndOfPathCallbackFunc(const tString &in asCallbackFunc)", +[](PF *p, S f) { p->msEndOfPathCallback = f; });
	SOMA_METHOD(e, T, "bool IsMoving()", +[](PF *p) { return p->mbMoving; });
	SOMA_METHOD(e, T, "cVector3f GetNextGoalPos()", +[](PF *p) { return p->mvPath.empty() ? p->Feet() : p->mvPath[std::min(p->mlPathIdx, p->mvPath.size() - 1)]; });
	SOMA_METHOD(e, T, "cAINode@ GetNodeFromName(const tString&in asName)", +[](PF *p, S n) { cNodeData *d = p->Nodes(); return d ? d->mpContainer->GetNodeFromName(n) : (cAINode *)NULL; });
	SOMA_METHOD(e, T, "cAINodeContainer@ GetNodeContainer()", +[](PF *p) { cNodeData *d = p->Nodes(); return d ? d->mpContainer.get() : (cAINodeContainer *)NULL; });
	// ponytail: NOCOUNT type, never released; a ring of 64 covers per-frame locals
	SOMA_METHOD(e, "cAINodeContainer", "cAINodeIterator@ GetNodeIterator(const cVector3f &in avPosition, float afRadius)", +[](cAINodeContainer *c, V v, float r) {
		static std::vector<cAINodeIterator> vRing;
		static size_t lNext = 0;
		if (vRing.size() < 64)
		{
			vRing.reserve(64);
			vRing.emplace_back(c, v, r);
			return &vRing.back();
		}
		cAINodeIterator *p = &vRing[lNext++ & 63];
		*p = cAINodeIterator(c, v, r);
		return p;
	});
	SOMA_METHOD(e, "cAINodeIterator", "bool HasNext()", +[](cAINodeIterator *i) { return i->HasNext(); });
	SOMA_METHOD(e, "cAINodeIterator", "cAINode@ Next()", +[](cAINodeIterator *i) { return i->Next(); });
	SOMA_METHOD(e, T, "cAINode@ GetNodeAtPos(const cVector3f &in avPos,float afMinDistance,float afMaxDistance, bool abGetClosest, bool abPosToNodeFreeDirectPathCheck,bool abAgentToNodeFreeDirectPathCheck, cAINode@ apSkipNode, int alFreePathRayNum, uint alFreePathFlags, bool abSkipUsedNodes)",
				+[](PF *p, V v, float mn, float mx, bool c, bool los, bool, cAINode *skip, int, asUINT, bool) { return p->NodeAtPos(v, mn, mx, c, los, skip); });
	SOMA_METHOD(e, T, "cAINode@ GetNodeAtPos(const cVector3f &in avPos,float afMinDistance,float afMaxDistance, bool abGetClosest, bool abPosToNodeFreeDirectPathCheck,bool abAgentToNodeFreeDirectPathCheck, cAINode@ apSkipNode)",
				+[](PF *p, V v, float mn, float mx, bool c, bool los, bool, cAINode *skip) { return p->NodeAtPos(v, mn, mx, c, los, skip); });
	SOMA_METHOD(e, T, "cAINode@ GetNodeInPosLOS(const cVector3f &in avPos, float afMinDistance,float afMaxDistance,bool abAgentToNodeFreeDirectPathCheck=false)",
				+[](PF *p, V v, float mn, float mx, bool) { return p->NodeAtPos(v, mn, mx, true, true, NULL); });
	SOMA_METHOD(e, T, "bool CheckFreePath(const cVector3f &in avStartPos, const cVector3f &in avTargetPos)", +[](PF *p, V a, V b) {
		cNodeData *d = p->Nodes();
		return d ? d->mpContainer->FreePath(a, b, 1, eAIFreePathFlag_SkipDynamic) : SomaLineOfSight(a, b, p->mpEntity);
	});
	SOMA_METHOD(e, T, "bool BuildPathNodeArrayToPos(const cVector3f &in avPos)", +[](PF *p, V v) {
		p->mvNodeArray.clear();
		p->mvNodeArrayDist.clear();
		cNodeData *d = p->Nodes();
		if (d == NULL)
			return false;
		tAINodeList lst;
		if (d->mpAStar->GetPath(p->Feet(), v, &lst) == false)
			return false;
		float fTotal = 0;
		cVector3f vPrev = p->Feet();
		for (auto it = lst.rbegin(); it != lst.rend(); ++it)
		{
			fTotal += cMath::Vector3Dist(vPrev, (*it)->GetPosition());
			vPrev = (*it)->GetPosition();
			p->mvNodeArray.push_back(*it);
			p->mvNodeArrayDist.push_back(fTotal);
		}
		return true;
	});
	SOMA_METHOD(e, T, "int GetPathNodeArraySize()", +[](PF *p) { return (int)p->mvNodeArray.size(); });
	SOMA_METHOD(e, T, "cAINode@ GetPathNodeArrayNode(int alIdx)", +[](PF *p, int i) { return i >= 0 && i < (int)p->mvNodeArray.size() ? p->mvNodeArray[i] : (cAINode *)NULL; });
	SOMA_METHOD(e, T, "float GetPathNodeArrayDist(int alIdx)", +[](PF *p, int i) { return i >= 0 && i < (int)p->mvNodeArrayDist.size() ? p->mvNodeArrayDist[i] : 0.0f; });
	SOMA_METHOD(e, T, "float GetPathNodeArrayFullLength()", +[](PF *p) { return p->mvNodeArrayDist.empty() ? 0.0f : p->mvNodeArrayDist.back(); });
	SOMA_METHOD(e, T, "void ClearTrackNodes()", +[](PF *p) { p->mvTrack.clear(); p->mlTrackIdx = -1; });
	SOMA_METHOD(e, T, "void AddTrackNode(const tString&in asNodeName, float afMinWaitTime, float afMaxWaitTime, const tString&in asAnimName, bool abLoopAnim)",
				+[](PF *p, S n, float mn, float mx, S a, bool l) { p->mvTrack.push_back(cAgentTrackNode{n, mn, mx, a, l}); });
	SOMA_METHOD(e, T, "void StartTrack(bool abLoop, float afUpdateFreq, const tString &in asEndOfTrackCallback)", +[](PF *p, bool l, float f, S cb) {
		p->mbTrackLoop = l;
		p->mfTrackFreq = f;
		p->msTrackCallback = cb;
		p->mbTrackActive = true;
		p->mbTrackPaused = false;
		p->mlTrackIdx = 0;
		p->StartTrackNode();
	});
	SOMA_METHOD(e, T, "void StopTrack()", +[](PF *p) { p->mbTrackActive = false; p->Stop(); });
	SOMA_METHOD(e, T, "void ResetCurrentTrackNode()", +[](PF *p) { p->mlTrackIdx = 0; });
	SOMA_METHOD(e, T, "int GetTrackNodeNum()", +[](PF *p) { return (int)p->mvTrack.size(); });
	SOMA_METHOD(e, T, "cLuxTrackNode@ GetTrackNode(int alIdx)", +[](PF *p, int i) -> void * { return i >= 0 && i < (int)p->mvTrack.size() ? &p->mvTrack[i] : NULL; });
	SOMA_METHOD(e, T, "int GetCurrentTrackNode()", +[](PF *p) { return p->mlTrackIdx; });
	SOMA_METHOD(e, T, "cLuxTrackNode@ GetCurrentTrackNodeData()", +[](PF *p) -> void * { return p->mlTrackIdx >= 0 && p->mlTrackIdx < (int)p->mvTrack.size() ? &p->mvTrack[p->mlTrackIdx] : NULL; });
	SOMA_METHOD(e, T, "void SetCurrentTrackWaitTime(float afX)", +[](PF *p, float x) { p->mfTrackWait = x; });
	SOMA_METHOD(e, T, "float GetCurrentTrackWaitTime()", +[](PF *p) { return p->mfTrackWait; });
	SOMA_METHOD(e, T, "void GoToNextTrackNode()", +[](PF *p) { p->GoToNextTrackNode(); });
	SOMA_METHOD(e, T, "bool GetTrackActive()", +[](PF *p) { return p->mbTrackActive; });
	SOMA_METHOD(e, T, "void SetTrackPaused(bool abX)", +[](PF *p, bool b) {
		if (p->mbTrackPaused == b)
			return;
		p->mbTrackPaused = b;
		if (b)
			p->Stop();
		else if (p->mbTrackActive && p->mbAtTrackNode == false)
			p->StartTrackNode();
	});
	SOMA_METHOD(e, T, "bool GetTrackPaused()", +[](PF *p) { return p->mbTrackPaused; });
	SOMA_METHOD(e, T, "void SetTrackLoop(bool abX)", +[](PF *p, bool b) { p->mbTrackLoop = b; });
	SOMA_METHOD(e, T, "bool GetTrackLoop()", +[](PF *p) { return p->mbTrackLoop; });
	SOMA_METHOD(e, T, "const tString& GetTrackCallback()", +[](PF *p) -> const tString & { return p->msTrackCallback; });
	SOMA_METHOD(e, T, "float GetTrackUpdateFreq()", +[](PF *p) { return p->mfTrackFreq; });
	SOMA_METHOD(e, T, "void SetNodeContainerName(const tString &in asName)", +[](PF *p, S n) { p->msContainer = n; p->mpNodes = NULL; });
	SOMA_METHOD(e, T, "void SetMaxHeight(float afX)", +[](PF *p, float x) { p->mfMaxHeight = x; p->mpNodes = NULL; });
	SOMA_METHOD(e, T, "void SetNodeName(const tString &in asName)", +[](PF *, S) {});
	SOMA_METHOD(e, T, "void SetNodeIsAtCenter(bool abX)", +[](PF *p, bool b) { p->mbAtCenter = b; p->mpNodes = NULL; });
	SOMA_METHOD(e, T, "void SetMinEdges(int alX)", +[](PF *, int) {});
	SOMA_METHOD(e, T, "void SetMaxEdges(int alX)", +[](PF *, int) {});
	SOMA_METHOD(e, T, "void SetMaxEdgeDistance(float afX)", +[](PF *p, float x) { p->mfMaxEdgeDist = x; p->mpNodes = NULL; });

	T = "cLuxTrackNode";
	SOMA_METHOD(e, T, "const tString& GetNodeName()", +[](cAgentTrackNode *n) -> const tString & { return n->msNode; });
	SOMA_METHOD(e, T, "float GetMinWaitTime()", +[](cAgentTrackNode *n) { return n->mfMinWait; });
	SOMA_METHOD(e, T, "float GetMaxWaitTime()", +[](cAgentTrackNode *n) { return n->mfMaxWait; });
	SOMA_METHOD(e, T, "const tString& GetAnimName()", +[](cAgentTrackNode *n) -> const tString & { return n->msAnim; });
	SOMA_METHOD(e, T, "bool GetLoopAnim()", +[](cAgentTrackNode *n) { return n->mbLoopAnim; });

	T = "cLuxBarkMachine";
	typedef cAgentBarkMachine BM;
	SOMA_METHOD(e, T, "void AddState(int alId)", +[](BM *b, int id) { b->mapStates[id]; b->mlEdit = id; });
	SOMA_METHOD(e, T, "void ChangeState(int alId)", +[](BM *b, int id) { if (b->mlCur != id) { b->mlCur = id; b->mfCount = 0; auto it = b->mapStates.find(id); if (it != b->mapStates.end()) b->mfCount = cMath::RandRectf(it->second.mfMin, it->second.mfMax); } });
	SOMA_METHOD(e, T, "void SetState_SoundBark(const tString&in asSound, float afMinBetweenTime, float afMaxBetweenTime, bool abWaitForSoundToBeDone)",
				+[](BM *b, S s, float mn, float mx, bool) { if (b->mlEdit >= 0) b->mapStates[b->mlEdit] = cAgentBarkMachine::cState{s, mn, mx}; });
	SOMA_METHOD(e, T, "void SetState_VoiceBark(const tString&in asSubject, float afMinBetweenTime, float afMaxBetweenTime, bool abWaitForSoundToBeDone,int alPrio=0,float afMinDistance=-1, float afMaxDistance=-1, float afMaxPlayerListeningRange=-1)",
				+[](BM *, S, float, float, bool, int, float, float, float) {});
	SOMA_METHOD(e, T, "void SetupVoice(const tString&in asCharacter, bool abUse3D, float afDefaultMinDistance,float afDefaultMaxDistance ,float afDefaultMaxPlayerListeningRange)",
				+[](BM *, S, bool, float, float, float) {});
	SOMA_METHOD(e, T, "void PlayVoice(const tString&in asSubject, int alPrio, float afMinDistance=-1, float afMaxDistance=-1, float afMaxPlayerListeningRange=-1)",
				+[](BM *, S, int, float, float, float) {});
	SOMA_METHOD(e, T, "void SetActive(bool abX)", +[](BM *b, bool x) { b->mbActive = x; });
	SOMA_METHOD(e, T, "bool IsActive()", +[](BM *b) { return b->mbActive; });

	T = "cLuxSoundListener";
	typedef cAgentSoundListener SL;
	SOMA_METHOD(e, T, "void LoadFromInstanceVariables(cResourceVarsObject@ apInstanceVars)", +[](SL *, cResourceVarsObject *) {});
	SOMA_METHOD(e, T, "void SetHearRadius(float afX)", +[](SL *l, float x) { l->mfHearRadius = x; });
	SOMA_METHOD(e, T, "float GetHearRadius()", +[](SL *l) { return l->mfHearRadius; });
	SOMA_METHOD(e, T, "void SetMinHearPrio(int alX)", +[](SL *l, int x) { l->mlMinPrio = x; });
	SOMA_METHOD(e, T, "int GetMinHearPrio()", +[](SL *l) { return l->mlMinPrio; });
	SOMA_METHOD(e, T, "float GetSoundRadiusMul()", +[](SL *l) { return l->mfRadiusMul; });
	SOMA_METHOD(e, T, "float GetSoundMinRadius()", +[](SL *l) { return l->mfMinRadius; });
	SOMA_METHOD(e, T, "float GetSoundMaxRadius()", +[](SL *l) { return l->mfMaxRadius; });
	SOMA_METHOD(e, T, "void SetSoundRadiusMul(float afX)", +[](SL *l, float x) { l->mfRadiusMul = x; });
	SOMA_METHOD(e, T, "void SetSoundMinRadius(float afX)", +[](SL *l, float x) { l->mfMinRadius = x; });
	SOMA_METHOD(e, T, "void SetSoundMaxRadius(float afX)", +[](SL *l, float x) { l->mfMaxRadius = x; });
	SOMA_METHOD(e, T, "void SetActive(bool bX)", +[](SL *l, bool b) { l->mbActive = b; });
	SOMA_METHOD(e, T, "bool IsActive()", +[](SL *l) { return l->mbActive; });
	SOMA_METHOD(e, T, "bool IsListening()", +[](SL *l) { return l->mbActive; });
	SOMA_METHOD(e, T, "void SetIgnoreSoundRadius(float afX)", +[](SL *l, float x) { l->mfIgnoreRadius = x; });
	SOMA_METHOD(e, T, "float GetIgnoreSoundRadius()", +[](SL *l) { return l->mfIgnoreRadius; });
	SOMA_METHOD(e, T, "void SetMaxPlayerPhysicsInteractTime(float afX)", +[](SL *l, float x) { l->mfMaxPlayerInteractTime = x; });
	SOMA_METHOD(e, T, "float GetMaxPlayerPhysicsInteractTime()", +[](SL *l) { return l->mfMaxPlayerInteractTime; });

	T = "cLuxHeadTracker";
	typedef cAgentHeadTracker HT;
	SOMA_METHOD(e, T, "void SetTrackEntity(iLuxEntity @apEntity)", +[](HT *h, E *p) { h->mpTrack = p; });
	SOMA_METHOD(e, T, "void SetActive(bool abX)", +[](HT *h, bool b) { h->mbActive = b; });
	SOMA_METHOD(e, T, "bool IsActive()", +[](HT *h) { return h->mbActive; });
	SOMA_METHOD(e, T, "float GetMaxAngle()", +[](HT *h) { return h->mfMaxAngle; });
	SOMA_METHOD(e, T, "void SetMaxAngle(float afX)", +[](HT *h, float x) { h->mfMaxAngle = x; });
	SOMA_METHOD(e, T, "void LoadFromVariables(cResourceVarsObject@ apVars)", +[](HT *, cResourceVarsObject *) {});
	SOMA_METHOD(e, T, "void SetMoveSpeedMul(float afX)", +[](HT *, float) {});
	SOMA_METHOD(e, T, "void SetMoveMaxSpeed(float afX)", +[](HT *, float) {});
	SOMA_METHOD(e, T, "void SetAngleOffset(float afX)", +[](HT *, float) {});
}
