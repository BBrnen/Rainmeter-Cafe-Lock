#include "../Library/CafeShelf/F1Compatibility.h"

#include <cassert>
#include <cwchar>
#include <iostream>

int wmain(int argc, wchar_t** argv)
{
	assert(argc == 6);
	const auto result = CafeShelf::F1::Inspect(argv[1], argv[2]);
	const std::wstring expected = argv[3];
	if (expected == L"refused")
	{
		assert(!result.ok);
		assert(result.value.status == CafeShelf::F1::Status::Refused);
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