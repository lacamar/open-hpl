#include "fakegl.h"

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

std::string gTrace;
int gGen = 0, gShaders = 0, gPrograms = 0;
int gWinW = 0, gWinH = 0;

struct Range { const char *base; size_t bytes; std::string label; };
static std::vector<Range> gRanges;

void AddRange(const void *base, size_t bytes, const char *label) { gRanges.push_back({(const char*)base, bytes, label}); }
void ClearRanges() { gRanges.clear(); }

static std::string Fmt(const char *f, ...) __attribute__((format(printf, 1, 2)));
static std::string Fmt(const char *f, ...)
{
	char buf[64];
	va_list ap;
	va_start(ap, f);
	vsnprintf(buf, sizeof(buf), f, ap);
	va_end(ap);
	return buf;
}

std::string A(unsigned v) { return Fmt("%u", v); }
std::string A(int v) { return Fmt("%d", v); }
std::string A(unsigned char v) { return Fmt("%u", (unsigned)v); }
std::string A(long v) { return Fmt("%ld", v); }
std::string A(float v) { uint32_t b; memcpy(&b, &v, 4); return Fmt("f%08x", b); }
std::string A(double v) { uint64_t b; memcpy(&b, &v, 8); return Fmt("d%016llx", (unsigned long long)b); }
std::string A(const void *p)
{
	if(p == NULL) return "null";
	for(const Range &r : gRanges)
		if((const char*)p >= r.base && (const char*)p < r.base + r.bytes)
			return r.label + "+" + std::to_string((const char*)p - r.base);
	return "p";
}

uint64_t Fnv(const char *s)
{
	uint64_t h = 1469598103934665603ull;
	for(; *s; ++s) { h ^= (unsigned char)*s; h *= 1099511628211ull; }
	return h;
}

void Rec(const char *name, std::initializer_list<std::string> args, const std::string &extra)
{
	gTrace += name;
	gTrace += '(';
	bool first = true;
	for(const std::string &a : args) { if(!first) gTrace += ','; first = false; gTrace += a; }
	gTrace += ')';
	gTrace += extra;
	gTrace += '\n';
}

const char *FakeExtensions()
{
	static const char *kFull =
		"GL_ARB_multitexture GL_ARB_vertex_buffer_object GL_EXT_stencil_two_side GL_EXT_texture_filter_anisotropic "
		"GL_ARB_multisample GL_ARB_texture_compression GL_EXT_texture_compression_s3tc GL_SGIS_generate_mipmap "
		"GL_EXT_framebuffer_object GL_EXT_packed_depth_stencil GL_ARB_texture_float GL_ARB_fragment_program "
		"GL_ARB_fragment_shader GL_EXT_gpu_shader4 GL_EXT_blend_func_separate GL_ARB_occlusion_query "
		"GL_ARB_texture_rectangle GL_ARB_shading_language_100";
	static const char *kLimited =
		"GL_ARB_multitexture GL_ARB_vertex_buffer_object GL_ATI_separate_stencil GL_EXT_framebuffer_object "
		"GL_ARB_texture_compression GL_ARB_fragment_shader GL_ATI_shader_texture_lod GL_ARB_occlusion_query";
	const char *set = getenv("FAKE_EXT_SET");
	return set && !strcmp(set, "B") ? kLimited : kFull;
}

static std::vector<std::string> ExtTokens()
{
	std::vector<std::string> v;
	std::string cur;
	for(const char *c = FakeExtensions(); ; ++c)
	{
		if(*c == ' ' || *c == 0) { if(!cur.empty()) v.push_back(cur); cur.clear(); if(!*c) break; }
		else cur += *c;
	}
	return v;
}
int FakeExtCount() { return (int)ExtTokens().size(); }

extern "C" {

const unsigned char *glGetStringi(unsigned name, unsigned index)
{
	static std::vector<std::string> sTokens = ExtTokens();
	return index < sTokens.size() ? (const unsigned char*)sTokens[index].c_str() : (const unsigned char*)"";
}

static long Unknown() { Rec("unknown_gl", {}, ""); return 0; }

void *SDL_GL_GetProcAddress(const char *name)
{
	if(!strcmp(name, "glGetStringi")) return (void*)&glGetStringi;
	for(const FakeEntry *e = gFakeGlTable; e->name; ++e)
		if(!strcmp(e->name, name)) return e->fn;
	if(!strncmp(name, "gl", 2) && strncmp(name, "glX", 3)) return (void*)&Unknown;
	return NULL;
}

void *glXGetProcAddress(const unsigned char *name) { return SDL_GL_GetProcAddress((const char*)name); }
void *glXGetProcAddressARB(const unsigned char *name) { return SDL_GL_GetProcAddress((const char*)name); }

int SDL_GL_SetAttribute(int a, int v) { Rec("SDL_GL_SetAttribute", {A(a), A(v)}, ""); return 0; }
const char *SDL_GetCurrentVideoDriver() { Rec("SDL_GetCurrentVideoDriver", {}, ""); return "wayland"; }
const char *SDL_GetError() { return "fake error"; }
void *SDL_CreateWindow(const char *title, int x, int y, int w, int h, unsigned flags)
{
	Rec("SDL_CreateWindow", {A(x), A(y), A(w), A(h), A(flags)}, std::string(" ") + title);
	gWinW = w;
	gWinH = h;
	return (void*)0x1000;
}
void SDL_GetWindowSize(void *win, int *w, int *h) { Rec("SDL_GetWindowSize", {A(win)}, ""); *w = gWinW; *h = gWinH; }
void *SDL_GL_CreateContext(void *win) { Rec("SDL_GL_CreateContext", {A(win)}, ""); return (void*)0x2000; }
int SDL_ShowCursor(int t) { Rec("SDL_ShowCursor", {A(t)}, ""); return 0; }
void SDL_SetWindowGrab(void *win, int g) { Rec("SDL_SetWindowGrab", {A(win), A(g)}, ""); }
int SDL_SetRelativeMouseMode(int e) { Rec("SDL_SetRelativeMouseMode", {A(e)}, ""); return 0; }
void SDL_SetWindowTitle(void *win, const char *t) { Rec("SDL_SetWindowTitle", {A(win)}, std::string(" ") + t); }
unsigned SDL_GetWindowFlags(void *win) { Rec("SDL_GetWindowFlags", {A(win)}, ""); return 0x4 | 0x400; }
int SDL_GL_SetSwapInterval(int i) { Rec("SDL_GL_SetSwapInterval", {A(i)}, ""); return 0; }
int SDL_SetWindowFullscreen(void *win, unsigned f) { Rec("SDL_SetWindowFullscreen", {A(win), A(f)}, ""); return 0; }
void SDL_SetWindowSize(void *win, int w, int h) { Rec("SDL_SetWindowSize", {A(win), A(w), A(h)}, ""); gWinW = w; gWinH = h; }
void SDL_GL_SwapWindow(void *win) { Rec("SDL_GL_SwapWindow", {A(win)}, ""); }
void SDL_DestroyWindow(void *win) { Rec("SDL_DestroyWindow", {A(win)}, ""); }

}
