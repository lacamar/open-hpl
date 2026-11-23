#ifndef HPL_RUST_XML_H
#define HPL_RUST_XML_H

// C ABI of rust/src/xml/ffi.rs.

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

struct OhplXmlDoc;

struct OhplXmlTreeSink
{
	void* ctx;
	void (*begin)(void*, const char*, size_t);
	void (*attr)(void*, const char*, size_t, const char*, size_t);
	void (*end)(void*);
};

struct OhplLangSink
{
	void* ctx;
	void (*directory)(void*, const char*, size_t, bool);
	void (*no_resources)(void*);
	void (*category)(void*, const char*, size_t);
	void (*entry)(void*, const char*, size_t, const int*, size_t);
};

extern "C" {
	OhplXmlDoc* ohpl_xml_doc_new();
	void ohpl_xml_doc_free(OhplXmlDoc* doc);
	bool ohpl_xml_doc_parse(OhplXmlDoc* doc, const char* s);
	bool ohpl_xml_doc_load(OhplXmlDoc* doc, const char* data, size_t len, bool readOk);
	void ohpl_xml_doc_error_info(const OhplXmlDoc* doc, int* row, int* col, void (*sink)(void*, const char*, size_t), void* ctx);
	bool ohpl_xml_doc_walk(const OhplXmlDoc* doc, const OhplXmlTreeSink* sink);
	unsigned ohpl_xml_doc_add_element(OhplXmlDoc* doc, unsigned parent, const char* name, size_t n);
	void ohpl_xml_doc_set_attr(OhplXmlDoc* doc, unsigned id, const char* name, size_t nn, const char* val, size_t vn);
	void ohpl_xml_doc_print_file(const OhplXmlDoc* doc, void (*sink)(void*, const char*, size_t), void* ctx);
	void ohpl_xml_doc_print_stream(const OhplXmlDoc* doc, void (*sink)(void*, const char*, size_t), void* ctx);
	void ohpl_xml_lang_decode(const char* s, size_t n, void (*sink)(void*, const int*, size_t), void* ctx);
}

inline void OhplXmlAssign(void* ctx, const char* p, size_t n) { static_cast<std::string*>(ctx)->assign(p, n); }

struct OhplXmlDocPtr
{
	OhplXmlDoc* p = ohpl_xml_doc_new();
	OhplXmlDocPtr() = default;
	OhplXmlDocPtr(const OhplXmlDocPtr&) = delete;
	OhplXmlDocPtr& operator=(const OhplXmlDocPtr&) = delete;
	~OhplXmlDocPtr() { ohpl_xml_doc_free(p); }
};

// whole-file load: the whole file, size taken from the file position
inline bool OhplXmlLoadFile(OhplXmlDoc* doc, FILE* file)
{
	fseek(file, 0, SEEK_END);
	long length = ftell(file);
	fseek(file, 0, SEEK_SET);
	if (length <= 0)
		return ohpl_xml_doc_load(doc, nullptr, 0, true);
	std::vector<char> buf(length);
	bool ok = fread(buf.data(), length, 1, file) == 1;
	return ohpl_xml_doc_load(doc, buf.data(), length, ok);
}

inline unsigned OhplXmlAddElement(OhplXmlDoc* doc, unsigned parent, const std::string& name)
{
	return ohpl_xml_doc_add_element(doc, parent, name.c_str(), name.size());
}

inline void OhplXmlSetAttr(OhplXmlDoc* doc, unsigned id, const char* name, const std::string& val)
{
	ohpl_xml_doc_set_attr(doc, id, name, strlen(name), val.c_str(), val.size());
}

inline void OhplXmlDumpBytes(const char* tag, const char* data, size_t n)
{
	const char* path = getenv("OHPL_L9_XMLDUMP");
	if (!path) return;
	unsigned long long h = 1469598103934665603ull;
	for (size_t i = 0; i < n; ++i) h = (h ^ (unsigned char)data[i]) * 1099511628211ull;
	if (FILE* f = fopen(path, "a")) {
		fprintf(f, "%s %zu %016llx\n", tag, n, h);
		fclose(f);
	}
}

inline void OhplXmlDumpFile(FILE* file)
{
	if (!getenv("OHPL_L9_XMLDUMP")) return;
	fflush(file);
	long end = ftell(file);
	if (end < 0) return;
	std::string buf(end, '\0');
	fseek(file, 0, SEEK_SET);
	size_t got = fread(&buf[0], 1, end, file);
	fseek(file, end, SEEK_SET);
	OhplXmlDumpBytes("file", buf.data(), got);
}

inline bool OhplXmlSaveFile(const OhplXmlDoc* doc, FILE* file)
{
	std::string out;
	ohpl_xml_doc_print_file(doc, OhplXmlAssign, &out);
	fwrite(out.data(), 1, out.size(), file);
	OhplXmlDumpFile(file);
	return ferror(file) == 0;
}

#endif
