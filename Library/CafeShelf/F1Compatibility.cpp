#include "Json.h"
#include "F1Compatibility.h"

#include <windows.h>
#include <bcrypt.h>

#include <algorithm>
#include <cwchar>
#include <stdexcept>
#include <string>
#include <vector>

namespace CafeShelf { namespace F1
{
#ifdef CAFE_F1_TESTING
std::function<void(const wchar_t*, const std::wstring&)> testHook;
void SetTestHook(const std::function<void(const wchar_t*, const std::wstring&)>& hook) { testHook = hook; }
#endif
namespace
{
using Json = CafeShelfJson::json;
constexpr size_t MaxTargetBytes = 4 * 1024 * 1024;
constexpr wchar_t ShelfRootName[] = L"Shelf Suite";
constexpr char UpstreamRevision[] = "d4f186ba0b5c262c7559b80841132f5fd3884f3c";
constexpr char F1Revision[] = "c6ce82ab7c2e9f901b01c712a0efd22bdee6d5c9";
constexpr char PatchHash[] = "6c4913af57f1b8542d287c72b08e3ee840275cae51b7f7a96df40707891956e9";
constexpr wchar_t ManifestHash[] = L"1b4869f15aa72ce9febc31ccf3b8df10f0cbbf91e8b7f82755386526affe0aed";

struct CatalogEntry
{
	std::wstring path;
	std::wstring inputHash;
	std::wstring outputHash;
	bool shared = false;
};

struct Handle
{
	HANDLE value = INVALID_HANDLE_VALUE;
	explicit Handle(HANDLE handle) : value(handle) {}
	Handle(const Handle&) = delete;
	Handle& operator=(const Handle&) = delete;
	Handle(Handle&& other) noexcept : value(other.value) { other.value = INVALID_HANDLE_VALUE; }
	~Handle() { if (value != INVALID_HANDLE_VALUE) CloseHandle(value); }
};
using Pins = std::vector<Handle>;

std::wstring ToWide(const std::string& value)
{
	return std::wstring(value.begin(), value.end());
}

Result<Preview> Refuse(const std::wstring& message)
{
	return { false, {}, Error::Unsupported, message };
}

bool IsShelfName(const std::wstring& name)
{
	return name.size() > 5 && name.compare(0, 5, L"Shelf") == 0 && name[5] != L'0' &&
		std::all_of(name.begin() + 5, name.end(), [](wchar_t ch) { return ch >= L'0' && ch <= L'9'; });
}

std::wstring JoinPath(const std::wstring& left, const std::wstring& right)
{
	return left + (left.empty() || left.back() == L'\\' ? L"" : L"\\") + right;
}

std::wstring FullPath(const std::wstring& path)
{
	std::wstring ordinary = path;
	std::replace(ordinary.begin(), ordinary.end(), L'/', L'\\');
	while (ordinary.size() > 3 && ordinary.back() == L'\\') ordinary.pop_back();
	if (ordinary.size() <= 3 || ordinary.size() >= 30000 || ordinary[1] != L':' || ordinary[2] != L'\\' ||
		!((ordinary[0] >= L'A' && ordinary[0] <= L'Z') || (ordinary[0] >= L'a' && ordinary[0] <= L'z')) ||
		ordinary.find_first_of(L"?*\r\n\"<>|") != std::wstring::npos || ordinary.find(L'\0') != std::wstring::npos ||
		ordinary.find(L':', 2) != std::wstring::npos) throw std::runtime_error("not an ordinary local path");
	for (size_t start = 3; start < ordinary.size();)
	{
		const auto end = ordinary.find(L'\\', start);
		const auto component = ordinary.substr(start, end == std::wstring::npos ? ordinary.size() - start : end - start);
		if (component.empty() || component == L"." || component == L".." || component.back() == L'.' || component.back() == L' ')
			throw std::runtime_error("ambiguous path");
		start = end == std::wstring::npos ? ordinary.size() : end + 1;
	}
	const DWORD length = GetFullPathNameW(ordinary.c_str(), 0, nullptr, nullptr);
	if (!length || length >= 30000) throw std::runtime_error("invalid path");
	std::wstring result(length, L'\0');
	if (!GetFullPathNameW(ordinary.c_str(), length, &result[0], nullptr)) throw std::runtime_error("invalid path");
	result.resize(wcslen(result.c_str()));
	return result;
}

void PinDirectories(const std::wstring& path, Pins& pins)
{
	const auto full = FullPath(path);
	const auto drive = full.size() >= 3 ? full.substr(0, 3) : std::wstring();
	if (drive.empty() || GetDriveTypeW(drive.c_str()) != DRIVE_FIXED) throw std::runtime_error("not a local fixed disk");
	for (size_t end = 2; end < full.size();)
	{
	end = full.find(L'\\', end + 1);
	const auto prefix = end == std::wstring::npos ? full : full.substr(0, end);
	Handle handle(CreateFileW(prefix.c_str(), FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE,
		nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
	if (handle.value == INVALID_HANDLE_VALUE) throw std::runtime_error("directory unavailable");
	BY_HANDLE_FILE_INFORMATION info{};
	const bool valid = GetFileInformationByHandle(handle.value, &info) &&
		(info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0 &&
		(info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0;
	if (!valid) throw std::runtime_error("directory redirected");
	ULONG flags = 0;
	if (GetFileInformationByHandleEx(handle.value, static_cast<FILE_INFO_BY_HANDLE_CLASS>(23), &flags, sizeof(flags)) && (flags & 1))
		throw std::runtime_error("case-sensitive directory");
	pins.push_back(std::move(handle));
	if (end == std::wstring::npos) break;
	}
}

std::vector<unsigned char> ReadTarget(const std::wstring& path)
{
	HANDLE handle = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
		FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
	if (handle == INVALID_HANDLE_VALUE) throw std::runtime_error("missing, busy, or unreadable");
	BY_HANDLE_FILE_INFORMATION info{};
	const bool valid = GetFileInformationByHandle(handle, &info) &&
		(info.dwFileAttributes & (FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DIRECTORY)) == 0 &&
		info.nNumberOfLinks == 1 && info.nFileSizeHigh == 0 && info.nFileSizeLow <= MaxTargetBytes;
	if (!valid)
	{
		CloseHandle(handle);
		throw std::runtime_error("redirected, linked, or oversized");
	}
	std::vector<unsigned char> bytes(info.nFileSizeLow);
	DWORD read = 0;
	const bool complete = ReadFile(handle, bytes.empty() ? nullptr : bytes.data(), static_cast<DWORD>(bytes.size()), &read, nullptr) && read == bytes.size();
	CloseHandle(handle);
	if (!complete) throw std::runtime_error("changed while being read");
	return bytes;
}

std::wstring Sha256(const std::vector<unsigned char>& bytes)
{
	BCRYPT_ALG_HANDLE algorithm = nullptr;
	BCRYPT_HASH_HANDLE hash = nullptr;
	DWORD objectSize = 0;
	DWORD received = 0;
	if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0 ||
		BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&objectSize), sizeof(objectSize), &received, 0) < 0)
	{
		if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0);
		throw std::runtime_error("SHA-256 unavailable");
	}
	std::vector<unsigned char> object(objectSize);
	std::vector<unsigned char> digest(32);
	const bool complete = BCryptCreateHash(algorithm, &hash, object.data(), objectSize, nullptr, 0, 0) >= 0 &&
		BCryptHashData(hash, bytes.empty() ? nullptr : const_cast<PUCHAR>(bytes.data()), static_cast<ULONG>(bytes.size()), 0) >= 0 &&
		BCryptFinishHash(hash, digest.data(), static_cast<ULONG>(digest.size()), 0) >= 0;
	if (hash) BCryptDestroyHash(hash);
	BCryptCloseAlgorithmProvider(algorithm, 0);
	if (!complete) throw std::runtime_error("SHA-256 failed");
	static constexpr wchar_t Digits[] = L"0123456789abcdef";
	std::wstring value;
	value.reserve(64);
	for (const auto byte : digest)
	{
		value.push_back(Digits[byte >> 4]);
		value.push_back(Digits[byte & 15]);
	}
	return value;
}

bool IsExactPayloadPath(const std::string& path)
{
	return path == "payload/@Resources/ShelfEngine.lua" || path == "payload/@Resources/Variables.inc";
}

std::vector<CatalogEntry> ReadCatalog(const std::wstring& payload)
{
	Pins pins;
	PinDirectories(JoinPath(payload, L"payload\\@Resources"), pins);
	const auto manifestBytes = ReadTarget(JoinPath(payload, L"manifest.json"));
	if (Sha256(manifestBytes) != ManifestHash) throw std::runtime_error("unapproved compatibility manifest");
	const std::string manifestText(manifestBytes.begin(), manifestBytes.end());
	const auto manifest = Json::parse(manifestText);
	if (manifest.value("SchemaVersion", 0) != 1 || manifest.value("UpstreamRevision", "") != UpstreamRevision ||
		manifest.value("F1Revision", "") != F1Revision || manifest.value("PatchSha256", "") != PatchHash)
	{
		throw std::runtime_error("unexpected manifest provenance");
	}
	const auto& files = manifest.at("Files");
	if (!files.is_array() || files.size() != 2) throw std::runtime_error("unexpected payload list");
	std::vector<std::string> filePaths;
	for (const auto& item : files)
	{
		const auto path = item.at("Path").get<std::string>();
		const auto expected = ToWide(item.at("Hash").get<std::string>());
		if (!IsExactPayloadPath(path) || std::find(filePaths.begin(), filePaths.end(), path) != filePaths.end() ||
			Sha256(ReadTarget(JoinPath(payload, ToWide(path)))) != expected)
		{
			throw std::runtime_error("untrusted payload file");
		}
		filePaths.push_back(path);
	}
	std::sort(filePaths.begin(), filePaths.end());
	if (filePaths != std::vector<std::string>{ "payload/@Resources/ShelfEngine.lua", "payload/@Resources/Variables.inc" })
	{
		throw std::runtime_error("incomplete payload list");
	}
	const auto& recognition = manifest.at("Recognition");
	if (!recognition.is_array() || recognition.size() != 72) throw std::runtime_error("incomplete recognition catalog");
	std::vector<CatalogEntry> entries;
	for (const auto& item : recognition)
	{
		const auto kind = item.at("Kind").get<std::string>();
		CatalogEntry entry;
		entry.shared = kind == "Shared";
		if (!entry.shared && kind != "Ini") throw std::runtime_error("unknown recognition entry");
		entry.inputHash = ToWide(item.at("InputHash").get<std::string>());
		entry.outputHash = ToWide(item.at("OutputHash").get<std::string>());
		if (entry.inputHash.size() != 64 || entry.outputHash.size() != 64) throw std::runtime_error("invalid catalog hash");
		if (entry.shared)
		{
			entry.path = ToWide(item.at("Path").get<std::string>());
			const auto source = item.at("Payload").get<std::string>();
            if ((entry.path != L"@Resources/ShelfEngine.lua" && entry.path != L"@Resources/Variables.inc") || source != "payload/" + item.at("Path").get<std::string>())
			{
				throw std::runtime_error("unapproved shared catalog path");
			}
		}
		entries.push_back(std::move(entry));
	}
	return entries;
}
}

Result<Preview> Inspect(const std::wstring& skinPath, const std::wstring& payloadPath)
{
	std::wstring inspecting = L"Shelf Suite\\@Resources";
	try
	{
		const std::wstring skins = FullPath(skinPath);
		const std::wstring root = JoinPath(skins, ShelfRootName);
		Pins pins;
		PinDirectories(JoinPath(root, L"@Resources"), pins);
		inspecting = L"Compatibility\\ShelfSuiteF1";
		const auto catalog = ReadCatalog(FullPath(payloadPath));
		inspecting = L"Shelf Suite";
		std::vector<std::wstring> targets{ L"@Resources\\ShelfEngine.lua", L"@Resources\\Variables.inc" };
		size_t shelves = 0;
		WIN32_FIND_DATAW child{};
		HANDLE find = FindFirstFileW(JoinPath(root, L"Shelf*").c_str(), &child);
		struct FindGuard { HANDLE handle; ~FindGuard() { if (handle != INVALID_HANDLE_VALUE) FindClose(handle); } } findGuard{ find };
		if (find != INVALID_HANDLE_VALUE)
		{
			do
			{
				const std::wstring name(child.cFileName);
				if (!IsShelfName(name)) continue;
				inspecting = name;
				PinDirectories(JoinPath(root, name), pins);
				targets.push_back(name + L"\\Shelf.ini");
				++shelves;
			} while (FindNextFileW(find, &child));
			if (GetLastError() != ERROR_NO_MORE_FILES) throw std::runtime_error("shelf enumeration failed");
		}
		if (!shelves) return Refuse(L"No Shelf<number> folders were found. No files were changed.");
		Preview preview;
		preview.status = Status::Preview;
		preview.shelfRoot = root;
		preview.proposedBackup = JoinPath(skins, L"Shelf Suite-F1-Backup-after-confirmation");
		for (const auto& relative : targets)
		{
			inspecting = relative;
			const auto hash = Sha256(ReadTarget(JoinPath(root, relative)));
			const bool shared = relative.rfind(L"@Resources\\", 0) == 0;
			std::vector<const CatalogEntry*> matches;
			for (const auto& entry : catalog)
			{
				auto expectedPath = relative; std::replace(expectedPath.begin(), expectedPath.end(), L'\\', L'/');
				if (entry.shared == shared && entry.inputHash == hash && (!shared || entry.path == expectedPath)) matches.push_back(&entry);
			}
			if (matches.size() != 1) return Refuse(relative + L" is missing, busy, unreadable, or not an approved ShelfSuite v2.1/F1 file. No files were changed.");
			const Change change{ relative, hash, matches.front()->outputHash };
			if (hash == change.outputHash) preview.compatible.push_back(change);
			else preview.changes.push_back(change);
		}
		if (preview.changes.empty())
		{
			preview.status = Status::AlreadyCompatible;
			preview.message = L"Already compatible. No files need changing and no backup will be created.";
		}
		return { true, std::move(preview), Error::None, {} };
	}
	catch (const std::exception& error)
	{
		return Refuse(inspecting + L": " + ToWide(error.what()) + L". No files were changed.");
	}
}

Result<ApplyResult> Apply(const Preview&, const std::function<bool()>&)
{
	return { false, {}, Error::Unsupported, L"ShelfSuite F1 application is not available yet." };
}
}
}
