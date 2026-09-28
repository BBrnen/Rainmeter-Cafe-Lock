// Rainmeter Cafe Lock. GNU GPL v2 or later; see LICENSE.
#pragma once
#include <windows.h>
#include <shellapi.h>
#include <sddl.h>
#include <string>

namespace CafeSecurity
{
// Query the OS token, never a PID, flag or identity supplied in the payload.
inline bool IsElevatedAdministrator(HANDLE token)
{
	DWORD size = 0;
	TOKEN_ELEVATION elevation = {};
	BYTE integrity[SECURITY_MAX_SID_SIZE + sizeof(TOKEN_MANDATORY_LABEL)] = {};
	BYTE administrators[SECURITY_MAX_SID_SIZE] = {};
	DWORD sidSize = sizeof(administrators);
	BOOL member = FALSE;
	if (!GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &size) ||
		!elevation.TokenIsElevated ||
		!GetTokenInformation(token, TokenIntegrityLevel, integrity, sizeof(integrity), &size) ||
		!CreateWellKnownSid(WinBuiltinAdministratorsSid, nullptr, administrators, &sidSize) ||
		!CheckTokenMembership(token, administrators, &member) || !member) return false;
	PSID sid = reinterpret_cast<TOKEN_MANDATORY_LABEL*>(integrity)->Label.Sid;
	return IsValidSid(sid) && *GetSidSubAuthority(sid, *GetSidSubAuthorityCount(sid) - 1) >= SECURITY_MANDATORY_HIGH_RID;
}

inline ULONGLONG CreationTime(HANDLE process)
{
	FILETIME created, exited, kernel, user;
	if (!GetProcessTimes(process, &created, &exited, &kernel, &user)) return 0;
	return (static_cast<ULONGLONG>(created.dwHighDateTime) << 32) | created.dwLowDateTime;
}

// UI-thread-owned, one-shot authorization. No persisted state or public unlock setter.
class Authorization
{
public:
	Authorization() = default;
	~Authorization() { Lock(); }
	Authorization(const Authorization&) = delete;
	Authorization& operator=(const Authorization&) = delete;
	bool IsAuthorized() const { return m_Authorized; }
	bool IsPending() const { return m_Pipe != INVALID_HANDLE_VALUE; }
	void Lock() { m_Authorized = false; Cancel(); }

	bool Begin(HWND owner, const std::wstring& helper)
	{
		if (IsPending() || m_Authorized) return false;
		GUID guid;
		WCHAR text[40];
		if (FAILED(CoCreateGuid(&guid)) || !StringFromGUID2(guid, text, 40)) return false;
		std::wstring name = L"\\\\.\\pipe\\RainmeterCafe-";
		name += text;
		// Local authenticated clients may connect, but cannot authorize without a
		// high-integrity enabled administrator token AND the runas process handle.
		PSECURITY_DESCRIPTOR descriptor = nullptr;
		if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(
			L"D:P(A;;GA;;;SY)(A;;GA;;;BA)(A;;GRGW;;;AU)", SDDL_REVISION_1, &descriptor, nullptr)) return false;
		SECURITY_ATTRIBUTES attributes = { sizeof(attributes), descriptor, FALSE };
		m_Pipe = CreateNamedPipeW(name.c_str(), PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED | FILE_FLAG_FIRST_PIPE_INSTANCE,
			PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS,
			1, sizeof(DWORD), sizeof(DWORD), 0, &attributes);
		LocalFree(descriptor);
		if (!IsPending()) return false;
		m_Overlap = {};
		m_Overlap.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
		if (!m_Overlap.hEvent) { Cancel(); return false; }
		m_Phase = Phase::Connect;
		BOOL connected = ConnectNamedPipe(m_Pipe, &m_Overlap);
		DWORD error = connected ? ERROR_SUCCESS : GetLastError();
		m_Waiting = error == ERROR_IO_PENDING;
		if (!m_Waiting && error != ERROR_SUCCESS && error != ERROR_PIPE_CONNECTED) { Cancel(); return false; }
		const std::wstring args = L"\"" + name + L"\" " + std::to_wstring(GetCurrentProcessId()) + L" " +
			std::to_wstring(CreationTime(GetCurrentProcess()));
		SHELLEXECUTEINFOW execute = { sizeof(execute) };
		execute.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOASYNC | SEE_MASK_FLAG_NO_UI;
		execute.hwnd = owner;
		execute.lpVerb = L"runas";
		execute.lpFile = helper.c_str();
		execute.lpParameters = args.c_str();
		execute.nShow = SW_HIDE;
		if (!ShellExecuteExW(&execute) || !execute.hProcess) { Cancel(); return false; }
		if (!IsPending()) { CloseHandle(execute.hProcess); return false; }
		m_Helper = execute.hProcess;
		return true;
	}

	// Only completed pipe I/O can authorize. Spoofing a timer/window message merely polls.
	bool Poll()
	{
		if (!IsPending() || !m_Helper) return false;
		DWORD transferred = 0;
		if (m_Waiting && !GetOverlappedResult(m_Pipe, &m_Overlap, &transferred, FALSE))
		{
			if (GetLastError() != ERROR_IO_INCOMPLETE || WaitForSingleObject(m_Helper, 0) != WAIT_TIMEOUT) Cancel();
			return false;
		}
		m_Waiting = false;
		if (m_Phase == Phase::Connect)
		{
			ResetEvent(m_Overlap.hEvent);
			m_Phase = Phase::Read;
			if (!ReadFile(m_Pipe, &m_Request, sizeof(m_Request), &transferred, &m_Overlap))
			{
				m_Waiting = GetLastError() == ERROR_IO_PENDING;
				if (!m_Waiting) Cancel();
				return false;
			}
		}
		if (m_Phase == Phase::Read)
		{
			ULONG peer = 0;
			DWORD session = 0, ownSession = 0, size = 0;
			HANDLE token = nullptr;
			bool valid = transferred == sizeof(m_Request) && m_Request == 1 &&
				GetNamedPipeClientProcessId(m_Pipe, &peer) && peer == GetProcessId(m_Helper) &&
				WaitForSingleObject(m_Helper, 0) == WAIT_TIMEOUT;
			if (valid && ImpersonateNamedPipeClient(m_Pipe))
			{
				valid = OpenThreadToken(GetCurrentThread(), TOKEN_QUERY, TRUE, &token) &&
					IsElevatedAdministrator(token) &&
					GetTokenInformation(token, TokenSessionId, &session, sizeof(session), &size) &&
					ProcessIdToSessionId(GetCurrentProcessId(), &ownSession) && session == ownSession;
				if (token) CloseHandle(token);
				// Identification-only impersonation never enables privileged operations.
				if (!RevertToSelf()) TerminateProcess(GetCurrentProcess(), ERROR_ACCESS_DENIED);
			}
			else valid = false;
			if (!valid) { Cancel(); return false; }
			m_Authorized = true;
			// The peer waits for this acknowledgement, keeping its process alive
			// during token/PID verification. The tiny write is also overlapped.
			ResetEvent(m_Overlap.hEvent);
			m_Phase = Phase::Reply;
			if (!WriteFile(m_Pipe, &m_Request, sizeof(m_Request), &transferred, &m_Overlap))
				m_Waiting = GetLastError() == ERROR_IO_PENDING;
			if (!m_Waiting) Cancel();
			return true;
		}
		Cancel();
		return false;
	}

private:
	void Cancel()
	{
		if (IsPending())
		{
			if (m_Waiting)
			{
				CancelIoEx(m_Pipe, &m_Overlap);
				DWORD ignored;
				GetOverlappedResult(m_Pipe, &m_Overlap, &ignored, TRUE);
			}
			CloseHandle(m_Pipe);
			m_Pipe = INVALID_HANDLE_VALUE;
		}
		if (m_Overlap.hEvent) CloseHandle(m_Overlap.hEvent);
		m_Overlap = {};
		if (m_Helper) CloseHandle(m_Helper);
		m_Helper = nullptr;
		m_Waiting = false;
	}
	bool m_Authorized = false;
	bool m_Waiting = false;
	HANDLE m_Pipe = INVALID_HANDLE_VALUE;
	HANDLE m_Helper = nullptr;
	OVERLAPPED m_Overlap = {};
	DWORD m_Request = 0;
	enum class Phase { Connect, Read, Reply };
	Phase m_Phase = Phase::Connect;
};
}
