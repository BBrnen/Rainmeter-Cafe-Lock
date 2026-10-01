#include "../Library/CafeShelf/F1Compatibility.h"

#include <cassert>
#include <cwchar>
#include <iostream>
#include <windows.h>
#include <fstream>
#include <iterator>
#include <stdexcept>

#ifdef CAFE_F1_TESTING
namespace CafeShelf { namespace F1 {
void SetTestHook(const std::function<void(const wchar_t*, const std::wstring&)>& hook);
} }
namespace
{
std::string Bytes(const std::wstring& path)
{
	std::ifstream file(path, std::ios::binary);
	assert(file.good());
	return { std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>() };
}
void ApplyCase(const std::wstring& skins, const std::wstring& bundle, const std::wstring& scenario)
{
	using namespace CafeShelf::F1;
	const auto inspected = Inspect(skins, bundle);
	assert(inspected.ok);
	const auto preview = inspected.value;
	std::vector<std::string> originals;
	for (const auto& change : preview.changes) originals.push_back(Bytes(preview.shelfRoot + L"\\" + change.relativePath));
	size_t replacements = 0;
	std::wstring backup;
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
	const auto applied = Apply(preview, [&]() { return scenario != L"apply-denied"; });
	SetTestHook({});
	if (scenario == L"apply-denied") assert(!applied.ok && applied.code == CafeShelf::Error::Locked);
	else if (scenario == L"apply-noop")
	{
		assert(applied.ok && applied.value.status == Status::AlreadyCompatible);
		assert(applied.value.backup.empty() && backup.empty());
	}
	else if (scenario == L"apply-backup-failure") assert(!applied.ok);
	else if (scenario == L"apply-recover") assert(applied.value.status == Status::FailedRecovered);
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
		const auto record = Bytes(backup + L"\\RESTORE.txt");
		assert(record.find("config.lua") == std::string::npos && record.find("sentinel") == std::string::npos);
	}
	if (scenario == L"apply-denied" || scenario == L"apply-backup-failure" || scenario == L"apply-recover")
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
