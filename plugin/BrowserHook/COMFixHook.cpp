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

// COMFixHook : hook registry functions to solve ActiveX-related freezes
//

#include "StdAfx.h"

#include "COMFixHook.h"
#include "HookImpl.h"

using namespace BrowserHook;

namespace BrowserHook
{
#define NTAPI __stdcall
	typedef LONG NTSTATUS;
	typedef NTSTATUS (NTAPI *NtQueryKey_Signature)(
		HANDLE  KeyHandle,
		int KeyInformationClass,
		PVOID  KeyInformation,
		ULONG  Length,
		PULONG  ResultLength);

	static NtQueryKey_Signature GetNtQueryKey()
	{
		HMODULE dll = LoadLibrary(L"ntdll.dll");
		if (dll != NULL)
		{

			NtQueryKey_Signature func = reinterpret_cast<NtQueryKey_Signature>(::GetProcAddress(dll, "NtQueryKey"));

			if (func != NULL)
				return func;
		}
		return NULL;
	}

	static CStringW GetKeyPathFromHKEY(HKEY key)
	{
		static const NTSTATUS STATUS_SUCCESS = 0x00000000L;
		static const NTSTATUS STATUS_BUFFER_TOO_SMALL = 0xC0000023L;

		static const int KeyNameInformation = 3;
		static const NtQueryKey_Signature NtQueryKey = GetNtQueryKey();

		typedef struct _KEY_NAME_INFORMATION {
			ULONG NameLength;
			WCHAR Name[1];
		} KEY_NAME_INFORMATION, *PKEY_NAME_INFORMATION;

		CStringW keyPath;

		HANDLE handle = NULL;
		if (DuplicateHandle(GetCurrentProcess(), key, GetCurrentProcess(), &handle, KEY_QUERY_VALUE, TRUE, 0) && handle)
		{
			DWORD size = 0;
			DWORD result = 0;
			result = NtQueryKey(handle, KeyNameInformation, NULL, 0, &size);
			if (result == STATUS_BUFFER_TOO_SMALL)
			{
				size = size + sizeof(WCHAR);
				WCHAR* buffer = new WCHAR[size / sizeof(WCHAR)]; // size is in bytes
				result = NtQueryKey(handle, KeyNameInformation, buffer, size, &size);
				if (result == STATUS_SUCCESS)
				{
					const KEY_NAME_INFORMATION* kni = reinterpret_cast<const KEY_NAME_INFORMATION*>(buffer);
					buffer[(sizeof(kni->NameLength) + kni->NameLength) / sizeof(WCHAR)] = L'\0';
					keyPath = CStringW(kni->Name);
				}

				delete[] buffer;
			}
			CloseHandle(handle);
		}
		return keyPath;
	}

	static LONG fixGetValueW(LONG lResult, LPCWSTR lpValueName, LPDWORD pwdType, LPVOID pData, LPDWORD pcbData)
	{
		static const WCHAR szValue[] = L"Apartment";

		if (wcscmp(lpValueName, L"ThreadingModel") == 0)
		{
			// write type of result
			if (pwdType)
				*pwdType = REG_SZ;

			// write size of result
			if (pcbData)
			{
				if (pData && *pcbData < sizeof(szValue))
				{
					// buffer size too small
					lResult = ERROR_MORE_DATA;
				}
				else
				{
					// buffer is enough - or - result not needed
					// either case returns ERROR_SUCCESS
					if (pData)
						memcpy(pData, szValue, sizeof(szValue));
					lResult = ERROR_SUCCESS;
				}
				// size of required buffer must be written if pcbData is not NULL
				*pcbData = sizeof(szValue);
			}
			else
			{
				// pData must be NULL if pcbData is NULL
				lResult = ERROR_SUCCESS;
			}
		}
		return lResult;
	}

	/*
	LONG WINAPI RegGetValue(
		_In_         HKEY hkey,
		_In_opt_     LPCTSTR lpSubKey,
		_In_opt_     LPCTSTR lpValue,
		_In_opt_     DWORD dwFlags,
		_Out_opt_    LPDWORD pdwType,
		_Out_opt_    PVOID pvData,
		_Inout_opt_  LPDWORD pcbData
	);
	*/
	typedef LONG (WINAPI *RegGetValueW_Signature)(HKEY hkey, LPCWSTR lpSubKey, LPCWSTR lpValue, DWORD dwFlags, LPDWORD pwdType, PVOID pvData, LPDWORD pcbData);
	typedef LONG (WINAPI *RegGetValueA_Signature)(HKEY hkey, LPCSTR lpSubKey, LPCSTR lpValue, DWORD dwFlags, LPDWORD pwdType, PVOID pvData, LPDWORD pcbData);
	static RegGetValueW_Signature RegGetValueW_original = NULL;
	static RegGetValueA_Signature RegGetValueA_original = NULL;
	
	static LONG WINAPI RegGetValueW_hook(HKEY hkey, LPCWSTR lpSubKey, LPCWSTR lpValue, DWORD dwFlags, LPDWORD pwdType, PVOID pvData, LPDWORD pcbData)
	{
		CStringW keyPath = GetKeyPathFromHKEY(hkey) + (lpSubKey ? (CStringW(L"\\") + lpSubKey) : L"");
		LONG lResult = RegGetValueW_original(hkey, lpSubKey, lpValue, dwFlags, pwdType, pvData, pcbData);
		if (lResult != ERROR_FILE_NOT_FOUND) return lResult;

		TRACE(L"[COMFixHook] RegGetValueW_hook: key=\"%s\", name=\"%s\", error=\"%d\"\n", keyPath, lpValue, lResult);
		return fixGetValueW(lResult, lpValue, pwdType, pvData, pcbData);
	}

	static LONG WINAPI RegGetValueA_hook(HKEY hkey, LPCSTR lpSubKey, LPCSTR lpValue, DWORD dwFlags, LPDWORD pwdType, PVOID pvData, LPDWORD pcbData)
	{
		LONG lResult = RegGetValueA_original(hkey, lpSubKey, lpValue, dwFlags, pwdType, pvData, pcbData);
		if (lResult != ERROR_FILE_NOT_FOUND) return lResult;

		TRACE("[COMFixHook] RegGetValueA_hook: name=\"%s\", error=\"%d\"\n", lpValue, lResult);
		return lResult;
	}

	/*
	LONG WINAPI RegQueryValueEx(
	  _In_         HKEY hKey,
	  _In_opt_     LPCTSTR lpValueName,
	  _Reserved_   LPDWORD lpReserved,
	  _Out_opt_    LPDWORD lpType,
	  _Out_opt_    LPBYTE lpData,
	  _Inout_opt_  LPDWORD lpcbData
	);
	*/

	typedef LONG (WINAPI *RegQueryValueExW_Signature)(HKEY hkey, LPCWSTR lpValueName, LPDWORD lpReserved, LPDWORD lpType, LPBYTE lpData, LPDWORD lpcbData);
	typedef LONG (WINAPI *RegQueryValueExA_Signature)(HKEY hkey, LPCSTR lpValueName, LPDWORD lpReserved, LPDWORD lpType, LPBYTE lpData, LPDWORD lpcbData);
	static RegQueryValueExW_Signature RegQueryValueExW_original = NULL;
	static RegQueryValueExA_Signature RegQueryValueExA_original = NULL;

	static LONG WINAPI RegQueryValueExW_hook(HKEY hkey, LPCWSTR lpValueName, LPDWORD lpReserved, LPDWORD lpType, LPBYTE lpData, LPDWORD lpcbData)
	{
		CString keyPath = GetKeyPathFromHKEY(hkey);
		LONG lResult = RegQueryValueExW_original(hkey, lpValueName, lpReserved, lpType, lpData, lpcbData);
		if (lResult != ERROR_FILE_NOT_FOUND) return lResult;

		TRACE(L"[COMFixHook] RegQueryValueExW_hook: key=\"%s\", name=\"%s\", error=\"%d\"\n", keyPath, lpValueName, lResult);
		return fixGetValueW(lResult, lpValueName, lpType, lpData, lpcbData);
	}

	static LONG WINAPI RegQueryValueExA_hook(HKEY hkey, LPCSTR lpValueName, LPDWORD lpReserved, LPDWORD lpType, LPBYTE lpData, LPDWORD lpcbData)
	{
		LONG lResult = RegQueryValueExA_original(hkey, lpValueName, lpReserved, lpType, lpData, lpcbData);
		if (lResult != ERROR_FILE_NOT_FOUND) return lResult;

		TRACE("[COMFixHook] RegQueryValueExA_hook: name=\"%s\", error=\"%d\"\n", lpValueName, lResult);
		return lResult;
	}

	static FunctionInfo s_COMFixHookFunctions[] =
	{
		DEFINE_FUNCTION_INFO("advapi32.dll", RegGetValueW),
		DEFINE_FUNCTION_INFO("advapi32.dll", RegGetValueA),
		DEFINE_FUNCTION_INFO("advapi32.dll", RegQueryValueExW),
		DEFINE_FUNCTION_INFO("advapi32.dll", RegQueryValueExA),
	};

} // namespace BrowserHook

COMFixHook COMFixHook::s_instance;

void COMFixHook::Install()
{
	if (!Do_MH_Initialize())
		return;

	Do_MH_HookFunctions(s_COMFixHookFunctions);
	GetKeyPathFromHKEY(NULL);
}

void COMFixHook::Uninstall()
{
	Do_MH_UnhookFunctions(s_COMFixHookFunctions);
	Do_MH_Uninitialize();
}
