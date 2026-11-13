#include <angelscript.h>

#include <cstdio>
#include <cstring>
#include <map>
#include <new>
#include <string>
#include <vector>

#include "as_shim_s2.h"

namespace {

asIScriptEngine* E(void* e) { return static_cast<asIScriptEngine*>(e); }
asIScriptGeneric* G(void* g) { return static_cast<asIScriptGeneric*>(g); }
asITypeInfo* T(void* t) { return static_cast<asITypeInfo*>(t); }
asIScriptFunction* F(void* f) { return static_cast<asIScriptFunction*>(f); }

asSFuncPtr Fn(void* fn)
{
	asSFuncPtr p(1);
	p.ptr.f.func = reinterpret_cast<asFUNCTION_t>(fn);
	return p;
}

class StringFactory : public asIStringFactory
{
public:
	const void* GetStringConstant(const char* data, asUINT length)
	{
		std::string s(data, length);
		auto it = cache.find(s);
		if (it == cache.end())
			it = cache.insert(std::make_pair(s, 0)).first;
		++it->second;
		return &it->first;
	}
	int ReleaseStringConstant(const void* str)
	{
		auto it = cache.find(*static_cast<const std::string*>(str));
		if (it != cache.end() && --it->second == 0)
			cache.erase(it);
		return 0;
	}
	int GetRawStringData(const void* str, char* data, asUINT* length) const
	{
		const std::string& s = *static_cast<const std::string*>(str);
		if (length)
			*length = (asUINT)s.size();
		if (data)
			memcpy(data, s.data(), s.size());
		return 0;
	}

private:
	std::map<std::string, int> cache;
};

void DumpFunc(FILE* f, asIScriptFunction* fn, const char* prefix)
{
	fprintf(f, "%s%s\n", prefix, fn->GetDeclaration(true, true, true));
}

} // namespace


namespace {

struct Rng
{
	uint64_t s;
	uint32_t Next()
	{
		s = s * 6364136223846793005ULL + 1442695040888963407ULL;
		return (uint32_t)(s >> 33);
	}
	float F()
	{
		static const float sp[] = {0, 1, -1, 0.5f, -0.5f, 2, 3.14159f, 1e-9f, 100, -100, 0.001f};
		uint32_t k = Next();
		if (k % 4 == 0)
			return sp[(k >> 3) % 11];
		return ((int)(Next() % 20001) - 10000) / 1000.0f;
	}
	int I()
	{
		uint32_t k = Next();
		if (k % 5 == 0)
			return (int)(k >> 3) % 3 - 1;
		return (int)(Next() % 41) - 10;
	}
	std::string Text()
	{
		static const char* pool[] = {"", "12", "3.5", "1 2 3", "true", "a,b,c", "Hello_World", "x007", "abc", "-4", "1 2 3 4", "[br]x", "a b", "A_b9"};
		return pool[Next() % 14];
	}
};

bool Known(const std::string& n)
{
	static const char* t[] = {"cVector2f", "cVector3f", "cVector4f", "cVector2l", "cVector3l", "cColor", "cMatrixf", "cQuaternion",
							  "cRect2f", "cRect2l", "cPlanef", "cDate", "tString", "tWString", "tID"};
	for (const char* x : t)
		if (n == x)
			return true;
	return false;
}

bool IntLanes(const std::string& n) { return n == "tID" || n == "cVector2l" || n == "cVector3l" || n == "cRect2l" || n == "cDate"; }

struct Slot
{
	int typeId = 0;
	asDWORD flags = 0;
	asITypeInfo* type = nullptr;
	void* obj = nullptr;
	uint64_t prim = 0;
	bool ref = false;
};

std::string Hex(const void* p, size_t n)
{
	std::string r;
	char b[4];
	for (size_t i = 0; i < n; ++i)
	{
		snprintf(b, sizeof b, "%02x", ((const unsigned char*)p)[i]);
		r += b;
	}
	return r;
}

size_t PrimSize(int id)
{
	switch (id)
	{
	case asTYPEID_BOOL: case asTYPEID_INT8: case asTYPEID_UINT8: return 1;
	case asTYPEID_INT16: case asTYPEID_UINT16: return 2;
	case asTYPEID_INT64: case asTYPEID_UINT64: case asTYPEID_DOUBLE: return 8;
	default: return 4;
	}
}

std::string Dump(const Slot& s)
{
	if (s.type && !(s.type->GetFlags() & asOBJ_ENUM))
	{
		std::string n = s.type->GetName();
		if (n == "tString")
		{
			const std::string& x = *(const std::string*)s.obj;
			return "s:" + Hex(x.data(), x.size());
		}
		if (n == "tWString")
		{
			const std::wstring& x = *(const std::wstring*)s.obj;
			return "w:" + Hex(x.data(), x.size() * sizeof(wchar_t));
		}
		if (n == "tID")
		{
			// padding after the uint8 member is uninitialised in the C++ natives
			unsigned char b[12];
			memcpy(b, s.obj, 12);
			memset(b + 1, 0, 3);
			return "o:" + Hex(b, 12);
		}
		return "o:" + Hex(s.obj, s.type->GetSize());
	}
	return "p:" + Hex(&s.prim, PrimSize(s.typeId));
}

bool Supported(asIScriptEngine* e, int id, asDWORD fl, asITypeInfo** type)
{
	*type = nullptr;
	if (id & asTYPEID_OBJHANDLE)
		return false;
	if (id <= asTYPEID_DOUBLE)
		return id != asTYPEID_VOID || fl == 0;
	asITypeInfo* t = e->GetTypeInfoById(id);
	if (!t)
		return false;
	*type = t;
	if (t->GetFlags() & asOBJ_ENUM)
		return true;
	return Known(t->GetName());
}

void FillObj(asIScriptEngine* e, Rng& r, Slot& s)
{
	std::string n = s.type->GetName();
	s.obj = e->CreateScriptObject(s.type);
	if (n == "tString")
		*(std::string*)s.obj = r.Text();
	else if (n == "tWString")
	{
		std::string t = r.Text();
		*(std::wstring*)s.obj = std::wstring(t.begin(), t.end());
	}
	else
	{
		unsigned lanes = s.type->GetSize() / 4;
		for (unsigned i = 0; i < lanes; ++i)
		{
			if (IntLanes(n))
				((int*)s.obj)[i] = r.I();
			else
				((float*)s.obj)[i] = r.F();
		}
	}
}

void FillPrim(Rng& r, Slot& s, bool small)
{
	switch (s.typeId)
	{
	case asTYPEID_BOOL: s.prim = r.Next() & 1; break;
	case asTYPEID_FLOAT: { float f = r.F(); memcpy(&s.prim, &f, 4); break; }
	case asTYPEID_DOUBLE: { double d = r.F(); memcpy(&s.prim, &d, 8); break; }
	default: { int v = small ? (int)(r.Next() % 4) : r.I(); s.prim = (uint64_t)(int64_t)v; if (PrimSize(s.typeId) == 4) s.prim &= 0xffffffffu; else if (PrimSize(s.typeId) == 1) s.prim &= 0xff; }
	}
}

void SetArg(asIScriptContext* c, unsigned i, Slot& s)
{
	if (s.type && !(s.type->GetFlags() & asOBJ_ENUM))
	{
		if (s.ref)
			c->SetArgAddress(i, s.obj);
		else
			c->SetArgObject(i, s.obj);
		return;
	}
	if (s.ref)
	{
		c->SetArgAddress(i, &s.prim);
		return;
	}
	switch (PrimSize(s.typeId))
	{
	case 1: c->SetArgByte(i, (asBYTE)s.prim); break;
	case 2: c->SetArgWord(i, (asWORD)s.prim); break;
	case 8:
		if (s.typeId == asTYPEID_DOUBLE) { double d; memcpy(&d, &s.prim, 8); c->SetArgDouble(i, d); }
		else c->SetArgQWord(i, s.prim);
		break;
	default:
		if (s.typeId == asTYPEID_FLOAT) { float f; memcpy(&f, &s.prim, 4); c->SetArgFloat(i, f); }
		else c->SetArgDWord(i, (asDWORD)s.prim);
	}
}

bool WantGlobal(const char* n)
{
	static const char* exact[] = {"cLux_ScriptDebugOn", "cLux_DebugModeOn", "cLux_GetGodModeActivated", "cLux_GetUnderwaterEffectsActive",
								  "cLux_SetUnderwaterEffectsActive", "cLux_IncUnderwaterEffectUserCount", "cLux_DecUnderwaterEffectUserCount",
								  "cLux_HasUnderwaterEffectUsers", "cLux_DropPageFocus", "cLux_PreloadWebpage", "cLux_SetAreaOffline",
								  "cLux_RegisterCollisionRadius", "cLux_ResetShudderEffects", "SlideDoor_SetClosed", "Log", "Warning", "Error",
								  "__print", "cLux_AddDebugMessage", "cLux_AddTodoMessage"};
	for (const char* x : exact)
		if (strcmp(n, x) == 0)
			return true;
	static const char* pre[] = {"cMath_", "Math_", "cString_", "String_", "cGenerate_", "cScript_"};
	for (const char* p : pre)
		if (strncmp(n, p, strlen(p)) == 0)
			return strcmp(n, "cMath_Randomize") != 0;
	return false;
}

void CallOne(asIScriptEngine* e, asIScriptContext* c, asIScriptFunction* f, Rng& r, FILE* out, unsigned k)
{
	asITypeInfo* objType = f->GetObjectType();
	if (objType && !Known(objType->GetName()))
		return;
	unsigned np = f->GetParamCount();
	std::vector<Slot> args(np);
	for (unsigned i = 0; i < np; ++i)
	{
		f->GetParam(i, &args[i].typeId, &args[i].flags);
		if (!Supported(e, args[i].typeId, args[i].flags, &args[i].type))
			return;
		args[i].ref = args[i].flags != 0;
	}
	Slot ret;
	ret.typeId = f->GetReturnTypeId(&ret.flags);
	if (ret.typeId != asTYPEID_VOID && !Supported(e, ret.typeId, 0, &ret.type))
		return;
	Slot self;
	if (objType)
	{
		self.type = objType;
		FillObj(e, r, self);
	}
	std::string line = std::string(f->GetDeclaration(true, true, true)) + "\t" + std::to_string(k) + "\tin";
	if (objType)
		line += " " + Dump(self);
	for (unsigned i = 0; i < np; ++i)
	{
		if (args[i].type && !(args[i].type->GetFlags() & asOBJ_ENUM))
			FillObj(e, r, args[i]);
		else
			FillPrim(r, args[i], PrimSize(args[i].typeId) == 8 && args[i].typeId != asTYPEID_DOUBLE);
		line += " " + Dump(args[i]);
	}
	c->Prepare(f);
	if (objType)
		c->SetObject(self.obj);
	for (unsigned i = 0; i < np; ++i)
		SetArg(c, i, args[i]);
	int rc = c->Execute();
	line += "\tout rc=" + std::to_string(rc);
	if (rc == asEXECUTION_FINISHED && ret.typeId != asTYPEID_VOID)
	{
		if (ret.type && !(ret.type->GetFlags() & asOBJ_ENUM))
		{
			ret.obj = (f->GetReturnTypeId(&ret.flags), ret.flags ? c->GetReturnAddress() : c->GetReturnObject());
			line += " ret " + Dump(ret);
		}
		else
		{
			switch (PrimSize(ret.typeId))
			{
			case 8: ret.prim = c->GetReturnQWord(); break;
			default: ret.prim = c->GetReturnDWord(); if (ret.typeId == asTYPEID_FLOAT) { float v = c->GetReturnFloat(); ret.prim = 0; memcpy(&ret.prim, &v, 4); } else if (PrimSize(ret.typeId) == 1) ret.prim &= 0xff; else if (PrimSize(ret.typeId) == 2) ret.prim &= 0xffff;
			}
			line += " ret " + Dump(ret);
		}
	}
	if (objType)
		line += " this " + Dump(self);
	for (unsigned i = 0; i < np; ++i)
		line += " a" + std::to_string(i) + " " + Dump(args[i]);
	fprintf(out, "%s\n", line.c_str());
	for (Slot& a : args)
		if (a.obj)
			e->ReleaseScriptObject(a.obj, a.type);
	if (self.obj)
		e->ReleaseScriptObject(self.obj, self.type);
}

} // namespace

extern "C" {

void* ohpl_as2_g_func(void* g) { return G(g)->GetFunction(); }
void* ohpl_as2_g_engine(void* g) { return G(g)->GetEngine(); }
void* ohpl_as2_g_object(void* g) { return G(g)->GetObject(); }
void* ohpl_as2_g_arg_object(void* g, unsigned i) { return G(g)->GetArgObject(i); }
void* ohpl_as2_g_arg_addr(void* g, unsigned i) { return G(g)->GetAddressOfArg(i); }
void* ohpl_as2_g_ret_loc(void* g) { return G(g)->GetAddressOfReturnLocation(); }
void ohpl_as2_g_set_ret_addr(void* g, void* p) { G(g)->SetReturnAddress(p); }
unsigned ohpl_as2_g_arg_count(void* g) { return G(g)->GetArgCount(); }
int ohpl_as2_g_arg_type_id(void* g, unsigned i) { return G(g)->GetArgTypeId(i); }
void* ohpl_as2_g_aux(void* g) { return G(g)->GetAuxiliary(); }

int ohpl_as2_reg_global_function(void* e, const char* decl, void* fn, void* aux)
{
	return E(e)->RegisterGlobalFunction(decl, Fn(fn), asCALL_GENERIC, aux);
}
int ohpl_as2_reg_object_method(void* e, const char* type, const char* decl, void* fn, void* aux)
{
	return E(e)->RegisterObjectMethod(type, decl, Fn(fn), asCALL_GENERIC, aux);
}
int ohpl_as2_reg_object_behaviour(void* e, int behaviour, const char* type, const char* decl, void* fn, void* aux)
{
	return E(e)->RegisterObjectBehaviour(type, (asEBehaviours)behaviour, decl, Fn(fn), asCALL_GENERIC, aux);
}
int ohpl_as2_reg_object_property(void* e, const char* type, const char* decl, int offset, int composite, int indirect)
{
	if (composite == 0 && !indirect)
		return E(e)->RegisterObjectProperty(type, decl, offset);
	return E(e)->RegisterObjectProperty(type, decl, offset, composite, indirect != 0);
}
int ohpl_as2_reg_interface(void* e, const char* name) { return E(e)->RegisterInterface(name); }
int ohpl_as2_reg_interface_method(void* e, const char* iface, const char* decl) { return E(e)->RegisterInterfaceMethod(iface, decl); }
int ohpl_as2_reg_string_factory(void* e, const char* type)
{
	static StringFactory factory;
	return E(e)->RegisterStringFactory(type, &factory);
}

void* ohpl_as2_type_by_name(void* e, const char* name) { return E(e)->GetTypeInfoByName(name); }
void* ohpl_as2_type_by_id(void* e, int id) { return E(e)->GetTypeInfoById(id); }
void* ohpl_as2_func_by_id(void* e, int id) { return E(e)->GetFunctionById(id); }
void* ohpl_as2_global_func_by_decl(void* e, const char* decl) { return E(e)->GetGlobalFunctionByDecl(decl); }
unsigned ohpl_as2_global_func_count(void* e) { return E(e)->GetGlobalFunctionCount(); }
void* ohpl_as2_global_func_by_index(void* e, unsigned i) { return E(e)->GetGlobalFunctionByIndex(i); }
int ohpl_as2_global_prop_index(void* e, const char* name) { return E(e)->GetGlobalPropertyIndexByName(name); }
int ohpl_as2_primitive_size(void* e, int type_id) { return E(e)->GetSizeOfPrimitiveType(type_id); }
void* ohpl_as2_create_object(void* e, void* type) { return E(e)->CreateScriptObject(T(type)); }
uint32_t ohpl_as2_type_flags(void* t) { return (uint32_t)T(t)->GetFlags(); }
int ohpl_as2_type_size(void* t) { return (int)T(t)->GetSize(); }
int ohpl_as2_type_id(void* t) { return T(t)->GetTypeId(); }
const char* ohpl_as2_type_name(void* t) { return T(t)->GetName(); }
unsigned ohpl_as2_type_prop_count(void* t) { return T(t)->GetPropertyCount(); }
int ohpl_as2_type_prop(void* t, unsigned i, const char** name, int* type_id, int* offset)
{
	return T(t)->GetProperty(i, name, type_id, nullptr, nullptr, offset);
}
void* ohpl_as2_type_method_by_decl(void* t, const char* decl) { return T(t)->GetMethodByDecl(decl); }

const char* ohpl_as2_func_name(void* f) { return F(f)->GetName(); }
const char* ohpl_as2_func_decl(void* f, int obj, int ns, int names) { return F(f)->GetDeclaration(obj != 0, ns != 0, names != 0); }
int ohpl_as2_func_return(void* f, uint32_t* flags)
{
	asDWORD fl = 0;
	int id = F(f)->GetReturnTypeId(&fl);
	if (flags)
		*flags = fl;
	return id;
}
unsigned ohpl_as2_func_param_count(void* f) { return F(f)->GetParamCount(); }
int ohpl_as2_func_param(void* f, unsigned i, int* type_id, uint32_t* flags)
{
	asDWORD fl = 0;
	int r = F(f)->GetParam(i, type_id, &fl);
	if (flags)
		*flags = fl;
	return r;
}
void* ohpl_as2_func_object_type(void* f) { return F(f)->GetObjectType(); }
void ohpl_as2_func_set_user(void* f, void* p, uintptr_t key) { F(f)->SetUserData(p, (asPWORD)key); }
void* ohpl_as2_func_user(void* f, uintptr_t key) { return F(f)->GetUserData((asPWORD)key); }

void ohpl_as2_str_ctor(void* mem, const char* p, size_t n) { new (mem) std::string(p, n); }
void ohpl_as2_str_dtor(void* s) { static_cast<std::string*>(s)->~basic_string(); }
void ohpl_as2_str_assign(void* s, const char* p, size_t n) { static_cast<std::string*>(s)->assign(p, n); }
void ohpl_as2_str_append(void* s, const char* p, size_t n) { static_cast<std::string*>(s)->append(p, n); }
void ohpl_as2_str_resize(void* s, size_t n) { static_cast<std::string*>(s)->resize(n); }
void ohpl_as2_wstr_ctor(void* mem, const uint32_t* p, size_t n) { new (mem) std::wstring((const wchar_t*)p, n); }
void ohpl_as2_wstr_dtor(void* s) { static_cast<std::wstring*>(s)->~basic_string(); }
void ohpl_as2_wstr_assign(void* s, const uint32_t* p, size_t n) { static_cast<std::wstring*>(s)->assign((const wchar_t*)p, n); }
void ohpl_as2_wstr_append(void* s, const uint32_t* p, size_t n) { static_cast<std::wstring*>(s)->append((const wchar_t*)p, n); }
void ohpl_as2_wstr_resize(void* s, size_t n) { static_cast<std::wstring*>(s)->resize(n); }
const uint32_t* ohpl_as2_wstr_data(const void* s) { return (const uint32_t*)static_cast<const std::wstring*>(s)->data(); }
size_t ohpl_as2_wstr_len(const void* s) { return static_cast<const std::wstring*>(s)->size(); }

int ohpl_as2_call_corpus(void* e, unsigned seed, unsigned calls, const char* path)
{
	asIScriptEngine* en = E(e);
	FILE* out = fopen(path, "w");
	if (!out)
		return -1;
	asIScriptContext* c = en->CreateContext();
	if (asIScriptFunction* rnd = en->GetGlobalFunctionByDecl("void cMath_Randomize(int alSeed)"))
	{
		c->Prepare(rnd);
		c->SetArgDWord(0, seed);
		c->Execute();
	}
	Rng r{seed};
	for (asUINT i = 0; i < en->GetGlobalFunctionCount(); ++i)
	{
		asIScriptFunction* f = en->GetGlobalFunctionByIndex(i);
		if (!WantGlobal(f->GetName()))
			continue;
		for (unsigned k = 0; k < calls; ++k)
			CallOne(en, c, f, r, out, k);
	}
	for (asUINT i = 0; i < en->GetObjectTypeCount(); ++i)
	{
		asITypeInfo* t = en->GetObjectTypeByIndex(i);
		if (!Known(t->GetName()))
			continue;
		for (asUINT j = 0; j < t->GetMethodCount(); ++j)
			for (unsigned k = 0; k < calls; ++k)
				CallOne(en, c, t->GetMethodByIndex(j, false), r, out, k);
	}
	c->Release();
	fclose(out);
	return 0;
}

int ohpl_as2_dump_decls(void* e, const char* path)
{
	asIScriptEngine* en = E(e);
	FILE* f = fopen(path, "w");
	if (!f)
		return -1;
	for (asUINT i = 0; i < en->GetEnumCount(); ++i)
	{
		asITypeInfo* t = en->GetEnumByIndex(i);
		fprintf(f, "enum %s\n", t->GetName());
		for (asUINT j = 0; j < t->GetEnumValueCount(); ++j)
		{
			int v;
			const char* n = t->GetEnumValueByIndex(j, &v);
			fprintf(f, "  %s = %d\n", n, v);
		}
	}
	for (asUINT i = 0; i < en->GetObjectTypeCount(); ++i)
	{
		asITypeInfo* t = en->GetObjectTypeByIndex(i);
		fprintf(f, "type %s flags=%x size=%d\n", t->GetName(), (unsigned)t->GetFlags() & ~0x3f00u, (int)t->GetSize());
		for (asUINT j = 0; j < t->GetBehaviourCount(); ++j)
		{
			asEBehaviours b;
			asIScriptFunction* fn = t->GetBehaviourByIndex(j, &b);
			char pre[32];
			snprintf(pre, sizeof pre, "  behaviour %d ", (int)b);
			DumpFunc(f, fn, pre);
		}
		for (asUINT j = 0; j < t->GetFactoryCount(); ++j)
			DumpFunc(f, t->GetFactoryByIndex(j), "  factory ");
		for (asUINT j = 0; j < t->GetMethodCount(); ++j)
			DumpFunc(f, t->GetMethodByIndex(j, false), "  method ");
		for (asUINT j = 0; j < t->GetPropertyCount(); ++j)
		{
			int off = 0;
			t->GetProperty(j, nullptr, nullptr, nullptr, nullptr, &off);
			fprintf(f, "  prop %s @%d\n", t->GetPropertyDeclaration(j, true), off);
		}
	}
	for (asUINT i = 0; i < en->GetGlobalFunctionCount(); ++i)
		DumpFunc(f, en->GetGlobalFunctionByIndex(i), "func ");
	for (asUINT i = 0; i < en->GetGlobalPropertyCount(); ++i)
	{
		const char* name;
		const char* ns;
		int id;
		en->GetGlobalPropertyByIndex(i, &name, &ns, &id);
		fprintf(f, "global %s::%s type=%s\n", ns ? ns : "", name, en->GetTypeDeclaration(id, true));
	}
	fclose(f);
	return 0;
}

}
