#ifndef SOMA_SCRIPT_CHECK_H
#define SOMA_SCRIPT_CHECK_H

#include <string>

int RunSomaScriptCheck(const std::string &asGameDir, const std::string &asApiFile, const std::string &asReport);

// OPENHPL_A1_CHECK=<decls.txt> OPENHPL_A1_DIRS=<dir:dir..>: compiles every .hps under the dirs against the declarations
int RunA1Check(const std::string &asDecls, const std::string &asDirs);

// OPENHPL_AS_SUITE=<file.as>: runs main() of the file in an Amnesia-style script engine, prints what it passes to say()
int RunAddonSuite(const std::string &asFile);

#endif // SOMA_SCRIPT_CHECK_H
