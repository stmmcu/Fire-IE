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
#include "StdAfx.h"
#include "AtlDepHook.h"
#include "HookImpl.h"

namespace BrowserHook
{
	BOOL FixThunk(LONG dwLong);

	static LONG (WINAPI *SetWindowLongA_original)(HWND hWnd, int nIndex, LONG dwNewLong) = NULL;

	static LONG WINAPI SetWindowLongA_hook(HWND hWnd, int nIndex, LONG dwNewLong) 
	{
		TRACE("[fireie] SetWindowLongA_hook\n");

		if (!FixThunk(dwNewLong))
		{
			TRACE("[fireie] FixThunk failed!\n");
		}

		return SetWindowLongA_original(hWnd, nIndex, dwNewLong);
	}

	static LONG (WINAPI *SetWindowLongW_original)(HWND hWnd, int nIndex, LONG dwNewLong) = NULL;

	static LONG WINAPI SetWindowLongW_hook(HWND hWnd, int nIndex, LONG dwNewLong) 
	{
		TRACE("[fireie] SetWindowLongW_hook\n");

		if (!FixThunk(dwNewLong))
		{
			TRACE("[fireie] FixThunk failed!\n");
		}

		return SetWindowLongW_original(hWnd, nIndex, dwNewLong);
	}

	static FunctionInfo s_Functions[] = 
	{
		DEFINE_FUNCTION_INFO("user32.dll", SetWindowLongA),
		DEFINE_FUNCTION_INFO("user32.dll", SetWindowLongW),
	};

	// WindowProc thunks 
#if !defined(_M_X64)
#pragma pack(push,1)
	struct _WndProcThunk
	{
		DWORD   m_mov;          // mov dword ptr [esp+0x4], pThis (esp+0x4 is hWnd)
		DWORD   m_this;         //
		BYTE    m_jmp;          // jmp WndProc
		DWORD   m_relproc;      // relative jmp
	};
#pragma pack(pop)

	BOOL CheckThunk(_WndProcThunk* pThunk)
	{
		if (pThunk->m_mov != 0x042444C7)
		{
			return FALSE;
		}
		if (pThunk->m_jmp != 0xE9)
		{
			return FALSE;
		}
		return TRUE;
	}
#else
#pragma pack(push,2)
	struct _WndProcThunk
	{
		USHORT  RcxMov;         // mov rcx, pThis
		ULONG64 RcxImm;         //
		USHORT  RaxMov;         // mov rax, target
		ULONG64 RaxImm;         //
		USHORT  RaxJmp;         // jmp target
	};

	BOOL CheckThunk(_WndProcThunk* pThunk)
	{
		if (pThunk->RcxMov != 0xb948)
		{
			return FALSE;
		}
		if (pThunk->RaxMov != 0xb848)
		{
			return FALSE;
		}
		if (pThunk->RaxJmp != 0xe0ff)
		{
			return FALSE;
		}
		return TRUE;
	}
#endif
	BOOL FixThunk(LONG dwLong)
	{
		_WndProcThunk* pThunk = (_WndProcThunk*)dwLong;

		MEMORY_BASIC_INFORMATION mbi;
		DWORD dwOldProtect;

		if (IsBadReadPtr(pThunk, sizeof(_WndProcThunk)) || !CheckThunk(pThunk))
		{
			return TRUE;
		}

		if (VirtualQuery((LPCVOID)pThunk, &mbi, sizeof(MEMORY_BASIC_INFORMATION)) == 0)
		{
			return FALSE;
		}
		// The memory is already PAGE_EXECUTE_READWRITE, so don't need fixing
		if (mbi.AllocationProtect == PAGE_EXECUTE_READWRITE)
		{
			return TRUE;
		}

		return VirtualProtect((LPVOID)pThunk, sizeof(_WndProcThunk), PAGE_EXECUTE_READWRITE, &dwOldProtect);
	}

	AtlDepHook AtlDepHook::s_instance;

	void AtlDepHook::Install(void)
	{
		if (!Do_MH_Initialize())
			return;

		Do_MH_HookFunctions(s_Functions);
	}

	void AtlDepHook::Uninstall(void)
	{
		Do_MH_UnhookFunctions(s_Functions);

		Do_MH_Uninitialize();
	}
}
