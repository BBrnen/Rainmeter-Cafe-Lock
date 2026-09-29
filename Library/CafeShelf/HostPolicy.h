#pragma once
#include "Protocol.h"
#include "Session.h"

namespace CafeShelf
{
// Native-only inputs: source comes from the WebView event, and topLevel from
// the subscribed event channel. Neither field is accepted from page JSON.
class HostPolicy
{
public:
	Ticket Open(bool locked);
	void Revoke();
	bool Allows(Ticket ticket, bool locked) const;
	Result<Request> Receive(Ticket ticket, bool locked, const std::wstring& source,
		bool topLevel, const std::string& message);
	static bool AllowsResource(const std::wstring& uri, const std::wstring& method);
	static const wchar_t* Page();

private:
	Session m_Session;
};
}
