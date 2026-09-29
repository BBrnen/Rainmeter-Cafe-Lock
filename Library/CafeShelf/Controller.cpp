#include "Controller.h"
#include <utility>

namespace CafeShelf
{
Controller::Controller(std::function<bool()> isLocked) : m_IsLocked(std::move(isLocked)) {}
Ticket Controller::Open() { return {}; }
void Controller::Revoke() {}
bool Controller::Allows(Ticket) const { return false; }
Result<Request> Controller::Receive(Ticket, const std::wstring&, bool, const std::string&) { return {}; }
Result<SelectionId> Controller::Remember(Ticket, const SelectedFile&) { return {}; }
Result<SelectedFile> Controller::GetSelection(Ticket, SelectionId) const { return {}; }
void Controller::CancelDraft(Ticket) {}
}
