#include <angelscript.h>
#include <cstdio>
#include <string>
#include "impl/scriptstring.h"

struct V3 { float x, y, z; };
struct I3 { int a, b, c; };
struct Obj;

extern "C" {
const char *probe_str_data(const void *s) { return static_cast<const std::string *>(s)->data(); }
size_t probe_str_len(const void *s) { return static_cast<const std::string *>(s)->size(); }
void *probe_str_new(const char *p, size_t n) { return new CScriptString(p, (unsigned)n); }
static int gPass, gFail;
void probe_report(const char *name, int ok)
{
	printf("%-34s %s\n", name, ok ? "ok" : "FAIL");
	(ok ? gPass : gFail)++;
}
int rs_add_i(int, int); float rs_add_f(float, float); double rs_add_d(double, double); bool rs_not(bool);
long long rs_add_i64(long long, long long);
int rs_sum9(int, int, int, int, int, int, int, int, int);
double rs_sum9d(double, double, double, double, double, double, double, double, double);
double rs_mixed(int, float, double, bool, long long);
V3 rs_v3_add(const V3 *, const V3 *); float rs_v3_dot_val(V3, V3); void rs_i3_inc(I3 *); I3 rs_i3_make(int);
float rs_v3_len2(const V3 *); void rs_v3_m_scale(V3 *, float); float rs_v3_m_sum(const V3 *); void rs_v3_ctor(V3 *);
Obj *rs_obj_factory(); void rs_obj_addref(Obj *); void rs_obj_release(Obj *); int rs_obj_get(Obj *); void rs_obj_set(Obj *, int);
int rs_obj_rc(Obj *); int rs_obj_pass(Obj *); Obj *rs_obj_ret(Obj *); Obj *rs_obj_null();
int rs_str_len(const void *); int rs_str_first(const void *); void *rs_str_upper(const void *); int rs_str_inout(void *);
void rs_check(bool, const void *); int *rs_prop_ptr();
}

static void Msg(const asSMessageInfo *m, void *) { printf("AS %s (%d,%d): %s\n", m->section, m->row, m->col, m->message); }

#define R(x) do { int r_ = (x); if (r_ < 0) { printf("register failed line %d: %d\n", __LINE__, r_); ++gFail; } } while (0)

int main()
{
	asIScriptEngine *e = asCreateScriptEngine(ANGELSCRIPT_VERSION);
	e->SetMessageCallback(asFUNCTION(Msg), 0, asCALL_CDECL);
	RegisterScriptString(e);

	R(e->RegisterGlobalFunction("int add_i(int,int)", asFUNCTION(rs_add_i), asCALL_CDECL));
	R(e->RegisterGlobalFunction("float add_f(float,float)", asFUNCTION(rs_add_f), asCALL_CDECL));
	R(e->RegisterGlobalFunction("double add_d(double,double)", asFUNCTION(rs_add_d), asCALL_CDECL));
	R(e->RegisterGlobalFunction("bool not_(bool)", asFUNCTION(rs_not), asCALL_CDECL));
	R(e->RegisterGlobalFunction("int64 add_i64(int64,int64)", asFUNCTION(rs_add_i64), asCALL_CDECL));
	R(e->RegisterGlobalFunction("int sum9(int,int,int,int,int,int,int,int,int)", asFUNCTION(rs_sum9), asCALL_CDECL));
	R(e->RegisterGlobalFunction("double sum9d(double,double,double,double,double,double,double,double,double)", asFUNCTION(rs_sum9d), asCALL_CDECL));
	R(e->RegisterGlobalFunction("double mixed(int,float,double,bool,int64)", asFUNCTION(rs_mixed), asCALL_CDECL));

	R(e->RegisterObjectType("V3", sizeof(V3), asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS | asOBJ_APP_CLASS_ALLFLOATS));
	R(e->RegisterObjectProperty("V3", "float x", 0));
	R(e->RegisterObjectProperty("V3", "float y", 4));
	R(e->RegisterObjectProperty("V3", "float z", 8));
	R(e->RegisterObjectBehaviour("V3", asBEHAVE_CONSTRUCT, "void f()", asFUNCTION(rs_v3_ctor), asCALL_CDECL_OBJFIRST));
	R(e->RegisterGlobalFunction("V3 v3_add(const V3 &in,const V3 &in)", asFUNCTION(rs_v3_add), asCALL_CDECL));
	R(e->RegisterGlobalFunction("float v3_dot_val(V3,V3)", asFUNCTION(rs_v3_dot_val), asCALL_CDECL));
	R(e->RegisterGlobalFunction("float v3_len2(const V3 &in)", asFUNCTION(rs_v3_len2), asCALL_CDECL));
	R(e->RegisterObjectMethod("V3", "void scale(float)", asFUNCTION(rs_v3_m_scale), asCALL_CDECL_OBJFIRST));
	R(e->RegisterObjectMethod("V3", "float sum() const", asFUNCTION(rs_v3_m_sum), asCALL_CDECL_OBJFIRST));

	R(e->RegisterObjectType("I3", sizeof(I3), asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS | asOBJ_APP_CLASS_ALLINTS));
	R(e->RegisterObjectProperty("I3", "int a", 0));
	R(e->RegisterObjectProperty("I3", "int b", 4));
	R(e->RegisterObjectProperty("I3", "int c", 8));
	R(e->RegisterGlobalFunction("void i3_inc(I3 &out)", asFUNCTION(rs_i3_inc), asCALL_CDECL));
	R(e->RegisterGlobalFunction("I3 i3_make(int)", asFUNCTION(rs_i3_make), asCALL_CDECL));

	R(e->RegisterObjectType("Obj", 0, asOBJ_REF));
	R(e->RegisterObjectBehaviour("Obj", asBEHAVE_FACTORY, "Obj@ f()", asFUNCTION(rs_obj_factory), asCALL_CDECL));
	R(e->RegisterObjectBehaviour("Obj", asBEHAVE_ADDREF, "void f()", asFUNCTION(rs_obj_addref), asCALL_CDECL_OBJFIRST));
	R(e->RegisterObjectBehaviour("Obj", asBEHAVE_RELEASE, "void f()", asFUNCTION(rs_obj_release), asCALL_CDECL_OBJFIRST));
	R(e->RegisterObjectMethod("Obj", "int get()", asFUNCTION(rs_obj_get), asCALL_CDECL_OBJFIRST));
	R(e->RegisterObjectMethod("Obj", "void set(int)", asFUNCTION(rs_obj_set), asCALL_CDECL_OBJFIRST));
	R(e->RegisterObjectMethod("Obj", "int rc()", asFUNCTION(rs_obj_rc), asCALL_CDECL_OBJFIRST));
	R(e->RegisterGlobalFunction("int obj_pass(Obj@)", asFUNCTION(rs_obj_pass), asCALL_CDECL));
	R(e->RegisterGlobalFunction("Obj@ obj_ret(Obj@)", asFUNCTION(rs_obj_ret), asCALL_CDECL));
	R(e->RegisterGlobalFunction("Obj@ obj_null()", asFUNCTION(rs_obj_null), asCALL_CDECL));

	R(e->RegisterGlobalFunction("int str_len(const string &in)", asFUNCTION(rs_str_len), asCALL_CDECL));
	R(e->RegisterGlobalFunction("int str_first(const string &in)", asFUNCTION(rs_str_first), asCALL_CDECL));
	R(e->RegisterGlobalFunction("string@ str_upper(const string &in)", asFUNCTION(rs_str_upper), asCALL_CDECL));
	R(e->RegisterGlobalFunction("int str_inout(string &inout)", asFUNCTION(rs_str_inout), asCALL_CDECL));
	R(e->RegisterGlobalFunction("void check(bool, const string &in)", asFUNCTION(rs_check), asCALL_CDECL));

	R(e->RegisterEnum("Color"));
	R(e->RegisterEnumValue("Color", "Red", 1));
	R(e->RegisterEnumValue("Color", "Blue", 5));
	R(e->RegisterGlobalProperty("int rs_prop", rs_prop_ptr()));

	const char *src =
		"void main() {\n"
		" check(add_i(2,3)==5,'int'); check(add_f(1.5f,2.25f)==3.75f,'float'); check(add_d(1.5,2.25)==3.75,'double');\n"
		" check(not_(false)==true && not_(true)==false,'bool'); check(add_i64(4000000000,5)==4000000005,'int64');\n"
		" check(sum9(1,1,1,1,1,1,1,1,1)==45 && sum9(1,2,3,4,5,6,7,8,9)==285,'int stack spill');\n"
		" check(sum9d(1,1,1,1,1,1,1,1,1)==45.0 && sum9d(1,2,3,4,5,6,7,8,9)==285.0,'double stack spill');\n"
		" check(mixed(1,2.0f,3.0,true,4)==110.0,'mixed int/float/double/bool/i64');\n"
		" V3 a; check(a.x==1 && a.y==2 && a.z==3,'value ctor OBJFIRST');\n"
		" V3 b; b.x=10; V3 c = v3_add(a,b); check(c.x==11 && c.y==4 && c.z==6,'V3 const& in, V3 by value ret');\n"
		" V3 d; d.x=1; d.y=0; d.z=0; check(v3_dot_val(a,d)==1.0f,'V3 (HFA) by value arg');\n"
		" check(v3_len2(a)==14.0f,'V3 const ref float ret');\n"
		" a.scale(2.0f); check(a.x==2 && a.z==6,'OBJFIRST method + float arg'); check(a.sum()==12.0f,'OBJFIRST const method float ret');\n"
		" I3 i = i3_make(7); check(i.a==7&&i.b==8&&i.c==9,'I3 (12B ints) ret by value'); i3_inc(i); check(i.a==100&&i.c==102,'value &out');\n"
		" Obj@ o = Obj(); check(o.get()==40,'ref factory + OBJFIRST method'); o.set(5); check(o.get()==5,'method int arg');\n"
		" check(obj_pass(o)==6,'handle arg'); Obj@ p = obj_ret(o); check(p is o,'handle ret identity'); check(o.rc()==2,'handle ret refcount');\n"
		" @p = null; check(o.rc()==1,'release on handle clear');\n"
		" check(obj_null() is null,'null handle ret');\n"
		" check(str_len('hello')==5 && str_first('A')==65,'string const&in via accessor');\n"
		" string u = str_upper('abc'); check(u=='ABC','string@ ret (CScriptString new)');\n"
		" string w='xyz'; check(str_inout(w)==3,'string &inout');\n"
		" check(Red==1 && Blue==5,'enum'); check(rs_prop==77,'global property'); rs_prop=78; check(rs_prop==78,'global property write');\n"
		"}\n";
	asIScriptModule *m = e->GetModule("m", asGM_ALWAYS_CREATE);
	m->AddScriptSection("s", src);
	if (m->Build() < 0) { printf("BUILD FAIL\n"); return 2; }
	asIScriptContext *c = e->CreateContext();
	c->Prepare(m->GetFunctionByName("main"));
	int r = c->Execute();
	printf("execute=%d pass=%d fail=%d\n", r, gPass, gFail);
	c->Release();
	e->ShutDownAndRelease();
	return (r == asEXECUTION_FINISHED && !gFail) ? 0 : 1;
}
