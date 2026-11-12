// Prints GetDataDir from binreloc and from ohpl_h1_data_dir; both lines must match. Also spawns `true`.
#include <cstdio>
#include <cstdlib>
#include <string>
#include <sys/wait.h>
#include "binreloc.h"
#include "system/RustH1.h"

int main()
{
	std::string a;
	BrInitError e;
	if(br_init(&e)) { char *d = br_find_exe_dir(NULL); if(d) { a = d; free(d); } }
	std::string b;
	ohpl_h1_data_dir([](void *c, const unsigned char *p, size_t n) { static_cast<std::string*>(c)->assign((const char*)p, n); }, &b);
	printf("binreloc=%s\nrust=%s\n", a.c_str(), b.c_str());
	const char *argv[] = { "false" };
	int pid = ohpl_h1_spawn("false", argv, 1), st = 0;
	waitpid(pid, &st, 0);
	const char *bad[] = { "nonexistent-ohpl" };
	int pid2 = ohpl_h1_spawn("nonexistent-ohpl", bad, 1), st2 = 0;
	waitpid(pid2, &st2, 0);
	printf("spawn false exit=%d, missing exit=%d\n", WEXITSTATUS(st), WEXITSTATUS(st2));
	return a == b ? 0 : 1;
}
