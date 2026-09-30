// The Windows 7 shell helpers require IE7 declarations in the SDK.
// Keep this local; upstream Rainmeter's build target is unchanged.
#if defined(_WIN32_IE) && _WIN32_IE < 0x0700
#undef _WIN32_IE
#define _WIN32_IE 0x0700
#endif
#include "Launcher.h"
#include <windows.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <wrl.h>
#include <vector>
#include <algorithm>

namespace CafeShelf
{
namespace
{
Result<LauncherDraft> Fail(Error code, const wchar_t* message)
{
	Result<LauncherDraft> result;
	result.code = code; result.message = message;
	return result;
}
bool Absolute(const std::wstring& path)
{
	const bool drive = path.size() >= 3 &&
		((path[0] >= L'A' && path[0] <= L'Z') || (path[0] >= L'a' && path[0] <= L'z')) &&
		path[1] == L':' && path[2] == L'\\';
	const bool unc = path.size() >= 5 && path[0] == L'\\' && path[1] == L'\\' &&
		path[2] != L'?' && path[2] != L'.' && path[2] != L'\\' &&
		path.find(L'\\', 2) != std::wstring::npos;
	return (drive || unc) && path.find(L':', drive ? 2 : 0) == std::wstring::npos;
}
std::wstring Trim(std::wstring value)
{
	const auto begin = value.find_first_not_of(L" \t\r\n");
	if (begin == std::wstring::npos) return {};
	return value.substr(begin, value.find_last_not_of(L" \t\r\n") - begin + 1);
}
std::wstring Leaf(const std::wstring& path)
{
	auto end = path.find_last_not_of(L"\\/");
	if (end == std::wstring::npos) return {};
	auto begin = path.find_last_of(L"\\/", end);
	return path.substr(begin == std::wstring::npos ? 0 : begin + 1,
		end - (begin == std::wstring::npos ? 0 : begin + 1) + 1);
}
std::wstring DisplayName(const std::wstring& path)
{
	Microsoft::WRL::ComPtr<IShellItem> item;
	if (FAILED(SHCreateItemFromParsingName(path.c_str(), nullptr, IID_PPV_ARGS(&item)))) return {};
	LPWSTR raw = nullptr;
	const HRESULT hr = item->GetDisplayName(SIGDN_NORMALDISPLAY, &raw);
	std::wstring result;
	if (SUCCEEDED(hr) && raw)
	{
		const auto size = wcsnlen_s(raw, MaxStringUnits + 1);
		if (size <= MaxStringUnits) result.assign(raw, size);
	}
	CoTaskMemFree(raw);
	return result;
}
std::wstring Description(const std::wstring& path)
{
	const DWORD flags = FILE_VER_GET_NEUTRAL;
	const DWORD size = GetFileVersionInfoSizeExW(flags, path.c_str(), nullptr);
	if (!size || size > 4 * 1024 * 1024) return {};
	std::vector<BYTE> bytes(size);
	if (!GetFileVersionInfoExW(flags, path.c_str(), 0, size, bytes.data())) return {};
	struct Translation { WORD language, codepage; };
	Translation* translations = nullptr;
	UINT length = 0;
	if (!VerQueryValueW(bytes.data(), L"\\VarFileInfo\\Translation",
		reinterpret_cast<void**>(&translations), &length) || !translations) return {};
	const auto first = reinterpret_cast<ULONG_PTR>(bytes.data());
	const auto last = first + bytes.size();
	const auto address = reinterpret_cast<ULONG_PTR>(translations);
	if (address < first || address > last || length > last - address) return {};
	const auto count = (std::min)(static_cast<size_t>(length / sizeof(Translation)), size_t(64));
	for (size_t i = 0; i < count; ++i)
	{
		wchar_t query[80] = {};
		swprintf_s(query, L"\\StringFileInfo\\%04x%04x\\FileDescription",
			static_cast<unsigned>(translations[i].language), static_cast<unsigned>(translations[i].codepage));
		LPWSTR value = nullptr;
		UINT units = 0;
		if (!VerQueryValueW(bytes.data(), query, reinterpret_cast<void**>(&value), &units) ||
			!value || !units || units > MaxStringUnits) continue;
		const auto textAddress = reinterpret_cast<ULONG_PTR>(value);
		if (textAddress < first || textAddress > last || units > (last - textAddress) / sizeof(wchar_t)) continue;
		const auto used = wcsnlen_s(value, units);
		if (used == units) continue;
		auto name = Trim(std::wstring(value, used));
		if (!name.empty()) return name;
	}
	return {};
}
}

Result<LauncherDraft> InspectLauncher(const SelectedFile& selection)
{
	try
	{
		const auto& path = selection.path;
		if (path.empty() || path.size() > MaxStringUnits || path.find(L'\0') != std::wstring::npos || !Absolute(path))
			return Fail(Error::InvalidInput, L"Choose an absolute filesystem file or folder.");
		if (path.find_first_of(L"#%[]\"\r\n") != std::wstring::npos)
			return Fail(Error::Unsupported, L"This path contains characters Rainmeter may interpret as an action. Choose a Windows shortcut with a name and location without #, %, brackets or quotes.");
		if (!WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, path.data(), static_cast<int>(path.size()),
			nullptr, 0, nullptr, nullptr))
			return Fail(Error::Unsupported, L"The selected path cannot be represented safely as Unicode.");
		const auto attributes = GetFileAttributesW(path.c_str());
		if (attributes == INVALID_FILE_ATTRIBUTES)
			return Fail(Error::NotFound, L"The selected file or folder is no longer available.");
		if (((attributes & FILE_ATTRIBUTE_DIRECTORY) != 0) != selection.directory)
			return Fail(Error::Conflict, L"The selected item changed. Choose it again.");
		auto leaf = Leaf(path);
		const auto dot = leaf.find_last_of(L'.');
		const auto extension = dot == std::wstring::npos ? std::wstring() : leaf.substr(dot);
		const bool shortcut = !selection.directory && _wcsicmp(extension.c_str(), L".lnk") == 0;
		const bool executable = !selection.directory && _wcsicmp(extension.c_str(), L".exe") == 0;
		std::wstring name = executable ? Description(path) : DisplayName(path);
		if (name.empty()) name = leaf;
		if (shortcut || (executable && name == leaf))
		{
			const auto nameDot = name.find_last_of(L'.');
			if (nameDot != std::wstring::npos && _wcsicmp(name.c_str() + nameDot, extension.c_str()) == 0)
				name.resize(nameDot);
		}
		if (name.empty()) return Fail(Error::Unsupported, L"Windows could not determine an item name.");
		Result<LauncherDraft> result;
		result.ok = true; result.code = Error::None;
		// Do not resolve, repair, expand, copy or execute shortcuts. The shell
		// receives the original .lnk on an ordinary launcher click.
		result.value = {name, path, path};
		return result;
	}
	catch (...)
	{
		return Fail(Error::IoError, L"Windows could not inspect the selected item.");
	}
}
}
