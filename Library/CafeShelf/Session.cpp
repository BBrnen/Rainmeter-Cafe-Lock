#include "Session.h"

namespace CafeShelf
{
// Fail-closed contract scaffolding for the initial failing regression tests.
// Not integrated into the Rainmeter application.
Ticket Session::Open(bool) { return {}; }
void Session::Revoke() {}
bool Session::Allows(Ticket, bool) const { return false; }
bool Session::Accept(Ticket, bool, uint64_t) { return false; }
}
