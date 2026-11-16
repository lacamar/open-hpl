#ifndef SOMA_POST_EFFECTS_H
#define SOMA_POST_EFFECTS_H

#include "hpl.h"

using namespace hpl;

class asIScriptEngine;

// iPostEffect shell of the Rust post effects (rust/src/somaui/s8c_pfx.rs); `this` is their key
class cSomaPostEffect : public iPostEffect
{
public:
	cSomaPostEffect();

	void Reset() override;

	cPostEffectComposite *Composite() { return mpCurrentComposite; }
	void Final(iFrameBuffer *apBuffer) { SetFinalFrameBuffer(apBuffer); }
	void Buffer(iFrameBuffer *apBuffer) { SetFrameBuffer(apBuffer); }
	void Quad(iTexture *apTexture, bool abFlipY) { DrawQuad(0, 1, apTexture, abFlipY); }

protected:
	void OnSetActive(bool abX) override { if (abX == false) Reset(); }
	void OnSetParams() override {}
	iPostEffectParams *GetTypeSpecificParams() override { return NULL; }
	iTexture *RenderEffect(iTexture *apInputTexture, iFrameBuffer *apFinalTempBuffer) override;
};

namespace cSomaPostEffects
{
	cPostEffectComposite *GetViewportComposite();
	void RegisterNatives(asIScriptEngine *apEngine);
}

#endif
