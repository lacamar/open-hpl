#ifndef HPL_RUST_XML_H
#define HPL_RUST_XML_H

// C ABI of rust/src/xml/ffi.rs.

#include <cstddef>
#include <cstdio>
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
	bool ohpl_xml_doc_error(const OhplXmlDoc* doc);
	void ohpl_xml_doc_error_info(const OhplXmlDoc* doc, int* row, int* col, void (*sink)(void*, const char*, size_t), void* ctx);
	bool ohpl_xml_doc_walk(const OhplXmlDoc* doc, const OhplXmlTreeSink* sink);
	unsigned ohpl_xml_doc_add_element(OhplXmlDoc* doc, unsigned parent, const char* name, size_t n);
	void ohpl_xml_doc_set_value(OhplXmlDoc* doc, unsigned id, const char* name, size_t n);
	void ohpl_xml_doc_set_attr(OhplXmlDoc* doc, unsigned id, const char* name, size_t nn, const char* val, size_t vn);
	void ohpl_xml_doc_print_file(const OhplXmlDoc* doc, void (*sink)(void*, const char*, size_t), void* ctx);
	void ohpl_xml_doc_print_stream(const OhplXmlDoc* doc, void (*sink)(void*, const char*, size_t), void* ctx);
	bool ohpl_xml_config_get(const OhplXmlDoc* doc, const char* level, size_t ln, const char* name, size_t nn, void (*sink)(void*, const char*, size_t), void* ctx);
	void ohpl_xml_config_set(OhplXmlDoc* doc, const char* level, size_t ln, const char* name, size_t nn, const char* val, size_t vn);
	bool ohpl_xml_lang_read(const char* data, size_t len, bool readOk, bool addDirs, const OhplLangSink* sink);
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

// TiXmlDocument::LoadFile(FILE*): the whole file, size taken from the file position
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

inline bool OhplXmlSaveFile(const OhplXmlDoc* doc, FILE* file)
{
	std::string out;
	ohpl_xml_doc_print_file(doc, OhplXmlAssign, &out);
	fwrite(out.data(), 1, out.size(), file);
	return ferror(file) == 0;
}

#endif
