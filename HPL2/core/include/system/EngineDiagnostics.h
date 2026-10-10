
#ifndef HPL_ENGINE_DIAGNOSTICS_H
#define HPL_ENGINE_DIAGNOSTICS_H

#include "system/SystemTypes.h"
#include "math/MathTypes.h"

namespace hpl {

	class cWorld;
	class cViewport;
	class cGraphics;
	class iTexture;
	class cRenderList;

	class cEngineDiagnostics
	{
	public:
		static tString JsonEscape(const tString &asIn);

		static void ReportShader(const tString &asName, const char *apStage, bool abOk, const tString &asInfoLog);
		static tString GetShaderReportJson(bool abFailedOnly);
		static int GetShaderFailCount() { return mlShaderFailCount; }

		static void CountDrawCall() { ++mlDrawCalls; }
		static void EndFrame();
		static void AddFrameTiming(double afLogicMs, int alSteps, double afRenderMs, double afSwapMs);
		static unsigned int GetRenderedFrameCount() { return mlRenderedFrames; }
		static int GetLastFrameDrawCalls() { return mlLastFrameDrawCalls; }

		static bool mbGpuTiming;
		static void GpuPassBegin(const char *apName);
		static void GpuPassEnd();

		static tString PollGLErrorsJson(bool abReset);

		static tString GetWorldStatsJson(cWorld *apWorld);
		static tString GetRenderStatsJson(cViewport *apViewport, cGraphics *apGraphics);
		static tString GetLightsJson(cWorld *apWorld, const cVector3f &avPos, int alMax, cRenderList *apRenderList=NULL);
		static tString GetEntityInfoJson(cWorld *apWorld, const tString &asName);

		static tString GetPixelStatsJson(const float *apPixels, int alWidth, int alHeight);
		static tString GetFrameStatsJson(const unsigned char *apPixels, int alWidth, int alHeight, int alBytesPerPixel);

	private:
		static int mlDrawCalls;
		static int mlLastFrameDrawCalls;
		static int mlShaderFailCount;
		static unsigned int mlRenderedFrames;
		static double mfTimingMs[3];
		static int mlTimingSteps, mlTimingFrames;
	};

}
#endif // HPL_ENGINE_DIAGNOSTICS_H
