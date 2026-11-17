#include <angelscript.h>

#include "as_shim_addon.h"

namespace {

asIScriptEngine* E(void* e) { return static_cast<asIScriptEngine*>(e); }
asIScriptContext* C(void* c) { return static_cast<asIScriptContext*>(c); }
asITypeInfo* T(void* t) { return static_cast<asITypeInfo*>(t); }
asIScriptFunction* F(void* f) { return static_cast<asIScriptFunction*>(f); }

class CbFactory : public asIStringFactory
{
public:
	explicit CbFactory(const OhplAs3StringFactory& f) : f(f) {}
	const void* GetStringConstant(const char* data, asUINT length) override { return f.get(data, length); }
	int ReleaseStringConstant(const void* str) override { return f.release(str); }
	int GetRawStringData(const void* str, char* data, asUINT* length) const override { return f.raw(str, data, length); }

private:
	OhplAs3StringFactory f;
};

} // namespace

extern "C" {

void* ohpl_as3_active_ctx(void) { return asGetActiveContext(); }
void ohpl_as3_lock(int acquire)
{
	if (acquire)
		asAcquireExclusiveLock();
	else
		asReleaseExclusiveLock();
}

void* ohpl_as3_type_engine(void* t) { return T(t)->GetEngine(); }
int ohpl_as3_type_sub_id(void* t) { return T(t)->GetSubTypeId(); }
void* ohpl_as3_type_sub(void* t) { return T(t)->GetSubType(); }
uint64_t ohpl_as3_type_flags(void* t) { return T(t)->GetFlags(); }
void ohpl_as3_type_addref(void* t) { T(t)->AddRef(); }
void ohpl_as3_type_release(void* t) { T(t)->Release(); }
void* ohpl_as3_type_user(void* t, uintptr_t key) { return T(t)->GetUserData(key); }
void ohpl_as3_type_set_user(void* t, void* p, uintptr_t key) { T(t)->SetUserData(p, key); }
unsigned ohpl_as3_type_behaviour_count(void* t) { return T(t)->GetBehaviourCount(); }
void* ohpl_as3_type_behaviour(void* t, unsigned i, int* beh)
{
	asEBehaviours b = asBEHAVE_CONSTRUCT;
	void* f = T(t)->GetBehaviourByIndex(i, &b);
	*beh = (int)b;
	return f;
}
unsigned ohpl_as3_type_factory_count(void* t) { return T(t)->GetFactoryCount(); }
void* ohpl_as3_type_factory(void* t, unsigned i) { return T(t)->GetFactoryByIndex(i); }
unsigned ohpl_as3_type_method_count(void* t) { return T(t)->GetMethodCount(); }
void* ohpl_as3_type_method(void* t, unsigned i) { return T(t)->GetMethodByIndex(i); }

int ohpl_as3_func_read_only(void* f) { return F(f)->IsReadOnly(); }

int ohpl_as3_engine_prop(void* e, int prop) { return (int)E(e)->GetEngineProperty((asEEngineProp)prop); }
void ohpl_as3_write_message(void* e, const char* section, int row, int col, int type, const char* msg)
{
	E(e)->WriteMessage(section, row, col, (asEMsgType)type, msg);
}
const char* ohpl_as3_type_decl(void* e, int type_id, int ns) { return E(e)->GetTypeDeclaration(type_id, ns != 0); }
int ohpl_as3_assign_object(void* e, void* dst, void* src, void* type) { return E(e)->AssignScriptObject(dst, src, T(type)); }
void ohpl_as3_addref_object(void* e, void* obj, void* type) { E(e)->AddRefScriptObject(obj, T(type)); }
void ohpl_as3_release_object(void* e, void* obj, void* type) { E(e)->ReleaseScriptObject(obj, T(type)); }
void ohpl_as3_notify_gc(void* e, void* obj, void* type) { E(e)->NotifyGarbageCollectorOfNewObject(obj, T(type)); }
void ohpl_as3_gc_enum(void* e, void* obj) { E(e)->GCEnumCallback(obj); }
void ohpl_as3_gc_forward_enum(void* e, void* obj, void* type) { E(e)->ForwardGCEnumReferences(obj, T(type)); }
void* ohpl_as3_request_ctx(void* e) { return E(e)->RequestContext(); }
void ohpl_as3_return_ctx(void* e, void* c) { E(e)->ReturnContext(C(c)); }
void ohpl_as3_type_cleanup_cb(void* e, void (*cb)(void*), uintptr_t key)
{
	E(e)->SetTypeInfoUserDataCleanupCallback(reinterpret_cast<asCLEANTYPEINFOFUNC_t>(cb), key);
}
int ohpl_as3_reg_default_array(void* e, const char* decl) { return E(e)->RegisterDefaultArrayType(decl); }

int ohpl_as3_reg_string_factory(void* e, const char* type, const OhplAs3StringFactory* f)
{
	static CbFactory factory(*f);
	return E(e)->RegisterStringFactory(type, &factory);
}

void* ohpl_as3_ctx_engine(void* c) { return C(c)->GetEngine(); }
int ohpl_as3_ctx_set_exception(void* c, const char* info) { return C(c)->SetException(info); }
int ohpl_as3_ctx_push(void* c) { return C(c)->PushState(); }
int ohpl_as3_ctx_pop(void* c) { return C(c)->PopState(); }
uint8_t ohpl_as3_ctx_ret_byte(void* c) { return C(c)->GetReturnByte(); }
void* ohpl_as3_ctx_ret_addr_of(void* c) { return C(c)->GetAddressOfReturnValue(); }

int ohpl_as3_compile_function(void* m, const char* name, const char* code, void** out)
{
	asIScriptFunction* f = nullptr;
	int r = static_cast<asIScriptModule*>(m)->CompileFunction(name, code, -1, 0, &f);
	*out = f;
	return r;
}
void ohpl_as3_func_release(void* f) { F(f)->Release(); }

}
