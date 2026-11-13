#include "SomaAgent.h"
#include "RustSomaAgentS4.h"
#include "impl/RustBackends.h"
#include "SomaBase.h"
#include "SomaLux.h"
#include "SomaLuxEntity.h"
#include "SomaLuxPlayer.h"
#include "SomaLuxScriptable.h"
#include "SomaLuxVoice.h"
#include "SomaScriptApi.h"
#include "SomaScriptBind.h"
#include "SomaScriptRuntime.h"
#include "RustSomaAgent.h"

#include "impl/scriptarray.h"
#include "impl/tinyXML/tinyxml.h"


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
		// Rebirth values as left by cSomaScriptApi::RebaseEnums; script user ids follow
		eComp_ProximitySensor,
		eComp_AttackDamage,
		eComp_FootPlacement,
		eComp_HandPlacement,
		eComp_EdgeGlow = 1000,
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
		bool mbFootActive[4] = {true, true, true, true};
		cAgentComponent(E *apEnt, int alType) : mpEntity(apEnt), mlType(alType) {}
		virtual ~cAgentComponent() {}
		virtual void Update(float) {}
		virtual void OnMessage(int) {}
		virtual void OnSetActive() {}
		virtual void OnDestroy() {}
	};

	struct cAgent;
	cAgent *Agent(E *apEnt);

	struct cAgentStateMachine : cAgentComponent, OhplGs2SmPod
	{
		std::map<int, tString> mapStates, mapSubStates;

		cAgentStateMachine(E *p) : cAgentComponent(p, eComp_StateMachine) { ohpl_gs2_sm_init(this); }
		~cAgentStateMachine() { ohpl_gs2_sm_free(this); }

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

		void Reset() { ohpl_gs2_sm_reset(this, mapStates.empty() == false); }

		void OnSetActive() override { Reset(); }

		void AddTimer(uint64_t alId, float afTime) { ohpl_gs2_sm_add_timer(this, mbInSub, alId, afTime); }
		void StopTimer(uint64_t alId) { ohpl_gs2_sm_stop_timer(this, mbInSub, alId); }
		bool TimerExists(uint64_t alId) { return ohpl_gs2_sm_timer_exists(this, mbInSub, alId); }
		int TimerCount(bool abSub) { return (int)ohpl_gs2_sm_timer_len(this, abSub); }

		static bool Exists(void *apCtx, bool abSub, int alId)
		{
			cAgentStateMachine *p = (cAgentStateMachine *)apCtx;
			return (abSub ? p->mapSubStates : p->mapStates).count(alId) != 0;
		}

		// each script call is stepped out of Rust, so a script's effect is seen by the phases after it
		void Drive()
		{
			OhplGs2Action a;
			while (ohpl_gs2_sm_next(this, this, &Exists, &a))
				switch (a.kind)
				{
				case eOhplGs2Act_Leave: Run(a.sub, a.state, "Leave()"); break;
				case eOhplGs2Act_Enter: Run(a.sub, a.state, "Enter()"); break;
				case eOhplGs2Act_Update: Run(a.sub, a.state, "Update(float)", [dt = a.arg_f](asIScriptContext *c) { c->SetArgFloat(0, dt); }); break;
				case eOhplGs2Act_TimerUp: Run(a.sub, a.state, "TimerUp(uint64)", [lId = a.arg_u](asIScriptContext *c) { c->SetArgQWord(0, lId); }); break;
				case eOhplGs2Act_SubStateOver: Run(false, a.state, "SubStateOver(int)", [lOld = a.arg_i](asIScriptContext *c) { c->SetArgDWord(0, lOld); }); break;
				case eOhplGs2Act_Message: Run(a.sub, a.state, "Message(int)", [lMsg = a.arg_i](asIScriptContext *c) { c->SetArgDWord(0, lMsg); }); break;
				}
		}

		// cLuxStateMachine::OnUpdate: state changes requested last frame are applied here
		void Update(float afTimeStep) override
		{
			ohpl_gs2_sm_begin_update(this, afTimeStep);
			Drive();
		}

		void OnMessage(int alMessage) override
		{
			ohpl_gs2_sm_begin_message(this, alMessage);
			Drive();
		}
	};

	typedef OhplGs2SpeedState cAgentSpeedState;
	typedef OhplGs2WallSample cWallSample;

	struct cAgentCharMover : cAgentComponent, OhplGs2CharMover
	{
		iCharacterBody *mpBody;
		tString msIdleAnim = "Idle", msWalkAnim = "Walk", msRunAnim = "Run", msBackwardAnim;
		std::map<int, cAgentSpeedState> mapSpeedStates;
		int mlEditState = -1;
		tString msTurnedCallback;
		tString msIdleExtra;
		std::vector<cWallSample> mvWallSamples;

		cAgentCharMover(E *p, iCharacterBody *apBody) : cAgentComponent(p, eComp_CharMover), mpBody(apBody) { mfTurnMinBreakAngle = cMath::ToRad(40); }

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
			tString sExtra = apVars->GetVarString("CharMover_IdleExtraAnim", "");
			if (sExtra != "")
				SetupIdleExtra(sExtra, apVars->GetVarFloat("CharMover_IdleExtraMinWait", 0), apVars->GetVarFloat("CharMover_IdleExtraMaxWait", 0));
		}

		void SetupIdleExtra(const tString &asName, float afMin, float afMax)
		{
			msIdleExtra = asName;
			mfIdleExtraMin = afMin;
			mfIdleExtraMax = afMax;
			mlIdleExtraCount = 0;
			while (mpEntity->mpMesh && mpEntity->mpMesh->GetAnimationStateFromName(asName + cString::ToString(mlIdleExtraCount + 1)))
				++mlIdleExtraCount;
			if (mlIdleExtraCount > 0)
				mfIdleExtraWait = cMath::RandRectf(afMin, afMax);
			else
				Error("Could not find any animation start with '%s' in entity '%s' to be used for idle extra!\n", asName.c_str(), mpEntity->msName.c_str());
		}

		void MoveToPos(const cVector3f &avPos, bool abSlowDown) { ohpl_gs2_cm_move_to_pos(this, avPos.v, abSlowDown); }
		void Stop() { ohpl_gs2_cm_stop(this); }
		void TurnTo(float afYaw) { ohpl_gs2_cm_turn_to(this, afYaw); }

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

		void SetUseMoveStateAnims(bool abX) { ohpl_gs2_cm_set_use_move_state_anims(this, abX); }

		void Update(float afTimeStep) override;
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
		auto key = std::make_pair(pWorld, asName + "|" + cString::ToString(afMaxHeight) + "|" + cString::ToString(afMaxEdgeDist) + "|" + cString::ToString(abAtCenter));
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
			if (pEnt->msClassName == "PathNode" || pEnt->msClassName == "PathNodePatrol" || pEnt->msClassName == "PathNodeInterestPoint")
			{
				auto idIt = mapIds.find(pEnt->msName);
				pCont->AddNode(pEnt->msName, idIt != mapIds.end() ? idIt->second : lNextId++, pEnt->GetPosition(), NULL);
			}
		// official cLuxMap::AddPathNodeGeneratorArea output, cached next to the map
		if (FILE *pGen = cPlatform::OpenFile(cString::SetFileExtW(pWorld->GetFilePath(), _W("gen_node_cache")), _W("rb")))
		{
			TiXmlDocument genDoc;
			bool bGen = genDoc.LoadFile(pGen);
			fclose(pGen);
			if (bGen && genDoc.RootElement())
				for (TiXmlElement *p = genDoc.RootElement()->FirstChildElement("Node"); p; p = p->NextSiblingElement("Node"))
				{
					tString sName = cString::ToString(p->Attribute("Name"), "");
					auto idIt = mapIds.find(sName);
					if (idIt != mapIds.end())
						pCont->AddNode(sName, idIt->second, cString::ToVector3f(p->Attribute("Pos"), 0), NULL);
				}
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

	struct cAgentPathfinder : cAgentComponent, OhplGs2Pathfinder
	{
		tString msContainer;
		cNodeData *mpNodes = NULL;
		float mfMaxHeight = 1.0f, mfMaxEdgeDist = 5;
		bool mbAtCenter = false;
		std::vector<cVector3f> mvPath;
		tString msResultCallback, msEndOfPathCallback;
		std::vector<cAINode *> mvNodeArray;
		std::vector<float> mvNodeArrayDist;
		std::vector<cAgentTrackNode> mvTrack;
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

		void MoveTo(const cVector3f &avGoal, bool abExact, const tString &asCallback, bool abInMap);
		void Stop();

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

		void OnMessage(int alMessage) override;
		void Update(float afTimeStep) override;
		void SendGoal();
		void StartTrackNode();
		void GoToNextTrackNode();

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
			bool bHas = it != mapStates.end() && it->second.msSound != "";
			if (ohpl_gs2_bark_due(&mfCount, afTimeStep, mbActive, bHas, bHas ? it->second.mfMin : 0, bHas ? it->second.mfMax : 0) == false)
				return;
			if (cSomaLuxMap *pMap = cSomaLuxMap::GetCurrent())
				if (cSoundEntity *pSound = pMap->GetWorld()->CreateSoundEntity(mpEntity->msName + "_Bark", it->second.msSound, true))
					pSound->SetPosition(mpEntity->GetPosition());
		}
	};

	struct cAgentSoundListener : cAgentComponent, OhplGs2SoundListener
	{
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

	struct cAgent : OhplGs2AgentState
	{
		cSomaLuxEntity *mpEnt;
		iCharacterBody *mpBody = NULL;
		cMatrixf mtxMeshOffset = cMatrixf::Identity;
		std::vector<std::unique_ptr<cAgentComponent>> mvComponents;
		cAgentMessageData mMessage;
		tString msMessageCallback;
		cBoneState *mpPosBone = NULL;
		tString msAutoDisableCallback;
		std::vector<iPhysicsBody *> mvAvoidBodies;

		cAgent() { mfFOV = cMath::ToRad(90); }

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

		void Replay(OhplGs2Events *apEvents);
		void PreUpdate(float afTimeStep);
		void SetSensesActive(bool abX);
		void ResetPlayerDetectionState() { ohpl_gs2_agent_reset_detection(this); }
		void StepUp();
		void AlignWithGround(float afTimeStep);
		void SyncMesh();
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
		struct cGlow { cSomaID mID; cColor mColor; float mfY; };
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
				mpOutline->SetVec4f(0, glow.mColor.r, glow.mColor.g, glow.mColor.b, glow.mColor.a);
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

	namespace gs2
	{
		cAgent *A(void *c) { return (cAgent *)c; }
		void Out(float *o, const cVector3f &v)
		{
			o[0] = v.x;
			o[1] = v.y;
			o[2] = v.z;
		}
		cVector3f In(const float *p) { return cVector3f(p[0], p[1], p[2]); }
		cSomaLuxPlayer *Player() { return cSomaLuxPlayer::Get(); }
		cAgentCharMover *Mover(void *c) { return A(c)->Find<cAgentCharMover>(eComp_CharMover); }
		cAgentPathfinder *Path(void *c) { return A(c)->Find<cAgentPathfinder>(eComp_Pathfinder); }
		const tString *MoveAnim(void *c, int s)
		{
			cAgentCharMover *m = Mover(c);
			return m ? (s == 0 ? &m->msIdleAnim : s == 1 ? &m->msWalkAnim : &m->msRunAnim) : NULL;
		}

		const OhplGs2Host &Host()
		{
			static const OhplGs2Host h = {
				NULL,
				[](void *c) { return A(c)->mpBody != NULL; },
				[](void *c) { return A(c)->mpEnt->mpMap != NULL; },
				[](void *c) { return A(c)->mpEnt->mpMesh != NULL; },
				[](void *c, float *o) { Out(o, A(c)->mpEnt->GetPosition()); },
				[](void *c) { return A(c)->mpEnt->mvScale.y; },
				[](void *c) { return A(c)->mpEnt->mbGlobalSpaceAnim; },
				[](void *c, float *o) { Out(o, A(c)->mpBody->GetPosition()); },
				[](void *c, float *o) { Out(o, A(c)->mpBody->GetFeetPosition()); },
				[](void *c, float *o) { Out(o, A(c)->mpBody->GetSize()); },
				[](void *c, float *o) { Out(o, A(c)->mpBody->GetForward()); },
				[](void *c) { return A(c)->mpBody->GetYaw(); },
				[](void *c) { return A(c)->mpBody->GetPitch(); },
				[](void *c) { return A(c)->mpBody->GetEntityPitchAmount(); },
				[](void *c) { return A(c)->mpBody->GetMoveSpeed(eCharDir_Forward); },
				[](void *c) { return A(c)->mpBody->IsOnGround(); },
				[](void *c) { return A(c)->mpBody->GetTestCollision(); },
				[](void *c) { return A(c)->mpBody->GravityIsActive(); },
				[](void *c) { return A(c)->mpBody->GetMaxStepSize(); },
				[](void *c, float y) { A(c)->mpBody->SetYaw(y); },
				[](void *c, const float *p) { A(c)->mpBody->SetPosition(In(p)); },
				[](void *c, const float *p) { A(c)->mpBody->SetFeetPosition(In(p)); },
				[](void *c, bool b) { A(c)->mpBody->SetGravityActive(b); },
				[](void *c, bool b) { A(c)->mpBody->SetTestCollision(b); },
				[](void *c) { return A(c)->mpPosBone != NULL; },
				[](void *c, float *o) { Out(o, A(c)->mpPosBone->GetWorldPosition()); },
				[](void *c, const float *m) { cMatrixf mtx; memcpy(mtx.v, m, sizeof(float) * 16); A(c)->mpEnt->mpMesh->SetMatrix(mtx); },
				[](void *) { return Player() && Player()->GetCharacterBody(); },
				[](void *) { return Player() && Player()->IsDead(); },
				[](void *) { return Player() ? Player()->GetVisibilityRangeMul() : 1.0f; },
				[](void *) { return Player() ? Player()->GetVisibilityMaxRange() : -1.0f; },
				[](void *, float *o) { iCharacterBody *b = PlayerBody(); Out(o, b ? b->GetPosition() : cVector3f(0)); },
				[](void *, float *o) { iCharacterBody *b = PlayerBody(); Out(o, b ? b->GetFeetPosition() : cVector3f(0)); },
				[](void *) { iCharacterBody *b = PlayerBody(); return b ? b->GetSize().y : 0.0f; },
				[](void *c, const float *a, const float *b) { return SomaLineOfSight(In(a), In(b), A(c)->mpEnt); },
				[](void *c) { return SomaEntityInPlayerLOS(A(c)->mpEnt, true); },
				[](void *c, const float *a, const float *b) { return A(c)->FloorDist(In(a), In(b)); },
				[](void *c, float *o) { Out(o, A(c)->mpBody->GetLastPosition()); },
				[](void *c, float *o) { Out(o, A(c)->mpBody->GetRight()); },
				[](void *c, float dt, float *o) { Out(o, A(c)->mpBody->GetVelocity(dt)); },
				[](void *c, float p) { A(c)->mpBody->AddPitch(p); },
				[](void *c, float v) { A(c)->mpBody->SetMaxPositiveMoveSpeed(eCharDir_Forward, v); },
				[](void *c, float v) { A(c)->mpBody->SetMoveAcc(eCharDir_Forward, v); },
				[](void *c, float v) { A(c)->mpBody->SetMoveDeacc(eCharDir_Forward, v); },
				[](void *c) { A(c)->mpBody->Move(eCharDir_Forward, 1); },
				[](void *c, const float *a, const float *b, float *d, float *n) {
					cVector3f vNormal;
					bool bHit = SomaRaycast(A(c)->mpEnt, In(a), In(b), *d, vNormal);
					Out(n, vNormal);
					return bHit;
				},
				[](void *c, const float *p, float d) -> size_t {
					cAgent *a = A(c);
					cVector3f vPos = In(p);
					cBoundingVolume bv;
					bv.SetLocalMinMax(vPos - d, vPos + d);
					a->mvAvoidBodies.clear();
					a->mpEnt->mpMap->GetWorld()->GetPhysicsWorld()->GetBodiesInBV(&bv, &a->mvAvoidBodies);
					return a->mvAvoidBodies.size();
				},
				[](void *c, size_t i, OhplGs2AvoidBody *o) {
					cAgent *a = A(c);
					iPhysicsBody *pBody = a->mvAvoidBodies[i];
					iCharacterBody *pPlayer = PlayerBody();
					iPhysicsBody *pPlayerBody = pPlayer ? pPlayer->GetCurrentBody() : NULL;
					o->mass = pBody->GetMass();
					o->active = pBody->IsActive();
					o->collide = pBody->GetCollide();
					o->character = pBody->IsCharacter();
					o->player = pBody == pPlayerBody;
					o->own = pBody == a->mpBody->GetCurrentBody();
					Out(o->center, pBody->GetBoundingVolume()->GetWorldCenter());
					o->radius = pBody->GetBoundingVolume()->GetRadius();
				},
				[](void *c) { return A(c)->mpEnt->mlCurrentAnim; },
				[](void *c, int s) { const tString *p = MoveAnim(c, s); return p && *p != ""; },
				[](void *c, int s) { A(c)->mpEnt->PlayAnimation(*MoveAnim(c, s), 0.3f, true, ""); },
				[](void *c, float v) {
					cSomaLuxEntity *p = A(c)->mpEnt;
					if (p->mpMesh && p->mlCurrentAnim >= 0)
						if (cAnimationState *pAnim = p->mpMesh->GetAnimationState(p->mlCurrentAnim))
							pAnim->SetSpeed(v);
				},
				[](void *c) {
					cAgentCharMover *m = Mover(c);
					cMeshEntity *pMesh = m->mpEntity->mpMesh;
					int lIdx = m->mpEntity->PlayAnimation(m->msIdleAnim, 0.3f, true, "");
					if (lIdx >= 0)
						pMesh->GetAnimationState(lIdx)->SetRelativeTimePosition(cMath::RandRectf(0, 1));
				},
				[](void *c, int i, float *o) {
					cAgentCharMover *m = Mover(c);
					cAnimationState *pAnim = m->mpEntity->mpMesh->GetAnimationStateFromName(m->msIdleExtra + cString::ToString(i));
					if (pAnim)
						*o = pAnim->GetLength();
					return pAnim != NULL;
				},
				[](void *c, int i) {
					cAgentCharMover *m = Mover(c);
					m->mpEntity->PlayAnimation(m->msIdleExtra + cString::ToString(i), 0.3f, false, "");
				},
				[](void *c, float *o) { Path(c) ? Out(o, Path(c)->Feet()) : Out(o, cVector3f(0)); },
				[](void *c) -> size_t { return Path(c) ? Path(c)->mvPath.size() : 0; },
				[](void *c, size_t i, float *o) { Out(o, Path(c) && i < Path(c)->mvPath.size() ? Path(c)->mvPath[i] : cVector3f(0)); },
				[](void *c, const float *g) {
					cAgentPathfinder *p = Path(c);
					if (p == NULL)
						return;
					cVector3f vGoal = In(g);
					if (p->BuildPath(vGoal, p->mvPath) == false)
						p->mvPath.assign(1, vGoal);
				},
				[](void *c) -> size_t { return Path(c) ? Path(c)->mvTrack.size() : 0; },
				[](void *c, size_t i, OhplGs2TrackNodeInfo *o) {
					const cAgentTrackNode &n = Path(c)->mvTrack[i];
					*o = {n.mfMinWait, n.mfMaxWait, n.msAnim != "", n.mbLoopAnim};
				},
				[](void *c, size_t i, float *o) {
					cAgentPathfinder *p = Path(c);
					cNodeData *pNodes = p->Nodes();
					cAINode *pNode = pNodes ? pNodes->mpContainer->GetNodeFromName(p->mvTrack[i].msNode) : NULL;
					if (pNode == NULL)
					{
						Warning("SOMA agent '%s': track node '%s' not found\n", p->mpEntity->msName.c_str(), p->mvTrack[i].msNode.c_str());
						return false;
					}
					Out(o, pNode->GetPosition());
					return true;
				},
				[](void *c) -> size_t { return A(c)->mpEnt->mpMap ? A(c)->mpEnt->mpMap->GetEntities().size() : 0; },
				[](void *c, size_t i, const float *origin, const float *dir, float fMax, float *o) {
					cSomaLuxEntity *pDoor = A(c)->mpEnt->mpMap->GetEntities()[i];
					float fDist;
					if (pDoor->mbIsDoor && pDoor->mbIsClosedDoor && pDoor->mbActive && SomaRayHitsEntity(pDoor, In(origin), In(dir), fMax, fDist))
					{
						Out(o, pDoor->GetPosition());
						return true;
					}
					return false;
				},
			};
			return h;
		}

		struct cEvents
		{
			OhplGs2Events *mp = ohpl_gs2_events_new();
			~cEvents() { ohpl_gs2_events_free(mp); }
			operator OhplGs2Events *() { return mp; }
		};

		// the table's ctx is the agent; copy it per call so one static table serves all agents
		struct cCall
		{
			OhplGs2Host mHost;
			cCall(cAgent *apAgent) : mHost(Host()) { mHost.ctx = apAgent; }
			operator const OhplGs2Host *() { return &mHost; }
		};
	}

	void cAgent::Replay(OhplGs2Events *apEvents)
	{
		for (size_t i = 0; i < ohpl_gs2_events_len(apEvents); ++i)
		{
			OhplGs2Event e = ohpl_gs2_events_data(apEvents)[i];
			cAgentPathfinder *pPF = Find<cAgentPathfinder>(eComp_Pathfinder);
			cAgentCharMover *pMover = Find<cAgentCharMover>(eComp_CharMover);
			switch (e.kind)
			{
			case eOhplGs2Ev_Message:
				SomaAgentSendMessage(mpEnt, e.a, cVector3f(e.v[0], e.v[1], e.v[2]), e.b);
				break;
			case eOhplGs2Ev_SetInactive:
				mpEnt->SetActive(false);
				break;
			case eOhplGs2Ev_AutoDisableCallback:
			{
				cSomaLuxMap *pMap = cSomaLuxMap::GetCurrent();
				if (msAutoDisableCallback != "" && pMap && pMap->GetScript())
					cSomaScriptRuntime::Get()->Call(pMap->GetScript(), "void " + msAutoDisableCallback + "(const tString &in)",
													[this](asIScriptContext *c) { c->SetArgObject(0, &mpEnt->msName); });
				break;
			}
			case eOhplGs2Ev_DoorMessage:
			{
				const std::vector<cSomaLuxEntity *> &vEnts = mpEnt->mpMap->GetEntities();
				if (e.a >= 0 && (size_t)e.a < vEnts.size())
					memcpy(mMessage.mBlock + 44, &vEnts[e.a]->mID, sizeof(cSomaID));
				SomaAgentSendMessage(mpEnt, 18, cVector3f(e.v[0], e.v[1], e.v[2]));
				break;
			}
			case eOhplGs2Ev_TurnedCallback:
				if (pMover && pMover->msTurnedCallback != "")
					SomaMapScriptCall("void " + pMover->msTurnedCallback + "(const tString &in)", [&](asIScriptContext *c) { c->SetArgObject(0, &mpEnt->msName); });
				break;
			case eOhplGs2Ev_EndOfPathCallback:
				if (pPF && pPF->msEndOfPathCallback != "")
				{
					tString sFunc = pPF->msEndOfPathCallback;
					pPF->msEndOfPathCallback = "";
					bool bOk = e.a != 0;
					SomaMapScriptCall("void " + sFunc + "(const tString &in, bool)", [&](asIScriptContext *c) {
						c->SetArgObject(0, &mpEnt->msName);
						c->SetArgByte(1, bOk);
					});
				}
				break;
			case eOhplGs2Ev_ResultCallback:
				if (pPF)
					pPF->RunResultCallback(e.a != 0);
				break;
			case eOhplGs2Ev_TrackCallback:
				if (pPF && pPF->msTrackCallback != "")
					SomaMapScriptCall("void " + pPF->msTrackCallback + "(const tString &in)", [&](asIScriptContext *c) { c->SetArgObject(0, &mpEnt->msName); });
				break;
			case eOhplGs2Ev_TrackPlayAnim:
				if (pPF && pMover && e.a >= 0 && (size_t)e.a < pPF->mvTrack.size())
				{
					const cAgentTrackNode &node = pPF->mvTrack[e.a];
					pMover->PlayAnimation(node.msAnim, 0.3f, node.mbLoopAnim, "");
				}
				break;
			}
		}
	}

	void cAgent::PreUpdate(float afTimeStep)
	{
		gs2::cEvents ev;
		ohpl_gs2_agent_pre_update(this, gs2::cCall(this), ev, afTimeStep);
		Replay(ev);
	}

	void cAgent::SetSensesActive(bool abX)
	{
		gs2::cEvents ev;
		ohpl_gs2_agent_set_senses_active(this, gs2::cCall(this), ev, abX);
		Replay(ev);
	}

	void cAgent::StepUp()
	{
		gs2::cEvents ev;
		ohpl_gs2_agent_step_up(this, gs2::cCall(this), ev);
	}

	void cAgent::AlignWithGround(float afTimeStep)
	{
		gs2::cEvents ev;
		ohpl_gs2_agent_align(this, gs2::cCall(this), ev, afTimeStep);
	}

	void cAgent::SyncMesh()
	{
		gs2::cEvents ev;
		cAgentCharMover *pMover = Find<cAgentCharMover>(eComp_CharMover);
		ohpl_gs2_agent_sync_mesh(this, gs2::cCall(this), ev, mtxMeshOffset.v, pMover ? pMover->mfRoll : 0.0f);
	}

	void cAgentCharMover::Update(float afTimeStep)
	{
		cAgent *pAgent = Agent(mpEntity);
		if (pAgent == NULL)
			return;
		gs2::cEvents ev;
		ohpl_gs2_cm_update(this, Current(), mvWallSamples.data(), mvWallSamples.size(), afTimeStep, gs2::cCall(pAgent), ev);
		pAgent->Replay(ev);
	}

	void cAgentPathfinder::Update(float afTimeStep)
	{
		cAgent *pAgent = Agent(mpEntity);
		if (pAgent == NULL)
			return;
		gs2::cEvents ev;
		ohpl_gs2_pf_update(this, Mover(), afTimeStep, gs2::cCall(pAgent), ev);
		pAgent->Replay(ev);
	}

	void cAgentPathfinder::OnMessage(int alMessage)
	{
		cAgent *pAgent = Agent(mpEntity);
		if (pAgent == NULL)
			return;
		gs2::cEvents ev;
		ohpl_gs2_pf_on_message(this, Mover(), alMessage, gs2::cCall(pAgent), ev);
		pAgent->Replay(ev);
	}

	void cAgentPathfinder::SendGoal()
	{
		cAgent *pAgent = Agent(mpEntity);
		if (pAgent)
			ohpl_gs2_pf_send_goal(this, Mover(), gs2::cCall(pAgent));
	}

	void cAgentPathfinder::MoveTo(const cVector3f &avGoal, bool abExact, const tString &asCallback, bool abInMap)
	{
		msResultCallback = asCallback;
		cAgent *pAgent = Agent(mpEntity);
		if (pAgent)
			ohpl_gs2_pf_move_to(this, Mover(), gs2::cCall(pAgent), avGoal.v, abExact, abInMap);
	}

	void cAgentPathfinder::Stop() { ohpl_gs2_pf_stop(this, Mover()); }

	void cAgentPathfinder::StartTrackNode()
	{
		cAgent *pAgent = Agent(mpEntity);
		if (pAgent)
			ohpl_gs2_pf_start_track_node(this, Mover(), gs2::cCall(pAgent));
	}

	void cAgentPathfinder::GoToNextTrackNode()
	{
		cAgent *pAgent = Agent(mpEntity);
		if (pAgent == NULL)
			return;
		gs2::cEvents ev;
		ohpl_gs2_pf_go_to_next_track_node(this, Mover(), gs2::cCall(pAgent), ev);
		pAgent->Replay(ev);
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

	// cLuxUserComponent: scripts hold the cSomaLuxScriptable base
	struct cAgentUserComponent : cSomaLuxScriptable, cAgentComponent
	{
		// ponytail: component timers are not saved
		cAgentUserComponent(E *apEnt, int alID) : cAgentComponent(apEnt, alID) { mbSaved = false; }
		void Update(float afTimeStep) override
		{
			OnUpdate(afTimeStep);
			OnVariableUpdate(afTimeStep);
		}
		void OnMessage(int alMessage) override
		{
			Call("void OnRecieveMessage(int alMessageId, cLuxEntityMessageData @apData)", [&](asIScriptContext *c) {
				c->SetArgDWord(0, alMessage);
				c->SetArgAddress(1, Agent(mpEntity)->mMessage.mBlock);
			});
		}
		void OnSetActive() override
		{
			Call("void OnSetActive(bool abX)", [&](asIScriptContext *c) { c->SetArgByte(0, mpEntity->mbActive); });
		}
	};

	asIScriptObject *UserScript(cAgentComponent *apComp)
	{
		cAgentUserComponent *pUser = dynamic_cast<cAgentUserComponent *>(apComp);
		asIScriptObject *pObj = pUser ? pUser->GetScript() : NULL;
		if (pObj)
			pObj->AddRef();
		return pObj;
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

	// cLuxProximitySensor::OnVariableUpdate 0x9c9d00
	struct cAgentProximitySensor : cAgentComponent
	{
		typedef OhplGs2ProxState cState;
		std::vector<cState> mvStates;
		float mfTimer = 0;
		cAgentProximitySensor(E *p) : cAgentComponent(p, eComp_ProximitySensor) {}

		cState *Get(int alId)
		{
			if (alId >= 0 && alId < (int)mvStates.size() && mvStates[alId].mbExists)
				return &mvStates[alId];
			Error("BarkMachine state %d called by entity %s does not exist.\n", alId, mpEntity->msName.c_str());
			return NULL;
		}

		void Update(float afTimeStep) override;
	};

	void cAgentProximitySensor::Update(float afTimeStep)
	{
		cAgent *pAgent = Agent(mpEntity);
		if (pAgent == NULL)
			return;
		gs2::cEvents ev;
		ohpl_gs2_proximity_update(&mfTimer, mvStates.data(), mvStates.size(), pAgent, pAgent->mpBody != NULL, afTimeStep, gs2::cCall(pAgent), ev);
		pAgent->Replay(ev);
	}

	// cLuxAttackDamage::OnVariableUpdate / ShapeDamage, cLuxAttackShapeCallback::OnAABBResult 0xb17d90
	// ponytail: player only, sphere vs body capsule; props/agents are not hit
	struct cAgentAttackDamage : cAgentComponent
	{
		struct cShape { int mlSocket; cVector3f mvOffset; float mfRadius; };
		struct cType
		{
			bool mbExists = false, mbActive = false;
			float mfDuration = 0, mfFreq = 0, mfTimer = 0;
			int mlInstance = -1;
			float mfForce = 100, mfMaxImpulse = 10, mfMin = 1, mfMax = 1;
			int mlDamageType = 0;
			bool mbPlayer = true, mbLethal = true;
			std::vector<cShape> mvShapes;
		};
		std::vector<cType> mvTypes;
		int mlCur = -1;
		static inline int glInstanceCount = 0;
		cAgentAttackDamage(E *p) : cAgentComponent(p, eComp_AttackDamage) {}

		cType *Get(int alId)
		{
			if (alId >= 0 && alId < (int)mvTypes.size() && mvTypes[alId].mbExists)
				return &mvTypes[alId];
			Error("AttackDamage type %d called by entity %s does not exist.\n", alId, mpEntity->msName.c_str());
			return NULL;
		}
		cType *Cur() { return mlCur >= 0 && mlCur < (int)mvTypes.size() ? &mvTypes[mlCur] : NULL; }

		void Update(float afTimeStep) override
		{
			for (cType &t : mvTypes)
			{
				if (t.mbExists == false || t.mbActive == false)
					continue;
				if (t.mfDuration > 0)
				{
					t.mfDuration -= afTimeStep;
					if (t.mfDuration <= 0)
						t.mbActive = false;
				}
				t.mfTimer -= afTimeStep;
				if (t.mfTimer > 0)
					continue;
				t.mfTimer = t.mfFreq > 0.0001f ? 1 / t.mfFreq : 10000;
				for (cShape &s : t.mvShapes)
					ShapeDamage(t, s);
			}
		}

		void ShapeDamage(cType &t, const cShape &s)
		{
			iCharacterBody *pBody = PlayerBody();
			cSomaLuxPlayer *pPlayer = cSomaLuxPlayer::Get();
			if (t.mbPlayer == false || pBody == NULL || pPlayer == NULL)
				return;
			cMatrixf mtx = mpEntity->mpMesh ? mpEntity->mpMesh->GetWorldMatrix() : mpEntity->GetMatrix();
			if (s.mlSocket >= 0 && s.mlSocket < (int)mpEntity->mvSockets.size())
				mpEntity->GetSocketMatrix(mpEntity->mvSockets[s.mlSocket].msName, mtx);
			cVector3f vPos = cMath::MatrixMul(mtx, s.mvOffset);
			cVector3f vBody = pBody->GetPosition(), vSize = pBody->GetSize();
			cVector3f vClosest(vBody.x, cMath::Clamp(vPos.y, vBody.y - vSize.y * 0.5f, vBody.y + vSize.y * 0.5f), vBody.z);
			if (cMath::Vector3Dist(vPos, vClosest) > s.mfRadius + vSize.x * 0.5f)
				return;
			cAgent *pAgent = Agent(mpEntity);
			cVector3f vOrigin = pAgent && pAgent->mpBody ? pAgent->mpBody->GetPosition() : mpEntity->GetPosition();
			if (SomaLineOfSight(vOrigin, vClosest, mpEntity) == false)
				return;
			if (pPlayer->GiveDamage(cMath::RandRectf(t.mfMin, t.mfMax), t.mlDamageType, t.mbLethal ? 0 : 0.01f, mpEntity->mID, t.mlInstance) == false)
				return;
			cVector3f vDir = vClosest - vOrigin;
			vDir.y = 0;
			if (vDir.SqrLength() > 0.0001f)
				pBody->AddForceVelocity(cMath::Vector3Normalize(vDir) * std::min(t.mfForce / pBody->GetMass(), t.mfMaxImpulse));
		}
	};
}

iPhysicsBody *SomaAgentBody(cSomaLuxEntity *apEnt)
{
	cAgent *a = Agent(apEnt);
	return a && a->mpBody ? a->mpBody->GetCurrentBody() : NULL;
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
	if (OhplBackendRustRequested())
	{
		OhplS4PathPod pp = {pPF->mbMoving, pPF->mbExact, pPF->mbCallbackInMap, pPF->mbTrackActive, pPF->mbTrackPaused, pPF->mbTrackLoop, pPF->mbAtTrackNode,
							{pPF->mvGoal[0], pPF->mvGoal[1], pPF->mvGoal[2]}, pPF->mlTrackIdx, pPF->mfTrackWait, pPF->mfTrackFreq};
		auto S = [](const tString &x) { return OhplS4Str{(const unsigned char *)x.data(), x.size()}; };
		std::vector<OhplS4Track> vTracks;
		for (const cAgentTrackNode &n : pPF->mvTrack)
			vTracks.push_back(OhplS4Track{S(n.msNode), n.mfMinWait, n.mfMaxWait, S(n.msAnim), n.mbLoopAnim});
		ohpl_s4_path_save(&pp, S(pPF->msResultCallback), S(pPF->msEndOfPathCallback), S(pPF->msTrackCallback), vTracks.data(), vTracks.size(), &s,
						  +[](void *c, const unsigned char *p, size_t n) { ((std::string *)c)->assign((const char *)p, n); });
		return s;
	}
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
	if (OhplBackendRustRequested())
	{
		struct cCtx
		{
			cAgentPathfinder *mpPF;
			tString msResult;
		} ctx{pPF, ""};
		OhplS4PathPod pp = {false, false, false, pPF->mbTrackActive, pPF->mbTrackPaused, pPF->mbTrackLoop, pPF->mbAtTrackNode, {0, 0, 0}, pPF->mlTrackIdx, pPF->mfTrackWait, pPF->mfTrackFreq};
		pPF->mvTrack.clear();
		ohpl_s4_path_load((const unsigned char *)asData.data(), asData.size(), &pp, &ctx,
						  +[](void *c, uint32_t k, const unsigned char *p, size_t n) {
							  cCtx *x = (cCtx *)c;
							  tString &t = k == 0 ? x->msResult : k == 1 ? x->mpPF->msEndOfPathCallback : x->mpPF->msTrackCallback;
							  t.assign((const char *)p, n);
						  },
						  +[](void *c, const OhplS4Track *t) {
							  ((cCtx *)c)->mpPF->mvTrack.push_back(cAgentTrackNode{tString((const char *)t->node.ptr, t->node.len), t->min_wait, t->max_wait,
																				   tString((const char *)t->anim.ptr, t->anim.len), t->loop_anim});
						  });
		pPF->mlTrackIdx = pp.track_idx;
		pPF->mbTrackActive = pp.track_active, pPF->mbTrackPaused = pp.track_paused, pPF->mbTrackLoop = pp.track_loop, pPF->mbAtTrackNode = pp.at_node;
		pPF->mfTrackWait = pp.track_wait, pPF->mfTrackFreq = pp.track_freq;
		if (pp.moving)
			pPF->MoveTo(cVector3f(pp.goal[0], pp.goal[1], pp.goal[2]), pp.exact, ctx.msResult, pp.in_map);
		else
			pPF->Stop();
		return;
	}
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
		if (OhplBackendRustRequested())
		{
			std::vector<float> vNodes;
			for (cAINode *n : lst)
				vNodes.insert(vNodes.end(), {n->GetPosition().x, n->GetPosition().y, n->GetPosition().z});
			ohpl_s4_nav_path_text(vNodes.data(), lst.size(), &s, +[](void *c, const unsigned char *p, size_t n) { ((tString *)c)->assign((const char *)p, n); });
			return s;
		}
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
	if (OhplBackendRustRequested())
	{
		OhplS4DebugIn d = {};
		auto S = [](const tString &x) { return OhplS4Str{(const unsigned char *)x.data(), x.size()}; };
		tString sCur, sPrev, sNext, sAnim;
		if (cAgentStateMachine *pSM = pAgent->Find<cAgentStateMachine>(eComp_StateMachine))
		{
			sCur = pSM->Name(pSM->mlCur), sPrev = pSM->Name(pSM->mlPrev), sNext = pSM->Name(pSM->mlNext);
			d.has_sm = true;
			d.sm_cur = S(sCur), d.sm_prev = S(sPrev), d.sm_next = S(sNext);
			d.sm_timers = pSM->TimerCount(false);
		}
		if (cAgentPathfinder *pPF = pAgent->Find<cAgentPathfinder>(eComp_Pathfinder))
		{
			d.has_pf = true;
			d.pf_moving = pPF->mbMoving, d.pf_track_active = pPF->mbTrackActive, d.pf_paused = pPF->mbTrackPaused, d.pf_at_node = pPF->mbAtTrackNode;
			d.pf_path_idx = (int)pPF->mlPathIdx, d.pf_path_len = (int)pPF->mvPath.size();
			d.pf_goal[0] = pPF->mvGoal[0], d.pf_goal[1] = pPF->mvGoal[1], d.pf_goal[2] = pPF->mvGoal[2];
			d.pf_track_idx = pPF->mlTrackIdx, d.pf_track_len = (int)pPF->mvTrack.size(), d.pf_wait = pPF->mfTrackWait;
		}
		if (cAgentCharMover *pM = pAgent->Find<cAgentCharMover>(eComp_CharMover))
		{
			d.has_cm = true;
			d.cm_moving = pM->mbMoving, d.cm_turning = pM->mbTurning, d.cm_wall = pM->mbWallAvoid, d.cm_object_avoid = pM->mbObjectAvoid;
			d.cm_goal[0] = pM->mvGoal[0], d.cm_goal[1] = pM->mvGoal[1], d.cm_goal[2] = pM->mvGoal[2];
			d.cm_speed_state = pM->mlSpeedState, d.cm_fwd = pM->ForwardSpeed(), d.cm_stuck = pM->mfStuck;
			sAnim = apEnt->mpMesh && apEnt->mlCurrentAnim >= 0 ? apEnt->mpMesh->GetAnimationState(apEnt->mlCurrentAnim)->GetName() : tString("");
			d.cm_anim = S(sAnim);
			d.cm_idle_extra_count = pM->mlIdleExtraCount, d.cm_idle_extra_wait = pM->mfIdleExtraWait;
			d.cm_wall_hits = (int)std::count_if(pM->mvWallSamples.begin(), pM->mvWallSamples.end(), [](auto &w) { return w.mbHit; });
			d.cm_roll = pM->mfRoll;
		}
		ohpl_s4_debug_text(&d, &s, +[](void *c, const unsigned char *p, size_t n) { ((tString *)c)->assign((const char *)p, n); });
		return s;
	}
	if (cAgentStateMachine *pSM = pAgent->Find<cAgentStateMachine>(eComp_StateMachine))
		s += "state=" + pSM->Name(pSM->mlCur) + " prev=" + pSM->Name(pSM->mlPrev) + " next=" + pSM->Name(pSM->mlNext) + " timers=" + cString::ToString(pSM->TimerCount(false));
	if (cAgentPathfinder *pPF = pAgent->Find<cAgentPathfinder>(eComp_Pathfinder))
		s += " pf_moving=" + cString::ToString(pPF->mbMoving) + " path=" + cString::ToString((int)pPF->mlPathIdx) + "/" + cString::ToString((int)pPF->mvPath.size()) +
			 " goal=" + cVector3f(pPF->mvGoal[0], pPF->mvGoal[1], pPF->mvGoal[2]).ToString() + " track=" + cString::ToString(pPF->mlTrackIdx) + "/" + cString::ToString((int)pPF->mvTrack.size()) +
			 " track_active=" + cString::ToString(pPF->mbTrackActive) + " paused=" + cString::ToString(pPF->mbTrackPaused) + " at_node=" + cString::ToString(pPF->mbAtTrackNode) +
			 " wait=" + cString::ToString(pPF->mfTrackWait);
	if (cAgentCharMover *pM = pAgent->Find<cAgentCharMover>(eComp_CharMover))
		s += " cm_moving=" + cString::ToString(pM->mbMoving) + " cm_goal=" + cVector3f(pM->mvGoal[0], pM->mvGoal[1], pM->mvGoal[2]).ToString() + " speed_state=" + cString::ToString(pM->mlSpeedState) +
			 " fwd=" + cString::ToString(pM->ForwardSpeed()) + " stuck=" + cString::ToString(pM->mfStuck) + " turning=" + cString::ToString(pM->mbTurning) +
			 " anim=" + (apEnt->mpMesh && apEnt->mlCurrentAnim >= 0 ? apEnt->mpMesh->GetAnimationState(apEnt->mlCurrentAnim)->GetName() : tString("")) +
			 " idle_extra=" + cString::ToString(pM->mlIdleExtraCount) + "/" + cString::ToString(pM->mfIdleExtraWait) + " wall=" + cString::ToString(pM->mbWallAvoid) + "/" +
			 cString::ToString((int)std::count_if(pM->mvWallSamples.begin(), pM->mvWallSamples.end(), [](auto &w) { return w.mbHit; })) + " roll=" + cString::ToString(pM->mfRoll) + " doa=" + cString::ToString(pM->mbObjectAvoid);
	return s;
}

void SomaUpdateAgent(cSomaLuxEntity *apEnt, float afTimeStep)
{
	cAgent *pAgent = Agent(apEnt);
	if (pAgent == NULL || apEnt->mbActive == false)
		return;
	pAgent->PreUpdate(afTimeStep);
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
		if (pListener == NULL)
			continue;
		cVector3f vAgent = AgentPos(pAgent->mpEnt);
		if (ohpl_gs2_sound_heard(pListener, pListener->mbActive, pAgent->mpEnt->mbActive, vAgent.v, avPos.v, afRadius, alPrio))
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
	SOMA_METHOD(e, A, "float GetPlayerMovementTowardEntity()", +[](E *p) {
		iCharacterBody *b = PlayerBody();
		if (b == NULL)
			return -1.0f;
		cVector3f v = b->GetVelocity(gpSomaBase->mpEngine->GetStepSize()), d = AgentPos(p) - b->GetPosition();
		v.y = d.y = 0;
		if (v.SqrLength() < 0.01f)
			return -1.0f;
		return cMath::Vector3Dot(cMath::Vector3Normalize(v), cMath::Vector3Normalize(d));
	});
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
	// ponytail: official 0x8b0780 defers the callback via a LOS query; answered synchronously
	SOMA_METHOD_NEW(e, A, "void GetPlayerIsInLineOfSight(const tString&in asCallbackFunc)", +[](E *p, S f) {
		cAgent *a = Agent(p);
		iCharacterBody *b = PlayerBody();
		bool bLOS = a && b && cSomaLuxPlayer::Get()->IsDead() == false && SomaLineOfSight(a->Eye(), b->GetPosition(), p);
		p->Call("void " + f + "(bool)", [bLOS](asIScriptContext *c) { c->SetArgByte(0, bLOS); });
	});
	SOMA_METHOD_NEW(e, A, "void GetPlayerIsInLineOfSight(const tString&in asCallbackFunc, float afFOV, const cVector3f &in avForward, bool abCheckFOV)", +[](E *p, S f, float fov, V fwd, bool bFov) {
		cAgent *a = Agent(p);
		iCharacterBody *b = PlayerBody();
		bool bLOS = a && b && cSomaLuxPlayer::Get()->IsDead() == false;
		if (bLOS && bFov && cMath::Vector3Angle(cMath::Vector3Normalize(b->GetPosition() - a->Eye()), cMath::Vector3Normalize(fwd)) > fov * 0.5f)
			bLOS = false;
		bLOS = bLOS && SomaLineOfSight(a->Eye(), b->GetPosition(), p);
		p->Call("void " + f + "(bool)", [bLOS](asIScriptContext *c) { c->SetArgByte(0, bLOS); });
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
	SOMA_METHOD(e, A, "const cVector3f& GetLastKnownPlayerPos()", +[](E *p) -> const cVector3f & { static cVector3f v; cAgent *a = Agent(p); v = a ? cVector3f(a->mvLastKnownPlayerPos.x, a->mvLastKnownPlayerPos.y, a->mvLastKnownPlayerPos.z) : cVector3f(0); return v; });
	SOMA_METHOD(e, A, "float GetDistFromLastKnownToActualPlayerPos()", +[](E *p) { cAgent *a = Agent(p); return a ? cMath::Vector3Dist(cVector3f(a->mvLastKnownPlayerPos.x, a->mvLastKnownPlayerPos.y, a->mvLastKnownPlayerPos.z), PlayerFeet()) : 0.0f; });
	SOMA_METHOD(e, A, "void RevealPlayerPos()", +[](E *p) {
		if (cAgent *a = Agent(p))
		{
			a->mvLastKnownPlayerPos = {PlayerFeet().x, PlayerFeet().y, PlayerFeet().z};
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
	SOMA_METHOD(e, A, "const cVector3f& GetGroundAlignPosition()", +[](E *p) -> const cVector3f & { static cVector3f v; cAgent *a = Agent(p); v = a ? cVector3f(a->mvGroundAlignPos.x, a->mvGroundAlignPos.y, a->mvGroundAlignPos.z) : cVector3f(0); return v; });
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

	static auto AddGlow = +[](E *p, const cColor &col, float y) {
		if (p == NULL)
			return;
		gEdgeGlowRenderer.Register();
		for (auto &g : gEdgeGlowRenderer.mvGlow)
			if (g.mID == p->mID)
			{
				g.mColor = col;
				g.mfY = y;
				return;
			}
		gEdgeGlowRenderer.mvGlow.push_back({p->mID, col, y});
	};
	SOMA_METHOD(e, "cLuxEffectHandler", "void AddGlowObject(iLuxEntity@ apEntity, float afAlpha, float afY)", +[](void *, E *p, float a, float y) { AddGlow(p, cColor(0.2f, 0.2f, 1, a), y); });
	SOMA_METHOD(e, "cLuxEffectHandler", "void AddGlowObject(iLuxEntity@ apEntity, const cColor&in aColor, float afY)", +[](void *, E *p, const cColor &col, float y) { AddGlow(p, col, y); });
	SOMA_FUNC(e, "cLuxEdgeGlow@ cLux_CreateEntityComponent_EdgeGlow(iLuxEntity @apEntity)", +[](E *p) {
		gEdgeGlowRenderer.Register();
		return AddComponent(p, new cAgentEdgeGlow(p));
	});
	SOMA_FUNC(e, "cLuxForceEmitter@ cLux_CreateEntityComponent_ForceEmitter(iLuxEntity @apEntity)", +[](E *p) { return AddComponent(p, new cAgentForceEmitter(p)); });
	if (e->GetTypeInfoByName("cLuxProximitySensor"))
	{
		typedef cAgentProximitySensor PS;
		SOMA_FUNC(e, "cLuxProximitySensor@ cLux_CreateEntityComponent_ProximitySensor(iLuxEntity@ apEntity)", +[](E *p) { return p ? AddComponent(p, new PS(p)) : NULL; });
		SOMA_METHOD(e, "cLuxProximitySensor", "void AddState(int alId, float afEnterDistance, float afLeaveDistance, bool abLineOfSightCheck, bool abPlayerDetectedCheck)",
					+[](PS *c, int id, float fEnter, float fLeave, bool bLOS, bool bDet) {
			if (id < 0)
				return;
			if ((int)c->mvStates.size() <= id)
				c->mvStates.resize(id + 1);
			c->mvStates[id] = {true, true, false, bLOS, bDet, fEnter, fLeave};
		});
		SOMA_METHOD(e, "cLuxProximitySensor", "bool GetPlayerInsideState(int alId)", +[](PS *c, int id) { auto *s = c->Get(id); return s && s->mbInside; });
		SOMA_METHOD(e, "cLuxProximitySensor", "void SetStateActive(int alId, bool abX)", +[](PS *c, int id, bool b) { if (auto *s = c->Get(id)) s->mbActive = b; });
		SOMA_METHOD(e, "cLuxProximitySensor", "bool GetStateActive(int alId)", +[](PS *c, int id) { auto *s = c->Get(id); return s && s->mbActive; });
		SOMA_METHOD(e, "cLuxProximitySensor", "void ResetState(int alId)", +[](PS *c, int id) { if (auto *s = c->Get(id)) s->mbInside = false; });
		SOMA_METHOD(e, "cLuxProximitySensor", "void ResetAllStates()", +[](PS *c) { for (auto &s : c->mvStates) s.mbInside = false; });
		SOMA_METHOD(e, "cLuxProximitySensor", "iLuxEntity@ GetEntity()", +[](cAgentComponent *c) { return c->mpEntity; });
		SOMA_METHOD(e, "cLuxProximitySensor", "int GetType()", +[](cAgentComponent *c) { return c->mlType; });
	}
	if (e->GetTypeInfoByName("cLuxAttackDamage"))
	{
		typedef cAgentAttackDamage AD;
		const char *T = "cLuxAttackDamage";
		SOMA_FUNC(e, "cLuxAttackDamage@ cLux_CreateEntityComponent_AttackDamage(iLuxEntity @apEntity)", +[](E *p) { return p ? AddComponent(p, new AD(p)) : NULL; });
		SOMA_METHOD(e, T, "void AddType(int alId, const tString&in asName)", +[](AD *c, int id, S) {
			if (id < 0)
				return;
			if ((int)c->mvTypes.size() <= id)
				c->mvTypes.resize(id + 1);
			c->mvTypes[id] = {};
			c->mvTypes[id].mbExists = true;
			c->mlCur = id;
		});
		SOMA_METHOD(e, T, "void SetType_Damage(float afMinDamage, float afMaxDamage=-1, bool abLethal=true)", +[](AD *c, float fMin, float fMax, bool bLethal) {
			if (auto *t = c->Cur())
			{
				t->mfMin = fMin;
				t->mfMax = fMax <= 0 ? fMin : fMax;
				t->mbLethal = bLethal;
			}
		});
		SOMA_METHOD(e, T, "void SetType_HitSpeed(float afX)", +[](AD *, float) {});
		SOMA_METHOD(e, T, "void SetType_DamageType(int alX)", +[](AD *c, int x) { if (auto *t = c->Cur()) t->mlDamageType = x; });
		SOMA_METHOD(e, T, "void SetType_Strength(int alX)", +[](AD *, int) {});
		// official ignores afMaxImpulse and always stores 10
		SOMA_METHOD(e, T, "void SetType_Force(float afForce, float afMaxImpulse)", +[](AD *c, float f, float) { if (auto *t = c->Cur()) { t->mfForce = f; t->mfMaxImpulse = 10; } });
		SOMA_METHOD(e, T, "void SetType_Affects(bool abProps, bool abAgents, bool abPlayer)", +[](AD *c, bool, bool, bool b) { if (auto *t = c->Cur()) t->mbPlayer = b; });
		SOMA_METHOD(e, T, "void SetType_AddShapeFromCenter(const cVector3f&in avOffset, float afRadius)", +[](AD *c, V v, float r) { if (auto *t = c->Cur()) t->mvShapes.push_back({-1, v, r}); });
		SOMA_METHOD(e, T, "void SetType_AddShapeAtSocket(const tString&in asSocket, const cVector3f&in avOffset, float afRadius)", +[](AD *c, S n, V v, float r) {
			auto *t = c->Cur();
			if (t == NULL)
				return;
			auto &sk = c->mpEntity->mvSockets;
			if (sk.empty())
			{
				Error("Couldn't add shape to attack damage component for entity '%s' because mesh or sockets are missing.\n", c->mpEntity->msName.c_str());
				return;
			}
			for (size_t i = 0; i < sk.size(); ++i)
				if (sk[i].msName == n)
				{
					t->mvShapes.push_back({(int)i, v, r});
					return;
				}
			Error("Shape attack damage component for entity '%s' failed because socket '%s' doesn't exist\n", c->mpEntity->msName.c_str(), n.c_str());
		});
		SOMA_METHOD(e, T, "void Start(int alId, float afDuration, float afCheckFreq=8.0f)", +[](AD *c, int id, float d, float f) {
			if (auto *t = c->Get(id))
			{
				t->mbActive = true;
				t->mfDuration = d;
				t->mfFreq = f;
				t->mfTimer = 0;
				t->mlInstance = AD::glInstanceCount++;
			}
		});
		SOMA_METHOD(e, T, "void Stop(int alId)", +[](AD *c, int id) { if (auto *t = c->Get(id)) t->mbActive = false; });
		SOMA_METHOD(e, T, "void StopAll()", +[](AD *c) { for (auto &t : c->mvTypes) t.mbActive = false; });
		SOMA_METHOD(e, T, "int GetTypeInstanceID(int alId)", +[](AD *c, int id) { auto *t = c->Get(id); return t && t->mbActive ? t->mlInstance : -1; });
		SOMA_METHOD(e, T, "void SetDamage(int alId, float afMinDamage, float afMaxDamage, bool abLethal)", +[](AD *c, int id, float fMin, float fMax, bool bLethal) {
			if (auto *t = c->Get(id))
			{
				t->mfMin = fMin;
				t->mfMax = fMax <= 0 ? fMin : fMax;
				t->mbLethal = bLethal;
			}
		});
		SOMA_METHOD(e, T, "iLuxEntity@ GetEntity()", +[](cAgentComponent *c) { return c->mpEntity; });
		SOMA_METHOD(e, T, "int GetType()", +[](cAgentComponent *c) { return c->mlType; });
	}
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
	if (e->GetTypeInfoByName("cLuxFootPlacement"))
	{
		// ponytail: no IK, only the active flag
		SOMA_FUNC(e, "cLuxFootPlacement@ cLux_CreateEntityComponent_FootPlacement(iLuxEntity @apEntity)", +[](E *p) { return AddComponent(p, new cAgentComponent(p, eComp_FootPlacement)); });
		SOMA_FUNC(e, "cLuxHandPlacement@ cLux_CreateEntityComponent_HandPlacement(iLuxEntity @apEntity)", +[](E *p) { return AddComponent(p, new cAgentComponent(p, eComp_HandPlacement)); });
		SOMA_METHOD(e, "cLuxFootPlacement", "void SetFootActive(bool abX, bool abRight, float afInactiveLength = -1.f, bool abFront = false)", +[](cAgentComponent *c, bool b, bool r, float, bool f) { c->mbFootActive[r + 2 * f] = b; });
		SOMA_METHOD(e, "cLuxFootPlacement", "bool GetFootActive(bool abRight, bool abFront = false)", +[](cAgentComponent *c, bool r, bool f) { return c->mbFootActive[r + 2 * f]; });
		for (const char *pType : {"cLuxFootPlacement", "cLuxHandPlacement"})
		{
			SOMA_METHOD(e, pType, "iLuxEntity@ GetEntity()", +[](cAgentComponent *c) { return c->mpEntity; });
			SOMA_METHOD(e, pType, "eLuxEntityComponentType GetType()", +[](cAgentComponent *c) { return c->mlType; });
			SOMA_METHOD(e, pType, "void SetActive(bool abX)", +[](cAgentComponent *c, bool b) { c->mbActive = b; });
			SOMA_METHOD(e, pType, "bool IsActive()", +[](cAgentComponent *c) { return c->mbActive; });
			SOMA_METHOD(e, pType, "void FadeIn(float afTime)", +[](cAgentComponent *c, float) { c->mbActive = true; });
			SOMA_METHOD(e, pType, "void FadeOut(float afTime)", +[](cAgentComponent *c, float) { c->mbActive = false; });
		}
		for (const char *pType : {"iLuxEntity", "cLuxProp", "cLuxArea", "cLuxAgent", "cLuxCritter", "cLuxLiquidArea"})
			SOMA_METHOD(e, pType, "iLuxEntityComponent@ GetComponentByType(eLuxEntityComponentType aType) const", +[](E *p, int t) {
				cAgent *a = Agent(p);
				return a ? a->Find<cAgentComponent>(t) : NULL;
			});
	}
	SOMA_FUNC(e, "cLuxBackboneTail@ cLux_CreateEntityComponent_BackboneTail(iLuxEntity @apEntity)", +[](E *p) { return AddComponent(p, new cAgentBackboneTail(p)); });
	for (const char *pType : {"iLuxEntityComponent", "cLuxStateMachine", "cLuxCharMover", "cLuxPathfinder", "cLuxBarkMachine", "cLuxSoundListener", "cLuxHeadTracker",
							  "cLuxEdgeGlow", "cLuxForceEmitter", "cLuxLightSensor", "cLuxBackboneTail"})
	{
		SOMA_METHOD(e, pType, "iLuxEntity@ GetEntity()", +[](cAgentComponent *c) { return c->mpEntity; });
		SOMA_METHOD(e, pType, "eLuxEntityComponentType GetType()", +[](cAgentComponent *c) { return c->mlType; });
	}
	if (e->GetTypeInfoByName("cLuxUserComponent"))
	{
		typedef cSomaLuxScriptable US;
		SOMA_FUNC(e, "iScrUserComponent_Interface@ cLux_CreateUserComponent(iLuxEntity@ apEntity, const tString&in asComponentName, const tString&in asScriptFile, const tString&in asScriptClass, int alID)",
				  +[](E *p, S sName, S sFile, S sClass, int lID) -> asIScriptObject * {
			if (p == NULL)
				return NULL;
			cAgentUserComponent *pComp = new cAgentUserComponent(p, lID);
			pComp->msName = sName;
			if (pComp->LoadScript(cSomaScriptRuntime::Get(), sFile, sClass, "cLuxUserComponent") == false)
			{
				Error("Could not load user component script for '%s' '%s' of class '%s'\n", sName.c_str(), sFile.c_str(), sClass.c_str());
				delete pComp;
				return NULL;
			}
			return UserScript(AddComponent(p, pComp));
		});
		SOMA_FUNC(e, "iScrUserComponent_Interface@ cLux_GetUserComponent(iLuxEntity@ apEntity, int alID)", +[](E *p, int lID) {
			cAgent *a = p ? Agent(p) : NULL;
			return UserScript(a ? a->Find<cAgentComponent>(lID) : NULL);
		});
		SOMA_FUNC(e, "iScrUserComponent_Interface@ cLux_GetUserComponentScriptInterface(iLuxEntityComponent@ apComponent)", +[](cAgentComponent *c) { return UserScript(c); });
		SOMA_METHOD(e, "cLuxUserComponent", "iLuxEntity@ GetEntity()", +[](US *s) { return static_cast<cAgentUserComponent *>(s)->mpEntity; });
		SOMA_METHOD(e, "cLuxUserComponent", "int GetType()", +[](US *s) { return static_cast<cAgentUserComponent *>(s)->mlType; });
		SOMA_METHOD(e, "cLuxUserComponent", "const tString& GetComponentName()", +[](US *s) -> const tString & { return static_cast<cAgentUserComponent *>(s)->msName; });
		cSomaLuxScriptable::RegisterTimerNatives(e, "cLuxUserComponent");
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
	SOMA_METHOD(e, T, "tString GetCurrentStateName()", +[](SM *s) { auto it = s->mapStates.find(s->mlCur); return it == s->mapStates.end() ? tString() : it->second; });
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
	SOMA_METHOD(e, T, "float GetMaxStuckCounter()", +[](CM *) { return 2.0f; });
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
	SOMA_METHOD(e, T, "void SetWallAvoidanceActive(bool abX)", +[](CM *m, bool b) { m->mbWallAvoid = b; });
	SOMA_METHOD(e, T, "void SetIdleExtraAnimActive(bool abX)", +[](CM *m, bool b) { m->mbIdleExtraActive = b; });
	SOMA_METHOD(e, T, "void SetDynamicObjectAvoidanceActive(bool abX)", +[](CM *m, bool b) { m->mbObjectAvoid = b; });
	SOMA_METHOD(e, T, "void SetBankingActive(bool abX)", +[](CM *m, bool b) { m->mbBanking = b; });
	SOMA_METHOD(e, T, "void SetBankingAngleMul(float afX)", +[](CM *m, float x) { m->mfBankAngleMul = x; });
	SOMA_METHOD(e, T, "void SetBankingMaxAngle(float afX)", +[](CM *m, float x) { m->mfBankMaxAngle = x; });
	SOMA_METHOD(e, T, "void SetBankingMaxSpeed(float afX)", +[](CM *m, float x) { m->mfBankMaxSpeed = x; });
	SOMA_METHOD(e, T, "void SetUse3DMovement(bool abX)", +[](CM *m, bool b) { m->mb3D = b; });
	for (const char *pNoop : {"void SetTurnStoppedToWalkSpeed(float afX)", "void SetTurnWalkToStoppedSpeed(float afX)", "void SetVerticalMoveSpeedExtraAnimMul(float afX)",
							  "void SetBankingSpeedMul(float afX)"})
		SOMA_METHOD(e, T, pNoop, +[](CM *, float) {});
	SOMA_METHOD(e, T, "void SetIdleExtraAnimName(const tString&in asName)", +[](CM *m, S n) { m->SetupIdleExtra(n, m->mfIdleExtraMin, m->mfIdleExtraMax); });
	SOMA_METHOD(e, T, "void SetupWallAvoidance(float afRadius, float afSteerAmount, int alSamples)", +[](CM *m, float r, float s, int n) {
		m->mfWallRadius = r;
		m->mfWallSteer = s;
		m->mvWallSamples.assign(cMath::Max(n, 0), {});
	});
	SOMA_METHOD(e, T, "void SetupDynamicObjectAvoidance(float afMaxDistance, float afMinMass, float afSteerAmount)", +[](CM *m, float d, float mass, float s) {
		m->mfObjectAvoidDist = d;
		m->mfObjectAvoidMinMass = mass;
		m->mfObjectAvoidSteer = s;
	});
	SOMA_METHOD(e, T, "void SetupIdleExtra(const tString&in asAnimName, float afMinWait, float afMaxWait, bool abPauseProceduralAnims)",
				+[](CM *m, S n, float a, float b, bool) { m->SetupIdleExtra(n, a, b); });
	SOMA_METHOD(e, T, "bool GetIdleExtraAnimActive()", +[](CM *m) { return m->mbIdleExtraActive; });

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
