#include <angelscript.h>

#include <cstdio>
#include <cstring>
#include <map>
#include <new>
#include <string>

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

extern "C" {

void* ohpl_as2_g_func(void* g) { return G(g)->GetFunction(); }
void* ohpl_as2_g_engine(void* g) { return G(g)->GetEngine(); }
void* ohpl_as2_g_object(void* g) { return G(g)->GetObject(); }
void* ohpl_as2_g_arg_object(void* g, unsigned i) { return G(g)->GetArgObject(i); }
void* ohpl_as2_g_arg_addr(void* g, unsigned i) { return G(g)->GetAddressOfArg(i); }
void* ohpl_as2_g_ret_loc(void* g) { return G(g)->GetAddressOfReturnLocation(); }
void ohpl_as2_g_set_ret_addr(void* g, void* p) { G(g)->SetReturnAddress(p); }
unsigned ohpl_as2_g_arg_count(void* g) { return G(g)->GetArgCount(); }
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
