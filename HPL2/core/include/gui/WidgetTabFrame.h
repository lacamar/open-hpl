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

#ifndef HPL_WIDGET_TAB_FRAME
#define HPL_WIDGET_TAB_FRAME

#include "gui/Widget.h"
#include "gui/RustGuiGw.h"
#include "gui/RustWidget2.h"
#include "gui/RustGuiH20.h"

using namespace std;

namespace hpl
{
	class cGuiSet;
	class cGuiSkin;
	class cWidgetFrame;
	class cWidgetButton;

	class cWidgetTab;
	class cWidgetTabLabel;
	class cWidgetTabFrame;

	class cGuiGfxElement;

	typedef vector<cWidgetTab*>			tWidgetTabVector;
	typedef tWidgetTabVector::iterator	tWidgetTabVectorIt;

	// Rust kinds (rust/src/gui/gw_h20.rs, logic in h20_tab.rs)
	class cWidgetTabLabel : public iWidget
	{
	public:
		cWidgetTabLabel(cGuiSet* apSet, cGuiSkin* apSkin) : iWidget(eWidgetType_Label, apSet, apSkin)
		{ ohpl_gw_set_kind((uintptr_t)this, kOhplGwTabLabel, 0, 0); }

		cGuiGfxElement* GetIcon() { return H().icon; }

	private:
		OhplH20TabLabel& H(){ return *(OhplH20TabLabel*)ohpl_gw_state((uintptr_t)this); }
	};

	class cWidgetTab : public iWidget
	{
	public:
		cWidgetTab(cWidgetTabFrame* apParent) : iWidget(eWidgetType_Tab, ((iWidget*)apParent)->GetSet(), ((iWidget*)apParent)->GetSet()->GetSkin())
		{ ohpl_gw_set_kind_ctr((uintptr_t)this, kOhplGwTab, 0, (uintptr_t)apParent); }

		int GetIndex() { return H().index; }

		void SetLabelPosX(float afPosX)
		{
			iWidget* pLabel = (iWidget*)H().label;
			cVector3f vNewPos = pLabel->GetLocalPosition();
			vNewPos.x = afPosX;
			pLabel->SetPosition(vNewPos);
		}

		void SetIcon(cGuiGfxElement* apIcon){ ohpl_h20_tab_set_icon(&H(), (uintptr_t)apIcon); }

		cWidgetTabFrame* GetParentTabFrame() { return H().frame; }
		cWidgetTabLabel* GetTabLabel() { return H().label; }

		void SetOnTop(bool abX){ ohpl_h20_tab_set_on_top(&H(), abX); }
		bool IsOnTop() { return H().onTop; }

	private:
		OhplH20Tab& H(){ return *(OhplH20Tab*)ohpl_gw_state((uintptr_t)this); }
	};

	class cWidgetTabFrame : public iWidget
	{
	public:
		cWidgetTabFrame(cGuiSet* apSet, cGuiSkin* apSkin) : iWidget(eWidgetType_TabFrame, apSet, apSkin)
		{ ohpl_gw_set_kind((uintptr_t)this, kOhplGwTabFrame, 0, 0); }

		cWidgetTab* AddTab(const tWString& asTabCaption){ return (cWidgetTab*)ohpl_h20_tf_add_tab(&H(), asTabCaption.data(), asTabCaption.size()); }
		void ClearTabs(){ ohpl_h20_tf_clear_tabs(&H()); }

		int GetTabNum(){ return ohpl_h20_tf_tab_num(&H()); }
		cWidgetTab* GetTab(int alIdx){ return (cWidgetTab*)ohpl_h20_tf_tab(&H(), alIdx); }
		cWidgetTab* GetTab(const tWString& asTabCaption)
		{
			for(int i=0;i<GetTabNum(); ++i)
			{
				cWidgetTab* pTab = GetTab(i);
				if(pTab->GetText()==asTabCaption)
					return pTab;
			}
			return NULL;
		}
		cWidgetTab* GetTabOnTop() { return H().top; }

		void SetBackgroundZ(float afZ) { H().bgZ = afZ; }
		float GetBackgroundZ() { return H().bgZ; }
		void SetForegroundZ(float afZ) { H().fgZ = afZ; }
		float GetForegroundZ() { return H().fgZ; }

		int GetTabOnTopIndex(){ return ohpl_h20_tf_tab_on_top_index(&H()); }
		void SetTabOnTop(cWidgetTab* apTab){ ohpl_h20_tf_set_tab_on_top(&H(), (uintptr_t)apTab); }
		void SetTabOnTopByIndex(int alIdx){ ohpl_h20_tf_set_tab_on_top_by_index(&H(), alIdx); }

		void SetHorizontalScrollEnabled(bool abX){ ohpl_h20_tf_set_hori_scroll(&H(), abX); }
		void SetVerticalScrollEnabled(bool abX){ ohpl_h20_tf_set_vert_scroll(&H(), abX); }

	private:
		OhplH20TabFrame& H(){ return *(OhplH20TabFrame*)ohpl_gw_state((uintptr_t)this); }
	};
};
#endif	// HPL_WIDGET_TAB_FRAME
