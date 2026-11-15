// Builds the AINodeContainer / AINodeGenerator document shapes through TiXml and prints SaveFile bytes
// (hex) for each string in strings.txt. Run at the pre-L9 commit:
//   g++ -I<core>/include gen.cpp <core>/sources/impl/tinyXml/*.cpp && ./a.out strings.txt > out.txt
#include "impl/tinyXML/tinyxml.h"
#include <cstdio>
#include <fstream>
#include <string>

static std::string Unhex(const std::string &h)
{
	std::string s;
	for(size_t i = 0; i + 1 < h.size(); i += 2) s += (char)std::stoi(h.substr(i, 2), nullptr, 16);
	return s;
}

int main(int, char **argv)
{
	std::ifstream in(argv[1]);
	std::string line;
	while(std::getline(in, line))
	{
		std::string s = Unhex(line);
		TiXmlDocument doc;
		TiXmlElement *root = static_cast<TiXmlElement*>(doc.InsertEndChild(TiXmlElement("AINodes")));
		TiXmlElement *n1 = static_cast<TiXmlElement*>(root->InsertEndChild(TiXmlElement("Node")));
		n1->SetAttribute("Name", s.c_str());
		n1->SetAttribute("ID", "7");
		TiXmlElement *e = static_cast<TiXmlElement*>(n1->InsertEndChild(TiXmlElement("Edge")));
		e->SetAttribute("Node", s.c_str());
		e->SetAttribute("Distance", s.c_str());
		TiXmlElement *n2 = static_cast<TiXmlElement*>(root->InsertEndChild(TiXmlElement("Node")));
		n2->SetAttribute("Pos", s.c_str());
		n2->SetAttribute("Name", s.c_str());
		n2->SetAttribute("Name", "9");
		root->InsertEndChild(TiXmlElement("Node"));
		FILE *f = tmpfile();
		doc.SaveFile(f);
		fflush(f);
		long n = ftell(f);
		fseek(f, 0, SEEK_SET);
		std::string out(n, 0);
		if(fread(&out[0], 1, n, f) != (size_t)n) return 1;
		fclose(f);
		for(unsigned char c : out) printf("%02x", c);
		printf("\n");
	}
}
