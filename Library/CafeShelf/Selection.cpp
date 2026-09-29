#include "Selection.h"
#include <utility>
namespace CafeShelf
{
HRESULT CreatePicker(SelectionKind, IFileOpenDialog** output)
{
	if (!output) return E_POINTER;
	*output = nullptr;
	return E_NOTIMPL;
}
Selection::Selection(std::function<bool(Ticket)> allows) : m_Allows(std::move(allows)) {}
Selection::~Selection() { Revoke(); }
Result<SelectedFile> Selection::Pick(HWND, SelectionKind, Ticket) { return {}; }
Result<SelectedFile> Selection::AcceptDrop(IUnknown*, Ticket) { return {}; }
Result<SelectedFile> Selection::FromNativePath(const std::wstring&, Ticket) { return {}; }
void Selection::Revoke() {}
}
