/*
 * SOMA's immediate-mode GUI (hpl::cImGui) on an HPL2 cGuiSet: widget state, fades and timers keyed
 * by GetHash64 ids, actions sent by the GUI handler script, and draw calls recorded between Begin()
 * and End() and replayed every rendered frame. The script data structs (cImGuiGfx, cImGuiFont,
 * cImGui*Data) are the recovered layouts, read by field offset.
 */

#ifndef SOMA_IMGUI_H
#define SOMA_IMGUI_H

#include "hpl.h"

#include <angelscript.h>
#include <map>

using namespace hpl;

class cSomaImGui
{
public:
	cSomaImGui(const tString &asName, cGuiSet *apSet);
	~cSomaImGui();

	static cSomaImGui *GetCurrent() { return mpCurrent; }
	static void SetCurrent(cSomaImGui *apImGui) { mpCurrent = apImGui; }
	static cSomaImGui *GetInputFocus() { return mpInputFocus; }
	static cSomaImGui *GetPrevInputFocus() { return mpPrevInputFocus; }
	static void SetInputFocus(cSomaImGui *apImGui, bool abShowMouse);
	static void UpdateFocusHistory() { mpPrevInputFocus = mpInputFocus; }

	void Begin(float afTimeStep);
	void End();
	// Replays the draw calls recorded by the last completed Begin/End
	void DrawAll();
	void ClearStates();

	cGuiSet *GetSet() { return mpSet; }
	const tString &GetName() { return msName; }
	bool IsFirstRun() { return mbFirstRun; }
	float GetTimeStep() { return mfTimeStep; }
	float GetTimeCount() { return mfTimeCount; }

	void SendAction(int alAction, bool abDown, bool abTriggered);
	bool ActionTriggered(int alAction) { return alAction >= 0 && alAction < 10 && mvActionTriggered[alAction]; }
	bool ActionIsDown(int alAction) { return alAction >= 0 && alAction < 10 && mvActionDown[alAction]; }
	void SendMouseVirtualPosition(const cVector2f &avPos, const cVector2f &avRel);
	void SendMousePosition(const cVector2l &avPos, const cVector2l &avRel);
	const cVector2f &GetMousePosition() { return mvMousePos; }
	const cVector2f &GetMouseRel() { return mvMouseRel; }
	bool mbShowMouse = false;
	bool mbShowMouseAutomatically = true;

	// Named state
	struct cState
	{
		int mlInt = 0;
		float mfFloat = 0;
		cVector3f mvVec = 0;
		cColor mCol = cColor(1, 1);
		bool mbSetInt = false, mbSetFloat = false, mbSetVec = false, mbSetCol = false;
	};
	std::map<uint64_t, cState> mmapStates;
	cState &State(uint64_t alId) { return mmapStates[alId]; }

	struct cFade
	{
		int mlType; // 0 float, 1 vector, 2 color
		float mvStart[4];
		float mvGoal[4];
		float mfTime;
		float mfCount;
		int mlEasing;
	};
	std::map<uint64_t, cFade> mmapFades;
	void Fade(uint64_t alId, int alType, const float *apGoal, float afTime, int alEasing, bool abReplace);
	std::map<uint64_t, float> mmapTimers;

	// Modifiers
	struct cModifiers
	{
		cColor mColorMul = cColor(1, 1);
		cColor mTextColorMul = cColor(1, 1);
		bool mbUseInput = true;
		bool mbUseUIPos = true;
		float mfRotateAngle = 0;
		bool mbRotateCustomPivot = false;
		cVector2f mvRotatePivot = 0;
	};
	cModifiers mMods;
	std::vector<cModifiers> mvModStack;
	int mlAlign = 0;

	// Groups and layouts
	struct cGroup
	{
		cVector3f mvPos;
		cVector2f mvSize;
	};
	std::vector<cGroup> mvGroups;
	struct cLayout
	{
		int mlType;
		cVector3f mvStart;
		cVector3f mvCursor;
		cVector2f mvSize;
		cVector2f mvSpacing;
		float mfLineMax;
	};
	std::vector<cLayout> mvLayouts;
	cVector3f GroupPos() { return mvGroups.empty() ? cVector3f(0) : mvGroups.back().mvPos; }
	cVector2f GroupSize() { return mvGroups.empty() ? mpSet->GetVirtualSize() : mvGroups.back().mvSize; }

	// Previous widget
	struct cPrev
	{
		bool mbPressed = false, mbBecamePressed = false, mbInFocus = false, mbBecameInFocus = false;
		bool mbWasInFocus = false, mbMouseOver = false, mbUpdated = false;
		cVector3f mvPos = 0;
		cVector2f mvSize = 0;
	};
	cPrev mPrev;

	std::vector<tWString> mvItems;

	// Default data blocks (script struct layouts)
	std::map<tString, void *> mmapDefaults;
	void *GetDefault(const char *apType);
	void SetDefault(const char *apType, const void *apData);

	// Widgets
	void Layout(cVector3f &avPos, cVector2f &avSize, const cVector2f &avDefaultSize);
	void Advance(const cVector3f &avPos, const cVector2f &avSize);
	bool MouseOver(const cVector3f &avPos, const cVector2f &avSize);
	void DrawWidgetBase(const void *apData, const cVector3f &avPos, const cVector2f &avSize, bool abInFocus, bool abTriggered, int alInFocusGfx,
						int alTriggeredGfx);
	void DrawText(const tWString &asText, const void *apFont, const cColor &aColor, int alAlign, const cVector3f &avPos, const cVector2f &avSize,
				  float afSizeMul);
	bool DoButton(const tString &asName, const tWString &asText, const void *apData, cVector3f avPos, cVector2f avSize, int alMode);
	void DoLabel(const tWString &asText, const void *apData, cVector3f avPos, cVector2f avSize, float afSizeMul);
	void DoImage(const void *apGfx, cVector3f avPos, cVector2f avSize);
	float DoTextFrame(const tWString &asText, const cVector2f &avEdge, float afRowSpace, float afStartRow, const void *apData, cVector3f avPos,
					  cVector2f avSize);
	void DoFrame(const void *apData, cVector3f avPos, cVector2f avSize);
	float DoSlider(const tString &asName, float afDefault, float afMin, float afMax, float afStep, const void *apData, cVector3f avPos,
				   cVector2f avSize, bool abVertical);
	bool DoCheckBox(const tString &asName, const tWString &asText, bool abDefault, const void *apData, cVector3f avPos, cVector2f avSize);
	int DoMultiSelect(const tString &asName, int alDefault, const void *apData, cVector3f avPos, cVector2f avSize);
	void DoGauge(const void *apData, float afFill, cVector3f avPos, cVector2f avSize);
	void DoWindowStart(const tWString &asCaption, const void *apData, cVector3f avPos, cVector2f avSize);
	void DoWindowEnd();
	void DoMouse(const void *apGfx, const cVector3f &avOffset, cVector2f avSize);

	// Drawing
	void DrawGfx(const void *apGfx, const cVector3f &avPos, cVector2f avSize, const cColor &aColor);
	void DrawFont(const tWString &asText, const void *apFont, const cVector3f &avPos, int alAlign, const cVector2f &avSizeMul, const cColor &aColor);
	cVector2f GetGfxSize(const void *apGfx);
	iFontData *GetFont(const void *apFont);
	cVector2f FontSize(const void *apFont, float afMul);
	float GetFontLength(const void *apFont, float afMul, const tWString &asText);

	static void RegisterNatives(asIScriptEngine *apEngine);

private:
	struct cOp
	{
		cGuiGfxElement *mpGfx;
		iFontData *mpFont;
		tWString msText;
		cVector3f mvPos;
		cVector2f mvSize;
		cColor mColor;
		int mlMaterial;
		int mlAlign;
		float mfAngle;
	};
	void Record(const cOp &aOp) { mvBuilding.push_back(aOp); }

	static cSomaImGui *mpCurrent;
	static cSomaImGui *mpInputFocus;
	static cSomaImGui *mpPrevInputFocus;

	tString msName;
	cGuiSet *mpSet;
	bool mbFirstRun = true;
	float mfTimeStep = 0;
	float mfTimeCount = 0;
	bool mvActionDown[10] = {};
	bool mvActionTriggered[10] = {};
	cVector2f mvMousePos = 0;
	cVector2f mvMouseRel = 0;
	std::vector<cOp> mvBuilding;
	std::vector<cOp> mvDrawn;
};

// Struct helpers shared with the natives of other script types
void *SomaNewScriptStruct(const char *apType);
const tString *SomaIntern(const tString &asStr);

#endif // SOMA_IMGUI_H
