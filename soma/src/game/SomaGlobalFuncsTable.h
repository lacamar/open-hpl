#ifndef SOMA_GLOBAL_FUNCS_TABLE_H
#define SOMA_GLOBAL_FUNCS_TABLE_H

struct cSomaGlobalFunc
{
	const char *mpDecl;
	const char *mpClass;
	const char *mpFunc;
};

extern const cSomaGlobalFunc gvSomaGlobalFuncs[];
extern const int glSomaGlobalFuncsNum;

#endif // SOMA_GLOBAL_FUNCS_TABLE_H
