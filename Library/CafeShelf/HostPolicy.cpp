#include "HostPolicy.h"

namespace CafeShelf
{
Ticket HostPolicy::Open(bool locked) { return m_Session.Open(locked); }
void HostPolicy::Revoke() { m_Session.Revoke(); }
bool HostPolicy::Allows(Ticket ticket, bool locked) const { return m_Session.Allows(ticket, locked); }
Result<Request> HostPolicy::Receive(Ticket, bool, const std::wstring&, bool, const std::string&) { return {}; }
bool HostPolicy::AllowsResource(const std::wstring&, const std::wstring&) { return false; }
const wchar_t* HostPolicy::Page() { return L"https://cafe-shelf.invalid/index.html"; }
}
