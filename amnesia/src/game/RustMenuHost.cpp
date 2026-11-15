#include "RustMenuHost.h"

#include <cstdio>
#include <cstdlib>
#include <map>

static void DumpWidget(FILE* apOut, iWidget* apWidget, int alDepth, std::map<iWidget*,int>& amIds)
{
	int lId = amIds[apWidget];
	fprintf(apOut, "%*s#%d type=%d name=%s", alDepth*2, "", lId, (int)apWidget->GetType(), apWidget->GetName().c_str());
	const cVector3f& vPos = apWidget->GetLocalPosition();
	cVector2f vSize = apWidget->GetSize();
	fprintf(apOut, " pos=%.9g,%.9g,%.9g size=%.9g,%.9g", vPos.x, vPos.y, vPos.z, vSize.x, vSize.y);
	fprintf(apOut, " text=\"%s\" vis=%d en=%d uv=%d ud=%d", cString::To8Char(apWidget->GetText()).c_str(), apWidget->IsVisible(), apWidget->IsEnabled(), apWidget->GetUserValue(), apWidget->GetUserData() != NULL);
	const cColor& col = apWidget->GetDefaultFontColor();
	fprintf(apOut, " col=%.9g,%.9g,%.9g,%.9g fs=%.9g,%.9g", col.r, col.g, col.b, col.a, apWidget->GetDefaultFontSize().x, apWidget->GetDefaultFontSize().y);
	fprintf(apOut, " nav=");
	const eUIArrow vArrows[4] = { eUIArrow_Up, eUIArrow_Right, eUIArrow_Down, eUIArrow_Left };
	for(int i=0;i<4;++i)
	{
		iWidget* pNav = apWidget->GetFocusNavigation(vArrows[i]);
		fprintf(apOut, "%d%s", pNav && amIds.count(pNav) ? amIds[pNav] : -1, i<3 ? "," : "");
	}
	if(cWidgetCheckBox* pCheck = dynamic_cast<cWidgetCheckBox*>(apWidget))
		fprintf(apOut, " checked=%d", pCheck->IsChecked());
	if(cWidgetSlider* pSlider = dynamic_cast<cWidgetSlider*>(apWidget))
		fprintf(apOut, " slider=%d/%d", pSlider->GetValue(), pSlider->GetMaxValue());
	if(cWidgetComboBox* pCombo = dynamic_cast<cWidgetComboBox*>(apWidget))
	{
		fprintf(apOut, " combo=%d[", pCombo->GetSelectedItem());
		for(int i=0;i<pCombo->GetItemNum();++i)
			fprintf(apOut, "%s\"%s\"", i ? "," : "", cString::To8Char(pCombo->GetItem(i)->GetText()).c_str());
		fprintf(apOut, "]");
	}
	if(cWidgetLabel* pLabel = dynamic_cast<cWidgetLabel*>(apWidget))
		fprintf(apOut, " align=%d wrap=%d", (int)pLabel->GetTextAlign(), pLabel->GetWordWrap());
	fprintf(apOut, "\n");

	tWidgetList& lstChildren = apWidget->GetChildren();
	for(tWidgetListIt it = lstChildren.begin(); it != lstChildren.end(); ++it)
		DumpWidget(apOut, *it, alDepth+1, amIds);
}

static void NumberWidgets(iWidget* apWidget, std::map<iWidget*,int>& amIds)
{
	int lId = (int)amIds.size();
	amIds[apWidget] = lId;
	tWidgetList& lstChildren = apWidget->GetChildren();
	for(tWidgetListIt it = lstChildren.begin(); it != lstChildren.end(); ++it)
		NumberWidgets(*it, amIds);
}

void OhplA10aDumpWindow(const char* asName, iWidget* apRoot)
{
	const char* sPath = getenv("OHPL_A10A_DUMP");
	if(sPath == NULL || apRoot == NULL) return;

	FILE* pOut = fopen(sPath, "a");
	if(pOut == NULL) return;

	std::map<iWidget*,int> mIds;
	NumberWidgets(apRoot, mIds);
	fprintf(pOut, "== %s\n", asName);
	DumpWidget(pOut, apRoot, 0, mIds);
	fclose(pOut);
}
