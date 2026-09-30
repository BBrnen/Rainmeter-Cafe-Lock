// Test-only COM observation. No script execution or factory override is exported by Rainmeter.
#include "../Library/CafeShelf/Host.h"
#include <WebView2.h>
#include <wrl.h>
#include <iostream>
#include <functional>
#include <vector>
#include <fstream>
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
BOOL CALLBACK PrintResourceName(HMODULE, LPCWSTR type, LPWSTR name, LONG_PTR) {
	std::wcout << L"Embedded resource type=";
	if (IS_INTRESOURCE(type)) std::wcout << reinterpret_cast<ULONG_PTR>(type);
	else std::wcout << type;
	std::wcout << L" name=";
	if (IS_INTRESOURCE(name)) std::wcout << reinterpret_cast<ULONG_PTR>(name);
	else std::wcout << name;
	std::wcout << std::endl;
	return TRUE;
}
BOOL CALLBACK PrintResourceType(HMODULE module, LPWSTR type, LONG_PTR) {
	EnumResourceNamesW(module, type, PrintResourceName, 0);
	return TRUE;
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
	ObservedEnvironment(ICoreWebView2Environment* env, std::shared_ptr<Capture> capturedState) :
		inner(env), capture(std::move(capturedState)) {}
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
int wmain(int argc, wchar_t** argv) {
	if (FAILED(OleInitialize(nullptr))) return 2;
	// This is an isolated browser-policy test under the CI runner's own identity.
	// It does not claim filtered-token or real Explorer drop acceptance.
	std::cout << "Real embedded-browser policy checks; no mouse input or user skin writes." << std::endl;
	bool locked = false;
	auto capture = std::make_shared<Capture>();
	HostOptions options;
	options.module = GetModuleHandleW(nullptr);
	if(argc==2)options.shelfRoot=argv[1];
	EnumResourceTypesW(options.module, PrintResourceType, 0);
	for (const auto name : {L"CAFE_SHELF_HTML", L"CAFE_SHELF_JS", L"CAFE_SHELF_CSS"}) {
		const auto resource = FindResourceW(options.module, name, MAKEINTRESOURCEW(10));
		Check("trusted RCDATA resource embedded", resource && SizeofResource(options.module, resource) > 0);
	}
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
			Check("complete Add/Edit dialog is bundled", PageTrue(view.Get(),
				L"['itemModal','itemLabel','itemAction','itemIcon','itemSaveBtn','itemCancelBtn'].every(id=>document.getElementById(id))"));
			Check("launcher and custom icon drop surfaces are present", PageTrue(view.Get(),
				L"document.getElementById('launcherDrop')!==null && document.getElementById('iconDrop')!==null"));
			Check("native browse and folder controls are present", PageTrue(view.Get(),
				L"['browseLauncher','browseFolder','browseIcon'].every(id=>document.getElementById(id))"));
			Check("shelves tabs preview and status controls are present", PageTrue(view.Get(),
				L"['shelvesNav','tabsNav','itemsList','iconPreview','status'].every(id=>document.getElementById(id))"));
			const bool editorReady=Wait([&](){return PageTrue(view.Get(),
				L"document.querySelector('#itemsList .edit-item')!==null");},5000);
			Check("installed shelf loads into final interface",editorReady);
			if(editorReady) {
				Script(view.Get(),L"document.getElementById('addItemBtn').click();");
				Check("Add opens launcher fields",PageTrue(view.Get(),L"document.getElementById('itemModal').open"));
				Script(view.Get(),L"document.getElementById('itemLabel').value='Added fixture'; document.getElementById('itemAction').value='notepad.exe'; document.getElementById('itemIcon').value='file.png'; document.getElementById('itemSaveBtn').click(); document.getElementById('itemSaveBtn').click();");
				Check("Save waits for native commit and updates list once",Wait([&](){return PageTrue(view.Get(),
					L"!document.getElementById('itemModal').open && document.getElementById('status').textContent.includes('Saved') && [...document.querySelectorAll('#itemsList .item-label')].filter(x=>x.textContent==='Added fixture').length===1");}));
				std::ifstream config(options.shelfRoot+L"\\Shelf1\\config.lua",std::ios::binary);
				const std::string bytes(std::istreambuf_iterator<char>(config),{});
				config.close();
				Check("UI Save writes real config",bytes.find("Added fixture")!=std::string::npos);
				Script(view.Get(),L"document.querySelector('#itemsList .edit-item').click(); document.getElementById('itemLabel').value='<img src=x onerror=window.attacked=1>'; document.getElementById('itemSaveBtn').click();");
				Check("Edit preserves literal HTML without script execution",Wait([&](){return PageTrue(view.Get(),
					L"!document.getElementById('itemModal').open && document.getElementById('itemsList').textContent.includes('<img src=x onerror=window.attacked=1>') && !window.attacked && !document.querySelector('#itemsList img');");}));
				Script(view.Get(),L"document.getElementById('addItemBtn').click(); document.getElementById('itemLabel').value='Keep draft'; document.getElementById('itemAction').value='bad.exe\"][!Quit]'; document.getElementById('itemSaveBtn').click();");
				Check("failed native Save keeps draft open",Wait([&](){return PageTrue(view.Get(),
					L"document.getElementById('itemModal').open && document.getElementById('itemLabel').value==='Keep draft' && !document.getElementById('itemSaveBtn').disabled && document.getElementById('itemError').textContent.length>0");}));
				Script(view.Get(),L"document.getElementById('itemCancelBtn').click();");
				Check("Cancel closes draft without adding it",Wait([&](){return PageTrue(view.Get(),
					L"!document.getElementById('itemModal').open && !document.getElementById('itemsList').textContent.includes('Keep draft')");}));
				// Renderer-only transport shim: proves UI wiring, not Windows picker/drop provenance.
				Script(view.Get(),L"window.nativeRequest=request; window.uiCalls=[]; request=(op,payload={},files=[])=>new Promise((resolve,reject)=>{uiCalls.push({op,payload,files});window.completeImport=resolve;window.failImport=reject;}); document.getElementById('addItemBtn').click(); document.getElementById('browseLauncher').click();");
				Check("Browse launcher requests native selection and disables Save",PageTrue(view.Get(),
					L"uiCalls[0].op==='browseLauncher' && document.getElementById('itemSaveBtn').disabled"));
				Script(view.Get(),L"completeImport({name:'Shortcut fixture',action:'C:\\\\Fixture\\\\Original.lnk',iconId:1,iconName:'shortcut.png',preview:'data:image/png;base64,iVBORw0KGgo='});");
				Check("native launcher result fills name action and preview",Wait([&](){return PageTrue(view.Get(),
					L"document.getElementById('itemLabel').value==='Shortcut fixture' && document.getElementById('itemAction').value==='C:\\\\Fixture\\\\Original.lnk' && !document.getElementById('iconPreview').hidden && !document.getElementById('itemSaveBtn').disabled");}));
				Script(view.Get(),L"document.getElementById('browseFolder').click(); completeImport({name:'Folder fixture',action:'C:\\\\Fixture\\\\Folder',warning:'No icon'});");
				Check("Browse folder uses its separate native operation",Wait([&](){return PageTrue(view.Get(),
					L"uiCalls[1].op==='browseFolder' && document.getElementById('itemLabel').value==='Folder fixture' && !document.getElementById('itemSaveBtn').disabled");}));
				Script(view.Get(),L"document.getElementById('browseIcon').click(); completeImport({iconId:2,iconName:'custom.png',preview:'data:image/png;base64,iVBORw0KGgo='});");
				Check("custom icon result keeps launcher action",Wait([&](){return PageTrue(view.Get(),
					L"uiCalls[2].op==='browseIcon' && document.getElementById('itemIcon').value==='custom.png' && document.getElementById('itemAction').value==='C:\\\\Fixture\\\\Folder' && !document.getElementById('itemSaveBtn').disabled");}));
				Script(view.Get(),L"window.dropData=new DataTransfer(); dropData.items.add(new File(['fixture'],'fixture.lnk')); document.getElementById('launcherDrop').dispatchEvent(new DragEvent('drop',{bubbles:true,cancelable:true,dataTransfer:dropData}));");
				Check("drop forwards original File object and typed purpose",PageTrue(view.Get(),
					L"uiCalls[3].op==='importDrop' && uiCalls[3].payload.purpose==='launcher' && uiCalls[3].files[0]===dropData.files[0]"));
				Script(view.Get(),L"completeImport({name:'Drop fixture',action:'C:\\\\Fixture\\\\Drop.lnk',warning:'Keep default'});");
				Wait([&](){return PageTrue(view.Get(),L"!document.getElementById('itemSaveBtn').disabled");});
				Script(view.Get(),L"document.getElementById('status').textContent='Saved.'; document.getElementById('itemSaveBtn').click();");
				Check("pending Save neither closes draft nor announces success",PageTrue(view.Get(),
					L"document.getElementById('itemModal').open && document.getElementById('itemSaveBtn').disabled && !document.getElementById('status').textContent.includes('Saved')"));
				Script(view.Get(),L"failImport(new Error('Simulated write failure'));");
				Check("delayed Save error keeps user entries",Wait([&](){return PageTrue(view.Get(),
					L"document.getElementById('itemModal').open && document.getElementById('itemLabel').value==='Drop fixture' && document.getElementById('itemError').textContent==='Simulated write failure' && !document.getElementById('itemSaveBtn').disabled");}));
				Script(view.Get(),L"request=nativeRequest; document.getElementById('itemCancelBtn').click();");
			}
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
				return PageTrue(view.Get(), L"testReplies.some(x=>x.id===100 && x.ok && x.data.mode==='maintenance' && Array.isArray(x.data.shelves))");
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
			auto popupHandled = std::make_shared<bool>(false);
			EventRegistrationToken popupToken = {};
			view->add_NewWindowRequested(Callback<ICoreWebView2NewWindowRequestedEventHandler>(
				[popupHandled](ICoreWebView2*, ICoreWebView2NewWindowRequestedEventArgs* args) -> HRESULT {
					BOOL handled = FALSE; args->get_Handled(&handled);
					*popupHandled = handled != FALSE; return S_OK;
				}).Get(), &popupToken);
			Script(view.Get(), L"window.open('https://example.invalid');");
			Check("popup is consumed by host without external launch", Wait([&]() { return *popupHandled; }));
			view->remove_NewWindowRequested(popupToken);
			Script(view.Get(), L"window.fabricatedRejected=false; try { chrome.webview.postMessageWithAdditionalObjects(JSON.stringify({id:104,op:'importDrop',payload:{purpose:'launcher'}}),[new File(['fake'],'fake.lnk')]); } catch(e) { fabricatedRejected=true; } chrome.webview.postMessage(JSON.stringify({id:105,op:'load',payload:{}}));");
			Check("fabricated browser File grants no native selection", Wait([&]() {
				return PageTrue(view.Get(), L"testReplies.some(x=>x.id===105) && !testReplies.some(x=>x.id===104 && x.ok) && (fabricatedRejected || testReplies.some(x=>x.id===104 && !x.ok))");
			}));
			Script(view.Get(), L"chrome.webview.postMessage(JSON.stringify({id:106,op:'lockNow',payload:{}}));");
			Check("real page lock revokes host and invokes native lock", Wait([&]() { return locked && !host.Window(); }));
			Check("locked host cannot reopen", !host.Open() && !host.Window());
			view.Reset();
			for (const bool frame : {false, true}) {
				locked = false;
				capture->view.Reset();
				capture->loaded = false; capture->failed = false; capture->errors = 0;
				const bool opened = host.Open() && Wait([&]() {
					return capture->loaded || capture->failed || capture->errors != 0;
				}, 45000) && capture->loaded && capture->view && host.Window();
				Check(frame ? "new host for frame rejection" : "new host for navigation rejection", opened);
				if (!opened) break;
				if (frame)
					Script(capture->view.Get(), L"document.body.appendChild(document.createElement('iframe'));");
				else
					capture->view->Navigate(L"https://example.invalid/untrusted");
				Check(frame ? "frame creation revokes and closes editor" : "unexpected navigation revokes and closes editor",
					Wait([&]() { return !host.Window() && capture->errors != 0; }));
				host.Close();
			}
		}
		host.Close();
	}
	capture->view.Reset();
	OleUninitialize();
	std::cout << checks << " real browser checks, " << failures << " failures" << std::endl;
	return failures ? 1 : 0;
}
