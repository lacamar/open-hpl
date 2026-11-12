#ifndef OHPL_VK_BACKEND_H
#define OHPL_VK_BACKEND_H

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque iLowLevelGraphics* for the retained C++ Vulkan backend. */
typedef struct OhplVkBackend OhplVkBackend;

OhplVkBackend* ohpl_vk_create(const char* color_output, float paper_white, float peak_nits);

#ifdef __cplusplus
}
#endif
#endif
