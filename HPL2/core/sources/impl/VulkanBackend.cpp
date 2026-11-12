#include "impl/LowLevelGraphicsVulkan.h"
#include "../../../../cshim/vk_backend.h"

using namespace hpl;

extern "C" OhplVkBackend* ohpl_vk_create(const char* color_output, float paper_white, float peak_nits)
{
	cLowLevelGraphicsVulkan* pVulkan = hplNew( cLowLevelGraphicsVulkan,() );
	if(color_output) pVulkan->SetColorOutput(color_output, paper_white, peak_nits);
	return reinterpret_cast<OhplVkBackend*>(static_cast<iLowLevelGraphics*>(pVulkan));
}
