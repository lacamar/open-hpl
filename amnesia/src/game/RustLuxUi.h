#ifndef LUX_RUST_UI_H
#define LUX_RUST_UI_H

// C ABI of rust/src/luxui/ffi.rs: the decisions, timing and layout math of the inventory,
// journal, item types, hints, messages, commentary icons and key field. Calls that produce
// ordered side effects fill an OhplGa8Ev array; OhplGa8Take copies it (strings included) before
// any event runs, since running one may call back into Rust.

#include <cstddef>
#include <cstring>
#include <cstdint>
#include <string>
#include <vector>

struct OhplGa8Str
{
	const unsigned char *p;
	size_t n;
};

struct OhplGa8Ev
{
	unsigned kind;
	int a, b;
	float v[10];
	OhplGa8Str s0, s1;
};
static_assert(sizeof(OhplGa8Ev) == 88, "OhplGa8Ev layout");

#define OHPL_GA8_EVMAX 64

struct OhplGa8Event
{
	unsigned kind;
	int a, b;
	float v[10];
	std::string s0, s1;
};

inline std::vector<OhplGa8Event> OhplGa8Take(const OhplGa8Ev *apEv, size_t alNum)
{
	std::vector<OhplGa8Event> vOut;
	vOut.reserve(alNum);
	for(size_t i=0; i<alNum; ++i)
	{
		OhplGa8Event e;
		e.kind = apEv[i].kind;
		e.a = apEv[i].a;
		e.b = apEv[i].b;
		for(int j=0; j<10; ++j) e.v[j] = apEv[i].v[j];
		if(apEv[i].s0.p) e.s0.assign((const char*)apEv[i].s0.p, apEv[i].s0.n);
		if(apEv[i].s1.p) e.s1.assign((const char*)apEv[i].s1.p, apEv[i].s1.n);
		vOut.push_back(e);
	}
	return vOut;
}

struct OhplGa8Buf
{
	OhplGa8Ev mvRaw[OHPL_GA8_EVMAX];
};

inline OhplGa8Str OhplGa8S(const std::string &asX)
{
	OhplGa8Str s = { (const unsigned char*)asX.data(), asX.size() };
	return s;
}

typedef void (*OhplGa8Sink)(void*, const unsigned char*, size_t);

inline void OhplGa8StrSink(void *apCtx, const unsigned char *apData, size_t alLen)
{
	((std::string*)apCtx)->assign((const char*)apData, alLen);
}

enum
{
	eOhplGa8Ps_None = 0,
	eOhplGa8Ps_Create = 1,
	eOhplGa8Ps_Kill = 2,
};

extern "C"
{
int ohpl_ga8_key_verify(unsigned char c);
int ohpl_ga8_key_route(int keyboard, int paste, int tab);
int ohpl_ga8_key_action(int composed, int len, int pos, int mark, unsigned char ascii, int* out_pos, int* out_epos, unsigned char* out_ch);
size_t ohpl_ga8_key_paste(const unsigned char* p, size_t n, unsigned char* out, size_t cap);
float ohpl_ga8_comm_spin(float y, int playing, float dt);
int ohpl_ga8_comm_set_playing(int current, int next);
void ohpl_ga8_comm_focus_text(const unsigned char* talker, size_t tn, const unsigned char* topic, size_t pn, OhplGa8Sink s, void* c);
void ohpl_ga8_comm_file(const unsigned char* file, size_t n, OhplGa8Sink s, void* c);
}

#endif // LUX_RUST_UI_H
