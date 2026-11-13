// H5b harness: iFontData glyph table, GetLength and GetWordWrapRows with a seeded font and queries.
// Run with OHPL_IMPL=cpp and =rust and diff stdout. usage: H5B_SEED=<n> h5b_font
// Lines: "font <first> <last> <height bits>", "g <idx> <advance> <ox> <oy> <sx> <sy>" (hex float bits),
// "len <size_x> <text> -> <length>", "wrap <max> <size_x> <text> -> <rows>|throw" (text = hex units, ',' apart).
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

#include "graphics/FontData.h"
#include "resources/GpuShaderManager.h"
#include "system/PreprocessParser.h"

using namespace hpl;

class cFontProbe : public iFontData
{
public:
	cFontProbe() : iFontData("probe", NULL) {}
	bool CreateFromFontFile(const tWString&, int, unsigned short, unsigned short) { return false; }
	bool CreateFromBitmapFile(const tWString&) { return false; }
	void Setup(unsigned short alFirst, unsigned short alLast, float afHeight)
	{
		mlFirstChar = alFirst; mlLastChar = alLast; mfHeight = afHeight; mvSizeRatio = cVector2f(1, 1);
	}
	void Add(cGlyph *apGlyph) { AddGlyph(apGlyph); }
};

static unsigned long long gRng;
static unsigned Rnd(unsigned n)
{
	gRng = gRng * 6364136223846793005ull + 1442695040888963407ull;
	return (unsigned)((gRng >> 33) % n);
}

static std::string Bits(float afX)
{
	unsigned u; memcpy(&u, &afX, 4);
	char b[16]; snprintf(b, sizeof(b), "%08x", u);
	return b;
}

static std::string Hex(const tWString &asText)
{
	std::string s;
	for(size_t i = 0; i < asText.size(); ++i)
	{
		char b[16]; snprintf(b, sizeof(b), "%s%x", i ? "," : "", (unsigned)asText[i]);
		s += b;
	}
	return s.empty() ? "-" : s;
}

static tWString RandomText(int alFirst, int alLast)
{
	static const wchar_t kSpecial[] = { 32, 32, 32, 10, 12290, 65292, 65311, 65281, 0x10000 + 65 };
	tWString w;
	int n = Rnd(40);
	for(int i = 0; i < n; ++i)
	{
		if(Rnd(5) == 0) w.push_back(kSpecial[Rnd(sizeof(kSpecial) / sizeof(kSpecial[0]))]);
		else w.push_back((wchar_t)(alFirst - 3 + (int)Rnd(alLast - alFirst + 8)));
	}
	return w;
}

static void PushRow(std::string &asOut, const tWString &asRow) { asOut += (asOut.empty() ? "" : ";") + Hex(asRow); }

// Link-time hook of the shared build wrapper; unused here.
extern "C" iGpuShader* __wrap__ZN3hpl17cGpuShaderManager12CreateShaderERKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEENS_14eGpuShaderTypeEPNS_19cParserVarContainerE(
	void*, const tString&, eGpuShaderType, cParserVarContainer*)
{
	return NULL;
}

int hplMain(const tString&)
{
	const char *sSeed = getenv("H5B_SEED");
	if(!sSeed) return 2;
	gRng = (unsigned long long)atoi(sSeed) * 7919 + 17;

	int lFirst = Rnd(40), lLast = lFirst + 20 + Rnd(200);
	cFontProbe font;
	float fHeight = Rnd(2000) / 100.0f;
	font.Setup((unsigned short)lFirst, (unsigned short)lLast, fHeight);
	printf("font %d %d %s\n", lFirst, lLast, Bits(fHeight).c_str());
	for(int i = 0; i <= lLast - lFirst; ++i)
	{
		if(Rnd(10) == 0) { font.Add(NULL); continue; }
		cVector2f off(Rnd(2000) / 1000.0f - 1.0f, Rnd(2000) / 1000.0f - 1.0f), size(Rnd(2000) / 1000.0f, Rnd(2000) / 1000.0f);
		float fAdv = Rnd(2000) / 1000.0f;
		font.Add(new cGlyph(NULL, off, size, fAdv));
		printf("g %d %s %s %s %s %s\n", i, Bits(fAdv).c_str(), Bits(off.x).c_str(), Bits(off.y).c_str(), Bits(size.x).c_str(), Bits(size.y).c_str());
	}

	for(int q = 0; q < 60; ++q)
	{
		tWString text = RandomText(lFirst, lLast);
		float fSize = 0.5f + Rnd(3000) / 1000.0f;
		if(q % 2 == 0)
		{
			float fLen = font.GetLength(cVector2f(fSize, fSize), text.c_str());
			printf("len %s %s -> %s\n", Bits(fSize).c_str(), Hex(text).c_str(), Bits(fLen).c_str());
		}
		else
		{
			float fMax = Rnd(8000) / 100.0f;
			tWStringVec rows;
			std::string sRows;
			try
			{
				font.GetWordWrapRows(fMax, 1.0f, cVector2f(fSize, fSize), text, &rows);
				for(size_t i = 0; i < rows.size(); ++i) PushRow(sRows, rows[i]);
				if(rows.empty()) sRows = "none";
			}
			catch(std::out_of_range&) { sRows = "throw"; }
			printf("wrap %s %s %s -> %s\n", Bits(fMax).c_str(), Bits(fSize).c_str(), Hex(text).c_str(), sRows.c_str());
		}
	}
	return 0;
}
