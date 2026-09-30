#include "Controller.h"
#include <utility>

namespace CafeShelf
{
Controller::Controller(std::function<bool()> isLocked) : m_IsLocked(std::move(isLocked)) {}

Ticket Controller::Open()
{
	if (Allows(m_Ticket)) return m_Ticket;
	Revoke();
	m_Ticket = m_Policy.Open(!m_IsLocked || m_IsLocked());
	return m_Ticket;
}

void Controller::Revoke()
{
	m_Policy.Revoke();
	m_Ticket = {};
	m_Selections.clear();
}

bool Controller::Allows(Ticket ticket) const
{
	return m_IsLocked && m_Policy.Allows(ticket, m_IsLocked());
}

Result<Request> Controller::Receive(Ticket ticket, const std::wstring& source,
	bool topLevel, const std::string& message)
{
	return m_Policy.Receive(ticket, !m_IsLocked || m_IsLocked(), source, topLevel, message);
}

Result<SelectionId> Controller::Remember(Ticket ticket, const SelectedFile& file)
{
	Result<SelectionId> result;
	if (!Allows(ticket)) { result.code = Error::Stale; return result; }
	if (file.path.empty() || file.path.size() > MaxStringUnits || file.path.find(L'\0') != std::wstring::npos)
		return result;
	// JSON numbers must remain exactly representable in JavaScript.
	if (m_Selections.size() >= 64 || m_NextSelection == 9007199254740991ULL) { result.code = Error::Unsupported; return result; }
	result.value = ++m_NextSelection;
	m_Selections.emplace(result.value, file);
	result.ok = true;
	result.code = Error::None;
	return result;
}

Result<SelectedFile> Controller::GetSelection(Ticket ticket, SelectionId id) const
{
	Result<SelectedFile> result;
	if (!Allows(ticket)) { result.code = Error::Stale; return result; }
	const auto found = m_Selections.find(id);
	if (found == m_Selections.end()) { result.code = Error::NotFound; return result; }
	result.value = found->second;
	result.ok = true;
	result.code = Error::None;
	return result;
}

void Controller::CancelDraft(Ticket ticket)
{
	if (Allows(ticket)) m_Selections.clear();
}
}
