#include "Session.h"
#include <atomic>
#include <limits>

namespace CafeShelf
{
namespace
{
std::atomic<uint64_t> nextEditor{0};

uint64_t AllocateEditor()
{
	auto current = nextEditor.load();
	while (current != std::numeric_limits<uint64_t>::max())
	{
		if (nextEditor.compare_exchange_weak(current, current + 1)) return current + 1;
	}
	return 0;  // Exhaustion must never recycle an old identity.
}
}

Ticket Session::Open(bool locked)
{
	Revoke();
	if (locked || m_Generation == std::numeric_limits<uint64_t>::max()) return {};
	if (!m_Editor) m_Editor = AllocateEditor();
	if (!m_Editor) return {};
	++m_Generation;
	m_Open = true;
	return {m_Editor, m_Generation};
}

void Session::Revoke()
{
	m_Open = false;
	m_LastRequest = 0;
}

bool Session::Allows(Ticket ticket, bool locked) const
{
	return !locked && m_Open && ticket.editor != 0 && ticket.generation != 0 &&
		ticket.editor == m_Editor && ticket.generation == m_Generation;
}

bool Session::Accept(Ticket ticket, bool locked, uint64_t requestId)
{
	if (!Allows(ticket, locked) || !requestId || requestId <= m_LastRequest ||
		requestId > 9007199254740991ULL) return false;
	m_LastRequest = requestId;
	return true;
}
}
