#include <angelscript.h>
#include <string>

#include "as_shim.h"

namespace {

constexpr asPWORD kCbUserData = 0x4F4C37;

const OhplAsAddons* gAddons = nullptr;

struct Cb {
	OhplAsMsgFn fn;
	void* ctx;
};

void Trampoline(const asSMessageInfo* m, void* p)
{
	const Cb* cb = static_cast<const Cb*>(p);
	OhplAsMsg out{m->section, m->row, m->col, (int32_t)m->type, m->message};
	cb->fn(&out, cb->ctx);
}

asSFuncPtr Fn(void* fn)
{
	asSFuncPtr p(2);
	p.ptr.f.func = reinterpret_cast<asFUNCTION_t>(fn);
	return p;
}

asIScriptEngine* E(void* e) { return static_cast<asIScriptEngine*>(e); }
asIScriptModule* M(void* m) { return static_cast<asIScriptModule*>(m); }
asIScriptContext* C(void* c) { return static_cast<asIScriptContext*>(c); }
asIScriptFunction* F(void* f) { return static_cast<asIScriptFunction*>(f); }

} // namespace

extern "C" {

void* ohpl_as_engine_new(void) { return asCreateScriptEngine(ANGELSCRIPT_VERSION); }

int ohpl_as_engine_release(void* e)
{
	delete static_cast<Cb*>(E(e)->GetUserData(kCbUserData));
	return E(e)->Release();
}

int ohpl_as_engine_shutdown(void* e)
{
	delete static_cast<Cb*>(E(e)->GetUserData(kCbUserData));
	return E(e)->ShutDownAndRelease();
}

int ohpl_as_set_message_cb(void* e, OhplAsMsgFn fn, void* ctx)
{
	Cb* cb = new Cb{fn, ctx};
	delete static_cast<Cb*>(E(e)->SetUserData(cb, kCbUserData));
	return E(e)->SetMessageCallback(asFUNCTION(Trampoline), cb, asCALL_CDECL);
}

int ohpl_as_set_engine_property(void* e, int prop, size_t value) { return E(e)->SetEngineProperty((asEEngineProp)prop, value); }
int ohpl_as_set_default_namespace(void* e, const char* ns) { return E(e)->SetDefaultNamespace(ns); }

int ohpl_as_reg_global_function(void* e, const char* decl, void* fn, int conv) { return E(e)->RegisterGlobalFunction(decl, Fn(fn), conv); }
int ohpl_as_reg_global_property(void* e, const char* decl, void* ptr) { return E(e)->RegisterGlobalProperty(decl, ptr); }
int ohpl_as_reg_object_type(void* e, const char* name, int size, uint64_t flags) { return E(e)->RegisterObjectType(name, size, flags); }
int ohpl_as_reg_object_property(void* e, const char* obj, const char* decl, int offset) { return E(e)->RegisterObjectProperty(obj, decl, offset); }
int ohpl_as_reg_object_method(void* e, const char* obj, const char* decl, void* fn, int conv) { return E(e)->RegisterObjectMethod(obj, decl, Fn(fn), conv); }
int ohpl_as_reg_object_behaviour(void* e, int behaviour, const char* obj, const char* decl, void* fn, int conv)
{
	return E(e)->RegisterObjectBehaviour(obj, (asEBehaviours)behaviour, decl, Fn(fn), conv);
}
int ohpl_as_reg_enum(void* e, const char* name) { return E(e)->RegisterEnum(name); }
int ohpl_as_reg_enum_value(void* e, const char* name, const char* value, int v) { return E(e)->RegisterEnumValue(name, value, v); }
int ohpl_as_reg_funcdef(void* e, const char* decl) { return E(e)->RegisterFuncdef(decl); }
int ohpl_as_reg_typedef(void* e, const char* ty, const char* decl) { return E(e)->RegisterTypedef(ty, decl); }
int ohpl_as_type_id_by_decl(void* e, const char* decl) { return E(e)->GetTypeIdByDecl(decl); }

void* ohpl_as_get_module(void* e, const char* name, int flag) { return E(e)->GetModule(name, (asEGMFlags)flag); }
int ohpl_as_discard_module(void* e, const char* name) { return E(e)->DiscardModule(name); }
int ohpl_as_module_add_section(void* m, const char* name, const char* code, size_t len) { return M(m)->AddScriptSection(name, code, len); }
int ohpl_as_module_build(void* m) { return M(m)->Build(); }
void* ohpl_as_module_function_by_name(void* m, const char* name) { return M(m)->GetFunctionByName(name); }
unsigned ohpl_as_module_function_count(void* m) { return M(m)->GetFunctionCount(); }
void* ohpl_as_module_function_by_index(void* m, unsigned i) { return M(m)->GetFunctionByIndex(i); }
unsigned ohpl_as_module_global_count(void* m) { return M(m)->GetGlobalVarCount(); }
int ohpl_as_module_global_index_by_name(void* m, const char* name) { return M(m)->GetGlobalVarIndexByName(name); }

const char* ohpl_as_func_name(void* f) { return F(f)->GetName(); }
const char* ohpl_as_func_decl(void* f, int obj, int ns, int names) { return F(f)->GetDeclaration(obj, ns, names); }
int ohpl_as_func_param_count(void* f) { return (int)F(f)->GetParamCount(); }

void* ohpl_as_ctx_new(void* e) { return E(e)->CreateContext(); }
int ohpl_as_ctx_release(void* c) { return C(c)->Release(); }
int ohpl_as_ctx_prepare(void* c, void* func) { return C(c)->Prepare(F(func)); }
int ohpl_as_ctx_unprepare(void* c) { return C(c)->Unprepare(); }
int ohpl_as_ctx_execute(void* c) { return C(c)->Execute(); }
int ohpl_as_ctx_abort(void* c) { return C(c)->Abort(); }
int ohpl_as_ctx_state(void* c) { return (int)C(c)->GetState(); }
int ohpl_as_ctx_set_object(void* c, void* obj) { return C(c)->SetObject(obj); }
int ohpl_as_ctx_set_arg_dword(void* c, unsigned arg, uint32_t v) { return C(c)->SetArgDWord(arg, v); }
int ohpl_as_ctx_set_arg_qword(void* c, unsigned arg, uint64_t v) { return C(c)->SetArgQWord(arg, v); }
int ohpl_as_ctx_set_arg_float(void* c, unsigned arg, float v) { return C(c)->SetArgFloat(arg, v); }
int ohpl_as_ctx_set_arg_double(void* c, unsigned arg, double v) { return C(c)->SetArgDouble(arg, v); }
int ohpl_as_ctx_set_arg_address(void* c, unsigned arg, void* v) { return C(c)->SetArgAddress(arg, v); }
int ohpl_as_ctx_set_arg_object(void* c, unsigned arg, void* v) { return C(c)->SetArgObject(arg, v); }
uint32_t ohpl_as_ctx_ret_dword(void* c) { return C(c)->GetReturnDWord(); }
uint64_t ohpl_as_ctx_ret_qword(void* c) { return C(c)->GetReturnQWord(); }
float ohpl_as_ctx_ret_float(void* c) { return C(c)->GetReturnFloat(); }
double ohpl_as_ctx_ret_double(void* c) { return C(c)->GetReturnDouble(); }
void* ohpl_as_ctx_ret_address(void* c) { return C(c)->GetReturnAddress(); }
void* ohpl_as_ctx_ret_object(void* c) { return C(c)->GetReturnObject(); }
const char* ohpl_as_ctx_exception_string(void* c) { return C(c)->GetExceptionString(); }
int ohpl_as_ctx_exception_line(void* c, int* col, const char** section) { return C(c)->GetExceptionLineNumber(col, section); }


void ohpl_as_set_addons(const OhplAsAddons* t) { gAddons = t; }
void ohpl_as_register_addons(void* e) { if (gAddons) gAddons->register_addons(e); }
int ohpl_as_execute_string(void* e, const char* code, void* module) { return gAddons ? gAddons->execute_string(e, code, module) : asERROR; }
const char* ohpl_as_str_data(const void* s) { return static_cast<const std::string*>(s)->data(); }
size_t ohpl_as_str_len(const void* s) { return static_cast<const std::string*>(s)->size(); }
void* ohpl_as_str_new(const char* p, size_t n) { return gAddons ? gAddons->str_new(p, n) : nullptr; }
unsigned ohpl_as_array_size(const void* a) { return gAddons ? gAddons->array_size(a) : 0; }
void* ohpl_as_array_at(void* a, unsigned i) { return gAddons ? gAddons->array_at(a, i) : nullptr; }

}
