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
	static cSomaImGui *GetInputFocus();
	static cSomaImGui *GetScriptInputFocus() { return mpInputFocus; }
	static bool mbGameHudFocus;
	static cSomaImGui *GetPrevInputFocus() { return mpPrevInputFocus; }
	static void SetInputFocus(cSomaImGui *apImGui, bool abShowMouse);
	static void UpdateFocusHistory() { mpPrevInputFocus = mpInputFocus; }

	void Begin(float afTimeStep);
	void End();
	void DrawAll();
	void ClearStates();
	tString DebugOps(size_t alMax);

	cGuiSet *GetSet() { return mpSet; }
	const tString &GetName() { return msName; }
	bool IsFirstRun() { return mbFirstRun; }
	float GetTimeStep() { return mfTimeStep; }
	float GetTimeCount() { return mfTimeCount; }

	void SendAction(int alAction, bool abDown, bool abTriggered);
	bool ActionTriggered(int alAction, bool abCheckIfUsed = false) { return UseAction(alAction, abCheckIfUsed) && mvActionTriggered[alAction]; }
	bool ActionIsDown(int alAction, bool abCheckIfUsed = false) { return UseAction(alAction, abCheckIfUsed) && mvActionDown[alAction]; }
	bool UseAction(int alAction, bool abCheckIfUsed)
	{
		if (alAction < 0 || alAction >= 10 || (abCheckIfUsed && mvActionUsed[alAction]))
			return false;
		mvActionUsed[alAction] = true;
		return true;
	}
	void SendMouseVirtualPosition(const cVector2f &avPos, const cVector2f &avRel);
	void SendMousePosition(const cVector2l &avPos, const cVector2l &avRel);
	const cVector2f &GetMousePosition() { return mvMousePos; }
	const cVector2f &GetMouseRel() { return mvMouseRel; }
	bool mbShowMouse = false;
	cColor mScreenClear = cColor(0, 0);
	cColor mScreenOfflineClear = cColor(0, 0);
	cVector2f mvCursor3D = 0;
	bool mbShowMouseAutomatically = true;

	struct cState
	{
		int mlInt = 0;
		float mfFloat = 0;
		cVector3f mvVec = 0;
		cColor mCol = cColor(1, 1);
		bool mbSetInt = false, mbSetFloat = false, mbSetVec = false, mbSetCol = false;
		bool mbInFocus = false;
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
	struct cTimer
	{
		uint64_t mlId;
		float mfTime;
		bool mbRepeat, mbTouched;
	};
	std::vector<cTimer> mvTimers;
	cTimer *FindTimer(uint64_t alId)
	{
		for (cTimer &t : mvTimers)
			if (t.mlId == alId)
				return &t;
		return nullptr;
	}

	struct cModifiers
	{
		cColor mColorMul = cColor(1, 1);
		cColor mTextColorMul = cColor(1, 1);
		bool mbUseInput = true;
		bool mbUseUIPos = true;
		cVector2f mvExpHori = 0, mvExpVert = 0;
		float mfRotateAngle = 0;
		bool mbRotateCustomPivot = false;
		cVector2f mvRotatePivot = 0;
	};
	cModifiers mMods;
	std::vector<cModifiers> mvModStack;
	std::vector<cVector2f> mvLineStrip;
	int mlAlign = 0;

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

	struct cPrev
	{
		bool mbPressed = false, mbBecamePressed = false, mbInFocus = false, mbBecameInFocus = false;
		bool mbWasInFocus = false, mbMouseOver = false, mbUpdated = false;
		cVector3f mvPos = 0;
		cVector2f mvSize = 0;
	};
	cPrev mPrev;
	void SetPrevFocus(cState &aState, bool abOver);

	// Keyboard/gamepad focus: widgets register a nav rect per frame, End() moves focus along it
	struct cNavEntry
	{
		uint64_t mlId;
		cVector2l mvPos, mvSize;
		int mlWrap, mlGroup;
	};
	std::vector<cNavEntry> mvNav;
	uint64_t mlFocus = 0, mlPrevFocus = 0;
	cVector2l mvLastDir = 0;
	int mlLastCount = 0;
	bool mbFoundFocus = false;
	int mlWrapMode = 3, mlGroupFlags = 1;
	int mlMouseLock = 0;
	void SetFocus(const tString &asName);
	bool WidgetBase(uint64_t alId, const cVector3f &avPos, const cVector2f &avSize, cState &aState);
	bool BecamePressed(bool abKeys, bool abMouse);
	const cNavEntry *FindNav(uint64_t alId);
	uint64_t NavClosest(const cNavEntry &aCur, const cVector2l &avDir, bool abAhead, bool abLoose);
	void UpdateUIMovement();

	std::vector<tWString> mvItems;

	std::map<tString, void *> mmapDefaults;
	void *GetDefault(const char *apType);
	void SetDefault(const char *apType, const void *apData);

	void Layout(cVector3f &avPos, cVector2f &avSize, const cVector2f &avDefaultSize);
	cVector3f Align(const cVector3f &avPos, const cVector2f &avSize, int alAlign);
	void Advance(const cVector3f &avPos, const cVector2f &avSize, bool abUpdated = false);
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

	void DrawGfx(const void *apGfx, const cVector3f &avPos, cVector2f avSize, const cColor &aColor);
	void DrawFrame(const void *apFrame, const cVector3f &avPos, const cVector2f &avSize, const cColor &aColor);
	void DrawFont(const tWString &asText, const void *apFont, const cVector3f &avPos, int alAlign, const cVector2f &avSizeMul, const cColor &aColor);
	cVector2f GetGfxSize(const void *apGfx);
	iFontData *GetFont(const void *apFont);
	cVector2f FontSize(const void *apFont, float afMul);
	float GetFontLength(const void *apFont, float afMul, const tWString &asText);

	static void RegisterNatives(asIScriptEngine *apEngine);
	int GetDrawnOpNum() { return (int)mvDrawn.size(); }

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
	static const int kClipBegin = 1, kClipEnd = 2;
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
	bool mvActionUsed[10] = {};
	cVector2f mvMousePos = 0;
	cVector2f mvMouseRel = 0;
	std::vector<cOp> mvBuilding;
	std::vector<cOp> mvDrawn;
};

void *SomaNewScriptStruct(const char *apType);
void *SomaNewOwnedScriptStruct(const char *apType);
const tString *SomaIntern(const tString &asStr);

#endif // SOMA_IMGUI_H
