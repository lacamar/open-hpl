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

#ifndef LUX_POST_EFFECTS_H
#define LUX_POST_EFFECTS_H

#include "LuxBase.h"
#include "RustLuxFxA12a.h"
#include "LuxA12aDump.h"

class iLuxPostEffect : public iPostEffect
{
public:
	iLuxPostEffect(cGraphics *apGraphics, cResources *apResources) : iPostEffect(apGraphics, apResources, NULL){}
	virtual ~iLuxPostEffect(){}

	virtual void Update(float afTimeStep){}

protected:
	void OnSetParams(){}
	iPostEffectParams *GetTypeSpecificParams() { return NULL; }
};

// The wave clock and the draw plan live in Rust (rust/src/luxfx/rend.rs); this shell owns the
// program and textures and replays the plan.
class cLuxPostEffect_Insanity : public iLuxPostEffect
{
public:
	cLuxPostEffect_Insanity(cGraphics *apGraphics, cResources *apResources);
	~cLuxPostEffect_Insanity(){ ohpl_a12a_ins_free(mlH); }

	void Update(float afTimeStep){ ohpl_a12a_ins_update(mlH, afTimeStep); }

	void SetWaveAlpha(float afX){ ohpl_a12a_ins_set(mlH, 0, afX); }
	void SetZoomAlpha(float afX){ ohpl_a12a_ins_set(mlH, 1, afX); }
	void SetWaveSpeed(float afX){ ohpl_a12a_ins_set(mlH, 2, afX); }
	void GetClock(float *apOut){ ohpl_a12a_ins_get(mlH, apOut); }

	void Replay(const OhplGa9bPOp *apOps, size_t alOps, iTexture *apInputTexture, iFrameBuffer *apFinalTempBuffer);

private:
	iTexture* RenderEffect(iTexture *apInputTexture, iFrameBuffer *apFinalTempBuffer);

	uint64_t mlH;
	iGpuProgram *mpProgram;
	std::vector<iTexture*> mvAmpMaps;
	iTexture* mpZoomMap;
};

class cLuxPostEffectHandler : public iLuxUpdateable
{
public:
	cLuxPostEffectHandler();
	~cLuxPostEffectHandler(){ hplDelete(mpInsanity); }

	void OnStart(){}
	void Update(float afTimeStep)
	{
		if(OhplA12Enabled() && OhplA12InTick() == false) return;
		if(mpInsanity->IsActive()) mpInsanity->Update(afTimeStep);
	}
	void Reset(){}

	cLuxPostEffect_Insanity* GetInsanity(){ return mpInsanity; }

private:
	void LoadMainConfig();
	void SaveMainConfig();

	cLuxPostEffect_Insanity *mpInsanity;
};

#endif // LUX_POST_EFFECTS_H
