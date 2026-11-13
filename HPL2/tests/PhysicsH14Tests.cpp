#include <cstdint>
#include <cstdlib>
#include <map>
#include <set>
#include <string>

#include "PhysicsH14Scenes.h"
#include "physics/RustPhysicsH14.h"

using namespace hpl;

// OHPL_H14_VEC=<file> records every ohpl_h14_* call (inputs, host answers, outputs) through the
// linker --wrap hooks, for the replay in rust/src/physics/tests_h14.rs.

extern "C" {
OhplH14Aux* __real_ohpl_h14_aux_new();
void __real_ohpl_h14_aux_free(OhplH14Aux*);
void __real_ohpl_h14_reset_pids(OhplH14Aux*);
void __real_ohpl_h14_clear_smoothing(OhplH14Aux*);
void __real_ohpl_h14_clear_camera(OhplH14Aux*);
void __real_ohpl_h14_setup_safe_positions(OhplH14Aux*, float, float);
void __real_ohpl_h14_update_safe_positions(OhplH14Aux*, float, OhplV3);
bool __real_ohpl_h14_safe_position(const OhplH14Aux*, float, OhplV3*);
void __real_ohpl_h14_body_connection_pos(const OhplH14BodyVt*, const OhplH14Conn*, OhplV3*);
void __real_ohpl_h14_char_connection_pos(const OhplCharState*, const OhplH14Conn*, OhplV3*);
void __real_ohpl_h14_update_body_connection(OhplH14Aux*, const OhplH14BodyVt*, const OhplCharState*, const OhplH14Conn*, bool, float);
bool __real_ohpl_h14_update_character_connection(OhplH14Aux*, const OhplH14BodyVt*, OhplCharState*, const OhplH14Conn*, float, OhplV3*);
void __real_ohpl_h14_update_gravity_attachment(const OhplH14BodyVt*, OhplCharState*, OhplH14Grav*);
void __real_ohpl_h14_update_camera(OhplH14Aux*, const OhplH14CamIn*, OhplH14CamOut*);
void __real_ohpl_h14_update_entity(OhplH14Aux*, const OhplH14EntIn*, cMatrixf*);
}

namespace {

FILE* gVec = NULL;
bool gVecInit = false;
std::string gCalls;
const OhplH14BodyVt* gReal = NULL;
std::map<const void*, int> gCount;
std::set<const void*> gSafe;

const int kStateful = 48;
const int kGravEvery = 120;
int gGravSeen = 0;

bool Rec()
{
	if (!gVecInit)
	{
		gVecInit = true;
		const char* path = std::getenv("OHPL_H14_VEC");
		if (path) gVec = std::fopen(path, "w");
	}
	return gVec != NULL;
}

std::string Hex(const void* p, size_t n)
{
	static const char* d = "0123456789abcdef";
	std::string s;
	for (size_t i = 0; i < n; ++i)
	{
		s += d[((const unsigned char*)p)[i] >> 4];
		s += d[((const unsigned char*)p)[i] & 15];
	}
	return s;
}

template <class T> std::string H(const T& v) { return Hex(&v, sizeof(T)); }

void Call(const char* name, const std::string& args, const std::string& ret)
{
	if (!gCalls.empty()) gCalls += ';';
	gCalls += std::string(name) + "," + (args.empty() ? "-" : args) + "," + ret;
}

std::string Calls()
{
	std::string s = gCalls.empty() ? "-" : gCalls;
	gCalls.clear();
	return s;
}

OhplH14BodyVt gRecVt = {
	[](size_t b) { float r = gReal->mass(b); Call("mass", "", H(r)); return r; },
	[](size_t b, cMatrixf* m) { gReal->worldMatrix(b, m); Call("worldMatrix", "", H(*m)); },
	[](size_t b, cMatrixf* m) { gReal->localMatrix(b, m); Call("localMatrix", "", H(*m)); },
	[](size_t b) { OhplV3 r = gReal->worldPosition(b); Call("worldPosition", "", H(r)); return r; },
	[](size_t b, cMatrixf* m) { gReal->inertia(b, m); Call("inertia", "", H(*m)); },
	[](size_t b, OhplV3 f, OhplV3 p) { OhplV3 r = gReal->torqueFromForce(b, f, p); Call("torqueFromForce", H(f) + H(p), H(r)); return r; },
	[](size_t b, OhplV3 p) { OhplV3 r = gReal->velocityAtPosition(b, p); Call("velocityAtPosition", H(p), H(r)); return r; },
	[](size_t b, OhplV3 f) { gReal->addForce(b, f); Call("addForce", H(f), "-"); },
	[](size_t b, OhplV3 t) { gReal->addTorque(b, t); Call("addTorque", H(t), "-"); },
	[](size_t b) { bool r = gReal->gravityAttachmentRotation(b); Call("gravityAttachmentRotation", "", H(r)); return r; },
	[](size_t b) { bool r = gReal->gravityAttachmentVelocity(b); Call("gravityAttachmentVelocity", "", H(r)); return r; },
	[](size_t b) { int32_t r = gReal->gravityAttachmentAxes(b); Call("gravityAttachmentAxes", "", H(r)); return r; },
};

bool Take(const void* key)
{
	return ++gCount[key] <= kStateful;
}

void Line(const char* tag, const void* aux, const std::string& rest)
{
	std::fprintf(gVec, "%s %llx %s\n", tag, (unsigned long long)(uintptr_t)aux, rest.c_str());
}

} // namespace

extern "C" {

OhplH14Aux* __wrap_ohpl_h14_aux_new()
{
	OhplH14Aux* a = __real_ohpl_h14_aux_new();
	if (Rec())
	{
		Line("new", a, "-");
		gSafe.erase(a);
		for (int i = 1; i <= 5; ++i) gCount.erase((const char*)a + i);
	}
	return a;
}

void __wrap_ohpl_h14_aux_free(OhplH14Aux* a)
{
	if (Rec()) Line("free", a, "-");
	__real_ohpl_h14_aux_free(a);
}

void __wrap_ohpl_h14_reset_pids(OhplH14Aux* a)
{
	if (Rec()) Line("reset", a, "-");
	__real_ohpl_h14_reset_pids(a);
}

void __wrap_ohpl_h14_clear_smoothing(OhplH14Aux* a)
{
	if (Rec()) Line("clear", a, "-");
	__real_ohpl_h14_clear_smoothing(a);
}

void __wrap_ohpl_h14_clear_camera(OhplH14Aux* a)
{
	if (Rec()) Line("clearcam", a, "-");
	__real_ohpl_h14_clear_camera(a);
}

void __wrap_ohpl_h14_setup_safe_positions(OhplH14Aux* a, float interval, float buffer)
{
	if (Rec()) { Line("setup", a, H(interval) + " " + H(buffer)); gSafe.insert(a); }
	__real_ohpl_h14_setup_safe_positions(a, interval, buffer);
}

void __wrap_ohpl_h14_update_safe_positions(OhplH14Aux* a, float dt, OhplV3 pos)
{
	if (Rec() && gSafe.count(a)) Line("safeup", a, H(dt) + " " + H(pos));
	__real_ohpl_h14_update_safe_positions(a, dt, pos);
}

bool __wrap_ohpl_h14_safe_position(const OhplH14Aux* a, float elapsed, OhplV3* out)
{
	bool r = __real_ohpl_h14_safe_position(a, elapsed, out);
	if (Rec()) Line("safepos", a, H(elapsed) + " " + H(r) + " " + H(*out));
	return r;
}

void __wrap_ohpl_h14_body_connection_pos(const OhplH14BodyVt* vt, const OhplH14Conn* conn, OhplV3* out)
{
	if (!Rec() || !Take((const void*)&gCount)) { __real_ohpl_h14_body_connection_pos(vt, conn, out); return; }
	gReal = vt;
	gCalls.clear();
	__real_ohpl_h14_body_connection_pos(&gRecVt, conn, out);
	Line("bodypos", NULL, H(*conn) + " " + Calls() + " " + H(*out));
}

void __wrap_ohpl_h14_char_connection_pos(const OhplCharState* st, const OhplH14Conn* conn, OhplV3* out)
{
	__real_ohpl_h14_char_connection_pos(st, conn, out);
	if (Rec() && Take((const char*)&gCount + 1)) Line("charpos", NULL, Hex(st, sizeof(*st)) + " " + H(*conn) + " " + H(*out));
}

void __wrap_ohpl_h14_update_body_connection(OhplH14Aux* a, const OhplH14BodyVt* vt, const OhplCharState* st, const OhplH14Conn* conn, bool onGround, float dt)
{
	if (!Rec() || !Take((const char*)a + 2)) { __real_ohpl_h14_update_body_connection(a, vt, st, conn, onGround, dt); return; }
	gReal = vt;
	gCalls.clear();
	std::string in = Hex(st, sizeof(*st)) + " " + H(*conn) + " " + H(onGround) + " " + H(dt);
	__real_ohpl_h14_update_body_connection(a, &gRecVt, st, conn, onGround, dt);
	Line("bodyconn", a, in + " " + Calls());
}

bool __wrap_ohpl_h14_update_character_connection(OhplH14Aux* a, const OhplH14BodyVt* vt, OhplCharState* st, const OhplH14Conn* conn, float dt, OhplV3* force)
{
	if (!Rec() || !Take((const char*)a + 3)) return __real_ohpl_h14_update_character_connection(a, vt, st, conn, dt, force);
	gReal = vt;
	gCalls.clear();
	std::string in = Hex(st, sizeof(*st)) + " " + H(*conn) + " " + H(dt);
	bool r = __real_ohpl_h14_update_character_connection(a, &gRecVt, st, conn, dt, force);
	Line("charconn", a, in + " " + Calls() + " " + H(r) + " " + H(*force) + " " + Hex(st, sizeof(*st)));
	return r;
}

void __wrap_ohpl_h14_update_gravity_attachment(const OhplH14BodyVt* vt, OhplCharState* st, OhplH14Grav* grav)
{
	bool take = Rec() && grav->attached != 0 && (gGravSeen++ % kGravEvery) == 0;
	if (!take) { __real_ohpl_h14_update_gravity_attachment(vt, st, grav); return; }
	gReal = vt;
	gCalls.clear();
	std::string in = Hex(st, sizeof(*st)) + " " + H(*grav);
	__real_ohpl_h14_update_gravity_attachment(&gRecVt, st, grav);
	Line("grav", NULL, in + " " + Calls() + " " + Hex(st, sizeof(*st)) + " " + H(*grav));
}

void __wrap_ohpl_h14_update_camera(OhplH14Aux* a, const OhplH14CamIn* in, OhplH14CamOut* out)
{
	__real_ohpl_h14_update_camera(a, in, out);
	if (Rec() && Take((const char*)a + 4)) Line("cam", a, H(*in) + " " + H(*out));
}

void __wrap_ohpl_h14_update_entity(OhplH14Aux* a, const OhplH14EntIn* in, cMatrixf* out)
{
	__real_ohpl_h14_update_entity(a, in, out);
	if (Rec() && Take((const char*)a + 5)) Line("ent", a, H(*in) + " " + H(*out));
}

} // extern "C"

// LowLevelSystemSDL.cpp provides main() and expects the caller to define this.
int hplMain(const hpl::tString&)
{
	const char* dump = std::getenv("OHPL_H14_DUMP");
	h14scenes::RunAll(dump);
	if (gVec) std::fclose(gVec);
	uint64_t n[16];
	hpl::ohpl_h14_counts(n);
	std::fprintf(stderr, "h14 rust counters:");
	for (int i = 0; i < 16; ++i) std::fprintf(stderr, " %llu", (unsigned long long)n[i]);
	std::fprintf(stderr, "\n");
	return 0;
}
