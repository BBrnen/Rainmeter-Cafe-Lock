// Rainmeter Cafe Lock. GNU GPL v2 or later; see LICENSE.
#include <windows.h>
#include <shellapi.h>
#include <string>
#include "../Common/CafeAuthorization.h"

// This executable only acknowledges one Rainmeter-created authorization pipe.
// It never launches Rainmeter, an editor, a skin command, or another application.
int WINAPI wWinMain(HINSTANCE, HINSTANCE, LPWSTR, int)
{
	HANDLE token = nullptr;
	if (!OpenThreadToken(GetCurrentThread(), TOKEN_QUERY, TRUE, &token))
	{
		HANDLE primary = nullptr;
		if (!OpenProcessToken(GetCurrentProcess(), TOKEN_DUPLICATE | TOKEN_QUERY, &primary)) return 1;
		DuplicateToken(primary, SecurityIdentification, &token);
		CloseHandle(primary);
	}
	const bool elevated = token && CafeSecurity::IsElevatedAdministrator(token);
	if (token) CloseHandle(token);
	if (!elevated) return 2;

	int count = 0;
	LPWSTR* args = CommandLineToArgvW(GetCommandLineW(), &count);
	if (!args || count != 4) { if (args) LocalFree(args); return 3; }
	std::wstring name = args[1];
	WCHAR* end = nullptr;
	const DWORD expectedPid = wcstoul(args[2], &end, 10);
	bool valid = expectedPid != 0 && end && !*end;
	const ULONGLONG expectedTime = _wcstoui64(args[3], &end, 10);
	valid = valid && expectedTime != 0 && end && !*end;
	LocalFree(args);
	if (!valid || name.compare(0, 23, L"\\\\.\\pipe\\RainmeterCafe-") != 0) return 3;

	// Identification only: a pipe server cannot use this connection to act as admin.
	HANDLE pipe = CreateFileW(name.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
		SECURITY_SQOS_PRESENT | SECURITY_IDENTIFICATION, nullptr);
	if (pipe == INVALID_HANDLE_VALUE) return 4;
	ULONG serverPid = 0;
	HANDLE server = nullptr;
	valid = GetNamedPipeServerProcessId(pipe, &serverPid) && serverPid == expectedPid;
	if (valid) server = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE, FALSE, serverPid);
	valid = valid && server && CafeSecurity::CreationTime(server) == expectedTime &&
		WaitForSingleObject(server, 0) == WAIT_TIMEOUT;
	if (server) CloseHandle(server);
	DWORD request = 1, transferred = 0;
	valid = valid && WriteFile(pipe, &request, sizeof(request), &transferred, nullptr) && transferred == sizeof(request);
	// Bound the exchange (not the maintenance session) if the requesting process hangs.
	DWORD available = 0;
	for (int attempt = 0; valid && attempt < 300; ++attempt)
	{
		if (!PeekNamedPipe(pipe, nullptr, 0, nullptr, &available, nullptr)) { valid = false; break; }
		if (available) break;
		Sleep(100);
	}
	valid = valid && available == sizeof(request) && ReadFile(pipe, &request, sizeof(request), &transferred, nullptr) &&
		transferred == sizeof(request) && request == 1;
	CloseHandle(pipe);
	return valid ? 0 : 5;
}
