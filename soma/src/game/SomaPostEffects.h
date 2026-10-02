#ifndef SOMA_POST_EFFECTS_H
#define SOMA_POST_EFFECTS_H

#include "hpl.h"
#include <array>
#include <initializer_list>

using namespace hpl;

class asIScriptEngine;

class cSomaPostEffect : public iPostEffect
{
public:
	cSomaPostEffect(const tString &asType);
	~cSomaPostEffect();

	void Reset() override { mbClear = true; }
	void Set(std::initializer_list<float> alParams);

	tString msType;
	float mfParams[8] = {};
	std::array<iTexture *, 3> mvTextures = {};

private:
	void OnSetActive(bool abX) override { if (abX == false) Reset(); }
	void OnSetParams() override {}
	iPostEffectParams *GetTypeSpecificParams() override { return NULL; }
	iTexture *RenderEffect(iTexture *apInputTexture, iFrameBuffer *apFinalTempBuffer) override;

	int mlType;
	bool mbClear = true;
	float mfT = 0;
};

namespace cSomaPostEffects
{
	cPostEffectComposite *GetViewportComposite();
	void RegisterNatives(asIScriptEngine *apEngine);
}

#endif
