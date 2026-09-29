// Disposable Windows feasibility probe. Never linked into Rainmeter.
#include <windows.h>
#include <shobjidl.h>
#include <shlguid.h>
#include <wrl.h>
#include <WebView2.h>
#include <iostream>
#include <string>
#include <vector>
#include <atomic>
#include <thread>

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
std::atomic<bool> finished{false};
std::thread dragThread;
int result = 1;

void Fail(const char* message, HRESULT hr = E_FAIL)
{
	std::cout << "FAIL " << message << " HRESULT=" << std::hex << hr << std::dec << '\n';
	finished = true;
	if (window) PostMessageW(window, WM_NULL, 0, 0);
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

void RunDrop(const std::wstring& path)
{
	ComPtr<IShellItem> item;
	HRESULT hr = SHCreateItemFromParsingName(path.c_str(), nullptr, IID_PPV_ARGS(&item));
	if (FAILED(hr)) { Fail("create real shell item", hr); return; }
	std::cout << "Shell item ready\n";
	ComPtr<IDataObject> data;
	hr = item->BindToHandler(nullptr, BHID_DataObject, IID_PPV_ARGS(&data));
	if (FAILED(hr)) { Fail("create real shell drag data", hr); return; }
	std::cout << "Shell drag data ready\n";
	POINT point = {350, 250};
	ClientToScreen(window, &point);
	std::cout << "Focusing target window\n";
	const auto activated = SetForegroundWindow(window);
	const auto hit = WindowFromPoint(point);
	const auto root = GetAncestor(hit, GA_ROOT);
	std::cout << "Target visible=" << IsWindowVisible(window) << ", minimized=" << IsIconic(window) <<
		", activation accepted=" << activated << ", target=" << window << ", hit=" << hit <<
		", hit root=" << root << ", foreground=" << GetForegroundWindow() << '\n';
	if (!SetCursorPos(point.x, point.y)) { Fail("no input desktop cursor access", HRESULT_FROM_WIN32(GetLastError())); return; }
	if (GetAncestor(WindowFromPoint(point), GA_ROOT) != window)
	{
		Fail("test window is covered; refusing to send input elsewhere");
		return;
	}
	std::cout << "Cursor placed; pressing mouse\n";
	Mouse(MOUSEEVENTF_LEFTDOWN);
	if (finished) return;
	SetTimer(window, 2, 500, nullptr);
	ComPtr<IDropSource> source;
	source.Attach(new DropSource());
	DWORD effect = 0;
	std::cout << "Entering OLE drag loop\n";
	hr = DoDragDrop(data.Get(), source.Get(), DROPEFFECT_COPY | DROPEFFECT_LINK, &effect);
	std::cout << "OLE drag returned HRESULT=" << std::hex << hr << std::dec << '\n';
	KillTimer(window, 2);
	Mouse(MOUSEEVENTF_LEFTUP);
	if (hr != DRAGDROP_S_DROP) Fail("Windows drag did not complete", hr);
}

void StartDrop()
{
	if (finished || nextFixture >= fixtures.size()) return;
	if (dragThread.joinable()) dragThread.join();
	const auto path = fixtures[nextFixture];
	std::cout << "Starting Windows OLE drop " << nextFixture << '\n';
	// Model an external source (such as Explorer). Running OLE's modal source
	// loop on the browser UI thread prevents the target callbacks from pumping.
	dragThread = std::thread([path]()
	{
		const auto initialized = OleInitialize(nullptr);
		if (FAILED(initialized)) { Fail("initialize drag source apartment", initialized); return; }
		RunDrop(path);
		OleUninitialize();
	});
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wp, LPARAM lp)
{
	if (message == WM_APP + 1) { StartDrop(); return 0; }
	if (message == WM_TIMER)
	{
		if (wp == 2) { std::cout << "Release timer fired\n"; KillTimer(hwnd, 2); Mouse(MOUSEEVENTF_LEFTUP); }
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

bool CreateProbeWindow()
{
	WNDCLASSW cls = {};
	cls.lpfnWndProc = WindowProc;
	cls.hInstance = GetModuleHandleW(nullptr);
	cls.lpszClassName = L"CafeShelfDisposableDropProbe";
	RegisterClassW(&cls);
	window = CreateWindowW(cls.lpszClassName, L"Cafe Shelf disposable drop test",
		WS_OVERLAPPEDWINDOW | WS_VISIBLE, 60, 60, 900, 600, nullptr, nullptr, cls.hInstance, nullptr);
	if (!window) return false;
	// The launcher hides the console using STARTUPINFO. Windows applies that
	// setting to the first top-level show as well; explicitly show our UI.
	std::cout << "Window visible before explicit show=" << IsWindowVisible(window) << '\n';
	ShowWindow(window, SW_SHOWDEFAULT);
	ShowWindow(window, SW_RESTORE);
	UpdateWindow(window);
	std::cout << "Window visible after explicit show=" << IsWindowVisible(window) << '\n';
	return true;
}

// Separate process reproduces the launcher's STARTUPINFO, before WebView2
// or desktop input is involved. Both paths use the same window creation.
int CheckHiddenStartup()
{
	STARTUPINFOW startup = {};
	startup.cb = sizeof(startup);
	GetStartupInfoW(&startup);
	if (!(startup.dwFlags & STARTF_USESHOWWINDOW) || startup.wShowWindow != SW_HIDE)
	{
		std::cerr << "FAIL regression did not reproduce hidden startup\n";
		return 2;
	}
	if (!CreateProbeWindow()) return 2;
	const bool visible = IsWindowVisible(window) != FALSE;
	std::cout << (visible ? "PASS" : "FAIL") << " probe window is visible after hidden process startup\n";
	DestroyWindow(window);
	return visible ? 0 : 1;
}

int RunWindowRegression()
{
	wchar_t program[32768] = {};
	if (!GetModuleFileNameW(nullptr, program, ARRAYSIZE(program))) return 2;
	std::wstring command = L"\"" + std::wstring(program) + L"\" --window-check";
	STARTUPINFOW startup = {};
	startup.cb = sizeof(startup);
	startup.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
	startup.wShowWindow = SW_HIDE;
	startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
	startup.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
	startup.hStdError = GetStdHandle(STD_ERROR_HANDLE);
	PROCESS_INFORMATION child = {};
	if (!CreateProcessW(program, &command[0], nullptr, nullptr, TRUE, 0, nullptr, nullptr, &startup, &child))
		return 2;
	CloseHandle(child.hThread);
	DWORD exitCode = 2;
	if (WaitForSingleObject(child.hProcess, 10000) == WAIT_OBJECT_0)
		GetExitCodeProcess(child.hProcess, &exitCode);
	else
	{
		TerminateProcess(child.hProcess, 2);
		WaitForSingleObject(child.hProcess, 5000);
	}
	CloseHandle(child.hProcess);
	return static_cast<int>(exitCode);
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
	std::cout << std::unitbuf;
	std::wcout << std::unitbuf;
	wchar_t ci[8] = {};
	const bool onCi = GetEnvironmentVariableW(L"GITHUB_ACTIONS", ci, 8) && wcscmp(ci, L"true") == 0;
	if (onCi && argc == 2 && wcscmp(argv[1], L"--window-check") == 0) return CheckHiddenStartup();
	if (onCi && argc == 2 && wcscmp(argv[1], L"--window-regression") == 0) return RunWindowRegression();
	if (onCi && argc == 2 && wcscmp(argv[1], L"--exit-check-0") == 0) { Sleep(500); return 0; }
	if (onCi && argc == 2 && wcscmp(argv[1], L"--exit-check-7") == 0) { Sleep(500); return 7; }
	const bool disposableVm = argc == 7 && wcscmp(argv[6], L"--disposable-vm") == 0;
	if ((!onCi && !disposableVm) || (argc != 6 && argc != 7))
	{
		std::cerr << "Use the disposable CI or spare-PC diagnostic script; requires four fixtures and a private browser profile.\n";
		return 2;
	}
	HANDLE token = nullptr;
	if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) return 2;
	DWORD size = 0;
	GetTokenInformation(token, TokenIntegrityLevel, nullptr, 0, &size);
	std::vector<BYTE> labelBytes(size);
	if (!size || !GetTokenInformation(token, TokenIntegrityLevel, labelBytes.data(), size, &size))
	{
		CloseHandle(token);
		return 2;
	}
	const auto label = reinterpret_cast<TOKEN_MANDATORY_LABEL*>(labelBytes.data());
	const auto sid = label->Label.Sid;
	const auto integrity = *GetSidSubAuthority(sid, *GetSidSubAuthorityCount(sid) - 1);
	CloseHandle(token);
	std::cout << "Probe integrity RID=" << integrity << '\n';
	if (integrity >= SECURITY_MANDATORY_HIGH_RID)
	{
		std::cerr << "Real drag/drop must be tested as a standard user.\n";
		return 2;
	}
DWORD sessionId = 0;
	ProcessIdToSessionId(GetCurrentProcessId(), &sessionId);
	USEROBJECTFLAGS station = {};
	DWORD stationSize = 0;
	if (!GetUserObjectInformationW(GetProcessWindowStation(), UOI_FLAGS, &station, sizeof(station), &stationSize))
	{
		std::cerr << "Cannot inspect the input window station: " << GetLastError() << '\n';
		return 2;
	}
	std::cout << "Windows session=" << sessionId << ", interactive station=" <<
		((station.dwFlags & WSF_VISIBLE) ? "yes" : "no") << '\n';
	if (!(station.dwFlags & WSF_VISIBLE))
	{
		std::cerr << "A real interactive Windows desktop is required; this runner cannot prove drag/drop.\n";
		return 2;
	}
	if (!CreateDirectoryW(argv[5], nullptr) && GetLastError() != ERROR_ALREADY_EXISTS)
	{
		std::cerr << "Cannot create the private browser profile: " << GetLastError() << '\n';
		return 2;
	}
	const std::wstring profileMarker = std::wstring(argv[5]) + L"\\probe-write-check";
	HANDLE marker = CreateFileW(profileMarker.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (marker == INVALID_HANDLE_VALUE)
	{
		std::cerr << "Standard user cannot write the private browser profile: " << GetLastError() << '\n';
		return 2;
	}
	CloseHandle(marker);
	std::cout << "PASS private browser profile is writable by the standard user\n";
	for (int i = 1; i <= 4; ++i) fixtures.emplace_back(argv[i]);
	if (FAILED(OleInitialize(nullptr))) return 2;
	LPWSTR version = nullptr;
	const auto versionResult = GetAvailableCoreWebView2BrowserVersionString(nullptr, &version);
	if (FAILED(versionResult)) { Fail("WebView2 Runtime unavailable", versionResult); OleUninitialize(); return 1; }
	std::wcout << L"WebView2 Runtime: " << version << L'\n';
	CoTaskMemFree(version);
	if (!CreateProbeWindow()) { OleUninitialize(); return 2; }
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
	if (dragThread.joinable()) dragThread.join();
	KillTimer(window, 1);
	if (controller) controller->Close();
	view.Reset(); controller.Reset(); environment.Reset();
	DestroyWindow(window);
	OleUninitialize();
	return result;
}
