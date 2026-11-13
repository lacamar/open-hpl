// Compiles every .hps under the given dirs against the A1 declaration list (rust/tests/golden/A1/decls.txt).
// Built and run by scripts/a1-script-check.sh.
#include <angelscript.h>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include "impl/scriptstring.h"
#include "impl/scriptarray.h"

static void Dummy() {}

static std::vector<std::string> gvErrors;

static void OnMessage(const asSMessageInfo *m, void *)
{
	if (m->type == asMSGTYPE_ERROR)
	{
		char b[64];
		snprintf(b, sizeof b, ":%d:%d: ", m->row, m->col);
		gvErrors.push_back(std::string(m->section) + b + m->message);
	}
}

int main(int argc, char **argv)
{
	if (argc < 3) return 2;
	asIScriptEngine *e = asCreateScriptEngine(ANGELSCRIPT_VERSION);
	e->SetMessageCallback(asFUNCTION(OnMessage), 0, asCALL_CDECL);
	RegisterScriptString(e);
	RegisterScriptArray(e, true);
	std::ifstream d(argv[1]);
	std::string line;
	int nDecl = 0, nBad = 0;
	while (std::getline(d, line))
	{
		std::string decl = line.substr(0, line.rfind(" -> "));
		if (e->RegisterGlobalFunction(decl.c_str(), asFUNCTION(Dummy), asCALL_STDCALL) < 0)
		{
			++nBad;
			fprintf(stderr, "decl rejected: %s\n", decl.c_str());
		}
		++nDecl;
	}
	int nOk = 0, nFail = 0;
	for (int i = 2; i < argc; ++i)
		for (auto &p : std::filesystem::recursive_directory_iterator(argv[i]))
		{
			if (p.path().extension() != ".hps") continue;
			std::ifstream f(p.path());
			std::stringstream ss;
			ss << f.rdbuf();
			std::string src = ss.str();
			asIScriptModule *m = e->GetModule("m", asGM_ALWAYS_CREATE);
			m->AddScriptSection("main", src.c_str(), src.size());
			gvErrors.clear();
			if (m->Build() < 0)
			{
				++nFail;
				printf("FAIL %s\n", p.path().c_str());
				for (size_t j = 0; j < gvErrors.size() && j < 3; ++j) printf("  %s\n", gvErrors[j].c_str());
			}
			else
				++nOk;
		}
	printf("decls %d rejected %d compiled %d failed %d\n", nDecl, nBad, nOk, nFail);
	e->ShutDownAndRelease();
	return nBad || nFail ? 1 : 0;
}
