
#ifndef HPL_WORLD_LOADER_HPM_H
#define HPL_WORLD_LOADER_HPM_H

#include "resources/WorldLoader.h"

#include "resources/ResourcesTypes.h"

namespace hpl {

	class cXmlElement;
	class cHpmWorldBuilder;

	class cWorldLoaderHpm : public iWorldLoader
	{
	public:
		cWorldLoaderHpm();
		~cWorldLoaderHpm();

		cWorld* LoadWorld(const tWString& asFile, tWorldLoadFlag aFlags);

		static const tString& GetLastLoadReportJson() { return msLastLoadReportJson; }
		static cXmlElement* GetCurrentElement() { return mpCurrentElement; }
		static void (*mpObjectDoneCallback)(cWorld* apWorld);

	private:
		void BuildLoadReport(const tString& asMap, int alTotalTimeMs, cHpmWorldBuilder& aBuilder);

		static tString msLastLoadReportJson;
		static cXmlElement* mpCurrentElement;
	};

};
#endif // HPL_WORLD_LOADER_HPM_H
