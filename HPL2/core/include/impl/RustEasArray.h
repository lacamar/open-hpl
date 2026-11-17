#ifndef OHPL_RUST_EAS_ARRAY_H
#define OHPL_RUST_EAS_ARRAY_H

// array<T> objects are Rust (rust/src/script/addons); this is the view the C++ natives get of one.
extern "C" {
	unsigned ohpl_asx_arr_size(const void *);
	void *ohpl_asx_arr_at(void *, unsigned);
	void ohpl_asx_arr_resize(void *, unsigned);
	void ohpl_asx_arr_set(void *, unsigned, void *);
	void ohpl_asx_arr_insert_last(void *, void *);
	int ohpl_asx_arr_elem_type(const void *);
	int ohpl_asx_register_array(void *engine, int defaultArray);
}

struct OhplEasArray
{
	unsigned GetSize() const { return ohpl_asx_arr_size(this); }
	void *At(unsigned i) { return ohpl_asx_arr_at(this, i); }
	const void *At(unsigned i) const { return ohpl_asx_arr_at(const_cast<OhplEasArray *>(this), i); }
	void Resize(unsigned n) { ohpl_asx_arr_resize(this, n); }
	void SetValue(unsigned i, void *v) { ohpl_asx_arr_set(this, i, v); }
	void InsertLast(void *v) { ohpl_asx_arr_insert_last(this, v); }
	int GetElementTypeId() const { return ohpl_asx_arr_elem_type(this); }
	OhplEasArray() = delete;
};

#endif
