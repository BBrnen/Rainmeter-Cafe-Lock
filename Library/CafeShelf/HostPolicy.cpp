#include "HostPolicy.h"

namespace CafeShelf
{
Ticket HostPolicy::Open(bool locked) { return m_Session.Open(locked); }
void HostPolicy::Revoke() { m_Session.Revoke(); }
bool HostPolicy::Allows(Ticket ticket, bool locked) const { return m_Session.Allows(ticket, locked); }

Result<Request> HostPolicy::Receive(Ticket ticket, bool locked, const std::wstring& source,
	bool topLevel, const std::string& message)
{
	Result<Request> denied;
	denied.code = locked ? Error::Locked : Error::Stale;
	if (!m_Session.Allows(ticket, locked)) return denied;
	denied.code = Error::AccessDenied;
	if (!topLevel || source != Page()) return denied;
	auto request = DecodeRequest(message);
	if (!request.ok) return request;
	if (!m_Session.Accept(ticket, locked, request.value.id))
	{
		denied.code = Error::Stale;
		return denied;
	}
	return request;
}

bool HostPolicy::AllowsResource(const std::wstring& uri, const std::wstring& method)
{
	return method == L"GET" && (uri == Page() ||
		uri == L"https://cafe-shelf.invalid/app.js" ||
		uri == L"https://cafe-shelf.invalid/app.css");
}

const wchar_t* HostPolicy::Page() { return L"https://cafe-shelf.invalid/index.html"; }
}
