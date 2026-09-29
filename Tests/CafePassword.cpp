// Production password/session tests, Windows CNG and real configuration I/O.
#include "../Common/CafePassword.h"
#include "../Library/CafeLock.h"
#include <cassert>
#include <cstdio>
#include <fstream>
#include <iterator>
static CafeSecurity::PasswordSession* active = nullptr;
bool CafeLock::IsLocked() { return !active || active->IsLocked(); }
int wmain()
{
	WCHAR directory[MAX_PATH], filename[MAX_PATH];
	assert(GetTempPathW(MAX_PATH, directory));
	assert(GetTempFileNameW(directory, L"caf", 0, filename));
	assert(DeleteFileW(filename));
	CafeSecurity::PasswordSession session(filename);
	active = &session;
	const WCHAR* password = L"Cafe test \u2615 first password";
	const WCHAR* second = L"Cafe test second password";
	assert(session.IsLocked() && session.NeedsSetup());
	assert(!session.Setup(password,L"mismatch") && session.IsLocked() && session.NeedsSetup());
	assert(!session.Setup(L"",L""));
	assert(session.Setup(password,password) && !session.IsLocked());
	assert(!session.Setup(second,second));
	assert(CafeLock::AllowsLaunch(L"C:\\Skins\\Shelf Suite\\@Resources\\configurator.html",L"C:\\Skins\\"));
	WCHAR oldSalt[128];
	GetPrivateProfileStringW(L"CafeLock",L"Salt",L"",oldSalt,128,filename);
	assert(!session.Change(L"wrong",second,second));
	assert(!session.Change(password,second,L"mismatch"));
	assert(session.Change(password,second,second));
	WCHAR newSalt[128];
	GetPrivateProfileStringW(L"CafeLock",L"Salt",L"",newSalt,128,filename);
	assert(wcscmp(oldSalt,newSalt));
	session.Lock();
	assert(session.IsLocked());
	assert(!CafeLock::AllowsLaunch(L"C:\\Skins\\Shelf Suite\\@Resources\\configurator.html",L"C:\\Skins\\"));
	assert(CafeLock::AllowsLaunch(L"C:\\Apps\\Launcher.exe",L"C:\\Skins\\"));
	assert(!session.Change(second,password,password));
	assert(!session.Unlock(password) && session.IsLocked());
	assert(!session.Unlock(L"wrong") && session.IsLocked());
	assert(session.Unlock(second) && !session.IsLocked());
	CafeSecurity::PasswordSession restarted(filename);
	assert(restarted.IsLocked() && !restarted.NeedsSetup());
	assert(!restarted.Unlock(password) && restarted.Unlock(second));
	std::ifstream file(filename, std::ios::binary);
	std::string config((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
	file.close();
	for (const WCHAR* secret : {password,second})
		assert(config.find(std::string(reinterpret_cast<const char*>(secret),wcslen(secret)*sizeof(WCHAR))) == std::string::npos);
	// Malformed existing configuration never grants first-run setup or access.
	assert(WritePrivateProfileStringW(L"CafeLock",L"Iterations",L"1",filename));
	restarted.Lock();
	assert(!restarted.NeedsSetup() && !restarted.Setup(password,password) && !restarted.Unlock(second));
	assert(DeleteFileW(filename));
	puts("PASS: setup/mismatch/empty; correct/wrong; current-password change; fresh salt; old rejected/new accepted; relock/restart; ShelfSuite boundary; no plaintext; malformed record fails closed.");
	return 0;
}
