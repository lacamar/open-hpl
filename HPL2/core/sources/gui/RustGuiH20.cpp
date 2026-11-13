#include "gui/RustGuiH20.h"

#include "gui/Gui.h"
#include "gui/GuiGfxElement.h"
#include "gui/GuiSet.h"
#include "gui/GuiSkin.h"
#include "gui/Widget.h"
#include "gui/WidgetBaseClasses.h"
#include "gui/WidgetLabel.h"
#include "gui/WidgetListBoxBase.h"
#include "gui/WidgetSlider.h"
#include "graphics/FontData.h"

namespace hpl {

	// Reaches iWidget's protected members through a derived type's member pointers.
	struct OhplH20Access : public iWidget
	{
		typedef void (iWidget::*tBorders)(cGuiGfxElement*, cGuiGfxElement**, cGuiGfxElement**, const cVector3f&, const cVector2f&, bool);
		typedef void (iWidget::*tText)(const tWString&, const cVector3f&, eFontAlign);
		static tBorders Borders() { return &OhplH20Access::DrawBordersAndCorners; }
		static tText Text() { return (tText)&OhplH20Access::DrawDefaultText; }
		static tText Highlight() { return &OhplH20Access::DrawDefaultTextHighlight; }
		static cGuiSet* Set(iWidget* apW) { return apW->*(&OhplH20Access::mpSet); }
		static cGuiSkin* Skin(iWidget* apW) { return apW->*(&OhplH20Access::mpSkin); }
		static cVector2f FontSize(iWidget* apW) { return apW->*(&OhplH20Access::mvDefaultFontSize); }
		static iFontData* Font(iWidget* apW) { return apW->*(&OhplH20Access::mpDefaultFontType); }
		static cVector3f ToLocal(iWidget* apW, const cVector3f& avPos) { return (apW->*(&OhplH20Access::WorldToLocalPosition))(avPos); }
	};

	static iWidget* W(uintptr_t a) { return (iWidget*)a; }
	static cWidgetItem* Item(uintptr_t a) { return (cWidgetItem*)a; }
	static cWidgetSlider* Slider(uintptr_t a) { return (cWidgetSlider*)a; }
	static iWidgetItemContainer* Cont(uintptr_t a) { return (iWidgetItemContainer*)a; }

	// Argument use per op: w target widget; p/i/f inputs; text/textLen text; rp/ri/rf results.
	static uintptr_t H20Op(void*, int aOp, OhplH20Args* a)
	{
		switch(aOp)
		{
		case kOhplH20Op_SLIDER_SET_VALUE:
			if(a->i[1]) Slider(a->w)->SetValue(a->i[0], a->i[2]!=0);
			else		Slider(a->w)->SetValue(a->i[0]);
			return 0;
		case kOhplH20Op_SLIDER_GET_VALUE:
			a->ri[0] = Slider(a->w)->GetValue();
			return 0;
		case kOhplH20Op_SLIDER_SET_BAR_VALUE_SIZE:
			Slider(a->w)->SetBarValueSize(a->i[0]);
			return 0;
		case kOhplH20Op_SLIDER_SET_MAX_VALUE:
			Slider(a->w)->SetMaxValue(a->i[0]);
			return 0;
		case kOhplH20Op_SLIDER_SET_BUTTON_VALUE_ADD:
			Slider(a->w)->SetButtonValueAdd(a->i[0]);
			return 0;
		case kOhplH20Op_SLIDER_SET_BAR_CLICK_VALUE_ADD:
			Slider(a->w)->SetBarClickValueAdd(a->i[0]);
			return 0;
		case kOhplH20Op_SLIDER_GET_BUTTON_VALUE_ADD:
			a->ri[0] = Slider(a->w)->GetButtonValueAdd();
			return 0;
		case kOhplH20Op_WIDGET_SET_SIZE:
			W(a->w)->SetSize(cVector2f(a->f[0], a->f[1]));
			return 0;
		case kOhplH20Op_WIDGET_SET_POSITION:
			W(a->w)->SetPosition(cVector3f(a->f[0], a->f[1], a->f[2]));
			return 0;
		case kOhplH20Op_WIDGET_GET_SIZE:
		{
			cVector2f vSize = W(a->w)->GetSize();
			a->rf[0] = vSize.x;
			a->rf[1] = vSize.y;
			return 0;
		}
		case kOhplH20Op_ITEM_NUM:
			a->ri[0] = Cont(a->p[0])->GetItemNum();
			return 0;
		case kOhplH20Op_ITEM_GET:
			return (uintptr_t)Cont(a->p[0])->GetItem(a->i[0]);
		case kOhplH20Op_ITEM_IS_SELECTED:
			return Item(a->p[0])->IsSelected();
		case kOhplH20Op_ITEM_SET_SELECTED:
			Item(a->p[0])->SetSelected(a->i[0]!=0);
			return 0;
		case kOhplH20Op_ITEM_IS_SELECTABLE:
			return Item(a->p[0])->IsSelectable();
		case kOhplH20Op_ITEM_INDEX:
			a->ri[0] = Item(a->p[0])->GetIndex();
			return 0;
		case kOhplH20Op_ITEM_TEXT:
		{
			const tWString& sText = Item(a->p[0])->GetText();
			a->ri[0] = (int32_t)sText.size();
			return (uintptr_t)sText.data();
		}
		case kOhplH20Op_PROCESS_MESSAGE:
			if(a->p[0])
				return W(a->w)->ProcessMessage((eGuiMessage)a->i[0], *(const cGuiMessageData*)a->p[0]);
			return W(a->w)->ProcessMessage((eGuiMessage)a->i[0], cGuiMessageData(a->i[1]));
		case kOhplH20Op_WORLD_TO_LOCAL:
		{
			cVector3f vLocal = OhplH20Access::ToLocal(W(a->w), cVector3f(cVector2f(a->f[0], a->f[1])));
			a->rf[0] = vLocal.x;
			a->rf[1] = vLocal.y;
			a->rf[2] = vLocal.z;
			return 0;
		}
		case kOhplH20Op_GET_GLOBAL:
		{
			cVector3f vGlobal = W(a->w)->GetGlobalPosition();
			a->rf[0] = vGlobal.x;
			a->rf[1] = vGlobal.y;
			a->rf[2] = vGlobal.z;
			return 0;
		}
		case kOhplH20Op_GET_FONT_SIZE:
		{
			cVector2f vSize = OhplH20Access::FontSize(W(a->w));
			a->rf[0] = vSize.x;
			a->rf[1] = vSize.y;
			return 0;
		}
		case kOhplH20Op_HAS_FOCUS:
			return W(a->w)->HasFocus();
		case kOhplH20Op_CREATE_SLIDER:
			return (uintptr_t)(iWidget*)OhplH20Access::Set(W(a->w))->CreateWidgetSlider(eWidgetSliderOrientation_Vertical, 0, 0, 0, W(a->w));
		case kOhplH20Op_ADD_CALLBACK:
			W(a->w)->AddCallback((eGuiMessage)a->i[0], (void*)a->p[0], (tGuiCallbackFunc)a->p[1]);
			return 0;
		case kOhplH20Op_DRAW_GFX:
		{
			cGuiSet* pSet = OhplH20Access::Set(W(a->w));
			cVector3f vPos(a->f[0], a->f[1], a->f[2]);
			cVector2f vSize(a->f[3], a->f[4]);
			if(a->i[0]) pSet->DrawGfx((cGuiGfxElement*)a->p[0], vPos, vSize, cColor(a->f[5], a->f[6], a->f[7], a->f[8]));
			else		pSet->DrawGfx((cGuiGfxElement*)a->p[0], vPos, vSize);
			return 0;
		}
		case kOhplH20Op_DRAW_BORDERS:
		{
			iWidget* pWidget = W(a->w);
			cVector3f vPos(a->f[0], a->f[1], a->f[2]);
			cVector2f vSize(a->f[3], a->f[4]);
			(pWidget->*OhplH20Access::Borders())((cGuiGfxElement*)a->p[0], (cGuiGfxElement**)a->p[1], (cGuiGfxElement**)a->p[2], vPos, vSize,
												 a->i[1]!=0);
			return 0;
		}
		case kOhplH20Op_SKIN_GFX:
			return (uintptr_t)OhplH20Access::Skin(W(a->w))->GetGfx((eGuiSkinGfx)a->i[0]);
		case kOhplH20Op_SKIN_ATTRIBUTE:
		{
			cVector3f vAttr = OhplH20Access::Skin(W(a->w))->GetAttribute((eGuiSkinAttribute)a->i[0]);
			a->rf[0] = vAttr.x;
			a->rf[1] = vAttr.y;
			a->rf[2] = vAttr.z;
			return 0;
		}
		case kOhplH20Op_GFX_ACTIVE_SIZE:
		{
			cVector2f vSize = ((cGuiGfxElement*)a->p[0])->GetActiveSize();
			a->rf[0] = vSize.x;
			a->rf[1] = vSize.y;
			return 0;
		}
		case kOhplH20Op_GFX_IMAGE_SIZE:
		{
			cVector2f vSize = ((cGuiGfxElement*)a->p[0])->GetImageSize();
			a->rf[0] = vSize.x;
			a->rf[1] = vSize.y;
			return 0;
		}
		case kOhplH20Op_CLIP_CREATE_CHILD:
			return (uintptr_t)((cGuiClipRegion*)a->p[0])->CreateChild(cVector3f(a->f[0], a->f[1], a->f[2]), cVector2f(a->f[3], a->f[4]));
		case kOhplH20Op_SET_CLIP:
			OhplH20Access::Set(W(a->w))->SetCurrentClipRegion((cGuiClipRegion*)a->p[0]);
			return 0;
		case kOhplH20Op_DRAW_TEXT:
		{
			tWString sText(a->text, a->textLen);
			cVector3f vPos(a->f[0], a->f[1], a->f[2]);
			iWidget* pWidget = W(a->w);
			if(a->i[1]) (pWidget->*OhplH20Access::Highlight())(sText, vPos, (eFontAlign)a->i[0]);
			else		(pWidget->*OhplH20Access::Text())(sText, vPos, (eFontAlign)a->i[0]);
			return 0;
		}
		case kOhplH20Op_GFX_RECT:
			return (uintptr_t)cGui::mpGfxRect;
		case kOhplH20Op_CREATE_LABEL:
		{
			iWidget* pParent = W(a->w);
			cWidgetLabel* pLabel = OhplH20Access::Set(pParent)->CreateWidgetLabel(cVector3f(a->f[0], a->f[1], a->f[2]), -1, tWString(a->text, a->textLen), pParent);
			pLabel->SetDefaultFontSize(OhplH20Access::FontSize(pParent));
			pLabel->SetBackGroundColor(cColor(0.82f,0.81f,0.79f,1));
			return (uintptr_t)(iWidget*)pLabel;
		}
		case kOhplH20Op_TEXT_LENGTH:
			a->rf[0] = OhplH20Access::Font(W(a->w))->GetLength(OhplH20Access::FontSize(W(a->w)), W(a->p[0])->GetText().c_str());
			return 0;
		case kOhplH20Op_ITEM_PROP_GET:
			return (uintptr_t)Item(a->p[0])->GetProperty(a->i[0]);
		case kOhplH20Op_PROP_TYPE:
			return ((cWidgetItemProperty*)a->p[0])->GetType();
		case kOhplH20Op_PROP_TEXT:
		{
			const tWString& sText = ((cWidgetItemProperty*)a->p[0])->GetText();
			a->ri[0] = (int32_t)sText.size();
			return (uintptr_t)sText.data();
		}
		case kOhplH20Op_PROP_GFX:
			return (uintptr_t)((cWidgetItemProperty*)a->p[0])->GetGfx();
		case kOhplH20Op_DESTROY_WIDGET:
			OhplH20Access::Set(W(a->w))->DestroyWidget(W(a->p[0]));
			return 0;
		default:
			return 0;
		}
	}

	const OhplH20Host gH20Host = { NULL, H20Op };

}
