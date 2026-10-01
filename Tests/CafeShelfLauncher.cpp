#include "../Library/CafeShelf/Launcher.h"
#include <windows.h>
#include <objbase.h>
#include <iostream>
using namespace CafeShelf;
namespace {
int checks = 0, failures = 0;
void Check(const char* name, bool passed) {
	++checks; if (!passed) ++failures;
	std::cout << (passed ? "PASS " : "FAIL ") << name << '\n';
}
}
int wmain(int argc, wchar_t** argv) {
	if (argc != 2 || FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED))) return 2;
	const std::wstring root = argv[1];
	auto inspect = [&](const wchar_t* name, bool directory = false) {
		return InspectLauncher({root + L"\\" + name, directory});
	};
	const auto shortcut = inspect(L"Original shortcut.lnk");
	Check("shortcut display name detected", shortcut.ok && shortcut.value.name == L"Original shortcut");
	Check("shortcut action retains original link", shortcut.ok && shortcut.value.action == root + L"\\Original shortcut.lnk");
	Check("shortcut remains shell icon source", shortcut.ok && shortcut.value.iconSource == shortcut.value.action);
	const auto broken = inspect(L"Unavailable target.lnk");
	Check("unavailable target never retargets shortcut", broken.ok && broken.value.action == root + L"\\Unavailable target.lnk");
	const auto exe = inspect(L"Versioned application.exe");
	Check("executable description preferred to filename", exe.ok && exe.value.name == L"Cafe Shelf fixture application");
	Check("executable action preserved", exe.ok && exe.value.action == root + L"\\Versioned application.exe");
	const auto fallback = inspect(L"Without version.exe");
	Check("unversioned executable uses filename stem", fallback.ok && fallback.value.name == L"Without version");
	const auto folder = inspect(L"Folder with spaces", true);
	Check("folder display name detected", folder.ok && folder.value.name == L"Folder with spaces");
	Check("folder action preserved", folder.ok && folder.value.action == root + L"\\Folder with spaces");
	const auto file = inspect(L"Report.txt");
	Check("associated document retains its original path", file.ok && file.value.action == root + L"\\Report.txt" && !file.value.name.empty());
	const auto unicode = inspect(L"Caf\u00e9 \u65e5\u672c.txt");
	Check("unicode path preserved exactly", unicode.ok && unicode.value.action == root + L"\\Caf\u00e9 \u65e5\u672c.txt");
	for (const auto name : {L"Bracket[1].txt", L"Hash#name.txt", L"Percent%PATH%.txt"}) {
		const auto result = inspect(name);
		Check("ambiguous action punctuation fails clearly", !result.ok && result.code == Error::Unsupported && !result.message.empty());
	}
	Check("missing selection rejected", !inspect(L"Missing.txt").ok);
	Check("empty selection rejected", !InspectLauncher({L"", false}).ok);
	Check("relative selection rejected", !InspectLauncher({L"Report.txt", false}).ok);
	Check("changed file kind rejected", !inspect(L"Report.txt", true).ok);
	Check("device path rejected", !InspectLauncher({L"\\\\.\\NUL", false}).ok);
	CoUninitialize();
	std::cout << checks << " launcher metadata checks, " << failures << " failures\n";
	return failures ? 1 : 0;
}
