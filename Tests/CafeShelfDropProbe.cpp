// Disposable Windows feasibility probe. Never linked into Rainmeter.
#include <windows.h>
#include <shobjidl.h>
#include <shlguid.h>
#include <wrl.h>
#include <WebView2.h>
#include <iostream>
#include <string>
#include <vector>

using Microsoft::WRL::Callback;
using Microsoft::WRL::ComPtr;

namespace
{
HWND window = nullptr;
ComPtr<ICoreWebView2Environment> environment;
ComPtr<ICoreWebView2Controller> controller;
ComPtr<ICoreWebView2> view;
std::vector<std::wstring> fixtures;
size_t nextFixture = 0;
bool syntheticRejected = false;
bool finished = false;
int result = 1;

void Fail(const char* message, HRESULT hr = E_FAIL)
{
	std::cout << "FAIL " << message << " HRESULT=" << std::hex << hr << std::dec << '\n';
	finished = true;
	PostQuitMessage(1);
}
void Mouse(DWORD flags)
{
	INPUT input = {};
	input.type = INPUT_MOUSE;
	input.mi.dwFlags = flags;
	if (SendInput(1, &input, sizeof(input)) != 1) Fail("desktop mouse input unavailable");
}

class DropSource final : public IDropSource
{
public:
	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** output) override
	{
		if (!output) return E_POINTER;
		*output = nullptr;
		if (iid == IID_IUnknown || iid == IID_IDropSource)
		{
			*output = static_cast<IDropSource*>(this);
			AddRef();
			return S_OK;
		}
		return E_NOINTERFACE;
	}
	ULONG STDMETHODCALLTYPE AddRef() override { return ++references; }
	ULONG STDMETHODCALLTYPE Release() override
	{
		const auto left = --references;
		if (!left) delete this;
		return left;
	}
	HRESULT STDMETHODCALLTYPE QueryContinueDrag(BOOL escape, DWORD keys) override
	{
		if (escape || finished) return DRAGDROP_S_CANCEL;
		return (keys & MK_LBUTTON) ? S_OK : DRAGDROP_S_DROP;
	}
	HRESULT STDMETHODCALLTYPE GiveFeedback(DWORD) override { return DRAGDROP_S_USEDEFAULTCURSORS; }
private:
	ULONG references = 1;
};

void StartDrop()
{
	if (finished || nextFixture >= fixtures.size()) return;
	ComPtr<IShellItem> item;
	HRESULT hr = SHCreateItemFromParsingName(fixtures[nextFixture].c_str(), nullptr, IID_PPV_ARGS(&item));
	if (FAILED(hr)) { Fail("create real shell item", hr); return; }
	ComPtr<IDataObject> data;
	hr = item->BindToHandler(nullptr, BHID_DataObject, IID_PPV_ARGS(&data));
	if (FAILED(hr)) { Fail("create real shell drag data", hr); return; }
	POINT point = {350, 250};
	ClientToScreen(window, &point);
	SetForegroundWindow(window);
	SetCursorPos(point.x, point.y);
	Mouse(MOUSEEVENTF_LEFTDOWN);
	if (finished) return;
	SetTimer(window, 2, 500, nullptr);
	ComPtr<IDropSource> source;
	source.Attach(new DropSource());
	DWORD effect = 0;
	hr = DoDragDrop(data.Get(), source.Get(), DROPEFFECT_COPY | DROPEFFECT_LINK, &effect);
	KillTimer(window, 2);
	Mouse(MOUSEEVENTF_LEFTUP);
	if (hr != DRAGDROP_S_DROP) Fail("Windows drag did not complete", hr);
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wp, LPARAM lp)
{
	if (message == WM_APP + 1) { StartDrop(); return 0; }
	if (message == WM_TIMER)
	{
		if (wp == 2) { KillTimer(hwnd, 2); Mouse(MOUSEEVENTF_LEFTUP); }
		else Fail("timed out waiting for real browser drop");
		return 0;
	}
	if (message == WM_SIZE && controller)
	{
		RECT bounds;
		GetClientRect(hwnd, &bounds);
		controller->put_Bounds(bounds);
	}
	if (message == WM_CLOSE) { Fail("probe window closed before completion"); return 0; }
	return DefWindowProcW(hwnd, message, wp, lp);
}

HRESULT Receive(ICoreWebView2*, ICoreWebView2WebMessageReceivedEventArgs* args)
{
	LPWSTR message = nullptr;
	if (FAILED(args->TryGetWebMessageAsString(&message))) { Fail("message type"); return S_OK; }
	const std::wstring kind = message;
	CoTaskMemFree(message);
	if (kind == L"fakeRejected")
	{
		syntheticRejected = true;
		std::cout << "PASS fabricated browser File rejected by WebView2 before native delivery\n";
		PostMessageW(window, WM_APP + 1, 0, 0);
		return S_OK;
	}
	if (kind != L"fake" && kind != L"drop")
	{
		std::wcout << L"Browser diagnostic: " << kind << L'\n';
		Fail("browser test script error"); return S_OK;
	}
	ComPtr<ICoreWebView2WebMessageReceivedEventArgs2> args2;
	ComPtr<ICoreWebView2ObjectCollectionView> objects;
	UINT count = 0;
	if (FAILED(args->QueryInterface(IID_PPV_ARGS(&args2))) ||
		FAILED(args2->get_AdditionalObjects(&objects)) || !objects ||
		FAILED(objects->get_Count(&count)) || count != 1)
	{
		Fail("one additional file object required"); return S_OK;
	}
	ComPtr<IUnknown> object;
	ComPtr<ICoreWebView2File> file;
	LPWSTR path = nullptr;
	std::wstring actual;
	if (SUCCEEDED(objects->GetValueAtIndex(0, &object)) && object &&
		SUCCEEDED(object.As(&file)) && SUCCEEDED(file->get_Path(&path)) && path)
	{
		actual = path;
	}
	CoTaskMemFree(path);
	if (kind == L"fake")
	{
		if (!actual.empty()) { Fail("fabricated File unexpectedly has native path"); return S_OK; }
		syntheticRejected = true;
		std::cout << "PASS fabricated browser File has no selectable native path\n";
		PostMessageW(window, WM_APP + 1, 0, 0);
		return S_OK;
	}
	if (!syntheticRejected || nextFixture >= fixtures.size() ||
		_wcsicmp(actual.c_str(), fixtures[nextFixture].c_str()) != 0)
	{
		std::wcout << L"Observed drop path: " << actual << L'\n';
		Fail("drop must preserve original selected path"); return S_OK;
	}
	const char* labels[] = {"shortcut", "executable", "folder", "associated file"};
	std::cout << "PASS real Windows " << labels[nextFixture] << " drop preserves path\n";
	++nextFixture;
	if (nextFixture == fixtures.size())
	{
		finished = true;
		result = 0;
		PostQuitMessage(0);
	}
	else PostMessageW(window, WM_APP + 1, 0, 0);
	return S_OK;
}
}

int wmain(int argc, wchar_t** argv)
{
	wchar_t ci[8] = {};
	if (!GetEnvironmentVariableW(L"GITHUB_ACTIONS", ci, 8) || wcscmp(ci, L"true") || argc != 6)
	{
		std::cerr << "Disposable CI only; requires four fixtures and a private browser profile.\n";
		return 2;
	}
	for (int i = 1; i <= 4; ++i) fixtures.emplace_back(argv[i]);
	if (FAILED(OleInitialize(nullptr))) return 2;
	LPWSTR version = nullptr;
	const auto versionResult = GetAvailableCoreWebView2BrowserVersionString(nullptr, &version);
	if (FAILED(versionResult)) { Fail("WebView2 Runtime unavailable", versionResult); OleUninitialize(); return 1; }
	std::wcout << L"WebView2 Runtime: " << version << L'\n';
	CoTaskMemFree(version);
	WNDCLASSW cls = {};
	cls.lpfnWndProc = WindowProc;
	cls.hInstance = GetModuleHandleW(nullptr);
	cls.lpszClassName = L"CafeShelfDisposableDropProbe";
	RegisterClassW(&cls);
	window = CreateWindowW(cls.lpszClassName, L"Cafe Shelf disposable drop test",
		WS_OVERLAPPEDWINDOW | WS_VISIBLE, 60, 60, 900, 600, nullptr, nullptr, cls.hInstance, nullptr);
	if (!window) { OleUninitialize(); return 2; }
	SetTimer(window, 1, 90000, nullptr);
	HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(nullptr, argv[5], nullptr,
		Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
			[](HRESULT created, ICoreWebView2Environment* env) -> HRESULT
			{
				if (FAILED(created) || !env) { Fail("create environment", created); return S_OK; }
				environment = env;
				return env->CreateCoreWebView2Controller(window,
					Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
						[](HRESULT ready, ICoreWebView2Controller* ctrl) -> HRESULT
						{
							if (FAILED(ready) || !ctrl) { Fail("create browser controller", ready); return S_OK; }
							controller = ctrl;
							if (FAILED(ctrl->get_CoreWebView2(&view))) { Fail("get browser"); return S_OK; }
							RECT bounds;
							GetClientRect(window, &bounds);
							ctrl->put_Bounds(bounds);
							ctrl->put_IsVisible(TRUE);
							EventRegistrationToken token = {};
							if (FAILED(view->add_WebMessageReceived(
								Callback<ICoreWebView2WebMessageReceivedEventHandler>(Receive).Get(), &token)))
							{
								Fail("subscribe browser messages"); return S_OK;
							}
							// This isolated probe has no native write/execute bridge.
							return view->NavigateToString(LR"HTML(<!doctype html><html><body style="min-height:100vh;background:#eee">
<div>Disposable Windows drop probe</div><script>
window.addEventListener('dragover', e => e.preventDefault());
window.addEventListener('drop', e => {
 e.preventDefault();
 try { chrome.webview.postMessageWithAdditionalObjects('drop', Array.from(e.dataTransfer.files)); }
 catch (error) { chrome.webview.postMessage('error:' + String(error)); }
});
if (typeof chrome.webview.postMessageWithAdditionalObjects !== 'function') {
 chrome.webview.postMessage('error:additional file object API unavailable');
} else {
 try { chrome.webview.postMessageWithAdditionalObjects('fake', [new File(['test'], 'fake.lnk')]); }
 catch (error) { chrome.webview.postMessage('fakeRejected'); }
}
</script></body></html>)HTML");
						}).Get());
			}).Get());
	if (FAILED(hr)) Fail("start WebView2", hr);
	MSG message;
	while (!finished && GetMessageW(&message, nullptr, 0, 0) > 0)
	{
		TranslateMessage(&message);
		DispatchMessageW(&message);
	}
	KillTimer(window, 1);
	if (controller) controller->Close();
	view.Reset(); controller.Reset(); environment.Reset();
	DestroyWindow(window);
	OleUninitialize();
	return result;
}
