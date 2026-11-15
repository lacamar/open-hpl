#ifndef LUX_A10B_HOST_H
#define LUX_A10B_HOST_H

#include "LuxBase.h"
#include "RustLuxMenuA10b.h"

#include <deque>

enum eOhplA10bKind
{
	eOhplA10bKind_PreMenu,
	eOhplA10bKind_DemoEnd,
	eOhplA10bKind_Other,
};

class cOhplA10bShell
{
public:
	cOhplA10bShell(iLuxUpdateable *apOwner, int alKind);

	bool OnCallback(int alTag, iWidget *apWidget, const cGuiMessageData &aData);

	OhplA10bHost mHost;
	uint64_t mH;
	iLuxUpdateable *mpOwner;
	int mlKind;

	struct Cb
	{
		cOhplA10bShell *mpShell;
		int mlTag;
	};
	std::deque<Cb> mCbs;
};

#endif
