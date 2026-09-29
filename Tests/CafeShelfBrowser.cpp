// Test-only COM observation. No script execution or factory override is exported by Rainmeter.
#include "../Library/CafeShelf/Host.h"
#include <WebView2.h>
#include <wrl.h>
#include <iostream>
#include <functional>
#include <vector>
using Microsoft::WRL::Callback;
using Microsoft::WRL::ComPtr;
using namespace CafeShelf;
namespace {
int checks = 0, failures = 0;
void Check(const char* name, bool ok) {
	++checks; if (!ok) ++failures;
	std::cout << (ok ? "PASS " : "FAIL ") << name << std::endl;
}
bool Wait(const std::function<bool()>& predicate, DWORD timeout = 15000) {
	const auto end = GetTickCount64() + timeout;
	do {
		MSG msg;
		while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg); DispatchMessageW(&msg);
		}
		if (predicate()) return true;
		MsgWaitForMultipleObjects(0, nullptr, FALSE, 20, QS_ALLINPUT);
	} while (GetTickCount64() < end);
	return predicate();
}
struct Capture {
	ComPtr<ICoreWebView2> view;
	bool loaded = false;
	bool failed = false;
	int errors = 0;
};
class ObservedEnvironment final : public Microsoft::WRL::RuntimeClass<
	Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>, ICoreWebView2Environment> {
public:
	ObservedEnvironment(ICoreWebView2Environment* env, std::shared_ptr<Capture> value) :
		inner(env), capture(std::move(value)) {}
	HRESULT STDMETHODCALLTYPE CreateCoreWebView2Controller(HWND parent,
		ICoreWebView2CreateCoreWebView2ControllerCompletedHandler* handler) override {
		auto state = capture;
		ComPtr<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler> completion(handler);
		return inner->CreateCoreWebView2Controller(parent,
			Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
			[state, completion](HRESULT hr, ICoreWebView2Controller* controller) -> HRESULT {
				if (SUCCEEDED(hr) && controller) {
					controller->get_CoreWebView2(&state->view);
					if (state->view) {
						EventRegistrationToken token = {};
						state->view->add_NavigationCompleted(
							Callback<ICoreWebView2NavigationCompletedEventHandler>(
							[state](ICoreWebView2*, ICoreWebView2NavigationCompletedEventArgs* args) -> HRESULT {
								BOOL success = FALSE;
								args->get_IsSuccess(&success);
								state->loaded = success != FALSE;
								state->failed = success == FALSE;
								return S_OK;
							}).Get(), &token);
					}
				}
				return completion->Invoke(hr, controller);
			}).Get());
	}
	HRESULT STDMETHODCALLTYPE CreateWebResourceResponse(IStream* stream, int code,
		LPCWSTR reason, LPCWSTR headers, ICoreWebView2WebResourceResponse** response) override {
		return inner->CreateWebResourceResponse(stream, code, reason, headers, response);
	}
	HRESULT STDMETHODCALLTYPE get_BrowserVersionString(LPWSTR* version) override {
		return inner->get_BrowserVersionString(version);
	}
	HRESULT STDMETHODCALLTYPE add_NewBrowserVersionAvailable(
		ICoreWebView2NewBrowserVersionAvailableEventHandler* handler, EventRegistrationToken* token) override {
		return inner->add_NewBrowserVersionAvailable(handler, token);
	}
	HRESULT STDMETHODCALLTYPE remove_NewBrowserVersionAvailable(EventRegistrationToken token) override {
		return inner->remove_NewBrowserVersionAvailable(token);
	}
private:
	ComPtr<ICoreWebView2Environment> inner;
	std::shared_ptr<Capture> capture;
};
struct ScriptResult { bool done = false; HRESULT hr = E_PENDING; std::wstring json; };
std::wstring Script(ICoreWebView2* view, const wchar_t* code) {
	if (!view) return L"unavailable";
	auto result = std::make_shared<ScriptResult>();
	const auto hr = view->ExecuteScript(code, Callback<ICoreWebView2ExecuteScriptCompletedHandler>(
		[result](HRESULT status, LPCWSTR json) -> HRESULT {
			result->hr = status; result->json = json ? json : L""; result->done = true;
			return S_OK;
		}).Get());
	if (FAILED(hr) || !Wait([result]() { return result->done; }) || FAILED(result->hr)) return L"failed";
	return result->json;
}
bool PageTrue(ICoreWebView2* view, const wchar_t* code) { return Script(view, code) == L"true"; }
}
int main() {
	if (FAILED(OleInitialize(nullptr))) return 2;
	// This is an isolated browser-policy test under the CI runner's own identity.
	// It does not claim filtered-token or real Explorer drop acceptance.
	std::cout << "Real embedded-browser policy checks; no mouse input or user skin writes." << std::endl;
	bool locked = false;
	auto capture = std::make_shared<Capture>();
	HostOptions options;
	options.module = GetModuleHandleW(nullptr);
	options.isLocked = [&locked]() { return locked; };
	options.lockNow = [&locked]() { locked = true; };
	options.reportError = [capture](const wchar_t* message) {
		++capture->errors; std::wcerr << L"Browser startup error: " << message << std::endl;
	};
	options.createEnvironment = [capture](LPCWSTR profile,
		ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler* handler) -> HRESULT {
		ComPtr<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler> completion(handler);
		return CreateCoreWebView2EnvironmentWithOptions(nullptr, profile, nullptr,
			Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
			[capture, completion](HRESULT hr, ICoreWebView2Environment* env) -> HRESULT {
				if (FAILED(hr) || !env) return completion->Invoke(hr, nullptr);
				auto observed = Microsoft::WRL::Make<ObservedEnvironment>(env, capture);
				return completion->Invoke(S_OK, observed.Get());
			}).Get());
	};
	{
		Host host(options);
		Check("real host starts", host.Open());
		const bool ready = Wait([&]() { return capture->loaded || capture->failed || capture->errors != 0; }, 45000) &&
			capture->loaded && capture->view && host.Window();
		Check("bundled trusted page loads in real WebView2", ready);
		if (ready) {
			auto view = capture->view;
			Check("bundled script and exact origin are active", PageTrue(view.Get(),
				L"location.href === 'https://cafe-shelf.invalid/index.html' && typeof request === 'function'"));
			ComPtr<ICoreWebView2Settings> settings;
			ComPtr<ICoreWebView2Settings3> settings3;
			ComPtr<ICoreWebView2Settings4> settings4;
			BOOL dev = TRUE, menus = TRUE, objects = TRUE, accelerator = TRUE, passwords = TRUE, autofill = TRUE;
			view->get_Settings(&settings);
			if (settings) {
				settings->get_AreDevToolsEnabled(&dev);
				settings->get_AreDefaultContextMenusEnabled(&menus);
				settings->get_AreHostObjectsAllowed(&objects);
				settings.As(&settings3); settings.As(&settings4);
			}
			if (settings3) settings3->get_AreBrowserAcceleratorKeysEnabled(&accelerator);
			if (settings4) {
				settings4->get_IsPasswordAutosaveEnabled(&passwords);
				settings4->get_IsGeneralAutofillEnabled(&autofill);
			}
			Check("browser management and host objects disabled", !dev && !menus && !objects && !accelerator && !passwords && !autofill);
			Script(view.Get(), L"window.testReplies=[]; chrome.webview.addEventListener('message', e=>testReplies.push(e.data)); chrome.webview.postMessage(JSON.stringify({id:100,op:'load',payload:{}}));");
			Check("valid native request gets maintenance-only reply", Wait([&]() {
				return PageTrue(view.Get(), L"testReplies.some(x=>x.id===100 && x.ok && x.data.mode==='maintenance' && x.data.canSave===false)");
			}));
			Script(view.Get(), L"chrome.webview.postMessage(JSON.stringify({id:101,op:'importDrop',payload:{purpose:'launcher',path:'C:\\\\fake.exe'}})); chrome.webview.postMessage(JSON.stringify({id:101,op:'load',payload:{}}));");
			Check("page-supplied path rejected before consuming request id", Wait([&]() {
				return PageTrue(view.Get(), L"testReplies.some(x=>x.id===101 && x.ok && x.data.mode==='maintenance')");
			}));
			Script(view.Get(), L"chrome.webview.postMessage(JSON.stringify({id:101,op:'load',payload:{}})); chrome.webview.postMessage(JSON.stringify({id:102,op:'load',payload:{}}));");
			Check("replayed request never produces second response", Wait([&]() {
				return PageTrue(view.Get(), L"testReplies.some(x=>x.id===102) && testReplies.filter(x=>x.id===101).length===1");
			}));
			Script(view.Get(), L"chrome.webview.postMessage(JSON.stringify({id:103,op:'importDrop',payload:{purpose:'launcher'}}));");
			Check("drop without native object is rejected", Wait([&]() {
				return PageTrue(view.Get(), L"testReplies.some(x=>x.id===103 && !x.ok)");
			}));
			// Native request IDs bypass the UI's counter above. Use native transport
			// again for Lock Now, with a larger ID, not an out-of-date UI request.
			Script(view.Get(), L"window.networkBlocked=false; fetch('https://example.invalid/no-network').then(()=>{},()=>{networkBlocked=true;});");
			Check("CSP blocks page network requests", Wait([&]() { return PageTrue(view.Get(), L"networkBlocked"); }));
			Script(view.Get(), L"window.popupResult=window.open('https://example.invalid');");
			Check("popup does not replace trusted host", IsWindow(host.Window()) &&
				PageTrue(view.Get(), L"location.href==='https://cafe-shelf.invalid/index.html'"));
			Script(view.Get(), L"chrome.webview.postMessage(JSON.stringify({id:104,op:'lockNow',payload:{}}));");
			Check("real page lock revokes host and invokes native lock", Wait([&]() { return locked && !host.Window(); }));
			Check("locked host cannot reopen", !host.Open() && !host.Window());
			view.Reset();
		}
		host.Close();
	}
	capture->view.Reset();
	OleUninitialize();
	std::cout << checks << " real browser checks, " << failures << " failures" << std::endl;
	return failures ? 1 : 0;
}
