#include "Selection.h"
#include <WebView2.h>
#include <utility>

using Microsoft::WRL::ComPtr;

namespace CafeShelf
{
namespace
{
struct CoString
{
	LPWSTR value = nullptr;
	~CoString() { CoTaskMemFree(value); }
};
Result<SelectedFile> Failure(Error code, const wchar_t* message)
{
	Result<SelectedFile> result;
	result.code = code;
	result.message = message;
	return result;
}
bool AbsoluteFilePath(const std::wstring& path)
{
	const bool drive = path.size() >= 3 &&
		((path[0] >= L'A' && path[0] <= L'Z') || (path[0] >= L'a' && path[0] <= L'z')) &&
		path[1] == L':' && path[2] == L'\\';
	const bool unc = path.size() >= 5 && path[0] == L'\\' && path[1] == L'\\' &&
		path[2] != L'?' && path[2] != L'.' && path[2] != L'\\' &&
		path.find(L'\\', 2) != std::wstring::npos;
	return (drive || unc) && path.find(L':', drive ? 2 : 0) == std::wstring::npos;
}
}

HRESULT CreatePicker(SelectionKind kind, IFileOpenDialog** output)
{
	if (!output) return E_POINTER;
	*output = nullptr;
	if (kind != SelectionKind::LauncherFile && kind != SelectionKind::Folder && kind != SelectionKind::Icon)
		return E_INVALIDARG;
	ComPtr<IFileOpenDialog> dialog;
	HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog));
	if (FAILED(hr)) return hr;
	FILEOPENDIALOGOPTIONS options = {};
	hr = dialog->GetOptions(&options);
	if (FAILED(hr)) return hr;
	options &= ~(FOS_ALLOWMULTISELECT | FOS_PICKFOLDERS | FOS_ALLNONSTORAGEITEMS | FOS_NOVALIDATE);
	options |= FOS_FORCEFILESYSTEM | FOS_FILEMUSTEXIST | FOS_PATHMUSTEXIST |
		FOS_NODEREFERENCELINKS | FOS_NOCHANGEDIR | FOS_DONTADDTORECENT;
	if (kind == SelectionKind::Folder) options |= FOS_PICKFOLDERS;
	hr = dialog->SetOptions(options);
	if (FAILED(hr)) return hr;
	if (kind == SelectionKind::Icon)
	{
		const COMDLG_FILTERSPEC filters[] = {{L"Icons and applications", L"*.png;*.ico;*.exe;*.lnk"}};
		hr = dialog->SetFileTypes(ARRAYSIZE(filters), filters);
		if (FAILED(hr)) return hr;
	}
	hr = dialog->SetTitle(kind == SelectionKind::Folder ? L"Choose a launcher folder" :
		kind == SelectionKind::Icon ? L"Choose a custom icon" : L"Choose a launcher or file");
	if (FAILED(hr)) return hr;
	*output = dialog.Detach();
	return S_OK;
}

Selection::Selection(std::function<bool(Ticket)> allows) : m_Allows(std::move(allows)) {}
Selection::~Selection() { Revoke(); }

Result<SelectedFile> Selection::FromNativePath(const std::wstring& path, Ticket ticket)
{
	if (m_Revoked || !m_Allows || !m_Allows(ticket)) return Failure(Error::Stale, L"This editor session has ended.");
	if (path.empty() || path.size() > MaxStringUnits || path.find(L'\0') != std::wstring::npos || !AbsoluteFilePath(path))
		return Failure(Error::InvalidInput, L"Choose an existing file or folder on the filesystem.");
	const auto attributes = GetFileAttributesW(path.c_str());
	if (attributes == INVALID_FILE_ATTRIBUTES) return Failure(Error::NotFound, L"The selected file or folder is unavailable.");
	if (m_Revoked || !m_Allows(ticket)) return Failure(Error::Stale, L"This editor session has ended.");
	Result<SelectedFile> result;
	result.ok = true;
	result.code = Error::None;
	result.value = {path, (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0};
	return result;
}

Result<SelectedFile> Selection::AcceptDrop(IUnknown* fileObject, Ticket ticket)
{
	if (m_Revoked || !m_Allows || !m_Allows(ticket)) return Failure(Error::Stale, L"This editor session has ended.");
	if (!fileObject) return Failure(Error::InvalidInput, L"Drop one file or folder from File Explorer.");
	ComPtr<ICoreWebView2File> file;
	CoString path;
	if (FAILED(fileObject->QueryInterface(IID_PPV_ARGS(&file))) ||
		FAILED(file->get_Path(&path.value)) || !path.value)
		return Failure(Error::InvalidInput, L"Drop one file or folder from File Explorer.");
	const size_t size = wcsnlen_s(path.value, MaxStringUnits + 1);
	if (size > MaxStringUnits) return Failure(Error::InvalidInput, L"The selected path is too long.");
	return FromNativePath(std::wstring(path.value, size), ticket);
}

Result<SelectedFile> Selection::CaptureDrop(IUnknown* fileObject, Ticket ticket)
{
	return AcceptDrop(fileObject,ticket);
}

Result<SelectedFile> Selection::Pick(HWND owner, SelectionKind kind, Ticket ticket, bool captureOnly)
{
	(void)captureOnly;
	if (m_Revoked || !m_Allows || !m_Allows(ticket)) return Failure(Error::Stale, L"This editor session has ended.");
	if (m_Dialog) return Failure(Error::Conflict, L"A file selection dialog is already open.");
	ComPtr<IFileOpenDialog> dialog;
	if (FAILED(CreatePicker(kind, &dialog))) return Failure(Error::IoError, L"Windows could not open the file selection dialog.");
	m_Dialog = dialog;
	const auto shown = dialog->Show(owner);
	m_Dialog.Reset();
	// Show pumps messages: Lock Now may have revoked us and closed this dialog.
	if (m_Revoked || !m_Allows(ticket)) return Failure(Error::Stale, L"This editor session has ended.");
	if (shown == HRESULT_FROM_WIN32(ERROR_CANCELLED) || shown == E_ABORT)
		return Failure(Error::Cancelled, L"Selection cancelled.");
	if (FAILED(shown)) return Failure(Error::IoError, L"Windows could not select the item.");
	ComPtr<IShellItem> item;
	CoString path;
	if (FAILED(dialog->GetResult(&item)) || FAILED(item->GetDisplayName(SIGDN_FILESYSPATH, &path.value)) || !path.value)
		return Failure(Error::InvalidInput, L"Choose a filesystem file or folder.");
	const size_t size = wcsnlen_s(path.value, MaxStringUnits + 1);
	if (size > MaxStringUnits) return Failure(Error::InvalidInput, L"The selected path is too long.");
	auto result = FromNativePath(std::wstring(path.value, size), ticket);
	if (result.ok && ((kind == SelectionKind::Folder) != result.value.directory))
		return Failure(Error::InvalidInput, L"Choose the requested file or folder type.");
	return result;
}

void Selection::Revoke()
{
	m_Revoked = true;
	auto dialog = m_Dialog;
	m_Dialog.Reset();
	if (dialog) dialog->Close(HRESULT_FROM_WIN32(ERROR_CANCELLED));
}
}
