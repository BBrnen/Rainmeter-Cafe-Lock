// Harmless GUI app/editor fixture: records its effective token without a console.
#include <windows.h>
#include <string>
#include <cstring>
#include <shellapi.h>

int WINAPI wWinMain(HINSTANCE, HINSTANCE, LPWSTR, int)
{
	WCHAR path[32768];
	if (!GetModuleFileNameW(nullptr, path, 32768)) return 1;
	std::wstring output = path;
	const auto slash = output.find_last_of(L"\\/");
	const bool editor = output.substr(slash + 1) == L"Editor.exe";
	output.resize(slash + 1);
	const std::wstring detailsPath = output + L"launch-details.txt";
	output += editor ? L"editor-opened.txt" : L"launched.txt";
	WCHAR directory[32768] = {};
	const DWORD directoryLength = GetCurrentDirectoryW(32768, directory);
	if (!directoryLength || directoryLength >= 32768) return 4;
	const std::wstring details = std::wstring(L"\ufeffcwd=") + directory + L"\r\ncommand=" + GetCommandLineW();
	HANDLE detailFile = CreateFileW(detailsPath.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr,
		CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (detailFile == INVALID_HANDLE_VALUE) return 5;
	DWORD detailBytes = 0;
	const DWORD expectedBytes = static_cast<DWORD>(details.size() * sizeof(wchar_t));
	const BOOL detailsWritten = WriteFile(detailFile, details.data(), expectedBytes, &detailBytes, nullptr);
	CloseHandle(detailFile);
	if (!detailsWritten || detailBytes != expectedBytes) return 6;
	HANDLE token = nullptr;
	BYTE integrity[SECURITY_MAX_SID_SIZE + sizeof(TOKEN_MANDATORY_LABEL)];
	BYTE admin[SECURITY_MAX_SID_SIZE];
	DWORD size = 0, sidSize = sizeof(admin);
	BOOL isAdmin = TRUE;
	if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) return 2;
	bool standard = GetTokenInformation(token, TokenIntegrityLevel, integrity, sizeof(integrity), &size) &&
		CreateWellKnownSid(WinBuiltinAdministratorsSid, nullptr, admin, &sidSize) &&
		CheckTokenMembership(nullptr, admin, &isAdmin) && !isAdmin;
	if (standard)
	{
		PSID sid = reinterpret_cast<TOKEN_MANDATORY_LABEL*>(integrity)->Label.Sid;
		standard = *GetSidSubAuthority(sid, *GetSidSubAuthorityCount(sid) - 1) <= SECURITY_MANDATORY_MEDIUM_RID;
	}
	CloseHandle(token);
	HANDLE file = CreateFileW(output.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (file == INVALID_HANDLE_VALUE) return 3;
	const char* result = standard ? "standard" : "elevated";
	DWORD written;
	WriteFile(file, result, static_cast<DWORD>(strlen(result)), &written, nullptr);
	CloseHandle(file);
	return 0;
}
