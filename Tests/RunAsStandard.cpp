// CI-only launcher. Run the actual Rainmeter regression from a medium-integrity,
// non-administrator token even when the hosted runner itself is elevated.
#include <windows.h>
#include <sddl.h>
#include <shlwapi.h>
#include <cassert>
#include <vector>
#include <cwchar>

int wmain()
{
	HANDLE token = nullptr, standard = nullptr;
	assert(OpenProcessToken(GetCurrentProcess(), TOKEN_ALL_ACCESS, &token));
	BYTE admin[SECURITY_MAX_SID_SIZE];
	DWORD size = sizeof(admin);
	assert(CreateWellKnownSid(WinBuiltinAdministratorsSid, nullptr, admin, &size));
	SID_AND_ATTRIBUTES disabled = { admin, 0 };
	assert(CreateRestrictedToken(token, DISABLE_MAX_PRIVILEGE, 1, &disabled, 0, nullptr, 0, nullptr, &standard));
	PSID medium = nullptr;
	assert(ConvertStringSidToSidW(L"S-1-16-8192", &medium));
	TOKEN_MANDATORY_LABEL label = { { medium, SE_GROUP_INTEGRITY } };
	assert(SetTokenInformation(standard, TokenIntegrityLevel, &label, sizeof(label) + GetLengthSid(medium)));
	LocalFree(medium);
	const WCHAR* args = PathGetArgsW(GetCommandLineW());
	assert(args && *args);
	std::vector<WCHAR> command(args, args + wcslen(args) + 1);
	STARTUPINFOW startup = { sizeof(startup) };
	PROCESS_INFORMATION child = {};
	assert(CreateProcessAsUserW(standard, nullptr, command.data(), nullptr, nullptr, TRUE, 0, nullptr, nullptr, &startup, &child));
	CloseHandle(standard);
	CloseHandle(token);
	CloseHandle(child.hThread);
	WaitForSingleObject(child.hProcess, INFINITE);
	DWORD result = 1;
	GetExitCodeProcess(child.hProcess, &result);
	CloseHandle(child.hProcess);
	return static_cast<int>(result);
}
