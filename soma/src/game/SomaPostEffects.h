#ifndef SOMA_POST_EFFECTS_H
#define SOMA_POST_EFFECTS_H

#include "hpl.h"
#include <array>
#include <initializer_list>
#include <utility>
#include <vector>

using namespace hpl;

class asIScriptEngine;

class cSomaPostEffect
{
public:
	cSomaPostEffect(const tString &asType);
	~cSomaPostEffect();

	void Reset();
	void Set(std::initializer_list<float> alParams) { std::copy(alParams.begin(), alParams.end(), mfParams); }
	bool IsOn() const { return mbActive && mbDisabled == false; }

	tString msType;
	bool mbActive = true;
	bool mbDisabled = false;
	float mfParams[8] = {};
	std::array<iTexture *, 3> mvTextures = {};
};

class cSomaPostEffectComposite
{
public:
	void Add(cSomaPostEffect *apEffect, int alPrio);
	void Remove(cSomaPostEffect *apEffect);
	cSomaPostEffect *FromType(const tString &asType);

	static cSomaPostEffectComposite *GetViewport();

	std::vector<std::pair<int, cSomaPostEffect *>> mvEffects;
};

namespace cSomaPostEffects
{
	void RegisterNatives(asIScriptEngine *apEngine);
}

#endif
