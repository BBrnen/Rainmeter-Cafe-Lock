#pragma once
#include "Controller.h"
#include <windows.h>
#include <shobjidl.h>
#include <wrl/client.h>

namespace CafeShelf
{
enum class SelectionKind { LauncherFile, Folder, Icon };

// Used by Pick and tested against the real Windows dialog without showing it.
HRESULT CreatePicker(SelectionKind kind, IFileOpenDialog** output);

class Selection
{
public:
	explicit Selection(std::function<bool(Ticket)> allows);
	~Selection();
	Selection(const Selection&) = delete;
	Selection& operator=(const Selection&) = delete;
	Result<SelectedFile> Pick(HWND owner, SelectionKind kind, Ticket ticket, bool captureOnly = false);
	Result<SelectedFile> CaptureDrop(IUnknown* fileObject, Ticket ticket);
	// Only Host's origin-checked native WebView additional-object event calls this.
	Result<SelectedFile> AcceptDrop(IUnknown* fileObject, Ticket ticket);
	void Revoke();

private:
	Result<SelectedFile> FromNativePath(const std::wstring& path, Ticket ticket);
	std::function<bool(Ticket)> m_Allows;
	bool m_Revoked = false;
	Microsoft::WRL::ComPtr<IFileOpenDialog> m_Dialog;
};
}
