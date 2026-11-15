#include "LuxA11cDump.h"

#include "LuxJournal.h"
#include "LuxMapHandler.h"
#include "LuxSavedGameTypes.h"

#include <stdio.h>
#include <stdlib.h>

static FILE *gpDump = NULL;
static bool gbInit = false;

static FILE *Dump()
{
	if(gbInit == false)
	{
		gbInit = true;
		const char *sPath = getenv("OHPL_A11C_DUMP");
		if(sPath && *sPath) gpDump = fopen(sPath, "w");
	}
	return gpDump;
}

bool OhplA11cOn()
{
	return Dump() != NULL;
}

static void Text(FILE *f, const tWString &asText)
{
	for(size_t i=0; i<asText.size(); ++i)
	{
		unsigned c = (unsigned)asText[i];
		if(c >= 32 && c < 127 && c != '|') fputc((int)c, f);
		else fprintf(f, "\\u%04x", c);
	}
}

static void Trace(cGuiSet *apSet, const cGuiGfxElement *apGfx, const tWString *apText, const cVector3f &avPos, const cVector2f &avSize, const cColor &aCol, int alArg)
{
	FILE *f = Dump();
	if(f == NULL || gpBase->mpJournal == NULL || apSet != gpBase->mpJournal->GetSet()) return;
	if(apText)
	{
		fprintf(f, "F %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %d |", avPos.x, avPos.y, avPos.z, avSize.x, avSize.y, aCol.r, aCol.g, aCol.b, aCol.a, alArg);
		Text(f, *apText);
		fputc('\n', f);
	}
	else
	{
		cVector2f vSize = apGfx ? ((cGuiGfxElement*)apGfx)->GetActiveSize() : cVector2f(0);
		fprintf(f, "G %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %d %.9g %.9g\n", avPos.x, avPos.y, avPos.z, avSize.x, avSize.y, aCol.r, aCol.g, aCol.b, aCol.a, alArg, vSize.x, vSize.y);
	}
}

static void State(FILE *f, const char *asTag, cLuxJournal_SaveData &aSave)
{
	fprintf(f, "%s last=%d,%d,%d notes=%d conts=%d quests=%d\n", asTag, aSave.mlLastReadTextCat, aSave.mlLastReadTextEntry, aSave.mlLastReadTextType,
			(int)aSave.mvNotes.Size(), (int)aSave.mvDiaryConts.Size(), (int)aSave.mvQuestNotes.Size());
	for(size_t i=0; i<aSave.mvNotes.Size(); ++i)
	{
		cLuxNote &n = aSave.mvNotes[i];
		fprintf(f, "  n[%d] %s %s %s %s\n", (int)i, n.msNameEntry.c_str(), n.msTextEntry.c_str(), n.msIconFile.c_str(), n.msImageFile.c_str());
	}
	for(size_t c=0; c<aSave.mvDiaryConts.Size(); ++c)
	{
		cLuxJournal_DiaryCont_SaveData &cont = aSave.mvDiaryConts[c];
		fprintf(f, "  c[%d] %s %d\n", (int)c, cont.msType.c_str(), (int)cont.mvDiaries.Size());
		for(size_t i=0; i<cont.mvDiaries.Size(); ++i)
		{
			cLuxDiary &d = cont.mvDiaries[i];
			fprintf(f, "    d[%d] %s %s %s %s\n", (int)i, d.msNameEntry.c_str(), d.msTextEntry.c_str(), d.msIconFile.c_str(), d.msImageFile.c_str());
		}
	}
	for(size_t i=0; i<aSave.mvQuestNotes.Size(); ++i)
	{
		cLuxQuestNote &q = aSave.mvQuestNotes[i];
		fprintf(f, "  q[%d] %d %s %s %s\n", (int)i, q.mbActive, q.msName.c_str(), q.msNameEntry.c_str(), q.msTextEntry.c_str());
	}
}

static void Snap(FILE *f, const char *asTag)
{
	cLuxJournal_SaveData save;
	save.FromJournal(gpBase->mpJournal);
	State(f, asTag, save);
}

static int glFrame = 0;

static void Primary() { gpBase->mpEngine->GetGui()->SendUIButtonPress(eUIButton_Primary); }
static void Secondary() { gpBase->mpEngine->GetGui()->SendUIButtonPress(eUIButton_Secondary); }
static void Arrow(eUIArrow aArrow) { gpBase->mpEngine->GetGui()->SendUIArrowPress(aArrow); }

void OhplA11cTick()
{
	FILE *f = Dump();
	if(f == NULL) return;
	if(gpBase->mpMapHandler == NULL || gpBase->mpMapHandler->GetCurrentMap() == NULL) { glFrame = 0; return; }
	if(gfpOhplGuiTrace == NULL) gfpOhplGuiTrace = &Trace;

	cLuxJournal *pJ = gpBase->mpJournal;
	int lNum;
	int fr = ++glFrame;
	fprintf(f, "tick %d\n", fr);
	switch(fr)
	{
	case 3:
		pJ->Reset();
		pJ->AddNote("CH01L03_VillageTales", "papernote.tga");
		pJ->AddNote("CH01L11_AccountOfHowling", "letter.tga");
		pJ->AddNote("CH01L03_WineCellar", "old_note.tga");
		pJ->AddDiary("CH01L03_Daniel", "diary_page.tga", lNum);
		pJ->AddDiary("CH01L03_Daniel", "diary_page.tga", lNum);
		fprintf(f, "diary idx %d\n", lNum);
		pJ->AddDiary("CH01L06_Daniel02", "diary_page.tga", lNum);
		fprintf(f, "diary idx %d\n", lNum);
		fprintf(f, "quest add %d\n", pJ->AddQuestNote("00Trail", "00Trail"));
		fprintf(f, "quest again %d\n", pJ->AddQuestNote("00Trail", "00Trail"));
		fprintf(f, "quest add %d\n", pJ->AddQuestNote("02Web", "02Web"));
		fprintf(f, "quest add %d\n", pJ->AddQuestNote("03Books", "03Books"));
		fprintf(f, "quest off %d\n", pJ->DisableQuestNote("02Web"));
		fprintf(f, "quest off again %d\n", pJ->DisableQuestNote("02Web"));
		Snap(f, "added");
		break;
	case 5:
		{
			cLuxJournal_SaveData save;
			save.FromJournal(pJ);
			pJ->Reset();
			Snap(f, "reset");
			save.ToJournal(NULL, pJ);
			Snap(f, "restored");
		}
		break;
	case 28: case 98: case 158: case 188: case 228: case 300:
		if(gpBase->mpEngine->GetUpdater()->GetCurrentContainerName() != "Journal") gpBase->mpEngine->GetUpdater()->SetContainer("Journal");
		break;
	case 8:
		gpBase->mpEngine->GetUpdater()->SetContainer("Journal");
		break;
	default:
		if(gpBase->mpEngine->GetUpdater()->GetCurrentContainerName() != "Journal") break;
		switch(fr)
		{
		case 30: Primary(); break;
		case 40: Arrow(eUIArrow_Down); break;
		case 44: Primary(); break;
		case 60: Primary(); break;
		case 64: Primary(); break;
		case 68: Primary(); break;
		case 72: Secondary(); break;
		case 76: Secondary(); break;
		case 80: pJ->ExitPressed(false); break;
		case 84: pJ->ExitPressed(false); break;
		case 100: Arrow(eUIArrow_Down); break;
		case 104: Primary(); break;
		case 112: Arrow(eUIArrow_Down); break;
		case 116: Primary(); break;
		case 130: Primary(); break;
		case 134: Secondary(); break;
		case 138: Secondary(); break;
		case 142: pJ->ExitPressed(false); break;
		case 146: pJ->ExitPressed(false); break;
		case 160: Arrow(eUIArrow_Down); break;
		case 164: Arrow(eUIArrow_Down); break;
		case 168: Primary(); break;
		case 180: pJ->ExitPressed(false); break;
		case 190: pJ->ChangeState(eLuxJournalState_Notes); break;
		case 200: pJ->OpenNoteIdx(0, false); break;
		case 210: Arrow(eUIArrow_Right); break;
		case 214: Arrow(eUIArrow_Left); break;
		case 218: Arrow(eUIArrow_Down); break;
		case 230: pJ->OpenDiaryIdx(0, 1, false); break;
		case 240: Primary(); break;
		case 250: pJ->ExitPressed(false); break;
		case 254: pJ->ExitPressed(false); break;
		case 258: pJ->ExitPressed(false); break;
		case 262: pJ->ExitPressed(false); break;
		case 320: Primary(); break;
		case 324: Primary(); break;
		case 340: Secondary(); break;
		case 344: pJ->ExitPressed(false); break;
		case 348: pJ->ExitPressed(false); break;
		case 352: pJ->ExitPressed(false); break;
		case 430: pJ->ExitPressed(true); break;
		case 500: pJ->ExitPressed(true); break;
		}
		break;
	case 400:
		if(gpBase->mpEngine->GetUpdater()->GetCurrentContainerName() != "Journal") pJ->OpenLastReadText();
		break;
	case 470:
		Snap(f, "final");
		if(gpBase->mpEngine->GetUpdater()->GetCurrentContainerName() != "Journal")
		{
			pJ->SetForceInstantExit(true);
			gpBase->mpEngine->GetUpdater()->SetContainer("Journal");
			pJ->OpenNoteIdx(1, false);
		}
		break;
	case 540:
		pJ->AddDiary("CH01L03_Daniel", "diary_page.tga", lNum);
		fprintf(f, "diary idx %d\n", lNum);
		pJ->AddQuestNote("04FindChemicals", "04FindChemicals");
		Snap(f, "end");
		break;
	}
}
