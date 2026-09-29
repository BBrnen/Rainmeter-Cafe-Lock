#pragma once

#include "Types.h"

namespace CafeShelf
{
// UI-thread owned. The caller supplies the live native lock state, never page data.
class Session
{
public:
	Session() = default;
	Session(const Session&) = delete;
	Session& operator=(const Session&) = delete;

	Ticket Open(bool locked);
	void Revoke();
	bool Allows(Ticket ticket, bool locked) const;
	bool Accept(Ticket ticket, bool locked, uint64_t requestId);

private:
	uint64_t m_Editor = 0;
	uint64_t m_Generation = 0;
	uint64_t m_LastRequest = 0;
	bool m_Open = false;
};
}
