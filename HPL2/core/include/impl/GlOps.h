#ifndef HPL_GL_OPS_H
#define HPL_GL_OPS_H

#include <GL/glew.h>
#include "impl/RustGl.h"

namespace hpl {

	struct OhplGlOpSources
	{
		const void* const* element;
		const float* batchVertex;
		float* const* batchTexCoord;
	};

	inline void RunGlOps(const OhplGlOp* apOps, size_t alCount, const OhplGlOpSources& aSrc)
	{
		for(size_t i=0; i<alCount; ++i)
		{
			const OhplGlOp &op = apOps[i];
			switch(op.kind)
			{
			case kOhplGlOpClientActive:	glClientActiveTextureARB(op.array); break;
			case kOhplGlOpEnable:		glEnableClientState(op.array); break;
			case kOhplGlOpDisable:		glDisableClientState(op.array); break;
			case kOhplGlOpBindBuffer:	glBindBufferARB(GL_ARRAY_BUFFER_ARB, op.array); break;
			case kOhplGlOpPointer:
				{
					const void *pPtr = NULL;
					switch(op.src)
					{
					case kOhplGlSrcElement:			pPtr = aSrc.element[op.index]; break;
					case kOhplGlSrcBatchVertex:		pPtr = aSrc.batchVertex + op.index; break;
					case kOhplGlSrcBatchTexCoord:	pPtr = aSrc.batchTexCoord[op.index]; break;
					}
					switch(op.array)
					{
					case GL_VERTEX_ARRAY:			glVertexPointer(op.size, op.glType, op.stride, pPtr); break;
					case GL_COLOR_ARRAY:			glColorPointer(op.size, op.glType, op.stride, pPtr); break;
					case GL_SECONDARY_COLOR_ARRAY:	glSecondaryColorPointerEXT(op.size, op.glType, op.stride, pPtr); break;
					case GL_NORMAL_ARRAY:			glNormalPointer(op.glType, op.stride, pPtr); break;
					case GL_TEXTURE_COORD_ARRAY:	glTexCoordPointer(op.size, op.glType, op.stride, pPtr); break;
					}
				}
				break;
			}
		}
	}

}

#endif
