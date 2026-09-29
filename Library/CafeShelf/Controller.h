#pragma once
#include "HostPolicy.h"
#include <functional>
#include <map>

namespace CafeShelf
{
using SelectionId = uint64_t;
// Native data only: never populated by decoding a page-supplied path.
struct SelectedFile
{
	std::wstring path;
	bool directory = false;
};

// UI-thread owner of one editor lifetime. IsLocked must read live native state.
class Controller
{
public:
	explicit Controller(std::function<bool()> isLocked);
	Ticket Open();
	void Revoke();
	bool Allows(Ticket ticket) const;
	Result<Request> Receive(Ticket ticket, const std::wstring& source,
		bool topLevel, const std::string& message);
	Result<SelectionId> Remember(Ticket ticket, const SelectedFile& file);
	Result<SelectedFile> GetSelection(Ticket ticket, SelectionId id) const;
	void CancelDraft(Ticket ticket);

private:
	std::function<bool()> m_IsLocked;
	HostPolicy m_Policy;
	Ticket m_Ticket;
	SelectionId m_NextSelection = 0;
	std::map<SelectionId, SelectedFile> m_Selections;
};
}
