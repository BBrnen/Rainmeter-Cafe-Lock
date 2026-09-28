// Real Windows pipe/token tests. Run on the elevated, disposable CI runner.
#include "../Common/CafeAuthorization.h"
#include <cassert>
#include <cstdio>

static HANDLE RestrictedToken()
{
	HANDLE primary = nullptr, restricted = nullptr, token = nullptr;
	assert(OpenProcessToken(GetCurrentProcess(), TOKEN_ALL_ACCESS, &primary));
	BYTE admin[SECURITY_MAX_SID_SIZE];
	DWORD size = sizeof(admin);
	assert(CreateWellKnownSid(WinBuiltinAdministratorsSid, nullptr, admin, &size));
	SID_AND_ATTRIBUTES disabled = { admin, 0 };
	assert(CreateRestrictedToken(primary, DISABLE_MAX_PRIVILEGE, 1, &disabled, 0, nullptr, 0, nullptr, &restricted));
	PSID medium = nullptr;
	assert(ConvertStringSidToSidW(L"S-1-16-8192", &medium));
	TOKEN_MANDATORY_LABEL label = { { medium, SE_GROUP_INTEGRITY } };
	assert(SetTokenInformation(restricted, TokenIntegrityLevel, &label, sizeof(label) + GetLengthSid(medium)));
	assert(DuplicateTokenEx(restricted, TOKEN_ALL_ACCESS, nullptr, SecurityImpersonation, TokenImpersonation, &token));
	LocalFree(medium);
	CloseHandle(restricted);
	CloseHandle(primary);
	return token;
}

int wmain(int argc, wchar_t** argv)
{
	if (argc == 4)
	{
		// Deliberately forge the same payload using a non-admin, medium-integrity
		// pipe-client thread, even though Begin launched this process with runas.
		HANDLE token = RestrictedToken();
		assert(!CafeSecurity::IsElevatedAdministrator(token));
		assert(SetThreadToken(nullptr, token));
		HANDLE pipe = CreateFileW(argv[1], GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
			SECURITY_SQOS_PRESENT | SECURITY_IDENTIFICATION, nullptr);
		assert(pipe != INVALID_HANDLE_VALUE);
		DWORD message = 1, size = 0;
		assert(WriteFile(pipe, &message, sizeof(message), &size, nullptr));
		Sleep(2000);
		CloseHandle(pipe);
		assert(RevertToSelf());
		CloseHandle(token);
		return 0;
	}
	assert(argc == 2);
	CafeSecurity::Authorization auth;
	assert(!auth.IsAuthorized());
	assert(!auth.Poll()); // Fake notifications have no authorization state to consume.
	assert(!auth.Begin(nullptr, L"C:\\NoSuchCafeHelper\\missing.exe"));
	assert(!auth.IsAuthorized());
	auto wait = [&]() {
		for (int i = 0; i < 150 && auth.IsPending(); ++i) { auth.Poll(); Sleep(100); }
		assert(!auth.IsPending());
	};
	WCHAR self[32768];
	assert(GetModuleFileNameW(nullptr, self, 32768));
	assert(auth.Begin(nullptr, self));
	wait();
	assert(!auth.IsAuthorized());
	puts("PASS: forged payload with non-elevated pipe token rejected");
	assert(auth.Begin(nullptr, argv[1]));
	wait();
	assert(auth.IsAuthorized());
	puts("PASS: real elevated maintenance helper authorizes exact live server");
	auth.Lock();
	assert(!auth.IsAuthorized());
	assert(auth.Begin(nullptr, argv[1]));
	auth.Lock(); // Invalidate an in-flight, otherwise valid helper response.
	wait();
	assert(!auth.IsAuthorized());
	assert(!auth.Poll());
	assert(auth.Begin(nullptr, argv[1]));
	wait();
	assert(auth.IsAuthorized());
	auth.Lock();
	CafeSecurity::Authorization restarted;
	assert(!restarted.IsAuthorized());
	puts("PASS: immediate relock, late response rejected, repeat authorization, fresh instance locked");
	return 0;
}
