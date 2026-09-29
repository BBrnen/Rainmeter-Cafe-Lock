#include "../Library/CafeShelf/HostPolicy.h"
#include <iostream>

using namespace CafeShelf;

namespace
{
int failures = 0;
int checks = 0;
void Check(const char* name, bool passed)
{
	++checks;
	std::cout << (passed ? "PASS " : "FAIL ") << name << '\n';
	if (!passed) ++failures;
}
std::string Load(uint64_t id)
{
	return Json{{"id", id}, {"op", "load"}, {"payload", Json::object()}}.dump();
}
}

int main()
{
	HostPolicy host;
	auto ticket = host.Open(false);
	const auto page = HostPolicy::Page();
	Check("trusted main-frame request accepted", host.Receive(ticket, false, page, true, Load(1)).ok);
	Check("same request cannot replay", !host.Receive(ticket, false, page, true, Load(1)).ok);
	Check("frame message denied", !host.Receive(ticket, false, page, false, Load(2)).ok);
	Check("foreign page denied", !host.Receive(ticket, false, L"https://example.com/index.html", true, Load(2)).ok);
	Check("origin prefix is not authority", !host.Receive(ticket, false, L"https://cafe-shelf.invalid.example.com/index.html", true, Load(2)).ok);
	Check("trusted origin wrong document denied", !host.Receive(ticket, false, L"https://cafe-shelf.invalid/other.html", true, Load(2)).ok);
	Check("locked request denied", !host.Receive(ticket, true, page, true, Load(2)).ok);
	Check("rejected source does not consume request", host.Receive(ticket, false, page, true, Load(2)).ok);
	host.Revoke();
	Check("late callback denied after revocation", !host.Receive(ticket, false, page, true, Load(3)).ok);
	auto reopened = host.Open(false);
	Check("late callback denied after reopening", !host.Receive(ticket, false, page, true, Load(3)).ok);
	Check("new lifetime accepts new sequence", host.Receive(reopened, false, page, true, Load(1)).ok);
	Check("malformed page message denied", !host.Receive(reopened, false, page, true, "{").ok);
	Check("page cannot claim native source", !host.Receive(reopened, false, page, true,
		R"({"id":2,"op":"load","payload":{},"source":"https://cafe-shelf.invalid/index.html"})").ok);
	host.Open(true);
	Check("locked open invalidates existing host", !host.Allows(reopened, false));

	for (const auto* uri : {L"https://cafe-shelf.invalid/index.html",
		L"https://cafe-shelf.invalid/app.js", L"https://cafe-shelf.invalid/app.css"})
	{
		Check("owned GET resource allowed", HostPolicy::AllowsResource(uri, L"GET"));
		Check("resource writes denied", !HostPolicy::AllowsResource(uri, L"POST"));
	}
	for (const auto* uri : {L"https://cafe-shelf.invalid/index.html?x=1",
		L"https://cafe-shelf.invalid/../config.lua", L"file:///C:/config.lua",
		L"https://cafe-shelf.invalid/icons/user.html", L"https://example.com/app.js"})
	{
		Check("unowned resource denied", !HostPolicy::AllowsResource(uri, L"GET"));
	}
	std::cout << checks << " host checks, " << failures << " failures\n";
	return failures ? 1 : 0;
}
