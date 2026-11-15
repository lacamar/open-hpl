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
	case 8:
		gpBase->mpEngine->GetUpdater()->SetContainer("Journal");
		break;
	case 20: case 70: case 114: case 138: case 206:
		if(gpBase->mpEngine->GetUpdater()->GetCurrentContainerName() != "Journal") gpBase->mpEngine->GetUpdater()->SetContainer("Journal");
		break;
	case 250:
		if(gpBase->mpEngine->GetUpdater()->GetCurrentContainerName() != "Journal") pJ->OpenLastReadText();
		break;
	case 270:
		Snap(f, "final");
		if(gpBase->mpEngine->GetUpdater()->GetCurrentContainerName() != "Journal")
		{
			pJ->SetForceInstantExit(true);
			gpBase->mpEngine->GetUpdater()->SetContainer("Journal");
			pJ->OpenNoteIdx(1, false);
		}
		break;
	case 300:
		pJ->AddDiary("CH01L03_Daniel", "diary_page.tga", lNum);
		fprintf(f, "diary idx %d\n", lNum);
		pJ->AddQuestNote("04FindChemicals", "04FindChemicals");
		Snap(f, "end");
		break;
	default:
		if(gpBase->mpEngine->GetUpdater()->GetCurrentContainerName() != "Journal") break;
		switch(fr)
		{
		case 24: Primary(); break;
		case 28: Arrow(eUIArrow_Down); break;
		case 32: Primary(); break;
		case 40: Primary(); break;
		case 44: Primary(); break;
		case 48: Primary(); break;
		case 52: Secondary(); break;
		case 56: Secondary(); break;
		case 60: pJ->ExitPressed(false); break;
		case 64: pJ->ExitPressed(false); break;
		case 74: Arrow(eUIArrow_Down); break;
		case 78: Primary(); break;
		case 84: Arrow(eUIArrow_Down); break;
		case 88: Primary(); break;
		case 96: Primary(); break;
		case 100: Secondary(); break;
		case 104: pJ->ExitPressed(false); break;
		case 108: pJ->ExitPressed(false); break;
		case 116: Arrow(eUIArrow_Down); break;
		case 120: Arrow(eUIArrow_Down); break;
		case 124: Primary(); break;
		case 132: pJ->ExitPressed(false); break;
		case 140: pJ->ChangeState(eLuxJournalState_Notes); break;
		case 148: pJ->OpenNoteIdx(0, false); break;
		case 156: Arrow(eUIArrow_Right); break;
		case 160: Arrow(eUIArrow_Left); break;
		case 164: Arrow(eUIArrow_Down); break;
		case 172: pJ->OpenDiaryIdx(0, 1, false); break;
		case 180: Primary(); break;
		case 188: pJ->ExitPressed(false); break;
		case 192: pJ->ExitPressed(false); break;
		case 196: pJ->ExitPressed(false); break;
		case 200: pJ->ExitPressed(false); break;
		case 210: Primary(); break;
		case 214: Primary(); break;
		case 222: Secondary(); break;
		case 226: pJ->ExitPressed(false); break;
		case 230: pJ->ExitPressed(false); break;
		case 234: pJ->ExitPressed(false); break;
		case 262: pJ->ExitPressed(true); break;
		case 282: pJ->ExitPressed(true); break;
		}
		break;
	}
}
