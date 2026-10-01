#include "Json.h"
#include "F1Compatibility.h"

#include <windows.h>
#include <bcrypt.h>
#include <aclapi.h>

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
	Handle() = default;
	explicit Handle(HANDLE handle) : value(handle) {}
	Handle(const Handle&) = delete;
	Handle& operator=(const Handle&) = delete;
	Handle(Handle&& other) noexcept : value(other.value) { other.value = INVALID_HANDLE_VALUE; }
	Handle& operator=(Handle&& other) noexcept { if (this != &other) { Close(); value = other.value; other.value = INVALID_HANDLE_VALUE; } return *this; }
	void Close() { if (value != INVALID_HANDLE_VALUE) { CloseHandle(value); value = INVALID_HANDLE_VALUE; } }
	~Handle() { Close(); }
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

std::wstring Identity(const BY_HANDLE_FILE_INFORMATION& info)
{
	return std::to_wstring(info.dwVolumeSerialNumber) + L":" + std::to_wstring(info.nFileIndexHigh) + L":" + std::to_wstring(info.nFileIndexLow);
}

std::vector<unsigned char> ReadBytes(HANDLE handle, std::wstring* identity = nullptr)
{
	BY_HANDLE_FILE_INFORMATION info{};
	const bool valid = GetFileInformationByHandle(handle, &info) &&
		(info.dwFileAttributes & (FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DIRECTORY)) == 0 &&
		info.nNumberOfLinks == 1 && info.nFileSizeHigh == 0 && info.nFileSizeLow <= MaxTargetBytes;
	if (!valid)
	{
		throw std::runtime_error("redirected, linked, or oversized");
	}
	if (identity) *identity = Identity(info);
	std::vector<unsigned char> bytes(info.nFileSizeLow);
	LARGE_INTEGER start{};
	if (!SetFilePointerEx(handle, start, nullptr, FILE_BEGIN)) throw std::runtime_error("cannot read file");
	DWORD read = 0;
	const bool complete = ReadFile(handle, bytes.empty() ? nullptr : bytes.data(), static_cast<DWORD>(bytes.size()), &read, nullptr) && read == bytes.size();
	if (!complete) throw std::runtime_error("changed while being read");
	return bytes;
}

std::vector<unsigned char> ReadTarget(const std::wstring& path, std::wstring* identity = nullptr)
{
	Handle handle(CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
	if (handle.value == INVALID_HANDLE_VALUE) throw std::runtime_error("missing, busy, or unreadable");
	return ReadBytes(handle.value, identity);
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

std::wstring Nonce()
{
	BYTE bytes[16]{};
	if (BCryptGenRandom(nullptr, bytes, sizeof(bytes), BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0) throw std::runtime_error("random source unavailable");
	std::wstring result;
	for (const auto byte : bytes) { result += L"0123456789abcdef"[byte >> 4]; result += L"0123456789abcdef"[byte & 15]; }
	return result;
}
std::wstring BackupName()
{
	SYSTEMTIME time{}; GetSystemTime(&time);
	wchar_t stamp[24]{};
	swprintf_s(stamp, L"%04u%02u%02u-%02u%02u%02u-", time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond);
	return std::wstring(L"Shelf Suite-F1-Backup-") + stamp + Nonce();
}
std::wstring Parent(const std::wstring& path) { return path.substr(0, path.find_last_of(L'\\')); }
struct Failure { Error code; std::wstring message; };
void Authorize(const std::function<bool()>& authorized)
{
	if (!authorized || !authorized()) throw Failure{ Error::Locked, L"Maintenance Mode ended. No further writes or recovery moves are permitted." };
}
void Hook(const wchar_t* phase, const std::wstring& path)
{
#ifdef CAFE_F1_TESTING
	if (testHook) testHook(phase, path);
#else
	(void)phase; (void)path;
#endif
}
void Need(bool ok, const char* reason) { if (!ok) throw std::runtime_error(reason); }
std::vector<unsigned char> Utf8(const std::wstring& text)
{
	const int length = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
	Need(length > 0, "cannot encode recovery record");
	std::vector<unsigned char> result(length);
	Need(WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), reinterpret_cast<char*>(result.data()), length, nullptr, nullptr) == length, "cannot encode recovery record");
	return result;
}
void WriteFlush(HANDLE file, const std::vector<unsigned char>& bytes, const std::function<bool()>& authorized)
{
	Authorize(authorized); DWORD written = 0;
	Need(WriteFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr) && written == bytes.size() && FlushFileBuffers(file), "cannot flush recovery material");
}
Handle CreateFileNew(const std::wstring& path, const std::function<bool()>& authorized)
{
	Authorize(authorized);
	Handle file(CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE | DELETE | READ_CONTROL | WRITE_DAC,
		FILE_SHARE_READ, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr));
	Need(file.value != INVALID_HANDLE_VALUE, "cannot create new recovery or staging file"); return file;
}
void MakeDirectory(const std::wstring& path, Pins& pins, const std::function<bool()>& authorized, bool existing = false)
{
	Authorize(authorized);
	if (!CreateDirectoryW(path.c_str(), nullptr)) Need(existing && GetLastError() == ERROR_ALREADY_EXISTS, "cannot create new backup folder");
	PinDirectories(path, pins);
}
void MakeRelativeParents(const std::wstring& root, const std::wstring& relative, Pins& pins, const std::function<bool()>& authorized)
{
	for (size_t end = relative.find(L'\\'); end != std::wstring::npos; end = relative.find(L'\\', end + 1))
		MakeDirectory(JoinPath(root, relative.substr(0, end)), pins, authorized, true);
}
void PreserveMetadata(HANDLE source, HANDLE destination, const std::function<bool()>& authorized)
{
	PSECURITY_DESCRIPTOR descriptor = nullptr; PACL acl = nullptr;
	Need(GetSecurityInfo(source, SE_FILE_OBJECT, DACL_SECURITY_INFORMATION, nullptr, nullptr, &acl, nullptr, &descriptor) == ERROR_SUCCESS, "cannot read original permissions");
	struct Free { PSECURITY_DESCRIPTOR value; ~Free() { LocalFree(value); } } free{ descriptor };
	SECURITY_DESCRIPTOR_CONTROL control = 0; DWORD revision = 0;
	Need(GetSecurityDescriptorControl(descriptor, &control, &revision) && acl, "unsupported original permissions");
	Authorize(authorized);
	// New stages already inherit from the same target parent. Explicitly
	// re-enabling inheritance re-expands CREATOR OWNER for the staging owner,
	// introducing permissions absent from the verified original.
	Need(SetSecurityInfo(destination, SE_FILE_OBJECT, DACL_SECURITY_INFORMATION | ((control & SE_DACL_PROTECTED) ? PROTECTED_DACL_SECURITY_INFORMATION : 0),
		nullptr, nullptr, acl, nullptr) == ERROR_SUCCESS, "cannot preserve original permissions");
	FILE_BASIC_INFO basic{};
	Need(GetFileInformationByHandleEx(source, FileBasicInfo, &basic, sizeof(basic)) != FALSE, "cannot read original attributes");
	Authorize(authorized);
	Need(SetFileInformationByHandle(destination, FileBasicInfo, &basic, sizeof(basic)) && FlushFileBuffers(destination), "cannot preserve original attributes");
}
bool RenameBound(HANDLE file, const std::wstring& destination)
{
	const size_t length = destination.size() * sizeof(wchar_t);
	std::vector<BYTE> data(sizeof(FILE_RENAME_INFO) + length, 0);
	auto rename = reinterpret_cast<FILE_RENAME_INFO*>(data.data());
	rename->ReplaceIfExists = FALSE;
	rename->FileNameLength = static_cast<DWORD>(length);
	memcpy(rename->FileName, destination.data(), length);
	return SetFileInformationByHandle(file, FileRenameInfo, rename, static_cast<DWORD>(data.size())) != FALSE;
}
bool SameChanges(const std::vector<Change>& left, const std::vector<Change>& right)
{
	if (left.size() != right.size()) return false;
	for (size_t i = 0; i < left.size(); ++i)
		if (left[i].relativePath != right[i].relativePath || left[i].originalHash != right[i].originalHash || left[i].outputHash != right[i].outputHash || left[i].identity != right[i].identity) return false;
	return true;
}
std::vector<unsigned char> OutputBytes(const Change& change, const std::vector<unsigned char>& original, const std::wstring& payload)
{
	std::vector<unsigned char> output;
	if (change.relativePath.rfind(L"@Resources\\", 0) == 0)
	{
		output = ReadTarget(JoinPath(JoinPath(payload, L"payload"), change.relativePath));
		if (Sha256(output) != change.outputHash)
		{
			std::vector<unsigned char> crlf;
			for (const auto byte : output) { if (byte == '\n') crlf.push_back('\r'); crlf.push_back(byte); }
			output = std::move(crlf);
		}
	}
	else
	{
		std::string text(original.begin(), original.end());
		const std::string newline = text.find("\r\n") != std::string::npos ? "\r\n" : "\n";
		const std::string marker = "AccurateText=1" + newline;
		const auto position = text.find(marker);
		Need(position != std::string::npos && text.find(marker, position + marker.size()) == std::string::npos, "unrecognized INI insertion point");
		text.insert(position + marker.size(), "DynamicWindowSize=1" + newline);
		output.assign(text.begin(), text.end());
	}
	Need(Sha256(output) == change.outputHash, "staged output does not match approved hash"); return output;
}
struct Replacement
{
	Change change;
	Handle original, staged;
	std::wstring stagePath, holdingPath, outputIdentity;
	std::vector<unsigned char> bytes;
	bool moved = false, placed = false;
};
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
		preview.payloadPath = FullPath(payloadPath);
		preview.proposedBackup = JoinPath(skins, BackupName());
		std::sort(targets.begin(), targets.end());
		for (const auto& relative : targets)
		{
			inspecting = relative;
			std::wstring identity;
			const auto hash = Sha256(ReadTarget(JoinPath(root, relative), &identity));
			const bool shared = relative.rfind(L"@Resources\\", 0) == 0;
			std::vector<const CatalogEntry*> matches;
			for (const auto& entry : catalog)
			{
				auto expectedPath = relative; std::replace(expectedPath.begin(), expectedPath.end(), L'\\', L'/');
				if (entry.shared == shared && entry.inputHash == hash && (!shared || entry.path == expectedPath)) matches.push_back(&entry);
			}
			if (matches.size() != 1) return Refuse(relative + L" is missing, busy, unreadable, or not an approved ShelfSuite v2.1/F1 file. No files were changed.");
			const Change change{ relative, hash, matches.front()->outputHash, identity };
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

Result<ApplyResult> Apply(const Preview& preview, const std::function<bool()>& authorized)
{
	ApplyResult outcome;
	std::vector<Replacement> replacements;
	std::vector<Handle> backupLeases;
	Pins pins;
	Handle phase;
	std::wstring current;
	Error failureCode = Error::IoError;
	std::wstring failureMessage;
	try
	{
		Authorize(authorized);
		const auto root = FullPath(preview.shelfRoot);
		const auto skins = Parent(root);
		Need(root == JoinPath(skins, ShelfRootName), "unexpected shelf root");
		const auto fresh = Inspect(skins, preview.payloadPath);
		Need(fresh.ok && SameChanges(fresh.value.changes, preview.changes) && SameChanges(fresh.value.compatible, preview.compatible), "shelves changed since inspection; inspect again");
		if (fresh.value.status == Status::AlreadyCompatible)
		{
			outcome.status = Status::AlreadyCompatible; outcome.message = fresh.value.message;
			return { true, std::move(outcome), Error::None, {} };
		}
		Need(preview.status == Status::Preview && !preview.changes.empty(), "invalid preview");
		PinDirectories(root, pins);
		PinDirectories(JoinPath(preview.payloadPath, L"payload\\@Resources"), pins);
		ReadCatalog(preview.payloadPath);
		std::vector<Handle> unchanged;
		for (const auto& compatible : preview.compatible)
		{
			current = compatible.relativePath; PinDirectories(Parent(JoinPath(root, current)), pins);
			Handle lease(CreateFileW(JoinPath(root, current).c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
			Need(lease.value != INVALID_HANDLE_VALUE, "already-compatible target is busy");
			std::wstring identity; const auto bytes = ReadBytes(lease.value, &identity);
			Need(identity == compatible.identity && Sha256(bytes) == compatible.originalHash, "already-compatible target changed");
			unchanged.push_back(std::move(lease));
		}
		// Acquire every changing target before creating any backup or stage.
		for (const auto& change : preview.changes)
		{
			current = change.relativePath; PinDirectories(Parent(JoinPath(root, current)), pins);
			Replacement item; item.change = change;
			item.original = Handle(CreateFileW(JoinPath(root, current).c_str(), GENERIC_READ | READ_CONTROL | DELETE,
				FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
			Need(item.original.value != INVALID_HANDLE_VALUE, "target is busy or cannot be moved safely");
			std::wstring identity; item.bytes = ReadBytes(item.original.value, &identity);
			Need(identity == change.identity && Sha256(item.bytes) == change.originalHash, "target changed since inspection");
			BY_HANDLE_FILE_INFORMATION info{};
			Need(GetFileInformationByHandle(item.original.value, &info) && !(info.dwFileAttributes &
				(FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_ENCRYPTED | FILE_ATTRIBUTE_COMPRESSED | FILE_ATTRIBUTE_SPARSE_FILE | FILE_ATTRIBUTE_OFFLINE)), "unsupported or read-only target attributes");
			replacements.push_back(std::move(item));
		}
		Authorize(authorized); Hook(L"before-backup", root);
		const auto backup = FullPath(preview.proposedBackup);
		const auto name = backup.substr(backup.find_last_of(L'\\') + 1);
		const std::wstring prefix = L"Shelf Suite-F1-Backup-";
		Need(Parent(backup) == skins && name.rfind(prefix, 0) == 0 && name.size() == prefix.size() + 48 &&
			std::all_of(name.begin() + prefix.size(), name.end(), [](wchar_t ch) { return (ch >= L'0' && ch <= L'9') || (ch >= L'a' && ch <= L'f') || ch == L'-'; }), "unsafe backup location");
		MakeDirectory(backup, pins, authorized); outcome.backup = backup;
		MakeDirectory(JoinPath(backup, L"Holding"), pins, authorized);
		std::wstring record = L"ShelfSuite F1 recovery\r\nInstallation: " + root + L"\r\nBackup: " + backup + L"\r\n"
			L"If incomplete, lock Rainmeter and close it normally. Keep this entire backup.\r\n"
			L"Restore only the listed original files to their matching installation paths.\r\n"
			L"If a target contains an unexpected competing file, preserve it separately before manual restoration.\r\n"
			L"Do not overwrite an unexpected file automatically. Holding contains originals moved by this operation.\r\n"
			L"PHASE.txt records intentions flushed before each move; inspect actual paths before manual recovery.\r\n";
		for (auto& item : replacements)
		{
			current = item.change.relativePath;
			MakeRelativeParents(backup, current, pins, authorized);
			MakeRelativeParents(JoinPath(backup, L"Holding"), current, pins, authorized);
			item.holdingPath = JoinPath(JoinPath(backup, L"Holding"), current);
			item.stagePath = JoinPath(Parent(JoinPath(root, current)), L".cafe-f1-" + Nonce() + L".stage");
			Handle originalCopy = CreateFileNew(JoinPath(backup, current), authorized);
			WriteFlush(originalCopy.value, item.bytes, authorized);
			std::wstring backupIdentity;
			Need(Sha256(ReadBytes(originalCopy.value, &backupIdentity)) == item.change.originalHash, "backup verification failed");
			PreserveMetadata(item.original.value, originalCopy.value, authorized);
			originalCopy.Close();
			Handle snapshot(CreateFileW(JoinPath(backup, current).c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
			Need(snapshot.value != INVALID_HANDLE_VALUE, "cannot retain verified backup lease");
			std::wstring snapshotIdentity;
			Need(Sha256(ReadBytes(snapshot.value, &snapshotIdentity)) == item.change.originalHash && snapshotIdentity == backupIdentity, "backup changed before lease");
			backupLeases.push_back(std::move(snapshot));
			record += L"\r\nFile: " + current + L"\r\nOriginal SHA256: " + item.change.originalHash + L"\r\nF1 SHA256: " + item.change.outputHash +
				L"\r\nOriginal identity: " + item.change.identity + L"\r\nStaged: " + item.stagePath + L"\r\nHolding: " + item.holdingPath + L"\r\n";
		}
		Handle instructions = CreateFileNew(JoinPath(backup, L"RESTORE.txt"), authorized);
		WriteFlush(instructions.value, Utf8(record), authorized); instructions.Close();
		phase = CreateFileNew(JoinPath(backup, L"PHASE.txt"), authorized);
		WriteFlush(phase.value, Utf8(L"VERIFIED BACKUPS\r\n" + record), authorized);
		Hook(L"backup-ready", backup);
		for (auto& item : replacements)
		{
			current = item.change.relativePath;
			item.staged = CreateFileNew(item.stagePath, authorized);
			WriteFlush(item.staged.value, OutputBytes(item.change, item.bytes, preview.payloadPath), authorized);
			Need(Sha256(ReadBytes(item.staged.value, &item.outputIdentity)) == item.change.outputHash, "stage verification failed");
			PreserveMetadata(item.original.value, item.staged.value, authorized);
		}
		for (auto& item : replacements)
		{
			current = item.change.relativePath;
			const auto target = JoinPath(root, current);
			Hook(L"before-replace", target);
			Need(Sha256(ReadBytes(item.original.value)) == item.change.originalHash, "original changed before move");
			WriteFlush(phase.value, Utf8(L"MOVE ORIGINAL " + current + L"\r\n"), authorized);
			Authorize(authorized);
			Need(RenameBound(item.original.value, item.holdingPath), "original move refused"); item.moved = true;
			Hook(L"original-moved", target);
			WriteFlush(phase.value, Utf8(L"PLACE VERIFIED OUTPUT " + current + L"\r\n"), authorized);
			Authorize(authorized);
			Need(RenameBound(item.staged.value, target), "destination occupied or replacement placement failed"); item.placed = true;
			Need(Sha256(ReadBytes(item.staged.value)) == item.change.outputHash, "placed output verification failed");
			WriteFlush(phase.value, Utf8(L"VERIFIED OUTPUT " + current + L"\r\n"), authorized);
			item.staged.Close();
			outcome.changed.push_back(current);
			Hook(L"after-replace", target);
		}
		WriteFlush(phase.value, Utf8(L"COMPLETE\r\n"), authorized);
		outcome.status = Status::Updated;
		outcome.message = L"ShelfSuite F1 compatibility applied. Keep the backup, then reload affected shelves or restart Rainmeter normally.";
		return { true, std::move(outcome), Error::None, {} };
	}
	catch (const Failure& error) { failureCode = error.code; failureMessage = error.message; }
	catch (const std::exception&) { failureMessage = current + L": compatibility operation could not finish safely."; }

	bool hadMove = false;
	for (auto iterator = replacements.rbegin(); iterator != replacements.rend(); ++iterator)
	{
		auto& item = *iterator;
		if (!item.moved) continue;
		hadMove = true;
		try
		{
			Authorize(authorized);
			Need(Sha256(ReadBytes(item.original.value)) == item.change.originalHash &&
				Sha256(ReadTarget(JoinPath(outcome.backup, item.change.relativePath))) == item.change.originalHash, "recovery materials changed");
			const auto target = JoinPath(preview.shelfRoot, item.change.relativePath);
			if (item.placed)
			{
				item.staged.Close();
				Handle output(CreateFileW(target.c_str(), GENERIC_READ | DELETE, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
				Need(output.value != INVALID_HANDLE_VALUE, "recovery target busy");
				std::wstring identity; const auto bytes = ReadBytes(output.value, &identity);
				Need(identity == item.outputIdentity && Sha256(bytes) == item.change.outputHash, "recovery would overwrite an outside edit");
				WriteFlush(phase.value, Utf8(L"RECOVER REMOVE OWN OUTPUT " + item.change.relativePath + L"\r\n"), authorized);
				Authorize(authorized);
				Need(RenameBound(output.value, item.stagePath), "cannot retain own output for recovery");
				item.placed = false;
				Hook(L"recovery-vacant", target);
			}
			WriteFlush(phase.value, Utf8(L"RECOVER ORIGINAL " + item.change.relativePath + L"\r\n"), authorized);
			Authorize(authorized);
			Need(RenameBound(item.original.value, target), "recovery destination occupied");
			item.moved = false;
			Need(Sha256(ReadBytes(item.original.value)) == item.change.originalHash, "restored original verification failed");
		}
		catch (...) { outcome.manualRecovery.push_back(item.change.relativePath); }
	}
	outcome.status = !outcome.manualRecovery.empty() ? Status::ManualRecoveryRequired : hadMove ? Status::FailedRecovered : Status::Refused;
	outcome.message = failureMessage;
	if (outcome.status == Status::ManualRecoveryRequired)
		outcome.message += L" Incomplete operation. Do not reload affected shelves. Keep the verified backup, holding originals, stages and RESTORE.txt for manual recovery.";
	else if (hadMove) outcome.message += L" Originals were restored safely. Keep the backup.";
	else outcome.message += L" Original shelf files were not moved. Keep any created backup materials.";
	return { false, std::move(outcome), failureCode, failureMessage };
}
}
}
