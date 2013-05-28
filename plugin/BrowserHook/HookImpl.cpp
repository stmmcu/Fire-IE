#include "StdAfx.h"

#include "HookImpl.h"
#include "external\MinHook.h"

using namespace BrowserHook;

static UINT s_MHRefs = 0;

BOOL BrowserHook::Do_MH_Initialize()
{
	if (s_MHRefs)
		return TRUE;

	MH_STATUS mhs = MH_Initialize();
	if (mhs == MH_OK)
	{
		s_MHRefs++;
		return TRUE;
	}
	return FALSE;
}

BOOL BrowserHook::Do_MH_Uninitialize()
{
	MH_STATUS mhs = MH_OK;
	if (s_MHRefs)
	{
		s_MHRefs--;
		if (s_MHRefs == 0)
		{
			mhs = MH_Uninitialize();
		}
	}
	return mhs == MH_OK;
}

BOOL BrowserHook::Do_MH_HookFunction(FunctionInfo& info)
{
	if (info.bSucceeded) 
	{
		return FALSE;
	}

	HMODULE hModule = ::LoadLibraryA(info.szFunctionModule);
	if (!hModule)
	{
		DWORD dwErrorCode = ::GetLastError();
		TRACE("[fireie] Cannot LoadLibraryA(%s)! GetLastError: %d",info.szFunctionModule, dwErrorCode);
		return FALSE;
	}

	info.pTargetFunction = GetProcAddress(hModule, info.szFunctionName);
	if (info.pTargetFunction == NULL)
	{
		TRACE("[fireie] Cannot GetProcAddress of %s", info.szFunctionName);
		return FALSE;
	}
	if (MH_CreateHook(info.pTargetFunction, info.pHookFunction, info.ppOriginalFunction) != MH_OK)
	{
		TRACE("[fireie] MH_CreateHook failed! Module: %s  Function: %s", info.szFunctionModule, info.szFunctionName);
		return FALSE;
	}
	// Enable the hook
	if (MH_EnableHook(info.pTargetFunction) != MH_OK)
	{
		TRACE("[fireie] MH_EnableHook failed! Module: %s  Function: %s", info.szFunctionModule, info.szFunctionName);
		return FALSE;
	}
	return info.bSucceeded = TRUE;
}

BOOL BrowserHook::Do_MH_UnhookFunction(FunctionInfo& info)
{
	if (*info.ppOriginalFunction != NULL)
	{
		MH_STATUS mhs = MH_DisableHook(info.pTargetFunction);
		if (mhs == MH_OK)
		{
			*info.ppOriginalFunction = NULL;
			info.bSucceeded = FALSE;
			return TRUE;
		}
	}
	return FALSE;
}
