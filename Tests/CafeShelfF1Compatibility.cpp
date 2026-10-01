#include "../Library/CafeShelf/F1Compatibility.h"

#include <cassert>
#include <cwchar>
#include <iostream>
#include <windows.h>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <aclapi.h>

#ifdef CAFE_F1_TESTING
namespace CafeShelf { namespace F1 {
void SetTestHook(const std::function<void(const wchar_t*, const std::wstring&)>& hook);
} }
namespace
{
std::string Bytes(const std::wstring& path)
{
	HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, 0, nullptr);
	assert(file != INVALID_HANDLE_VALUE);
	LARGE_INTEGER size{}; assert(GetFileSizeEx(file, &size) && size.HighPart == 0);
	std::string bytes(size.LowPart, '\0'); DWORD read = 0;
	assert(ReadFile(file, bytes.empty() ? nullptr : &bytes[0], size.LowPart, &read, nullptr) && read == size.LowPart);
	CloseHandle(file); return bytes;
}
std::string Permissions(const std::wstring& path)
{
	HANDLE file = CreateFileW(path.c_str(), READ_CONTROL, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, 0, nullptr);
	assert(file != INVALID_HANDLE_VALUE);
	PSECURITY_DESCRIPTOR descriptor = nullptr; PACL acl = nullptr;
	assert(GetSecurityInfo(file, SE_FILE_OBJECT, DACL_SECURITY_INFORMATION, nullptr, nullptr, &acl, nullptr, &descriptor) == ERROR_SUCCESS);
	SECURITY_DESCRIPTOR_CONTROL control = 0; DWORD revision = 0;
	assert(GetSecurityDescriptorControl(descriptor, &control, &revision));
	assert(acl);
	std::string bytes(reinterpret_cast<const char*>(acl), acl->AclSize);
	bytes += (control & SE_DACL_PROTECTED) ? 'P' : 'I';
	LocalFree(descriptor); CloseHandle(file); return bytes;
}
void ApplyCase(const std::wstring& skins, const std::wstring& bundle, const std::wstring& scenario)
{
	using namespace CafeShelf::F1;
	if (scenario == L"apply-standard")
	{
		BYTE admin[SECURITY_MAX_SID_SIZE]{}; DWORD size = sizeof(admin); BOOL member = TRUE;
		assert(CreateWellKnownSid(WinBuiltinAdministratorsSid, nullptr, admin, &size));
		assert(CheckTokenMembership(nullptr, admin, &member) && !member);
		HANDLE token = nullptr; assert(OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token));
		DWORD length = 0; GetTokenInformation(token, TokenIntegrityLevel, nullptr, 0, &length);
		std::vector<BYTE> data(length);
		assert(GetTokenInformation(token, TokenIntegrityLevel, data.data(), length, &length));
		const auto label = reinterpret_cast<TOKEN_MANDATORY_LABEL*>(data.data());
		const auto sid = label->Label.Sid;
		assert(*GetSidSubAuthority(sid, *GetSidSubAuthorityCount(sid) - 1) == SECURITY_MANDATORY_MEDIUM_RID);
		CloseHandle(token);
	}
	const auto inspected = Inspect(skins, bundle);
	assert(inspected.ok);
	const auto preview = inspected.value;
	std::vector<std::string> originals;
	std::vector<std::string> permissions;
	std::vector<DWORD> attributes;
	for (const auto& change : preview.changes) originals.push_back(Bytes(preview.shelfRoot + L"\\" + change.relativePath));
	if (!preview.changes.empty()) assert(SetFileAttributesW((preview.shelfRoot + L"\\" + preview.changes.front().relativePath).c_str(), FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_ARCHIVE));
	for (const auto& change : preview.changes)
	{
		permissions.push_back(Permissions(preview.shelfRoot + L"\\" + change.relativePath));
		attributes.push_back(GetFileAttributesW((preview.shelfRoot + L"\\" + change.relativePath).c_str()));
	}
	size_t replacements = 0;
	std::wstring backup;
	bool authorized = scenario != L"apply-denied";
	bool gap = false;
	HANDLE busy = INVALID_HANDLE_VALUE;
	if (scenario == L"apply-busy")
	{
		busy = CreateFileW((preview.shelfRoot + L"\\" + preview.changes.front().relativePath).c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING, 0, nullptr);
		assert(busy != INVALID_HANDLE_VALUE);
	}
	SetTestHook([&](const wchar_t* phase, const std::wstring& path)
	{
		const std::wstring step(phase);
		if (step == L"backup-ready")
		{
			backup = path;
			for (size_t i = 0; i < preview.changes.size(); ++i)
				assert(Bytes(backup + L"\\" + preview.changes[i].relativePath) == originals[i]);
			assert(GetFileAttributesW((backup + L"\\Shelf1\\config.lua").c_str()) == INVALID_FILE_ATTRIBUTES);
		}
		if (step == L"before-backup" && scenario == L"apply-backup-failure") throw std::runtime_error("injected backup failure");
		if (step == L"original-moved" && replacements == 0)
		{
			gap = true;
			assert(!backup.empty());
			const auto phaseRecord = Bytes(backup + L"\\PHASE.txt");
			assert(phaseRecord.find("MOVE ORIGINAL") != std::string::npos);
			assert(Bytes(backup + L"\\Holding\\" + preview.changes.front().relativePath) == originals.front());
			assert(GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES);
			if (scenario == L"apply-crash-gap") ExitProcess(73);
			if (scenario == L"apply-revoke-gap") authorized = false;
			if (scenario == L"apply-abort-gap") throw std::runtime_error("interrupted original move");
			if (scenario == L"apply-compete-gap")
			{
				std::ofstream competitor(path, std::ios::binary); competitor << "competing file";
			}
		}
		if (step == L"before-replace" && replacements == 1 && (scenario == L"apply-recover" || scenario == L"apply-outside"))
		{
			if (scenario == L"apply-outside")
			{
				std::ofstream edited(preview.shelfRoot + L"\\" + preview.changes.front().relativePath, std::ios::binary | std::ios::trunc);
				edited << "outside edit";
			}
			throw std::runtime_error("injected later replacement failure");
		}
		if (step == L"after-replace") ++replacements;
	});
	const auto applied = Apply(preview, [&]() { return authorized; });
	if (busy != INVALID_HANDLE_VALUE) CloseHandle(busy);
	if (!applied.ok) std::wcerr << L"Application outcome: " << applied.message << L"; " << applied.value.message << std::endl;
	SetTestHook({});
	if (scenario == L"apply-denied") assert(!applied.ok && applied.code == CafeShelf::Error::Locked);
	else if (scenario == L"apply-noop")
	{
		assert(applied.ok && applied.value.status == Status::AlreadyCompatible);
		assert(applied.value.backup.empty() && backup.empty());
	}
	else if (scenario == L"apply-backup-failure") assert(!applied.ok);
	else if (scenario == L"apply-busy") assert(!applied.ok && applied.value.backup.empty());
	else if (scenario == L"apply-recover") assert(applied.value.status == Status::FailedRecovered);
	else if (scenario == L"apply-abort-gap") assert(gap && applied.value.status == Status::FailedRecovered);
	else if (scenario == L"apply-revoke-gap")
	{
		assert(gap && applied.value.status == Status::ManualRecoveryRequired);
		assert(GetFileAttributesW((preview.shelfRoot + L"\\" + preview.changes.front().relativePath).c_str()) == INVALID_FILE_ATTRIBUTES);
		assert(Bytes(backup + L"\\Holding\\" + preview.changes.front().relativePath) == originals.front());
	}
	else if (scenario == L"apply-compete-gap")
	{
		assert(gap && applied.value.status == Status::ManualRecoveryRequired);
		assert(Bytes(preview.shelfRoot + L"\\" + preview.changes.front().relativePath) == "competing file");
		assert(Bytes(backup + L"\\Holding\\" + preview.changes.front().relativePath) == originals.front());
	}
	else if (scenario == L"apply-outside")
	{
		assert(applied.value.status == Status::ManualRecoveryRequired);
		assert(Bytes(preview.shelfRoot + L"\\" + preview.changes.front().relativePath) == "outside edit");
		assert(!applied.value.manualRecovery.empty());
	}
	else
	{
		assert(applied.ok && applied.value.status == Status::Updated);
		assert(applied.value.changed.size() == preview.changes.size());
		const auto after = Inspect(skins, bundle);
		assert(after.ok && after.value.status == Status::AlreadyCompatible);
		assert(!backup.empty());
		for (size_t i = 0; i < preview.changes.size(); ++i)
		{
			assert(Permissions(preview.shelfRoot + L"\\" + preview.changes[i].relativePath) == permissions[i]);
			assert(GetFileAttributesW((preview.shelfRoot + L"\\" + preview.changes[i].relativePath).c_str()) == attributes[i]);
		}
		const auto record = Bytes(backup + L"\\RESTORE.txt");
		assert(record.find("config.lua") == std::string::npos && record.find("sentinel") == std::string::npos);
	}
	if (scenario == L"apply-denied" || scenario == L"apply-busy" || scenario == L"apply-backup-failure" || scenario == L"apply-recover" || scenario == L"apply-abort-gap")
		for (size_t i = 0; i < preview.changes.size(); ++i)
			assert(Bytes(preview.shelfRoot + L"\\" + preview.changes[i].relativePath) == originals[i]);
	std::wcout << L"PASS F1 " << scenario << std::endl;
}
}
#endif

int wmain(int argc, wchar_t** argv)
{
	assert(argc == 6 || argc == 7);
#ifdef CAFE_F1_TESTING
	if (std::wstring(argv[3]).find(L"apply-") == 0) { ApplyCase(argv[1], argv[2], argv[3]); return 0; }
#endif
	const auto result = CafeShelf::F1::Inspect(argv[1], argv[2]);
	if (!result.ok) std::wcerr << L"Inspection refusal: " << result.message << std::endl;
	const std::wstring expected = argv[3];
	if (expected == L"refused")
	{
		assert(!result.ok);
		assert(result.value.status == CafeShelf::F1::Status::Refused);
		if (argc == 7) assert(result.message.find(argv[6]) != std::wstring::npos);
		return 0;
	}
	assert(result.ok);
	const auto& preview = result.value;
	if (expected == L"preview") assert(preview.status == CafeShelf::F1::Status::Preview);
	else if (expected == L"already") assert(preview.status == CafeShelf::F1::Status::AlreadyCompatible);
	else assert(false);
	assert(preview.changes.size() == static_cast<size_t>(_wtoi(argv[4])));
	assert(preview.compatible.size() == static_cast<size_t>(_wtoi(argv[5])));
	std::wcout << L"PASS F1 native inspection " << expected << std::endl;
	return 0;
}
