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

#ifndef LUX_EFFECT_RENDERER_H
#define LUX_EFFECT_RENDERER_H

#include "LuxBase.h"
#include "RustLuxFxA12a.h"
#include "LuxA12aDump.h"

// The object lists, flash oscillation and pass plans live in Rust (rust/src/luxfx/rend.rs); this
// shell owns the GPU programs and buffers and replays the plans.

class cLuxEffectRenderer : public iLuxUpdateable
{
public:
	cLuxEffectRenderer();
	~cLuxEffectRenderer(){ ohpl_a12a_rend_free(mlH); }

	void Reset(){ ohpl_a12a_rend_clear(mlH, 0); }

	void Update(float afTimeStep)
	{
		if(OhplA12Enabled() && OhplA12InTick() == false) return;
		ohpl_a12a_rend_update(mlH, afTimeStep);
	}

	void ClearRenderLists(){ ohpl_a12a_rend_clear(mlH, 1); }

	void RenderSolid(cRendererCallbackFunctions* apFunctions){}
	void RenderTrans(cRendererCallbackFunctions* apFunctions){ ohpl_a12a_rend_render(mlH, (size_t)apFunctions); }

	void AddOutlineObject(iRenderable *apObject){ ohpl_a12a_rend_add(mlH, 2, (size_t)apObject, 0); }
	void ClearOutlineObjects(){ ohpl_a12a_rend_clear(mlH, 2); }

	void AddFlashObject(iRenderable *apObject, float afAlpha){ ohpl_a12a_rend_add(mlH, 0, (size_t)apObject, afAlpha); }
	void AddEnemyGlow(iRenderable *apObject, float afAlpha){ ohpl_a12a_rend_add(mlH, 1, (size_t)apObject, afAlpha); }

	void Replay(cRendererCallbackFunctions* apFunctions, const size_t *apObjects, const OhplGa9bROp *apOps, size_t alOps, const cRect2l &aClipRect);

private:
	uint64_t mlH;

	iFrameBuffer *mpDeferredAccumBuffer;
	iFrameBuffer *mpFrameBufferColor;

	int mlBlurSizeDiv;
	iGpuProgram *mpBlurProgram[2];
	iFrameBuffer *mpBlurBuffer[2];
	iTexture *mpBlurTexture[2];

	iTexture *mpOutlineColorTexture;

	iGpuProgram *mpOutlineColorProgram[2];
	iGpuProgram *mpOutlineStencilProgram;
	iGpuProgram *mpOutlineStencilAlphaProgram;

	iGpuProgram *mpFlashProgram;
	iGpuProgram *mpEnemyGlowProgram;

	cMatrixf m_mtxTemp;
};

#endif // LUX_EFFECT_RENDERER_H
