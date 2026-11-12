// fs_probe DIRLIST: adds every directory in DIRLIST (absolute, with subdirectories, mask *.*) to one searcher,
// then looks up every file found on disk by name, parent/name, grandparent/parent/name, a wrong dir and upper case.
#include "common.h"
#include "resources/FileSearcher.h"
#include "system/String.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>

using namespace hpl;
namespace fs = std::filesystem;

int main(int argc, char **argv)
{
	if(argc < 2) return 2;
	cFileSearcher searcher;
	std::vector<std::string> dirs;
	{
		std::ifstream list(argv[1]);
		std::string d;
		while(std::getline(list, d)) dirs.push_back(d);
	}
	for(auto &d : dirs) searcher.AddDirectory(cString::To16Char(d), "*.*", true);

	std::vector<std::string> files;
	for(auto &d : dirs)
	{
		std::error_code ec;
		for(fs::recursive_directory_iterator it(d, fs::directory_options::skip_permission_denied, ec), end; it != end; it.increment(ec))
			if(!ec && it->is_regular_file(ec)) files.push_back(it->path().string());
	}
	std::sort(files.begin(), files.end());

	uint64_t total = 1469598103934665603ull;
	size_t count = 0;
	std::string out;
	auto Query = [&](const std::string &q, bool withCount)
	{
		int eq = -99;
		const tWString &r = searcher.GetFilePath(q, withCount ? &eq : NULL);
		std::string line = q + "\t" + cString::To8Char(r) + "\t" + std::to_string(eq) + "\n";
		total = Fnv(line, total);
		++count;
		if(argc > 2) out += line;
	};
	for(auto &f : files)
	{
		fs::path p(f);
		std::string name = p.filename().string();
		std::string parent = p.parent_path().filename().string();
		std::string grand = p.parent_path().parent_path().filename().string();
		Query(name, false);
		Query(name, true);
		Query(parent + "/" + name, true);
		Query(grand + "\\" + parent + "/" + name, true);
		Query("nodir_zz/" + name, true);
		std::string up = name;
		std::transform(up.begin(), up.end(), up.begin(), ::toupper);
		Query(up, false);
	}
	Query("definitely_missing_file.xyz", true);
	searcher.ClearDirectories();
	Query(files.empty() ? "x" : fs::path(files[0]).filename().string(), true);
	std::cout << "files=" << files.size() << " queries=" << count << " digest=" << Hex(total) << "\n";
	if(argc > 2) { FILE *o = fopen(argv[2], "wb"); fwrite(out.data(), 1, out.size(), o); fclose(o); }
	return 0;
}
