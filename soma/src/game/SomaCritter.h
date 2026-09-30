#ifndef SOMA_CRITTER_H
#define SOMA_CRITTER_H

class asIScriptEngine;
class cSomaLuxEntity;

void SomaUpdateCritter(cSomaLuxEntity *apEnt, float afTimeStep);
void SomaInitCritterProps(cSomaLuxEntity *apEnt);
void SomaForgetCritter(cSomaLuxEntity *apEnt);
void SomaRegisterCritterNatives(asIScriptEngine *apEngine);

#endif
