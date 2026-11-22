/*
 * Copyright © 2009-2020 Frictional Games
 * 
 * This file is part of Amnesia: The Dark Descent.
 * 
 * Amnesia: The Dark Descent is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version. 

 * Amnesia: The Dark Descent is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with Amnesia: The Dark Descent.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef HPL_WIDGET_BASE_CLASSES_H
#define HPL_WIDGET_BASE_CLASSES_H

#include "gui/GuiTypes.h"
#include "system/String.h"

namespace hpl {

	class cWidgetMultiPropertyItem;
	class cWidgetItemProperty;
	class cWidgetItem;

	typedef std::vector<cWidgetItem*> tWidgetItemVec;
	typedef tWidgetItemVec::iterator tWidgetItemVecIt;

	typedef std::vector<cWidgetItemProperty*> tWidgetItemPropertyVec;
	typedef tWidgetItemPropertyVec::iterator tWidgetItemPropertyVecIt;

	enum eItemPropertyType
	{
		eItemPropertyType_String,
		eItemPropertyType_Image,

		eItemProperty_LastEnum
	};

	enum eItemSelectType
	{
		eItemSelectType_Select,
		eItemSelectType_Toggle,
		eItemSelectType_Deselect,
	};

	extern "C" {
	// rust/src/gui/gw_items.rs: items are owned by the list box or combo box (the container is its widget address).
	size_t ohpl_gwi_num(uintptr_t);
	cWidgetItem* ohpl_gwi_get(uintptr_t, int);
	cWidgetItem* ohpl_gwi_add(uintptr_t, const uint32_t*, size_t);
	void ohpl_gwi_remove_at(uintptr_t, int);
	void ohpl_gwi_remove_text(uintptr_t, const uint32_t*, size_t);
	void ohpl_gwi_clear(uintptr_t);
	bool ohpl_gwi_has(uintptr_t, const uint32_t*, size_t);
	void ohpl_gwi_set_text_at(uintptr_t, int, const uint32_t*, size_t);
	const uint32_t* ohpl_gwi_text(cWidgetItem*, size_t*);
	void ohpl_gwi_set_text(cWidgetItem*, const uint32_t*, size_t);
	int ohpl_gwi_index(cWidgetItem*);
	void ohpl_gwi_set_index(cWidgetItem*, int);
	bool ohpl_gwi_selectable(cWidgetItem*);
	void ohpl_gwi_set_selectable(cWidgetItem*, bool);
	bool ohpl_gwi_selected(cWidgetItem*);
	void ohpl_gwi_set_selected(cWidgetItem*, bool);
	void* ohpl_gwi_user_data(cWidgetItem*);
	void ohpl_gwi_set_user_data(cWidgetItem*, void*);
	int ohpl_gwi_user_value(cWidgetItem*);
	void ohpl_gwi_set_user_value(cWidgetItem*, int);
	int ohpl_gwi_prop_num(cWidgetItem*);
	cWidgetItemProperty* ohpl_gwi_prop(cWidgetItem*, int);
	void ohpl_gwi_add_prop_text(cWidgetItem*, const uint32_t*, size_t);
	void ohpl_gwi_add_prop_gfx(cWidgetItem*, uintptr_t);
	int ohpl_gwi_prop_type(cWidgetItemProperty*);
	uintptr_t ohpl_gwi_prop_gfx(cWidgetItemProperty*);
	const uint32_t* ohpl_gwi_prop_text(cWidgetItemProperty*, size_t*);
	}

	/////////////////////////////////////////////////////////////
	// ITEM PROPERTY CLASS (an opaque Rust object)
	/////////////////////////////////////////////////////////////

	class cWidgetItemProperty
	{
	public:
		tWString GetText() { size_t n; const uint32_t* p = ohpl_gwi_prop_text(this, &n); return tWString((const wchar_t*)p, n); }
		cGuiGfxElement* GetGfx() { return (cGuiGfxElement*)ohpl_gwi_prop_gfx(this); }

		eItemPropertyType GetType() { return (eItemPropertyType)ohpl_gwi_prop_type(this); }
	private:
		cWidgetItemProperty();
	};

	/////////////////////////////////////////////////////////////
	// ITEM CLASS (an opaque Rust object)
	/////////////////////////////////////////////////////////////

	class cWidgetItem
	{
	public:
		void SetIndex(int alIdx) { ohpl_gwi_set_index(this, alIdx); }
		int GetIndex() { return ohpl_gwi_index(this); }

		tWString GetText()const { size_t n; const uint32_t* p = ohpl_gwi_text((cWidgetItem*)this, &n); return tWString((const wchar_t*)p, n); }
		cWidgetItemProperty* GetProperty(int alIdx) const { return ohpl_gwi_prop((cWidgetItem*)this, alIdx); }

		void AddProperty(const tWString& asText) { ohpl_gwi_add_prop_text(this, (const uint32_t*)asText.data(), asText.size()); }
		void AddProperty(const cGuiGfxElement* apGfx) { ohpl_gwi_add_prop_gfx(this, (uintptr_t)apGfx); }

		int GetPropertyNum() { return ohpl_gwi_prop_num(this); }

		bool IsSelectable() { return ohpl_gwi_selectable(this); }
		void SetSelectable(bool abX) { ohpl_gwi_set_selectable(this, abX); }
		bool IsSelected() { return ohpl_gwi_selected(this); }
		void SetSelected(bool abX) { ohpl_gwi_set_selected(this, abX); }

		void SetText(const tWString& asText) { ohpl_gwi_set_text(this, (const uint32_t*)asText.data(), asText.size()); }

		void SetUserData(void* apData) { ohpl_gwi_set_user_data(this, apData); }
		void* GetUserData() { return ohpl_gwi_user_data(this); }

		void SetUserValue(int alValue) { ohpl_gwi_set_user_value(this, alValue); }
		int GetUserValue() { return ohpl_gwi_user_value(this); }
	private:
		cWidgetItem();
	};

	// The container API of the list box and combo box shells (`this` is the widget).
#define OHPL_ITEM_CONTAINER_API \
	cWidgetItem* AddItem(const tString& asItem){ return AddItem(cString::To16Char(asItem)); } \
	cWidgetItem* AddItem(const tWString &asItem){ return ohpl_gwi_add((uintptr_t)this, (const uint32_t*)asItem.data(), asItem.size()); } \
	cWidgetItem* AddItem(){ return AddItem(_W("")); } \
	void RemoveItem(int alX){ ohpl_gwi_remove_at((uintptr_t)this, alX); } \
	void RemoveItem(const tWString &asItem){ ohpl_gwi_remove_text((uintptr_t)this, (const uint32_t*)asItem.data(), asItem.size()); } \
	void ClearItems(){ ohpl_gwi_clear((uintptr_t)this); } \
	cWidgetItem* GetItem(int alX) const { return ohpl_gwi_get((uintptr_t)this, alX); } \
	tWString GetItemText(int alX) const { cWidgetItem* p = GetItem(alX); return p ? p->GetText() : tWString(); } \
	void SetItemText(int alX, const tWString& asText){ ohpl_gwi_set_text_at((uintptr_t)this, alX, (const uint32_t*)asText.data(), asText.size()); } \
	int GetItemNum() const { return (int)ohpl_gwi_num((uintptr_t)this); } \
	bool HasItem(const tWString &asItem){ return ohpl_gwi_has((uintptr_t)this, (const uint32_t*)asItem.data(), asItem.size()); } \
	bool HasItem(const tString &asItem){ return HasItem(cString::To16Char(asItem)); }

	//--------------------------------------

	// The file picker's browser is Rust-owned (rust/src/gui/h21_fb.rs); only its start folders are set from C++.
	extern "C" void ohpl_g2fb_set_dir(int alKind, const uint32_t* apText, size_t alLen);

	template<int K> struct OhplFbDir
	{
		OhplFbDir& operator=(const tWString& asDir){ ohpl_g2fb_set_dir(K, (const uint32_t*)asDir.data(), asDir.size()); return *this; }
	};

	struct iFileBrowser
	{
		static inline OhplFbDir<0> msGameDir;
		static inline OhplFbDir<1> msPersonalDir;
	};

	//--------------------------------------

};
#endif // HPL_WIDGET_BASE_CLASSES_H
