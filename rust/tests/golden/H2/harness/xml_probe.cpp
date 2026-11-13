// xml_probe MODE ROOT LIST: one line per file in LIST (paths relative to ROOT).
//   api   cXmlDocumentTiny: tree dump + SaveToString digest
//   tiny  raw tinyXML reference: load/error/print digest
//   rust  the Rust XML layer through its C ABI, same digest as tiny
#include "common.h"
#include "impl/XmlDocumentTiny.h"
#include "system/String.h"
#include <fstream>
#include <iostream>
#include "impl/tinyXML/tinyxml.h"
#include "system/RustXml.h"

using namespace hpl;

static void Dump(iXmlNode *apNode, std::string &out, int depth)
{
	cXmlElement *pElem = apNode->ToElement();
	out += std::string(depth, ' ') + apNode->GetValue();
	for(auto &kv : *pElem->GetAttributeMap()) out += "|" + kv.first + "=" + kv.second;
	out += "\n";
	cXmlNodeListIterator it = apNode->GetChildIterator();
	while(it.HasNext()) Dump(it.Next(), out, depth + 1);
}

static std::string ReadFile(const std::string &p)
{
	std::string out;
	FILE *f = fopen(p.c_str(), "rb");
	if(!f) return out;
	char buf[65536];
	size_t n;
	while((n = fread(buf, 1, sizeof buf, f)) > 0) out.append(buf, n);
	fclose(f);
	return out;
}

static std::string Header(bool ok, int row, int col, const std::string &desc)
{
	if(ok) return "1|0|0|\n";
	return "0|" + std::to_string(row) + "|" + std::to_string(col) + "|" + desc + "\n";
}

int main(int argc, char **argv)
{
	if(argc < 4) return 2;
	std::string mode = argv[1], root = argv[2], tmp = std::string(argv[3]) + ".tmp";
	std::ifstream list(argv[3]);
	std::string rel;
	while(std::getline(list, rel))
	{
		std::string path = root + "/" + rel;
		std::string line = rel + "\t";
		if(mode == "api")
		{
			cXmlDocumentTiny doc("probe");
			bool ok = doc.CreateFromFile(cString::To16Char(path));
			std::string dump, save;
			if(ok) { Dump(&doc, dump, 0); doc.SaveToString(&save); }
			else dump = std::to_string(doc.GetErrorRow()) + "|" + std::to_string(doc.GetErrorCol()) + "|" + doc.GetErrorDesc();
			line += (ok ? "1" : "0") + std::string("\t") + Hex(Fnv(dump)) + "\t" + Hex(Fnv(save));
		}
		else if(mode == "tiny")
		{
			TiXmlDocument doc;
			FILE *f = fopen(path.c_str(), "rb");
			bool ok = f && doc.LoadFile(f);
			if(f) fclose(f);
			std::string s = Header(ok, doc.ErrorRow(), doc.ErrorCol(), doc.ErrorDesc());
			FILE *o = fopen(tmp.c_str(), "wb");
			doc.SaveFile(o);
			fclose(o);
			s += ReadFile(tmp);
			std::string stream;
			stream << doc;
			s += stream;
			line += Hex(Fnv(s));
		}
		else if(mode == "rust")
		{
			OhplXmlDocPtr doc;
			FILE *f = fopen(path.c_str(), "rb");
			bool ok = f && OhplXmlLoadFile(doc.p, f);
			if(f) fclose(f);
			int row = 0, col = 0;
			std::string desc;
			ohpl_xml_doc_error_info(doc.p, &row, &col, OhplXmlAssign, &desc);
			std::string s = Header(ok, row, col, desc);
			std::string file, stream;
			ohpl_xml_doc_print_file(doc.p, OhplXmlAssign, &file);
			ohpl_xml_doc_print_stream(doc.p, OhplXmlAssign, &stream);
			line += Hex(Fnv(s + file + stream));
		}
		std::cout << line << "\n";
	}
	remove(tmp.c_str());
	return 0;
}
