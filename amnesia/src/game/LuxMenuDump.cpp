#include "LuxMainMenu.h"
#include "LuxMapHandler.h"
#include <cstdio>
#include <filesystem>
#include <map>

// OHPL_A9_MENUDUMP: window widget trees after CreateGui and after scripted presses on the real windows.
namespace
{
	typedef std::map<iWidget*, int> tIds;

	void Number(iWidget *w, tIds &ids)
	{
		int n = (int)ids.size();
		ids[w] = n;
		for(tWidgetListIt it = w->GetChildren().begin(); it != w->GetChildren().end(); ++it) Number(*it, ids);
	}

	int Id(const tIds &ids, iWidget *w)
	{
		tIds::const_iterator it = ids.find(w);
		return it == ids.end() ? -1 : it->second;
	}

	void Print(FILE *f, iWidget *w, const tIds &ids, int depth)
	{
		cVector3f p = w->GetLocalPosition();
		cVector2f s = w->GetSize();
		cColor c = w->GetDefaultFontColor();
		fprintf(f, "%*s%d t=%d '%s' p=%.3f,%.3f,%.3f s=%.3f,%.3f v=%d e=%d c=%.3f,%.3f,%.3f,%.3f nav=%d,%d,%d,%d",
				depth, "", Id(ids, w), (int)w->GetType(), cString::To8Char(w->GetText()).c_str(), p.x, p.y, p.z, s.x, s.y,
				(int)w->IsVisible(), (int)w->IsEnabled(), c.r, c.g, c.b, c.a,
				Id(ids, w->GetFocusNavigation(eUIArrow_Up)), Id(ids, w->GetFocusNavigation(eUIArrow_Right)),
				Id(ids, w->GetFocusNavigation(eUIArrow_Down)), Id(ids, w->GetFocusNavigation(eUIArrow_Left)));
		if(w->GetType() == eWidgetType_ListBox)
		{
			cWidgetListBox *l = static_cast<cWidgetListBox*>(w);
			fprintf(f, " sel=%d lock=%d items=", l->GetSelectedItem(), (int)l->GetIsLocked());
			for(int i = 0; i < l->GetItemNum(); ++i) fprintf(f, "[%s]", cString::To8Char(l->GetItem(i)->GetText()).c_str());
		}
		if(w->GetType() == eWidgetType_Image)
			fprintf(f, " gfx=%d", (int)(static_cast<cWidgetImage*>(w)->GetImage() != NULL));
		fprintf(f, "\n");
		for(tWidgetListIt it = w->GetChildren().begin(); it != w->GetChildren().end(); ++it) Print(f, *it, ids, depth + 1);
	}

	iWidget *Find(iWidget *w, eWidgetType type, const tWString *text, int &skip)
	{
		if(w->GetType() == type && (text == NULL || w->GetText() == *text) && skip-- == 0) return w;
		for(tWidgetListIt it = w->GetChildren().begin(); it != w->GetChildren().end(); ++it)
			if(iWidget *r = Find(*it, type, text, skip)) return r;
		return NULL;
	}
}

void cLuxMainMenu::DumpA9(const char *asFile)
{
	FILE *f = fopen(asFile, "a");
	if(f == NULL) return;
	OhplA9bState state;
	ohpl_a9b_state(mlMe, &state);
	cGuiSet *pSet = (cGuiSet*)state.mpSet;
	iWidget *root = (iWidget*)state.mpFirstLabel;
	while(root && root->GetParent()) root = root->GetParent();
	if(root == NULL) { fclose(f); return; }

	cResources *res = gpBase->mpEngine->GetResources();
	struct Step { const char *name; int window; const char *scope[2]; eWidgetType type; const char *cat, *key; int skip; };
	const char *const kStart[2] = {"MainMenu", "Start Game"}, *const kLoad[2] = {"LoadGame", "LoadGameTitle"},
		*const kStories[2] = {"MainMenu", "Custom Map"}, *const kProfiles[2] = {"MainMenu", "Profiles"},
		*const kName[2] = {"MainMenu", "Create Profile"};
	const Step steps[] = {
		{"created", -3, {0, 0}, eWidgetType_Label, 0, 0, 0},
		{"start", eLuxMainMenuWindow_StartGame, {0, 0}, eWidgetType_Label, 0, 0, 0},
		{"hard", -1, {kStart[0], kStart[1]}, eWidgetType_Button, "MainMenu", "HardMode", 0},
		{"normal", -1, {kStart[0], kStart[1]}, eWidgetType_Button, "MainMenu", "NormalMode", 0},
		{"startpress", -1, {kStart[0], kStart[1]}, eWidgetType_Button, "MainMenu", "Start Game", 0},
		{"startno", -1, {"", ""}, eWidgetType_Button, "MainMenu", "No", 0},
		{"startcancel", -1, {kStart[0], kStart[1]}, eWidgetType_Button, "Global", "Cancel", 0},
		{"load", eLuxMainMenuWindow_LoadGame, {0, 0}, eWidgetType_Label, 0, 0, 0},
		{"loadok", -1, {kLoad[0], kLoad[1]}, eWidgetType_Button, "Global", "OK", 0},
		{"loadpopup", -1, {"", ""}, eWidgetType_Button, "Global", "OK", 0},
		{"loadcancel", -1, {kLoad[0], kLoad[1]}, eWidgetType_Button, "Global", "Cancel", 0},
		{"stories", eLuxMainMenuWindow_CustomStoryList, {0, 0}, eWidgetType_Label, 0, 0, 0},
		{"storyok0", -1, {kStories[0], kStories[1]}, eWidgetType_Button, "Global", "OK", 0},
		{"storypopup", -1, {"", ""}, eWidgetType_Button, "Global", "OK", 0},
		{"storysel", -4, {kStories[0], kStories[1]}, eWidgetType_ListBox, "", "", 0},
		{"storyok", -1, {kStories[0], kStories[1]}, eWidgetType_Button, "Global", "OK", 0},
		{"storyback", -1, {"=", "A9 Story"}, eWidgetType_Button, "Global", "Cancel", 0},
		{"storycancel", -1, {kStories[0], kStories[1]}, eWidgetType_Button, "Global", "Cancel", 0},
		{"profiles", eLuxMainMenuWindow_Profiles, {0, 0}, eWidgetType_Label, 0, 0, 0},
		{"profcreate", -1, {kProfiles[0], kProfiles[1]}, eWidgetType_Button, "MainMenu", "Create", 0},
		{"profname", -2, {kName[0], kName[1]}, eWidgetType_TextBox, "", "", 0},
		{"profconfirm", -1, {kName[0], kName[1]}, eWidgetType_Button, "MainMenu", "Create", 0},
		{"profdelete", -1, {kProfiles[0], kProfiles[1]}, eWidgetType_Button, "MainMenu", "Delete", 0},
		{"profno", -1, {"", ""}, eWidgetType_Button, "MainMenu", "No", 0},
		{"top", eLuxMainMenuWindow_LastEnum, {0, 0}, eWidgetType_Label, 0, 0, 0},
		{"tick", -6, {0, 0}, eWidgetType_Label, 0, 0, 0},
		{"press_options", -5, {0, 0}, eWidgetType_Label, "MainMenu", "Options", 0},
		{"top2", eLuxMainMenuWindow_LastEnum, {0, 0}, eWidgetType_Label, 0, 0, 0},
		{"press_profile", -5, {0, 0}, eWidgetType_Label, "MainMenu", "Change Profile", 0},
		{"top3", eLuxMainMenuWindow_LastEnum, {0, 0}, eWidgetType_Label, 0, 0, 0},
		{"press_custom", -5, {0, 0}, eWidgetType_Label, "MainMenu", "Custom Map", 0},
		{"top4", eLuxMainMenuWindow_LastEnum, {0, 0}, eWidgetType_Label, 0, 0, 0},
		{"press_start", -5, {0, 0}, eWidgetType_Label, "MainMenu", "Start Game", 0},
		{"press_start_no", -1, {"", ""}, eWidgetType_Button, "MainMenu", "No", 0},
		{"top5", eLuxMainMenuWindow_LastEnum, {0, 0}, eWidgetType_Label, 0, 0, 0},
		{"press_exit", -5, {0, 0}, eWidgetType_Label, "MainMenu", "Exit", 0},
		{"exit_tick", -6, {0, 0}, eWidgetType_Label, 0, 0, 0},
		{"exit_no", -1, {"", ""}, eWidgetType_Button, "MainMenu", "No", 0},
		{"exit_tick2", -6, {0, 0}, eWidgetType_Label, 0, 0, 0},
	};

	tWString sStoryDir = cString::AddSlashAtEndW(gpBase->msUserResourceDir + cString::To16Char(gpBase->msCustomStoryPath)) + _W("A9story/");
	std::filesystem::create_directories(cString::To8Char(sStoryDir) + "maps");
	if(FILE *fs = fopen((cString::To8Char(sStoryDir) + "maps/start.hpm").c_str(), "w")) fclose(fs);
	if(FILE *fs = fopen((cString::To8Char(sStoryDir) + "custom_story_settings.cfg").c_str(), "w"))
	{
		fputs("<Main\n\tName = \"A9 Story\"\n\tAuthor = \"Tester\"\n\tStartMap = \"start.hpm\"\n\tMapsFolder = \"maps\"\n/>\n", fs);
		fclose(fs);
	}

	for(size_t i = 0; i < sizeof(steps) / sizeof(steps[0]); ++i)
	{
		const Step &s = steps[i];
		iWidget *scope = root;
		if(s.scope[0])
		{
			if(s.scope[0][0] == 0) scope = root->GetChildren().back();
			else
			{
				int none = 0;
				tWString sTitle = s.scope[0][0] == '=' ? cString::To16Char(s.scope[1]) : res->Translate(s.scope[0], s.scope[1]);
				scope = Find(root, eWidgetType_Window, &sTitle, none);
			}
		}
		if(s.window >= 0) SetWindowActive((eLuxMainMenuWindow)s.window);
		else if(s.window == -6)
		{
			for(int t = 0; t < 20; ++t) Update(1.0f / 60.0f);
		}
		else if(s.window == -5)
		{
			int skip = 0;
			tWString sText = res->Translate(s.cat, s.key);
			iWidget *w = Find(root, s.type, &sText, skip);
			if(w) w->ProcessMessage(eGuiMessage_MouseDown, cGuiMessageData());
			else fprintf(f, "!! no widget %s\n", s.name);
		}
		else if(s.window == -4)
		{
			int skip = 0;
			if(scope) if(iWidget *w = Find(scope, eWidgetType_ListBox, NULL, skip))
				static_cast<cWidgetListBox*>(w)->SetSelectedItem(0);
		}
		else if(s.window == -2)
		{
			int skip = 0;
			if(scope) if(iWidget *w = Find(scope, eWidgetType_TextBox, NULL, skip))
				static_cast<cWidgetTextBox*>(w)->SetText(_W("A9dump"));
		}
		else if(s.window == -1)
		{
			int skip = s.skip;
			tWString sText = res->Translate(s.cat, s.key);
			iWidget *w = scope ? Find(scope, s.type, &sText, skip) : NULL;
			if(w) w->ProcessMessage(eGuiMessage_ButtonPressed, cGuiMessageData());
			else fprintf(f, "!! no widget %s\n", s.name);
		}
		tIds ids;
		Number(root, ids);
		ohpl_a9b_state(mlMe, &state);
		fprintf(f, "== %s win=%d hard=%d focus=%d attn=%d drawfocus=%d popup=%d topvis=%d ta=%.6f fa=%.6f\n", s.name, state.mlWindow, (int)gpBase->mbHardMode,
				Id(ids, pSet->GetFocusedWidget()), Id(ids, pSet->GetAttentionWidget()),
				(int)pSet->GetDrawFocus(), (int)pSet->PopUpIsActive(), state.mlTopVisible, state.mfTopAlpha, state.mfFadeAlpha);
		Print(f, root, ids, 0);
	}
	SetWindowActive(eLuxMainMenuWindow_LastEnum);
	ohpl_a9b_dump_background(mlMe);
	ohpl_a9b_state(mlMe, &state);
	cCamera *pCamera = (cCamera*)state.mpCamera;
	fprintf(f, "== background world=%d camera=%d\n", (int)(state.mpWorld != 0), (int)(pCamera != NULL));
	if(pCamera)
	{
		cVector3f vPos = pCamera->GetPosition();
		const cMatrixf &m = pCamera->GetViewMatrix();
		fprintf(f, "cam fov=%.6f aspect=%.6f pos=%.4f,%.4f,%.4f\n", pCamera->GetFOV(), pCamera->GetAspect(), vPos.x, vPos.y, vPos.z);
		for(int r = 0; r < 4; ++r) fprintf(f, "view %.4f %.4f %.4f %.4f\n", m.m[r][0], m.m[r][1], m.m[r][2], m.m[r][3]);
	}
	fprintf(f, "logo=%d logosize=%.4f,%.4f\n", (int)(state.mpLogo != 0), state.mvLogoSize[0], state.mvLogoSize[1]);
	std::filesystem::remove_all(cString::To8Char(sStoryDir));
	cPlatform::RemoveFolder(gpBase->msBaseSavePath + _W("A9dump"), true, true);
	ohpl_a9b_dump_end(mlMe);
	fclose(f);
}
