#ifndef SOMA_GLOBAL_FUNCS_TABLE_H
#define SOMA_GLOBAL_FUNCS_TABLE_H

// Script globals the official engine forwards to an entity script's _Global_ function
struct cSomaGlobalFunc
{
	const char *mpDecl;
	const char *mpClass;
	const char *mpFunc;
};

extern const cSomaGlobalFunc gvSomaGlobalFuncs[];
extern const int glSomaGlobalFuncsNum;

#endif // SOMA_GLOBAL_FUNCS_TABLE_H
