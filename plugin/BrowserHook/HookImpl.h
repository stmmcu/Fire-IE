/*
This file is part of Fire-IE.

Fire-IE is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Fire-IE is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Fire-IE.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

// HookImpl.h : common minhook implementation utilities
//

#if defined _M_X64
#pragma comment(lib, "external\\MinHook.64.lib")
#elif defined _M_IX86
#pragma comment(lib, "external\\MinHook.32.lib")
#endif

#define DEFINE_FUNCTION_INFO(module, func) {module, #func, NULL, (PVOID *)&func##_original, (PVOID)func##_hook, FALSE}


namespace BrowserHook
{
	struct FunctionInfo
	{
		LPCSTR  szFunctionModule;
		LPCSTR  szFunctionName;
		PVOID   pTargetFunction;
		PVOID*  ppOriginalFunction;
		PVOID   pHookFunction;
		BOOL    bSucceeded;
	};

	// Reference-counted MH initialize
	BOOL Do_MH_Initialize();

	// Reference-counted MH uninitialize
	BOOL Do_MH_Uninitialize();

	template<UINT size>
	BOOL Do_MH_HookFunctions(FunctionInfo (&afuncs)[size])
	{
		BOOL succeeded = TRUE;
		for (UINT i = 0; i < size; i++)
		{
			Do_MH_HookFunction(afuncs[i]);
			succeeded = succeeded && afuncs[i].bSucceeded;
		}
		return succeeded;
	}

	BOOL Do_MH_HookFunction(FunctionInfo& info);


	template<UINT size>
	BOOL Do_MH_UnhookFunctions(FunctionInfo (&afuncs)[size])
	{
		BOOL succeeded = TRUE;
		for (UINT i = 0; i < size; i++)
		{
			Do_MH_UnhookFunction(afuncs[i]);
			succeeded = succeeded && (!afuncs[i].bSucceeded);
		}
		return succeeded;
	}

	BOOL Do_MH_UnhookFunction(FunctionInfo& info);
} // BrowserHook
