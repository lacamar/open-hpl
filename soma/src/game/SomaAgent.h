#ifndef SOMA_AGENT_H
#define SOMA_AGENT_H

#include "hpl.h"

using namespace hpl;

class asIScriptEngine;
class cSomaLuxEntity;

void SomaCreateAgent(cSomaLuxEntity *apEnt);
void SomaDestroyAgent(cSomaLuxEntity *apEnt);
void SomaUpdateAgent(cSomaLuxEntity *apEnt, float afTimeStep);
void SomaUpdateComponents(cSomaLuxEntity *apEnt, float afTimeStep);
tString SomaAgentDebug(cSomaLuxEntity *apEnt);
tString SomaNavPath(const cVector3f &avFrom, const cVector3f &avTo);
void SomaAgentSetActive(cSomaLuxEntity *apEnt, bool abX);
iCharacterBody *SomaAgentGetBody(cSomaLuxEntity *apEnt);
bool SomaAgentGetMatrix(cSomaLuxEntity *apEnt, cMatrixf &aMtx);
bool SomaAgentSetMatrix(cSomaLuxEntity *apEnt, const cMatrixf &aMtx);
int SomaAgentGetState(cSomaLuxEntity *apEnt);
void SomaAgentSaveExtra(cSomaLuxEntity *apEnt, float &afYaw, bool &abSenses, bool &abDetection);
void SomaAgentLoadExtra(cSomaLuxEntity *apEnt, float afYaw, bool abSenses, bool abDetection);
std::string SomaAgentSavePath(cSomaLuxEntity *apEnt);
void SomaAgentLoadPath(cSomaLuxEntity *apEnt, const std::string &asData);
void SomaAgentChangeState(cSomaLuxEntity *apEnt, int alState);
void SomaAgentSendMessage(cSomaLuxEntity *apEnt, int alMessage, const cVector3f &avX = 0, int alX = 0);
void SomaBroadcastSoundHeard(const cVector3f &avPos, float afRadius, int alPrio);
void SomaRegisterAgentNatives(asIScriptEngine *apEngine);

#endif
