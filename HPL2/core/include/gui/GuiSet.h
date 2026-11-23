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

#ifndef HPL_GUI_SET_H
#define HPL_GUI_SET_H

#include <list>
#include "gui/GuiTypes.h"
#include "graphics/GraphicsTypes.h"
#include "gui/RustGuiG1.h"
#include "gui/RustGuiGw.h"
#include "gui/GuiPopUpMessageBox.h"
#include "gui/GuiPopUpFilePicker.h"
#include "gui/GuiPopUpColorPicker.h"
#include "gui/GuiPopUpUIKeyboard.h"

namespace hpl {

	//---------------------------------------------

	class cResources;
	class cGraphics;
	class iLowLevelGraphics;
	class cSound;
	class cScene;

	class cFrustum;
	class iFontData;

	class cGui;
	class cGuiSkin;
	class iGuiMaterial;
	class iGpuProgram;
	class iGuiPopUp;
	class iWidget;

	class cWidgetWindow;
	class cWidgetFrame;
	class cWidgetButton;
	class cWidgetLabel;
	class cWidgetSlider;
	class cWidgetTextBox;
	class cWidgetCheckBox;
	class cWidgetImage;
	class cWidgetListBox;
	class cWidgetMultiPropertyListBox;
	class cWidgetComboBox;
	class cWidgetMenuItem;
	class cWidgetContextMenu;
	class cWidgetMainMenu;
	class cWidgetTab;
	class cWidgetTabLabel;
	class cWidgetTabFrame;
	class cWidgetGroup;
	class cWidgetDummy;

	class cGuiPopUpMessageBox;
	class cGuiPopUpFilePicker;
	class cGuiPopUpColorPicker;
	class cGuiPopUpUIKeyboard;

	//---------------------------------------------

	struct OhplGuiGfxInfo;
	class cGuiGfxElement;

	// Rust-owned (rust/src/gui/g1_set.rs); `this` is the Rust allocation.
	class cGuiClipRegion
	{
	public:
		cGuiClipRegion* CreateChild(const cVector3f &avPos, const cVector2f &avSize){ return ohpl_g1_clip_create_child(this, &avPos, &avSize); }
	private:
		cGuiClipRegion();
	};

	// Rust-owned, created by cGuiSet::AddGlobalShortcut.
	class cGuiGlobalShortcut
	{
	public:
		const cKeyPress& GetKey() { return *ohpl_g1_shortcut_key(this); }
		bool IsEnabled() { return ohpl_g1_shortcut_enabled(this); }
		void SetEnabled(bool abX) { ohpl_g1_shortcut_set_enabled(this, abX); }

		bool ProcessKeyPress(const cKeyPress& aKey) { return ohpl_g1_shortcut_process_key_press(this, &aKey); }
		bool Exec() { return ohpl_g1_shortcut_exec(this); }
		tString ToString()
		{
			tString sText;
			ohpl_g1_shortcut_to_string(this, [](void* c, const char* p, size_t n){ static_cast<tString*>(c)->assign(p, n); }, &sText);
			return sText;
		}
	private:
		cGuiGlobalShortcut();
	};

	// Rust-owned (rust/src/gui/g1_set.rs): plain fields are read through P(), the rest through
	// the ohpl_g1_set_* ABI. Widget, popup and tooltip construction and rendering stay C++
	// (GuiSetWidgets.cpp, RustGuiG1.cpp).
	class cGuiSet
	{
	friend class iGuiPopUp;
	static const uint32_t* U(const tWString &asStr) { return reinterpret_cast<const uint32_t*>(asStr.data()); }
	public:
		void Update(float afTimeStep){ ohpl_g1_set_update(this, afTimeStep); }
		void DrawAll(float afTimeStep){ ohpl_g1_set_draw_all(this, afTimeStep); }

		bool SendMessage(eGuiMessage aMessage, const cGuiMessageData& aData){ return ohpl_g1_set_send_message(this, aMessage, &aData); }

		cGuiGlobalShortcut* AddGlobalShortcut(int alKeyModifiers, eKey aKey,  iWidget* apWidget, eGuiMessage aMessage, bool abBypassVisibility=true, bool abBypassEnabled=true)
		{ return ohpl_g1_set_add_global_shortcut(this, alKeyModifiers, aKey, (uintptr_t)apWidget, aMessage, abBypassVisibility, abBypassEnabled); }
		void RemoveGlobalShortcut(cGuiGlobalShortcut* apShortcut){ ohpl_g1_set_remove_global_shortcut(this, apShortcut); }
		void ClearGlobalShortcuts(){ ohpl_g1_set_clear_global_shortcuts(this); }

		void DestroyAllWidgets(){ ohpl_g1_set_destroy_all_widgets(this); }
		void ResetMouseOver(){ ohpl_g1_set_reset_mouse_over(this); }

		void Render(cFrustum *apFrustum);
		void ClearRenderObjects(){ ohpl_g1_set_clear_render_objects(this); }

		void SetDrawOffset(const cVector3f& avOffset){ P().drawOffset = avOffset;}

		void SetCurrentClipRegion(cGuiClipRegion *apRegion){ ohpl_g1_set_set_current_clip(this, apRegion);}
		cGuiClipRegion* GetCurrentClipRegion(){ return ohpl_g1_set_current_clip(this);}
		cGuiClipRegion* GetBaseClipRegion(){ return ohpl_g1_set_base_clip(this);}

		void DrawGfx(	cGuiGfxElement* apGfx,
						const cVector3f &avPos,
						const cVector2f &avSize=-1,
						const cColor& aColor=cColor(1,1),
						eGuiMaterial aMaterial = eGuiMaterial_LastEnum,
						float afRotationAngle=0,
						bool abUseCustomPivot=false, const cVector3f& avCustomPivot=0)
		{
			if(ohpl_a11c_trace_on) ohpl_a11c_trace_gfx(this, apGfx, &avPos, &avSize, &aColor, (int)aMaterial);
			ohpl_g1_set_draw_gfx(this, apGfx, &avPos, &avSize, &aColor, aMaterial, afRotationAngle, abUseCustomPivot ? &avCustomPivot : NULL);
		}

		void DrawFont (	const tWString &asText,
						iFontData *apFont, const cVector3f &avPos,
						const cVector2f &avSize, const cColor& aColor,
						eFontAlign aAlign = eFontAlign_Left,
						eGuiMaterial aMaterial = eGuiMaterial_FontNormal);

		void DrawFont (	iFontData *apFont, const cVector3f &avPos,
						const cVector2f &avSize, const cColor& aColor,
						eFontAlign aAlign,eGuiMaterial aMaterial,
						const wchar_t* fmt,...);

		void DrawFont (	iFontData *apFont, const cVector3f &avPos,
						const cVector2f &avSize, const cColor& aColor,
						const wchar_t* fmt,...);

		cWidgetWindow* CreateWidgetWindow(	tWidgetWindowButtonFlag alFlags=eWidgetWindowButtonFlag_None,
											const cVector3f &avLocalPos=0,
											const cVector2f &avSize=0,
											const tWString &asText=_W(""),
											iWidget *apParent=NULL,
											const tString& asName = "" )
		{
			return (cWidgetWindow*)ohpl_gw_create(this, kOhplGwWindow, &avLocalPos, &avSize, U(asText), asText.size(), (uintptr_t)apParent, asName.data(), asName.size(), alFlags, 0, 0, NULL);
		}

		cWidgetFrame* CreateWidgetFrame(	const cVector3f &avLocalPos=0,
											const cVector2f &avSize=0,
											bool abDrawFrame=false,
											iWidget *apParent=NULL,
											bool abHScrollBar=false,
											bool abVScrollBar=false,
											const tString& asName = "" )
		{
			return (cWidgetFrame*)ohpl_gw_create(this, kOhplGwFrame, &avLocalPos, &avSize, NULL, 0, (uintptr_t)apParent, asName.data(), asName.size(), abHScrollBar, abVScrollBar, abDrawFrame, NULL);
		}

		cWidgetButton* CreateWidgetButton(	const cVector3f &avLocalPos=0,
											const cVector2f &avSize=0,
											const tWString &asText=_W(""),
											iWidget *apParent=NULL,
											bool abToggleable=false,
											const tString& asName = "")
		{
			return (cWidgetButton*)ohpl_gw_create(this, kOhplGwButton, &avLocalPos, &avSize, U(asText), asText.size(), (uintptr_t)apParent, asName.data(), asName.size(), abToggleable, 0, 0, NULL);
		}

		cWidgetLabel* CreateWidgetLabel(	const cVector3f &avLocalPos=0,
											const cVector2f &avSize=0,
											const tWString &asText=_W(""),
											iWidget *apParent=NULL,
											const tString& asName = "" )
		{
			return (cWidgetLabel*)ohpl_gw_create(this, kOhplGwLabel, &avLocalPos, &avSize, U(asText), asText.size(), (uintptr_t)apParent, asName.data(), asName.size(), 0, 0, 0, NULL);
		}

		cWidgetSlider* CreateWidgetSlider(	eWidgetSliderOrientation aOrientation,
											const cVector3f &avLocalPos=0,
											const cVector2f &avSize=0,
											int alMaxValue=10,
											iWidget *apParent=NULL,
											const tString& asName = "" )
		{
			return (cWidgetSlider*)ohpl_gw_create(this, kOhplGwSlider, &avLocalPos, &avSize, NULL, 0, (uintptr_t)apParent, asName.data(), asName.size(), aOrientation, 0, alMaxValue, NULL);
		}

		cWidgetTextBox* CreateWidgetTextBox(const cVector3f &avLocalPos=0,
											const cVector2f &avSize=0,
											const tWString &asText=_W(""),
											iWidget *apParent=NULL,
											eWidgetTextBoxInputType aType = eWidgetTextBoxInputType_Normal,
											float afNumericAdd=1.0f,
											bool abShowButtons=true,
											const tString& asName = "" )
		{
			return (cWidgetTextBox*)ohpl_gw_create_c(this, kOhplGwTextBox, &avLocalPos, &avSize, U(asText), asText.size(), (uintptr_t)apParent, asName.data(), asName.size(), (int)aType, abShowButtons, afNumericAdd);
		}

		cWidgetCheckBox* CreateWidgetCheckBox(const cVector3f &avLocalPos=0,
											const cVector2f &avSize=0,
											const tWString &asText=_W(""),
											iWidget *apParent=NULL,
											const tString& asName = "" )
		{
			return (cWidgetCheckBox*)ohpl_gw_create(this, kOhplGwCheck, &avLocalPos, &avSize, U(asText), asText.size(), (uintptr_t)apParent, asName.data(), asName.size(), 0, 0, 0, NULL);
		}

		cWidgetImage* CreateWidgetImage(const tString &asFile="",
										const cVector3f &avLocalPos=0,
										const cVector2f &avSize=-1,
										eGuiMaterial aMaterial = eGuiMaterial_Alpha,
										bool abAnimate=false,
										iWidget *apParent=NULL,
										const tString& asName = "" )
		{
			return (cWidgetImage*)ohpl_gw_create_image(this, asFile.data(), asFile.size(), &avLocalPos, &avSize, aMaterial, abAnimate, (uintptr_t)apParent, asName.data(), asName.size());
		}

		cWidgetListBox* CreateWidgetListBox(const cVector3f &avLocalPos=0,
											const cVector2f &avSize=0,
											iWidget *apParent=NULL,
											const tString& asName = "" )
		{
			tWString asText; return (cWidgetListBox*)ohpl_gw_create_c(this, kOhplGwList, &avLocalPos, &avSize, U(asText), asText.size(), (uintptr_t)apParent, asName.data(), asName.size(), 0, 0, 0);
		}

		cWidgetMultiPropertyListBox* CreateWidgetMultiPropertyListBox(	const cVector3f& avLocalPos=0,
																		const cVector2f& avSize=0,
																		iWidget* apParent=NULL,
																		const tString& asName = "")
		{
			tWString asText; return (cWidgetMultiPropertyListBox*)ohpl_gw_create_c(this, kOhplGwMpList, &avLocalPos, &avSize, U(asText), asText.size(), (uintptr_t)apParent, asName.data(), asName.size(), 0, 0, 0);
		}

		cWidgetComboBox* CreateWidgetComboBox(const cVector3f &avLocalPos=0,
												const cVector2f &avSize=0,
												const tWString &asText=_W(""),
												iWidget *apParent=NULL,
												const tString& asName = "")
		{
			return (cWidgetComboBox*)ohpl_gw_create_c(this, kOhplGwCombo, &avLocalPos, &avSize, U(asText), asText.size(), (uintptr_t)apParent, asName.data(), asName.size(), 0, 0, 0);
		}

		cWidgetMenuItem* CreateWidgetMenuItem(const cVector3f &avLocalPos=0,
												const cVector2f &avSize=0,
												const tWString &asText=_W(""),
												iWidget *apParent=NULL,
												const tString& asName = "")
		{
			return (cWidgetMenuItem*)ohpl_gw_create_c(this, kOhplGwMenuItem, &avLocalPos, &avSize, U(asText), asText.size(), (uintptr_t)apParent, asName.data(), asName.size(), 0, 0, 0);
		}

		cWidgetContextMenu* CreateWidgetContextMenu(const cVector3f &avLocalPos=0,
												const cVector2f &avSize=0,
												const tWString &asText=_W(""),
												iWidget *apParent=NULL,
												const tString& asName = "")
		{
			return (cWidgetContextMenu*)ohpl_gw_create_c(this, kOhplGwCtxMenu, &avLocalPos, &avSize, U(asText), asText.size(), (uintptr_t)apParent, asName.data(), asName.size(), 0, 0, 0);
		}

		cWidgetMainMenu* CreateWidgetMainMenu(iWidget *apParent=NULL,
												const tString& asName = "")
		{
			cVector3f avLocalPos; cVector2f avSize; tWString asText; return (cWidgetMainMenu*)ohpl_gw_create_c(this, kOhplGwMainMenu, &avLocalPos, &avSize, U(asText), asText.size(), (uintptr_t)apParent, asName.data(), asName.size(), 0, 0, 0);
		}

		cWidgetTabLabel* CreateWidgetTabLabel(const cVector3f &avLocalPos=0,
												const cVector2f &avSize=0,
												const tWString &asText=_W(""),
												iWidget *apParent=NULL,
												const tString& asName = "")
		{
			return (cWidgetTabLabel*)ohpl_gw_create_c(this, kOhplGwTabLabel, &avLocalPos, &avSize, U(asText), asText.size(), (uintptr_t)apParent, asName.data(), asName.size(), 0, 0, 0);
		}

		cWidgetTab* CreateWidgetTab(const cVector3f &avLocalPos=0,
												const cVector2f &avSize=0,
												const tWString &asText=_W(""),
												iWidget *apParent=NULL,
												const tString& asName = "")
		{
			return (cWidgetTab*)ohpl_gw_create_c(this, kOhplGwTab, &avLocalPos, &avSize, U(asText), asText.size(), (uintptr_t)apParent, asName.data(), asName.size(), 0, 0, 0);
		}

		cWidgetTabFrame* CreateWidgetTabFrame(const cVector3f &avLocalPos=0,
												const cVector2f &avSize=0,
												const tWString &asText=_W(""),
												iWidget *apParent=NULL,
												bool abAllowHScroll = true,
												bool abAllowVScroll = true,
												const tString& asName = "")
		{
			return (cWidgetTabFrame*)ohpl_gw_create_c(this, kOhplGwTabFrame, &avLocalPos, &avSize, U(asText), asText.size(), (uintptr_t)apParent, asName.data(), asName.size(), abAllowHScroll, abAllowVScroll, 0);
		}

		cWidgetGroup* CreateWidgetGroup(const cVector3f &avLocalPos=0,
												const cVector2f &avSize=0,
												const tWString &asText=_W(""),
												iWidget *apParent=NULL,
												const tString& asName = "")
		{
			return (cWidgetGroup*)ohpl_gw_create(this, kOhplGwGroup, &avLocalPos, &avSize, U(asText), asText.size(), (uintptr_t)apParent, asName.data(), asName.size(), 0, 0, 0, NULL);
		}

		cWidgetDummy* CreateWidgetDummy(const cVector3f &avLocalPos=0,
												iWidget *apParent=NULL,
												const tString& asName = "")
		{
			return (cWidgetDummy*)ohpl_gw_create(this, kOhplGwDummy, &avLocalPos, NULL, NULL, 0, (uintptr_t)apParent, asName.data(), asName.size(), 0, 0, 0, NULL);
		}


		iWidget *GetWidgetFromName(const tString& asName){ return (iWidget*)ohpl_g1_set_widget_from_name(this, asName.data(), asName.size()); }
		void DestroyWidget(iWidget *apWidget, bool abDestroyChildren=false){ ohpl_g1_set_destroy_widget(this, (uintptr_t)apWidget, abDestroyChildren); }
		bool IsValidWidget(iWidget *apWidget){ return ohpl_g1_set_is_valid_widget(this, (uintptr_t)apWidget); }

		cGuiPopUpMessageBox* CreatePopUpMessageBox(	const tWString& asLabel, const tWString& asText,
													const tWString& asButton1, const tWString& asButton2,
													void *apCallbackObject, tGuiCallbackFunc apCallback)
		{
			return static_cast<cGuiPopUpMessageBox*>(ohpl_g2_popup_msgbox(this, GetSkin(), U(asLabel), asLabel.size(), U(asText), asText.size(),
				U(asButton1), asButton1.size(), U(asButton2), asButton2.size(), (uintptr_t)apCallbackObject, reinterpret_cast<uintptr_t>(apCallback)));
		}

		cGuiPopUpFilePicker* CreatePopUpSaveFilePicker( tWString &asFileName, const tWString &asCategory,
															 const tWString &asFilter, const tWString &asStartPath, bool abShowHidden,
															void *apCallbackObject, tGuiCallbackFunc apCallback, const tWString& asStartFilename=_W(""))
		{
			cGuiPopUpFilePicker* pPicker = static_cast<cGuiPopUpFilePicker*>(ohpl_g2_popup_filepicker(this, GetSkin(), eFilePickerType_Save,
				U(asStartPath), asStartPath.size(), U(asStartFilename), asStartFilename.size(), abShowHidden, (uintptr_t)apCallbackObject, reinterpret_cast<uintptr_t>(apCallback)));
			ohpl_g2_popup_fp_init(pPicker);
			pPicker->SetSaveFileDest( asFileName );
			pPicker->AddCategory( asCategory, asFilter );
			return pPicker;
		}

		cGuiPopUpFilePicker* CreatePopUpLoadFilePicker( tWStringVec &avFileList, bool abAddAllFilesFilter,
															 const tWString &asStartPath, bool abShowHidden,
															 void *apCallbackObject, tGuiCallbackFunc apCallback)
		{
			cGuiPopUpFilePicker* pPicker = static_cast<cGuiPopUpFilePicker*>(ohpl_g2_popup_filepicker(this, GetSkin(), eFilePickerType_Load,
				U(asStartPath), asStartPath.size(), NULL, 0, abShowHidden, (uintptr_t)apCallbackObject, reinterpret_cast<uintptr_t>(apCallback)));
			ohpl_g2_popup_fp_init(pPicker);
			pPicker->SetLoadFileListDest( avFileList );
			if(abAddAllFilesFilter)
				pPicker->AddCategory(_W("All files"), _W("*.*"));
			return pPicker;
		}

		cGuiPopUpColorPicker* CreatePopUpColorPicker(cColor* apDestColor, const cVector3f& avPos, void *apCallbackObject, tGuiCallbackFunc apCallback)
		{
			return static_cast<cGuiPopUpColorPicker*>(ohpl_g2_popup_color(this, GetSkin(), (uintptr_t)apDestColor, (uintptr_t)apCallbackObject, reinterpret_cast<uintptr_t>(apCallback)));
		}

		cGuiPopUpUIKeyboard* CreatePopUpUIKeyboard(cWidgetTextBox* apTarget){ return static_cast<cGuiPopUpUIKeyboard*>(ohpl_g2_popup_kb_for((uintptr_t)apTarget)); }


		void DestroyPopUp(iGuiPopUp *apPopUp){ ohpl_g1_set_destroy_popup(this, (uintptr_t)apPopUp); }
		bool PopUpIsActive(){ return ohpl_g1_set_popup_is_active(this); }
		void AddPopUpCount(int alX){ ohpl_g1_set_add_popup_count(this, alX); }

		void ShowContextMenu( cWidgetContextMenu* apMenu, const cVector3f& avPosition ){ ohpl_gw_show_context_menu(this, (uintptr_t)apMenu, avPosition.x, avPosition.y, avPosition.z); }

		void RemoveWindow(cWidgetWindow* apWin){ ohpl_g1_set_remove_window(this, (uintptr_t)apWin); }
		void SetLastWindowZ(float afX){ ohpl_g1_set_set_last_window_z(this, afX); }
		void SetWindowOnTop(cWidgetWindow* apWin){ ohpl_g1_set_set_window_on_top(this, (uintptr_t)apWin); }

		void SetActive(bool abX){ P().active = abX; }
		bool IsActive(){return P().active;}

		tString GetName() { size_t n; const char* p = ohpl_g1_set_name(this, &n); return tString(p, n);}

		cGui* GetGui(){ return (cGui*)ohpl_g1_set_gui(this);}

		void SetDrawMouse(bool abX){ P().drawMouse = abX; }
		bool GetDrawMouse(){ return P().drawMouse;}
		void SetMouseZ(float afZ){ P().mouseZ = afZ;}
		float GetMouseZ(){ return P().mouseZ;}

		void SetMouseMovementEnabled(bool abX) { P().state.mouseMovement = abX; }
		bool GetMouseMovementEnabled() { return P().state.mouseMovement; }

		const cVector2f& GetMousePos(){ return P().state.mousePos;}

		void SetRootWidgetClips(bool abX){ ohpl_g1_set_root_clips_set(this, abX); }
		bool GetRootWidgetClips(){ return ohpl_g1_set_root_clips(this); }

		void SetVirtualSize(const cVector2f& avSize, float afMinZ, float afMaxZ, const cVector2f& avOffset=0, bool abIgnoreGlobalScale=false)
		{ ohpl_g1_set_set_virtual_size(this, &avSize, afMinZ, afMaxZ, &avOffset, abIgnoreGlobalScale); }
		const cVector2f& GetVirtualSize(){return P().virtualSize;}
		const cVector2f& GetVirtualSizeOffset(){return P().virtualOffset;}

		static void SetGlobalGuiScale(float afScale){ ohpl_g1_set_set_global_scale(afScale); }
		static float GetGlobalGuiScale(){ return ohpl_g1_set_global_scale(); }

		void SetFocusedWidget(iWidget* apWidget, bool abCheckForValidity=false){ ohpl_g1_set_set_focused(this, (uintptr_t)apWidget, abCheckForValidity); }
		iWidget* GetFocusedWidget(){return P().state.focused;}
		void PushFocusedWidget(){ ohpl_g1_set_push_focused(this); }
		void PopFocusedWidget(){ ohpl_g1_set_pop_focused(this); }

		void SetAttentionWidget(iWidget *apWidget, bool abClearFocus = true, bool abCheckForValidity = false){ ohpl_g1_set_set_attention(this, (uintptr_t)apWidget, abClearFocus, abCheckForValidity); }
		iWidget* GetAttentionWidget(){ return P().state.attention;}
		void PushAttentionWidget(){ ohpl_g1_set_push_attention(this); }
		void PopAttentionWidget(bool abClearFocus=true){ ohpl_g1_set_pop_attention(this, abClearFocus); }

		void SetIs3D(bool abX){ P().is3D = abX; }
		bool Is3D(){ return P().is3D;}

		void SetRendersBeforePostEffects(bool abX){ P().rendersBeforePost = abX; }
		bool RendersBeforePostEffects() { return P().rendersBeforePost; }

		int GetDrawPriority(){ return P().drawPrio;}
		void SetDrawPriority(int alPrio){ P().drawPrio = alPrio;}

		void SetCurrentPointer(cGuiGfxElement *apGfx){ ohpl_g1_set_set_pointer(this, apGfx); }
		cGuiGfxElement* GetCurrentPointer(){ return ohpl_g1_set_pointer(this);}

		void Set3DSize(const cVector3f& avSize){ P().size3D = avSize; }
		const cVector3f& Get3DSize(){return P().size3D;}

		void SetCullBackface(bool abX){ P().cullBackface = abX;}
		bool GetCullBackface(){ return P().cullBackface;}

		void SetFlipScreenY(bool abX){ P().flipY = abX;}

		void SetPrograms(iGpuProgram *apTextured, iGpuProgram *apFlat){ P().programs[0] = (uintptr_t)apTextured; P().programs[1] = (uintptr_t)apFlat;}

		void Set3DTransform(const cMatrixf& a_mtxTransform){ ohpl_g1_set_set_3d_transform(this, (const float*)&a_mtxTransform); }
		const cMatrixf& Get3DTransform(){ return *(const cMatrixf*)P().mtx3D;}

		bool HasFocus(){ return ohpl_g1_set_has_focus(this); }

		void PositionWidgetInsideBounds(iWidget* apWidget){ ohpl_g1_set_position_inside_bounds(this, (uintptr_t)apWidget); }

		void SetSkin(cGuiSkin* apSkin){ ohpl_g1_set_set_skin(this, apSkin); }
		cGuiSkin* GetSkin(){ return ohpl_g1_set_skin(this);}

		cResources* GetResources(){ return (cResources*)ohpl_g1_set_resources(this);}

		bool IsDestroyingSet(){ return P().destroying;}

		void IncContextMenuZ() { ohpl_g1_set_add_context_menu_z(this, 5); }
		void DecContextMenuZ() { ohpl_g1_set_add_context_menu_z(this, -5); }

		float GetPopUpZ() { return ohpl_g1_set_popup_z(this); }
		void IncPopUpZ(){ ohpl_g1_set_inc_popup_z(this, 0); }

		void AddToTabOrder(iWidget* apWidget){ ohpl_g1_set_add_to_tab_order(this, (uintptr_t)apWidget); }
		void ClearTabOrder(){ ohpl_g1_set_clear_tab_order(this); }

		iWidget* GetToolTipWidget() { return P().state.tooltipWidget; }
		void SetToolTipWidget(iWidget* apWidget) { P().state.tooltipWidget = apWidget; }

		void SetWidgetsNeedSorting() { P().sortWidgets = true; }

		void SetDefaultFocusNavWidget(iWidget* apWidget, bool abCheckForValidity=false){ ohpl_g1_set_set_default_nav(this, (uintptr_t)apWidget, abCheckForValidity); }
		iWidget* GetDefaultFocusNavWidget() { return P().state.defaultNav; }
		void PushDefaultFocusNavWidget(){ ohpl_g1_set_push_default_nav(this); }
		void PopDefaultFocusNavWidget(){ ohpl_g1_set_pop_default_nav(this); }

		void SetDrawFocus(bool abX)												{ P().drawFocus = abX; }
		void SetFocusDrawCallback(void* apObject, tGuiCallbackFunc apCallback)	{ ohpl_g1_set_set_focus_draw_callback(this, apObject, apCallback); }
		bool GetDrawFocus() { return P().drawFocus; }

	private:
		OhplG1SetPub& P(){ return *(OhplG1SetPub*)this; }
		cGuiSet();
	};
};
#endif // HPL_GUI_SET_H
