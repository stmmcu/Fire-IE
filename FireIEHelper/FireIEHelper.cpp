
#include "StdAfx.h"

#include "comfix.h"

int WINAPI _tWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, TCHAR* pCmdLine, int nCmdShow)
{
	if (Plugin::COMFix::ifNeedFix())
		Plugin::COMFix::doFix();
	return 0;
}
