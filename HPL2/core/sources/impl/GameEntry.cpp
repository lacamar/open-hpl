#include "impl/RustL1.h"
#include "system/String.h"

extern int hplMain(const hpl::tString &asCommandLine);

int OhplGameMain(const unsigned char *apCmd, size_t alLen)
{
	return hplMain(hpl::tString((const char *)apCmd, alLen));
}
