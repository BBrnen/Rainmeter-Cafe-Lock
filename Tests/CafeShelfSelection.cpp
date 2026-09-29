#include "../Library/CafeShelf/Selection.h"
#include <WebView2.h>
#include <iostream>
#include <cstring>
using namespace CafeShelf;
using Microsoft::WRL::ComPtr;
namespace
{
int checks = 0, failures = 0;
void Check(const char* name, bool passed)
{
	++checks; std::cout << (passed ? "PASS " : "FAIL ") << name << '\n';
	if (!passed) ++failures;
}
// Exercises native-path validation and reentrant revocation, not WebView
// provenance. Genuine browser/Explorer objects were tested separately.
class FileObject final : public ICoreWebView2File
{
public:
	std::wstring path;
	std::function<void()> onRead;
	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id, void** out) override
	{
		if (!out) return E_POINTER;
		*out = nullptr;
		if (id != IID_IUnknown && id != __uuidof(ICoreWebView2File)) return E_NOINTERFACE;
		*out = static_cast<ICoreWebView2File*>(this); AddRef(); return S_OK;
	}
	ULONG STDMETHODCALLTYPE AddRef() override { return ++refs; }
	ULONG STDMETHODCALLTYPE Release() override { const auto left = --refs; if (!left) delete this; return left; }
	HRESULT STDMETHODCALLTYPE get_Path(LPWSTR* out) override
	{
		if (!out) return E_POINTER;
		if (onRead) onRead();
		const auto bytes = (path.size() + 1) * sizeof(wchar_t);
		*out = static_cast<LPWSTR>(CoTaskMemAlloc(bytes));
		if (!*out) return E_OUTOFMEMORY;
		memcpy(*out, path.c_str(), bytes); return S_OK;
	}
private:
	ULONG refs = 1;
};
}
int wmain(int argc, wchar_t** argv)
{
	if (argc != 3 || FAILED(OleInitialize(nullptr))) return 2;
	for (const auto kind : {SelectionKind::LauncherFile, SelectionKind::Folder, SelectionKind::Icon})
	{
		ComPtr<IFileOpenDialog> dialog;
		const auto created = CreatePicker(kind, &dialog);
		Check("native picker created", SUCCEEDED(created) && dialog);
		FILEOPENDIALOGOPTIONS options = {};
		const bool read = dialog && SUCCEEDED(dialog->GetOptions(&options));
		Check("picker preserves shortcut rather than target", read && (options & FOS_NODEREFERENCELINKS));
		Check("picker requires existing filesystem item", read && (options & FOS_FORCEFILESYSTEM) &&
			(options & FOS_FILEMUSTEXIST) && (options & FOS_PATHMUSTEXIST));
		Check("picker cannot select multiple entries", read && !(options & FOS_ALLOWMULTISELECT));
		Check("folder mode matches request", read && (!!(options & FOS_PICKFOLDERS) == (kind == SelectionKind::Folder)));
	}
	bool locked = false;
	Controller control([&locked]() { return locked; });
	const auto ticket = control.Open();
	Selection selection([&control](Ticket t) { return control.Allows(t); });
	ComPtr<FileObject> object;
	object.Attach(new FileObject());
	object->path = argv[1];
	auto file = selection.AcceptDrop(object.Get(), ticket);
	Check("native shortcut path retained verbatim", file.ok && file.value.path == argv[1] && !file.value.directory);
	object->path = argv[2];
	auto folder = selection.AcceptDrop(object.Get(), ticket);
	Check("folder itself selected", folder.ok && folder.value.path == argv[2] && folder.value.directory);
	object->path.clear();
	Check("pathless browser file rejected", !selection.AcceptDrop(object.Get(), ticket).ok);
	Check("null native object rejected", !selection.AcceptDrop(nullptr, ticket).ok);
	object->path = L"relative.lnk";
	Check("relative native path rejected", !selection.AcceptDrop(object.Get(), ticket).ok);
	object->path = L"\\\\.\\PhysicalDrive0";
	Check("device path rejected", !selection.AcceptDrop(object.Get(), ticket).ok);
	object->path = std::wstring(argv[1]) + L".missing";
	Check("missing selected file rejected", !selection.AcceptDrop(object.Get(), ticket).ok);
	object->path = argv[1];
	locked = true;
	Check("locked drop rejected before path access", !selection.AcceptDrop(object.Get(), ticket).ok);
	Check("locked picker cannot show", !selection.Pick(nullptr, SelectionKind::LauncherFile, ticket).ok);
	locked = false;
	object->onRead = [&locked]() { locked = true; };
	Check("lock during native callback discards result", !selection.AcceptDrop(object.Get(), ticket).ok);
	object->onRead = {};
	locked = false;
	selection.Revoke();
	Check("revoked selection object stays closed", !selection.AcceptDrop(object.Get(), ticket).ok);
	control.Revoke();
	auto reopened = control.Open();
	Selection replacement([&control](Ticket t) { return control.Allows(t); });
	Check("old ticket cannot authorize new selection object", !replacement.AcceptDrop(object.Get(), ticket).ok);
	Check("new ticket authorizes new selection", replacement.AcceptDrop(object.Get(), reopened).ok);
	OleUninitialize();
	std::cout << checks << " selection checks, " << failures << " failures\n";
	return failures ? 1 : 0;
}
