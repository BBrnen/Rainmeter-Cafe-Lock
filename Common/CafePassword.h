// Rainmeter Cafe Lock. GNU GPL v2 or later; see LICENSE.
#pragma once
#include <Windows.h>
#include <bcrypt.h>
#include <string>
#include <array>

namespace CafeSecurity
{
// The encoding is part of the on-disk format: Windows UTF-16LE, no terminator.
class PasswordSession
{
public:
	explicit PasswordSession(const std::wstring& path) : m_Path(path) {}
	bool IsLocked() const { return m_Locked; }
	void Lock() { m_Locked = true; }
	bool NeedsSetup() const
	{
		return GetFileAttributesW(m_Path.c_str()) == INVALID_FILE_ATTRIBUTES &&
			GetLastError() == ERROR_FILE_NOT_FOUND;
	}
	bool Setup(const WCHAR* password, const WCHAR* confirmation)
	{
		if (!m_Locked || !NeedsSetup() || !Valid(password) || wcscmp(password, confirmation)) return false;
		if (!Save(password, false)) return false;
		m_Locked = false;
		return true;
	}
	bool Unlock(const WCHAR* password)
	{
		if (!Verify(password)) return false;
		m_Locked = false;
		return true;
	}
	bool Change(const WCHAR* current, const WCHAR* password, const WCHAR* confirmation)
	{
		return !m_Locked && Valid(password) && !wcscmp(password, confirmation) &&
			Verify(current) && Save(password, true);
	}
private:
	static constexpr ULONG Iterations = 600000;
	using Bytes = std::array<UCHAR, 32>;
	static bool Valid(const WCHAR* password) { const size_t n = wcslen(password); return n > 0 && n <= 256; }
	static std::wstring Hex(const Bytes& bytes)
	{
		std::wstring s;
		for (UCHAR b : bytes) { s += L"0123456789abcdef"[b >> 4]; s += L"0123456789abcdef"[b & 15]; }
		return s;
	}
	static bool Unhex(const std::wstring& s, Bytes& bytes)
	{
		if (s.size() != bytes.size() * 2) return false;
		for (size_t i = 0; i < s.size(); ++i)
		{
			const WCHAR c = s[i];
			const int n = c >= L'0' && c <= L'9' ? c - L'0' : c >= L'a' && c <= L'f' ? c - L'a' + 10 : -1;
			if (n < 0) return false;
			if (!(i & 1)) bytes[i / 2] = static_cast<UCHAR>(n << 4);
			else bytes[i / 2] |= static_cast<UCHAR>(n);
		}
		return true;
	}
	static bool Derive(const WCHAR* password, Bytes& salt, ULONG iterations, Bytes& output)
	{
		if (!Valid(password)) return false;
		BCRYPT_ALG_HANDLE algorithm = nullptr;
		if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, BCRYPT_ALG_HANDLE_HMAC_FLAG) < 0) return false;
		const NTSTATUS result = BCryptDeriveKeyPBKDF2(algorithm,
			reinterpret_cast<PUCHAR>(const_cast<WCHAR*>(password)), static_cast<ULONG>(wcslen(password) * sizeof(WCHAR)),
			salt.data(), static_cast<ULONG>(salt.size()), iterations, output.data(), static_cast<ULONG>(output.size()), 0);
		BCryptCloseAlgorithmProvider(algorithm, 0);
		return result >= 0;
	}
	std::wstring Read(const WCHAR* key) const
	{
		WCHAR value[128] = {};
		GetPrivateProfileStringW(L"CafeLock", key, L"", value, _countof(value), m_Path.c_str());
		return value;
	}
	bool Verify(const WCHAR* password) const
	{
		if (Read(L"Algorithm") != L"PBKDF2-HMAC-SHA256-UTF16LE-v1") return false;
		const auto count = Read(L"Iterations");
		if (count.empty() || count.find_first_not_of(L"0123456789") != std::wstring::npos) return false;
		WCHAR* end = nullptr;
		const unsigned long iterations = wcstoul(count.c_str(), &end, 10);
		if (*end || iterations < Iterations || iterations > 10000000) return false;
		Bytes salt{}, expected{}, actual{};
		if (!Unhex(Read(L"Salt"), salt) || !Unhex(Read(L"Verifier"), expected) || !Derive(password, salt, iterations, actual)) return false;
		volatile UCHAR difference = 0;
		for (size_t i = 0; i < actual.size(); ++i) difference |= actual[i] ^ expected[i];
		SecureZeroMemory(actual.data(), actual.size());
		return difference == 0;
	}
	bool Save(const WCHAR* password, bool replace)
	{
		Bytes salt{}, verifier{}, nonce{};
		if (BCryptGenRandom(nullptr, salt.data(), static_cast<ULONG>(salt.size()), BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0 ||
			BCryptGenRandom(nullptr, nonce.data(), static_cast<ULONG>(nonce.size()), BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0 ||
			!Derive(password, salt, Iterations, verifier)) return false;
		const std::wstring content = L"[CafeLock]\r\nAlgorithm=PBKDF2-HMAC-SHA256-UTF16LE-v1\r\nIterations=" +
			std::to_wstring(Iterations) + L"\r\nSalt=" + Hex(salt) + L"\r\nVerifier=" + Hex(verifier) + L"\r\n";
		SecureZeroMemory(verifier.data(), verifier.size());
		const std::wstring temporary = m_Path + L"." + Hex(nonce) + L".tmp";
		HANDLE file = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (file == INVALID_HANDLE_VALUE) return false;
		const std::wstring encoded = L"\xfeff" + content;
		const DWORD size = static_cast<DWORD>(encoded.size() * sizeof(WCHAR));
		DWORD written = 0;
		bool ok = WriteFile(file, encoded.data(), size, &written, nullptr) && written == size && FlushFileBuffers(file);
		CloseHandle(file);
		if (ok) ok = MoveFileExW(temporary.c_str(), m_Path.c_str(), MOVEFILE_WRITE_THROUGH | (replace ? MOVEFILE_REPLACE_EXISTING : 0)) != FALSE;
		if (!ok) DeleteFileW(temporary.c_str());
		// Flush the profile API cache after an atomic replacement.
		WritePrivateProfileStringW(nullptr, nullptr, nullptr, m_Path.c_str());
		return ok;
	}
	std::wstring m_Path;
	bool m_Locked = true; // Never serialized; each process begins locked.
};
}
