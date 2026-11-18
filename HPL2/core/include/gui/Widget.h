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

#ifndef HPL_WIDGET_H
#define HPL_WIDGET_H

#include <list>
#include <vector>
#include "gui/GuiTypes.h"
#include "gui/RustGuiH19.h"
#include "system/LowLevelSystem.h"

namespace hpl {

	class cGui;
	class cGuiSet;
	class cGuiSkin;
	class cGuiSkinFont;

	class cGuiGfxElement;
	class cGuiClipRegion;

	class cGuiGlobalShortcut;

	class iFontData;

	struct OhplGw;
	struct OhplH20Access;
	struct OhplG2PopupHost;

	struct OhplGwPub { cColor fontColor, fontHighlight; cVector2f fontSize; cColor colorMul; };

	extern "C" {
	// rust/src/gui/gw.rs: the widget is the shell's address, its state the Rust object right after the vptr.
	OhplGw* ohpl_gw_new(uintptr_t, int, cGuiSet*, cGuiSkin*);
	void ohpl_gw_destroy(uintptr_t);
	OhplH19Base* ohpl_gw_base(uintptr_t);
	const OhplGwPub* ohpl_gw_pub(uintptr_t);
	void ohpl_gw_init(uintptr_t);
	void ohpl_gw_load_graphics(uintptr_t);
	void ohpl_gw_update(uintptr_t, float);
	void ohpl_gw_draw(uintptr_t, float, uintptr_t);
	bool ohpl_gw_is_enabled(uintptr_t);
	bool ohpl_gw_is_visible(uintptr_t);
	void ohpl_gw_set_enabled(uintptr_t, bool);
	void ohpl_gw_set_visible(uintptr_t, bool);
	bool ohpl_gw_has_focus(uintptr_t);
	bool ohpl_gw_clips_graphics(uintptr_t);
	bool ohpl_gw_is_connected_to(uintptr_t, uintptr_t, bool);
	void ohpl_gw_attach_child_base(uintptr_t, uintptr_t);
	void ohpl_gw_remove_child_base(uintptr_t, uintptr_t);
	size_t ohpl_gw_child_count(uintptr_t);
	uintptr_t ohpl_gw_child(uintptr_t, size_t);
	void ohpl_gw_set_clip_active(uintptr_t, bool);
	void ohpl_gw_set_connected_to_children(uintptr_t, bool);
	void ohpl_gw_set_global_key_listener(uintptr_t, bool);
	bool ohpl_gw_global_key_listener(uintptr_t);
	void ohpl_gw_set_global_ui_listener(uintptr_t, bool);
	bool ohpl_gw_global_ui_listener(uintptr_t);
	void ohpl_gw_set_user_data(uintptr_t, void*);
	void* ohpl_gw_user_data(uintptr_t);
	void ohpl_gw_set_user_value(uintptr_t, int);
	int ohpl_gw_user_value(uintptr_t);
	bool ohpl_gw_tooltip_enabled(uintptr_t);
	void ohpl_gw_set_tooltip_enabled(uintptr_t, bool);
	cGuiGfxElement* ohpl_gw_pointer_gfx(uintptr_t);
	void ohpl_gw_set_pointer_gfx(uintptr_t, cGuiGfxElement*);
	iFontData* ohpl_gw_font_type(uintptr_t);
	cGuiSkinFont* ohpl_gw_skin_font(uintptr_t);
	void ohpl_gw_set_font_type_base(uintptr_t, iFontData*);
	void ohpl_gw_set_font_type(uintptr_t, iFontData*);
	void ohpl_gw_set_font_size_base(uintptr_t, float, float);
	void ohpl_gw_set_font_size(uintptr_t, float, float);
	void ohpl_gw_set_font_color_base(uintptr_t, const cColor*);
	void ohpl_gw_set_font_color(uintptr_t, const cColor*);
	void ohpl_gw_set_color_mul(uintptr_t, const cColor*);
	void ohpl_gw_set_global_position_(uintptr_t, float, float, float);
	void ohpl_gw_set_position_(uintptr_t, float, float, float);
	void ohpl_gw_set_size_(uintptr_t, float, float);
	void ohpl_gw_center_in_set(uintptr_t);
	void ohpl_gw_set_children_offset_(uintptr_t, float, float, float);
	void ohpl_gw_set_scroll_amount_(uintptr_t, float, float, float);
	void ohpl_gw_set_affected_by_scroll(uintptr_t, bool);
	const cVector3f* ohpl_gw_global_ptr(uintptr_t);
	bool ohpl_gw_has_focus_navigation(uintptr_t);
	void ohpl_gw_set_focus_navigation(uintptr_t, int, uintptr_t);
	uintptr_t ohpl_gw_focus_navigation(uintptr_t, int);
	bool ohpl_gw_process_message(uintptr_t, int, const cGuiMessageData*, bool, bool);
	void ohpl_gw_add_callback(uintptr_t, int, void*, tGuiCallbackFunc);
	bool ohpl_gw_focus_message(uintptr_t, int, int, const cGuiMessageData*);
	void ohpl_gw_forward_to_parent(uintptr_t, int, uintptr_t, const cGuiMessageData*);
	bool ohpl_gw_point_is_inside_base(uintptr_t, float, float, bool);
	bool ohpl_gw_point_is_inside(uintptr_t, float, float, bool);
	void ohpl_gw_attach_child(uintptr_t, uintptr_t);
	void ohpl_gw_remove_child(uintptr_t, uintptr_t);
	void ohpl_gw_attach_child(uintptr_t, uintptr_t);
	void ohpl_gw_remove_child(uintptr_t, uintptr_t);
	bool ohpl_gw_point_is_inside(uintptr_t, float, float, bool);
	void ohpl_gw_set_text(uintptr_t, const uint32_t*, size_t);
	const uint32_t* ohpl_gw_text(uintptr_t, size_t*);
	void ohpl_gw_set_tooltip(uintptr_t, const uint32_t*, size_t);
	const uint32_t* ohpl_gw_tooltip(uintptr_t, size_t*);
	void ohpl_gw_set_name(uintptr_t, const char*, size_t);
	const char* ohpl_gw_name(uintptr_t, size_t*);
	uintptr_t ohpl_gw_add_shortcut(uintptr_t, int, int, int, bool, bool);
	uintptr_t ohpl_gw_last_shortcut(uintptr_t);
	void ohpl_gw_draw_borders(uintptr_t, cGuiGfxElement*, cGuiGfxElement**, cGuiGfxElement**, const cVector3f*, const cVector2f*, bool);
	void ohpl_gw_set_kind(uintptr_t, int, int, int);
	}

	//--------------------------------

	// Facade over the Rust-owned widget (rust/src/gui/gw.rs): state and logic live there. This object is
	// the shell holding the Rust pointer right after the vptr (Rust reads it at that offset).
	class iWidget
	{
	friend class cGuiSet;
	friend struct OhplH20Access;
	friend struct OhplG2PopupHost;
	public:
		iWidget(eWidgetType aType,cGuiSet *apSet, cGuiSkin *apSkin)
			: mpGw(ohpl_gw_new((uintptr_t)this, aType, apSet, apSkin)), mB(*ohpl_gw_base((uintptr_t)this)),
			  mpSet(apSet), mpSkin(apSkin), mType(aType)
		{
		}
		virtual ~iWidget() { ohpl_gw_destroy(W()); }

		/////////////////////////
		//General
		void Update(float afTimeStep){ ohpl_gw_update(W(), afTimeStep); }

		void Draw(float afTimeStep, cGuiClipRegion *apClipRegion){ ohpl_gw_draw(W(), afTimeStep, (uintptr_t)apClipRegion); }

		bool ProcessMessage(eGuiMessage aMessage, const cGuiMessageData& aData, bool abSkipVisCheck=false, bool abSkipEnabledCheck=false)
		{ return ohpl_gw_process_message(W(), aMessage, &aData, abSkipVisCheck, abSkipEnabledCheck); }

		void AddCallback(eGuiMessage aMessage,void *apObject,tGuiCallbackFunc apFunc){ ohpl_gw_add_callback(W(), aMessage, apObject, apFunc); }

		eWidgetType GetType(){ return mType;}

		void Init(){ ohpl_gw_init(W()); }

		cGuiGlobalShortcut* AddShortcut(int alKeyModifiers, eKey aKey, eGuiMessage aMsg=eGuiMessage_ButtonPressed, bool abBypassVisibility=true, bool abBypassEnabled=true)
		{ return (cGuiGlobalShortcut*)ohpl_gw_add_shortcut(W(), alKeyModifiers, aKey, aMsg, abBypassVisibility, abBypassEnabled); }

		/////////////////////////
		// ToolTip functions
		void SetToolTip(const tWString& asToolTip){ ohpl_gw_set_tooltip(W(), U(asToolTip), asToolTip.size()); }
		tWString GetToolTip() { size_t n; const uint32_t* p = ohpl_gw_tooltip(W(), &n); return tWString((const wchar_t*)p, n); }
		void SetToolTipEnabled(bool abX) { ohpl_gw_set_tooltip_enabled(W(), abX); }
		bool IsToolTipEnabled() { return ohpl_gw_tooltip_enabled(W()); }

        /////////////////////////
		// Public Helper functions
		bool PointIsInside(const cVector2f& avPoint, bool abOnlyClipped){ return ohpl_gw_point_is_inside(W(), avPoint.x, avPoint.y, abOnlyClipped); }

		/////////////////////////
		//Hierarchy
		void AttachChild(iWidget *apChild){ ohpl_gw_attach_child(W(), (uintptr_t)apChild); }
		void RemoveChild(iWidget *apChild){ ohpl_gw_remove_child(W(), (uintptr_t)apChild); }
		tWidgetList GetChildren()
		{
			tWidgetList lst;
			size_t n = ohpl_gw_child_count(W());
			for(size_t i=0; i<n; ++i) lst.push_back((iWidget*)ohpl_gw_child(W(), i));
			return lst;
		}


		/////////////////////////
		//Properties
		cGuiSet* GetSet(){ return mpSet; }
		cGuiSkin* GetSkin(){ return mpSkin; }

		iWidget *GetParent(){ return mB.parent;}

        void SetEnabled(bool abX){ ohpl_gw_set_enabled(W(), abX); }
		bool IsEnabled(){ return ohpl_gw_is_enabled(W()); }
		void SetVisible(bool abX){ ohpl_gw_set_visible(W(), abX); }
		bool IsVisible(){ return ohpl_gw_is_visible(W()); }

		bool HasFocus(){ return ohpl_gw_has_focus(W()); }

		void SetName(const tString& asName){ ohpl_gw_set_name(W(), asName.data(), asName.size()); }
		tString GetName(){ size_t n; const char* p = ohpl_gw_name(W(), &n); return tString(p, n); }

		void SetText(const tWString& asText){ ohpl_gw_set_text(W(), U(asText), asText.size()); }
		tWString GetText(){ size_t n; const uint32_t* p = ohpl_gw_text(W(), &n); return tWString((const wchar_t*)p, n); }

		iFontData *GetDefaultFontType(){ return ohpl_gw_font_type(W());}
		void SetDefaultFontType(iFontData *apFont){ ohpl_gw_set_font_type(W(), apFont);}

		const cColor& GetDefaultFontColor(){ return ohpl_gw_pub(W())->fontColor;}
		void SetDefaultFontColor(const cColor& aColor){ ohpl_gw_set_font_color(W(), &aColor);}

		const cColor& GetDefaultFontHighlightColor(){ return ohpl_gw_pub(W())->fontHighlight;}
		cGuiSkinFont* GetDefaultSkinFont(){ return ohpl_gw_skin_font(W());}

		const cVector2f& GetDefaultFontSize(){ return ohpl_gw_pub(W())->fontSize;}
		void SetDefaultFontSize(const cVector2f& avSize){ ohpl_gw_set_font_size(W(), avSize.x, avSize.y);}

		void SetClipActive(bool abX){ ohpl_gw_set_clip_active(W(), abX);}
		bool GetClipActive(){ return mB.clips;}

		void SetPosition(const cVector3f &avPos){ ohpl_gw_set_position_(W(), avPos.x, avPos.y, avPos.z); }
		void SetGlobalPosition(const cVector3f &avPos){ ohpl_gw_set_global_position_(W(), avPos.x, avPos.y, avPos.z); }
		const cVector3f& GetLocalPosition(){ return mB.pos; }
		const cVector3f& GetGlobalPosition(){ return *ohpl_gw_global_ptr(W()); }

		void SetChildrenOffset(const cVector3f& avX){ ohpl_gw_set_children_offset_(W(), avX.x, avX.y, avX.z); }
		const cVector3f& GetChildrenOffset() { return mB.childrenOffset; }

		void SetAffectedByScroll(bool abX){ ohpl_gw_set_affected_by_scroll(W(), abX); }
		void SetScrollAmount(const cVector3f& avX){ ohpl_gw_set_scroll_amount_(W(), avX.x, avX.y, avX.z); }
		const cVector3f& GetScrollAmount() { return mB.scroll; }

		void CenterGlobalPositionInSet(){ ohpl_gw_center_in_set(W()); }

		void SetSize(const cVector2f &avSize){ ohpl_gw_set_size_(W(), avSize.x, avSize.y); }
		cVector2f GetSize(){ return mB.size;}

		void SetColorMul(const cColor &aColor){ ohpl_gw_set_color_mul(W(), &aColor);}
		const cColor& GetColorMul(){ return ohpl_gw_pub(W())->colorMul;}

		bool ClipsGraphics(){ return ohpl_gw_clips_graphics(W()); }

		bool GetMouseIsOver(){ return mB.mouseOver;}

		bool IsConnectedTo(iWidget *apWidget, bool abIsStartWidget=true){ return ohpl_gw_is_connected_to(W(), (uintptr_t)apWidget, abIsStartWidget); }
		bool IsConnectedToChildren(){ return mB.connectedToChildren;}
		void SetConnectedToChildren(bool abX){ ohpl_gw_set_connected_to_children(W(), abX);}

		cGuiGfxElement* GetPointerGfx(){ return ohpl_gw_pointer_gfx(W()); }
		void SetPointerGfx(cGuiGfxElement* apGfx){ ohpl_gw_set_pointer_gfx(W(), apGfx); }

		void SetGlobalKeyPressListener(bool abX) { ohpl_gw_set_global_key_listener(W(), abX); }
		bool IsGlobalKeyPressListener() { return ohpl_gw_global_key_listener(W()); }

		void SetUserData(void *apData) { ohpl_gw_set_user_data(W(), apData);}
		void * GetUserData() { return ohpl_gw_user_data(W());}

		void SetUserValue(int alX) { ohpl_gw_set_user_value(W(), alX);}
		int GetUserValue() { return ohpl_gw_user_value(W());}

		void SetCallbacksDisabled(bool abX) { mB.callbacksDisabled = abX; }
		bool GetCallbacksDisabled() { return mB.callbacksDisabled; }

		void SetFocusNavigation(eUIArrow aDir, iWidget* apWidget){ ohpl_gw_set_focus_navigation(W(), aDir, (uintptr_t)apWidget); }
		iWidget* GetFocusNavigation(eUIArrow aDir){ return (iWidget*)ohpl_gw_focus_navigation(W(), aDir); }

		bool HasFocusNavigation(){ return ohpl_gw_has_focus_navigation(W()); }

		void SetGlobalUIInputListener(bool abX) { ohpl_gw_set_global_ui_listener(W(), abX); }
		bool IsGlobalUIInputListener()			{ return ohpl_gw_global_ui_listener(W()); }

		cGuiGlobalShortcut* GetLastShortcut(){ return (cGuiGlobalShortcut*)ohpl_gw_last_shortcut(W()); }

	protected:
		/////////////////////////
		// Upper Widget functions







		bool GetFocus(const cGuiMessageData& aData){ return ohpl_gw_focus_message(W(), eGuiMessage_GotFocus, kOhplH19ChildGotFocus, &aData); }

		bool GetTabFocus(const cGuiMessageData& aData){ return ohpl_gw_focus_message(W(), eGuiMessage_GotTabFocus, kOhplH19ChildGotTabFocus, &aData); }

		bool LoseFocus(const cGuiMessageData& aData){ return ohpl_gw_focus_message(W(), eGuiMessage_LostFocus, kOhplH19ChildLostFocus, &aData); }






		/////////////////////////
		// Private Helper functions
		bool CheckPointInsideClippingParent(const cVector2f& avPoint)
		{
			if(mB.parent && mB.parent->ClipsGraphics())
				return mB.parent->PointIsInside(avPoint, true);
			return true;
		}
		cVector3f WorldToLocalPosition(const cVector3f &avPos){ return avPos - GetGlobalPosition(); }
		cVector2f GetPosRelativeToMouse(const cGuiMessageData& aData)
		{
			cVector3f vGlobal = GetGlobalPosition();
			return cVector2f(vGlobal.x - aData.mvPos.x, vGlobal.y - aData.mvPos.y);
		}

		//The order must be like this:
		//Borders: Right, Left, Up and Down
		//Corners: LEftUp, RightUp, RightDown and LEftDown.
		void DrawBordersAndCorners(cGuiGfxElement *apBackground,
									cGuiGfxElement **apBorderVec,cGuiGfxElement **apCornerVec,
									const cVector3f &avPosition, const cVector2f &avSize, bool abBordersOutside=false)
		{ ohpl_gw_draw_borders(W(), apBackground, apBorderVec, apCornerVec, &avPosition, &avSize, abBordersOutside); }

		void DrawSkinText(const tWString& asText,eGuiSkinFont aFont,const cVector3f& avPosition,
							eFontAlign aAlign = eFontAlign_Left);

		void DrawDefaultText(	const tWString& asText,
								const cVector3f& avPosition,eFontAlign aAlign);

		void DrawDefaultText(	const tWString& asText,
								const cVector3f& avPosition,eFontAlign aAlign, const cColor& aCol);

		void DrawDefaultTextHighlight(	const tWString& asText,
										const cVector3f& avPosition, eFontAlign aAlign);

		void LoadGraphics(){ ohpl_gw_load_graphics(W()); }

		/////////////////////////
		// Variables
		OhplGw *mpGw;	// first: Rust reads it right after the vptr

		// Shared with the Rust base; the references keep the member names.
		OhplH19Base &mB;

		cGuiSet *mpSet;
		cGuiSkin *mpSkin;

		cVector3f &mvPosition = mB.pos;
		cVector3f &mvGlobalPosition = mB.global;
		cVector2f &mvSize = mB.size;
		cVector3f &mvChildrenOffset = mB.childrenOffset;
		cVector3f &mvScrollAmount = mB.scroll;
		bool &mbAffectedByScroll = mB.affectedByScroll;

		eWidgetType mType;

        int &mlPositionCount = mB.positionCount;

		iWidget *&mpParent = mB.parent;

		bool &mbEnabled = mB.enabled;
		bool &mbVisible = mB.visible;

		bool &mbMouseIsOver = mB.mouseOver;

		bool &mbClipsGraphics = mB.clips;

        bool &mbConnectedToChildren = mB.connectedToChildren;

		bool &mbTextChanged = mB.textChanged;

		bool &mbCallbacksDisabled = mB.callbacksDisabled;
	protected:
		uintptr_t W() const { return (uintptr_t)this; }
		static const uint32_t* U(const tWString &asStr) { return reinterpret_cast<const uint32_t*>(asStr.data()); }
	private:
		void SetMouseIsOver(bool abX){ mB.mouseOver = abX;}
		bool &mbPositionIsUpdated = mB.positionUpdated;
	};

	static_assert(sizeof(wchar_t) == 4, "wide characters are UTF-32");
};
#endif // HPL_WIDGET_H
