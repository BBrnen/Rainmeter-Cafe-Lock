#include "Host.h"
#include "Controller.h"
#include "Selection.h"
#include <WebView2.h>
#include <wrl.h>
#include <shlwapi.h>
#include <deque>
#include <utility>

using Microsoft::WRL::Callback;
using Microsoft::WRL::ComPtr;

namespace CafeShelf
{
namespace
{
constexpr UINT WorkMessage = WM_APP + 240;
constexpr UINT FailureMessage = WM_APP + 241;
constexpr UINT LockMessage = WM_APP + 242;
struct CoString
{
	LPWSTR value = nullptr;
	~CoString() { CoTaskMemFree(value); }
	std::wstring Read() const
	{
		if (!value) return {};
		const auto size = wcsnlen_s(value, MaxMessageBytes + 1);
		return size <= MaxMessageBytes ? std::wstring(value, size) : std::wstring();
	}
};
std::string Utf8(const std::wstring& value)
{
	if (value.empty() || value.size() > MaxMessageBytes) return {};
	const int size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(),
		static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
	if (size <= 0 || size > static_cast<int>(MaxMessageBytes)) return {};
	std::string output(size, '\0');
	if (!WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(),
		static_cast<int>(value.size()), &output[0], size, nullptr, nullptr)) return {};
	return output;
}
std::wstring Wide(const std::string& value)
{
	if (value.empty() || value.size() > MaxMessageBytes) return {};
	const int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
		static_cast<int>(value.size()), nullptr, 0);
	if (size <= 0) return {};
	std::wstring output(size, L'\0');
	if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
		static_cast<int>(value.size()), &output[0], size)) return {};
	return output;
}
std::wstring NewProfile()
{
	wchar_t temporary[32768] = {};
	const auto size = GetTempPathW(ARRAYSIZE(temporary), temporary);
	if (!size || size >= ARRAYSIZE(temporary)) return {};
	GUID id = {};
	wchar_t name[40] = {};
	if (FAILED(CoCreateGuid(&id)) || !StringFromGUID2(id, name, ARRAYSIZE(name))) return {};
	const auto path = std::wstring(temporary) + L"RainmeterCafeShelf-" + name;
	// Exclusive creation: never reuse another session's profile or browser grants.
	return CreateDirectoryW(path.c_str(), nullptr) ? path : std::wstring();
}
std::shared_ptr<Host> activeHost;
}

struct Host::State : std::enable_shared_from_this<Host::State>
{
	explicit State(const HostOptions& value) : options(value), control(value.isLocked),
		selection([this](Ticket t) { return control.Allows(t); }) {}
	~State() { Close(); }

	struct Work { Ticket ticket; Request request; ComPtr<IUnknown> object; };
	HostOptions options;
	Controller control;
	Selection selection;
	Ticket ticket;
	HWND window = nullptr;
	bool closed = false;
	bool busy = false;
	std::wstring profile;
	std::wstring failure;
	std::deque<Work> queue;
	ComPtr<ICoreWebView2Environment> environment;
	ComPtr<ICoreWebView2Controller> browser;
	ComPtr<ICoreWebView2> view;

	bool Allowed() const { return !closed && control.Allows(ticket); }

	void Close()
	{
		// This ordering applies even while a native picker pumps nested messages.
		control.Revoke();
		closed = true;
		selection.Revoke();
		queue.clear();
		if (browser) browser->Close();
		view.Reset(); browser.Reset(); environment.Reset();
		if (window)
		{
			const auto old = window;
			window = nullptr;
			DestroyWindow(old);
		}
	}

	void FailLater(const wchar_t* message)
	{
		if (closed) return;
		control.Revoke();
		selection.Revoke();
		queue.clear();
		failure = message;
		if (window) PostMessageW(window, FailureMessage, 0, 0);
	}

	template<typename F>
	static HRESULT Event(const std::weak_ptr<State>& weak, F action) noexcept
	{
		try
		{
			if (auto self = weak.lock()) action(*self);
		}
		catch (...)
		{
			if (auto self = weak.lock())
			{
				// No exception crosses a COM callback.
				self->Close();
			}
			return E_FAIL;
		}
		return S_OK;
	}

	static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wp, LPARAM lp)
	{
		auto self = reinterpret_cast<State*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
		if (message == WM_NCCREATE)
		{
			self = static_cast<State*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);
			SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
		}
		if (!self) return DefWindowProcW(hwnd, message, wp, lp);
		try
		{
			auto keepAlive = self->shared_from_this();
			switch (message)
			{
			case LockMessage:
				if (self->options.lockNow) self->options.lockNow();
				self->Close();
				return 0;
			case WorkMessage:
				self->Drain();
				return 0;
			case FailureMessage:
			{
				const auto error = self->failure;
				auto report = self->options.reportError;
				self->Close();
				if (report && !error.empty()) report(error.c_str());
				return 0;
			}
			case WM_CLOSE:
				self->Close();
				return 0;
			case WM_QUERYENDSESSION:
				self->control.Revoke();
				self->selection.Revoke();
				return TRUE;
			case WM_ENDSESSION:
				if (wp) self->Close();
				return 0;
			case WM_SIZE:
				if (self->browser)
				{
					RECT bounds = {};
					GetClientRect(hwnd, &bounds);
					self->browser->put_Bounds(bounds);
				}
				return 0;
			case WM_NCDESTROY:
				SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
				break;
			}
		}
		catch (...) { self->Close(); return 0; }
		return DefWindowProcW(hwnd, message, wp, lp);
	}

	bool Start()
	{
		ticket = control.Open();
		if (!Allowed()) { closed = true; return false; }
		WNDCLASSW cls = {};
		cls.hInstance = options.module;
		cls.lpfnWndProc = WindowProc;
		cls.lpszClassName = L"RainmeterCafeShelfEditor";
		cls.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
		RegisterClassW(&cls);
		window = CreateWindowW(cls.lpszClassName, L"ShelfSuite - Maintenance Mode",
			WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 1050, 740,
			nullptr, nullptr, options.module, this);
		if (!window) { Close(); return false; }
		ShowWindow(window, SW_SHOWDEFAULT);
		ShowWindow(window, SW_RESTORE);
		profile = NewProfile();
		if (profile.empty())
		{
			Close();
			if (options.reportError) options.reportError(L"Could not create a private editor browser profile.");
			return false;
		}
		const std::weak_ptr<State> weak = shared_from_this();
		auto callback = Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
			[weak](HRESULT hr, ICoreWebView2Environment* env) -> HRESULT
			{
				return Event(weak, [hr, env](State& self)
				{
					if (!self.Allowed()) return;
					if (FAILED(hr) || !env)
					{
						self.FailLater(L"The ShelfSuite editor could not start WebView2. Install or repair the Microsoft WebView2 Runtime.");
						return;
					}
					self.environment = env;
					self.CreateBrowser();
				});
			});
		const auto hr = options.createEnvironment ?
			options.createEnvironment(profile.c_str(), callback.Get()) :
			CreateCoreWebView2EnvironmentWithOptions(nullptr, profile.c_str(), nullptr, callback.Get());
		if (FAILED(hr))
		{
			Close();
			if (options.reportError) options.reportError(L"The ShelfSuite editor needs the Microsoft WebView2 Runtime. Ordinary skin launchers remain available.");
			return false;
		}
		return !closed;
	}

	void CreateBrowser()
	{
		const std::weak_ptr<State> weak = shared_from_this();
		const auto hr = environment->CreateCoreWebView2Controller(window,
			Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
				[weak](HRESULT ready, ICoreWebView2Controller* controller) -> HRESULT
				{
					auto self = weak.lock();
					if (!self || !self->Allowed())
					{
						if (controller) controller->Close();
						return S_OK;
					}
					return Event(weak, [ready, controller](State& state)
					{
						if (FAILED(ready) || !controller) { state.FailLater(L"Windows could not create the editor browser."); return; }
						state.browser = controller;
						if (FAILED(controller->get_CoreWebView2(&state.view)) || !state.Configure())
						{
							state.FailLater(L"The editor requires a newer or repaired Microsoft WebView2 Runtime.");
							return;
						}
						RECT bounds = {};
						GetClientRect(state.window, &bounds);
						controller->put_Bounds(bounds);
						controller->put_IsVisible(TRUE);
						if (FAILED(state.view->Navigate(HostPolicy::Page())))
							state.FailLater(L"The bundled ShelfSuite editor could not be loaded.");
					});
				}).Get());
		if (FAILED(hr)) FailLater(L"Windows could not initialize the editor browser.");
	}

	bool Configure()
	{
		ComPtr<ICoreWebView2Settings> settings;
		ComPtr<ICoreWebView2Settings3> settings3;
		ComPtr<ICoreWebView2Settings4> settings4;
		ComPtr<ICoreWebView2_4> view4;
		ComPtr<ICoreWebView2_18> view18;
		if (FAILED(view->get_Settings(&settings)) || FAILED(settings.As(&settings3)) ||
			FAILED(settings.As(&settings4)) || FAILED(view.As(&view4)) || FAILED(view.As(&view18))) return false;
		if (FAILED(settings->put_AreDevToolsEnabled(FALSE)) ||
			FAILED(settings->put_AreDefaultContextMenusEnabled(FALSE)) ||
			FAILED(settings->put_AreHostObjectsAllowed(FALSE)) ||
			FAILED(settings->put_AreDefaultScriptDialogsEnabled(FALSE)) ||
			FAILED(settings->put_IsStatusBarEnabled(FALSE)) ||
			FAILED(settings->put_IsWebMessageEnabled(TRUE)) ||
			FAILED(settings3->put_AreBrowserAcceleratorKeysEnabled(FALSE)) ||
			FAILED(settings4->put_IsPasswordAutosaveEnabled(FALSE)) ||
			FAILED(settings4->put_IsGeneralAutofillEnabled(FALSE))) return false;
		const std::weak_ptr<State> weak = shared_from_this();
		EventRegistrationToken token = {};
		if (FAILED(view->add_NavigationStarting(Callback<ICoreWebView2NavigationStartingEventHandler>(
			[weak](ICoreWebView2*, ICoreWebView2NavigationStartingEventArgs* args) -> HRESULT
			{
				// Deny first; only this precise trusted native document may navigate.
				args->put_Cancel(TRUE);
				return Event(weak, [args](State& self)
				{
					CoString uri;
					if (self.Allowed() && SUCCEEDED(args->get_Uri(&uri.value)) && uri.Read() == HostPolicy::Page())
						args->put_Cancel(FALSE);
					else self.FailLater(L"The editor was closed because it attempted an unexpected navigation.");
				});
			}).Get(), &token))) return false;
		if (FAILED(view->add_NewWindowRequested(Callback<ICoreWebView2NewWindowRequestedEventHandler>(
			[](ICoreWebView2*, ICoreWebView2NewWindowRequestedEventArgs* args) -> HRESULT { return args->put_Handled(TRUE); }).Get(), &token))) return false;
		if (FAILED(view->add_PermissionRequested(Callback<ICoreWebView2PermissionRequestedEventHandler>(
			[](ICoreWebView2*, ICoreWebView2PermissionRequestedEventArgs* args) -> HRESULT { return args->put_State(COREWEBVIEW2_PERMISSION_STATE_DENY); }).Get(), &token))) return false;
		if (FAILED(view4->add_DownloadStarting(Callback<ICoreWebView2DownloadStartingEventHandler>(
			[](ICoreWebView2*, ICoreWebView2DownloadStartingEventArgs* args) -> HRESULT { return args->put_Cancel(TRUE); }).Get(), &token))) return false;
		if (FAILED(view18->add_LaunchingExternalUriScheme(Callback<ICoreWebView2LaunchingExternalUriSchemeEventHandler>(
			[](ICoreWebView2*, ICoreWebView2LaunchingExternalUriSchemeEventArgs* args) -> HRESULT { return args->put_Cancel(TRUE); }).Get(), &token))) return false;
		if (FAILED(view4->add_FrameCreated(Callback<ICoreWebView2FrameCreatedEventHandler>(
			[weak](ICoreWebView2*, ICoreWebView2FrameCreatedEventArgs*) -> HRESULT
			{ return Event(weak, [](State& self) { self.FailLater(L"Frames are not allowed in the ShelfSuite editor."); }); }).Get(), &token))) return false;
		if (FAILED(view->add_ProcessFailed(Callback<ICoreWebView2ProcessFailedEventHandler>(
			[weak](ICoreWebView2*, ICoreWebView2ProcessFailedEventArgs*) -> HRESULT
			{ return Event(weak, [](State& self) { self.FailLater(L"The editor browser stopped. Unsaved changes were not written."); }); }).Get(), &token))) return false;
		if (FAILED(view->add_WindowCloseRequested(Callback<ICoreWebView2WindowCloseRequestedEventHandler>(
			[weak](ICoreWebView2*, IUnknown*) -> HRESULT
			{ return Event(weak, [](State& self) { self.control.Revoke(); if (self.window) PostMessageW(self.window, WM_CLOSE, 0, 0); }); }).Get(), &token))) return false;
		if (FAILED(view->AddWebResourceRequestedFilter(L"*", COREWEBVIEW2_WEB_RESOURCE_CONTEXT_ALL))) return false;
		if (FAILED(view->add_WebResourceRequested(Callback<ICoreWebView2WebResourceRequestedEventHandler>(
			[weak](ICoreWebView2*, ICoreWebView2WebResourceRequestedEventArgs* args) -> HRESULT
			{ return Event(weak, [args](State& self) { self.Resource(args); }); }).Get(), &token))) return false;
		if (FAILED(view->add_WebMessageReceived(Callback<ICoreWebView2WebMessageReceivedEventHandler>(
			[weak](ICoreWebView2*, ICoreWebView2WebMessageReceivedEventArgs* args) -> HRESULT
			{ return Event(weak, [args](State& self) { self.Receive(args); }); }).Get(), &token))) return false;
		return true;
	}

	void Resource(ICoreWebView2WebResourceRequestedEventArgs* args)
	{
		ComPtr<ICoreWebView2WebResourceResponse> response;
		// Set a denial response before inspecting any request data.
		if (FAILED(environment->CreateWebResourceResponse(nullptr, 403, L"Forbidden",
			L"Content-Type: text/plain\r\nCache-Control: no-store", &response)) ||
			FAILED(args->put_Response(response.Get()))) { Close(); return; }
		if (!Allowed()) return;
		ComPtr<ICoreWebView2WebResourceRequest> request;
		CoString uri, method;
		if (FAILED(args->get_Request(&request)) || FAILED(request->get_Uri(&uri.value)) ||
			FAILED(request->get_Method(&method.value))) return;
		const auto path = uri.Read();
		if (!HostPolicy::AllowsResource(path, method.Read())) return;
		const wchar_t* name = path == HostPolicy::Page() ? L"CAFE_SHELF_HTML" :
			path == L"https://cafe-shelf.invalid/app.js" ? L"CAFE_SHELF_JS" : L"CAFE_SHELF_CSS";
		const wchar_t* type = path == HostPolicy::Page() ? L"text/html; charset=utf-8" :
			path == L"https://cafe-shelf.invalid/app.js" ? L"text/javascript; charset=utf-8" : L"text/css; charset=utf-8";
		const auto resource = FindResourceW(options.module, name, MAKEINTRESOURCEW(10));
		const auto size = resource ? SizeofResource(options.module, resource) : 0;
		const auto bytes = resource ? LockResource(LoadResource(options.module, resource)) : nullptr;
		if (!bytes || !size || size > 4 * 1024 * 1024) { FailLater(L"The bundled editor resources are missing."); return; }
		ComPtr<IStream> stream;
		stream.Attach(SHCreateMemStream(static_cast<const BYTE*>(bytes), size));
		const auto headers = std::wstring(L"Content-Type: ") + type +
			L"\r\nX-Content-Type-Options: nosniff\r\nCache-Control: no-store\r\nX-Frame-Options: DENY"
			L"\r\nContent-Security-Policy: default-src 'none'; script-src 'self'; style-src 'self'; img-src data:; "
			L"connect-src 'none'; frame-src 'none'; object-src 'none'; base-uri 'none'; form-action 'none'; frame-ancestors 'none'";
		response.Reset();
		if (!stream || FAILED(environment->CreateWebResourceResponse(stream.Get(), 200, L"OK", headers.c_str(), &response)) ||
			FAILED(args->put_Response(response.Get()))) FailLater(L"The editor resources could not be served.");
	}

	void Reply(const Response& response)
	{
		if (!Allowed() || !view) return;
		const auto text = Wide(EncodeResponse(response).dump());
		if (text.empty() || FAILED(view->PostWebMessageAsJson(text.c_str())))
			FailLater(L"The editor could not receive its result.");
	}

	void Receive(ICoreWebView2WebMessageReceivedEventArgs* args)
	{
		if (!Allowed()) return;
		CoString source, message;
		if (FAILED(args->get_Source(&source.value)) || FAILED(args->TryGetWebMessageAsString(&message.value))) return;
		auto decoded = control.Receive(ticket, source.Read(), true, Utf8(message.Read()));
		if (!decoded.ok) return;
		ComPtr<ICoreWebView2WebMessageReceivedEventArgs2> args2;
		ComPtr<ICoreWebView2ObjectCollectionView> objects;
		if (FAILED(args->QueryInterface(IID_PPV_ARGS(&args2))) || FAILED(args2->get_AdditionalObjects(&objects)))
		{
			FailLater(L"This WebView2 Runtime does not support native file drops. Update it before editing.");
			return;
		}
		UINT count = 0;
		if (objects && FAILED(objects->get_Count(&count))) return;
		Work work{ticket, decoded.value, {}};
		if (work.request.operation == Operation::ImportDrop)
		{
			if (count != 1 || FAILED(objects->GetValueAtIndex(0, &work.object)) || !work.object)
			{
				Reply({work.request.id, false, Error::InvalidInput, {{"message", "Drop one file or folder at a time."}}});
				return;
			}
		}
		else if (count != 0) return;
		if (work.request.operation == Operation::LockNow)
		{
			// Lock has priority over a modal picker and any queued ordinary work.
			control.Revoke();
			selection.Revoke();
			queue.clear();
			PostMessageW(window, LockMessage, 0, 0);
			return;
		}
		if (queue.size() >= 32) { FailLater(L"Too many pending editor requests."); return; }
		queue.push_back(std::move(work));
		// File dialogs must not open inside a WebView callback's stack.
		PostMessageW(window, WorkMessage, 0, 0);
	}

	void Drain()
	{
		if (busy || queue.empty() || !Allowed()) return;
		auto work = std::move(queue.front());
		queue.pop_front();
		if (!control.Allows(work.ticket)) return;
		busy = true;
		Response reply{work.request.id, false, Error::Unsupported, Json::object()};
		switch (work.request.operation)
		{
		case Operation::LockNow:
			control.Revoke();
			selection.Revoke();
			if (options.lockNow) options.lockNow();
			Close();
			busy = false;
			return;
		case Operation::CancelDraft:
			control.CancelDraft(work.ticket);
			reply.ok = true; reply.code = Error::None;
			break;
		case Operation::Load:
			reply.ok = true; reply.code = Error::None;
			reply.data = {{"mode", "maintenance"}, {"canSave", false}};
			break;
		case Operation::BrowseLauncher:
		case Operation::BrowseFolder:
		case Operation::BrowseIcon:
		case Operation::ImportDrop:
		{
			auto selected = work.request.operation == Operation::ImportDrop ?
				selection.AcceptDrop(work.object.Get(), work.ticket) :
				selection.Pick(window, work.request.operation == Operation::BrowseFolder ? SelectionKind::Folder :
					work.request.operation == Operation::BrowseIcon ? SelectionKind::Icon : SelectionKind::LauncherFile, work.ticket);
			if (!control.Allows(work.ticket)) { busy = false; return; }
			if (!selected.ok)
			{
				reply.code = selected.code;
				reply.data = {{"message", Utf8(selected.message)}};
				break;
			}
			auto retained = control.Remember(work.ticket, selected.value);
			reply.ok = retained.ok; reply.code = retained.code;
			if (retained.ok) reply.data = {{"selectionId", retained.value},
				{"path", Utf8(selected.value.path)}, {"directory", selected.value.directory}};
			break;
		}
		default: break;
		}
		busy = false;
		Reply(reply);
		if (!queue.empty() && Allowed()) PostMessageW(window, WorkMessage, 0, 0);
	}
};

Host::Host(HostOptions options) : m_Options(std::move(options)) {}
Host::~Host() { Close(); }
bool Host::Open()
{
	if (m_State && m_State->Allowed() && m_State->window)
	{
		ShowWindow(m_State->window, SW_RESTORE);
		SetForegroundWindow(m_State->window);
		return true;
	}
	Close();
	m_State = std::make_shared<State>(m_Options);
	return m_State->Start();
}
void Host::Close() { if (m_State) m_State->Close(); }
HWND Host::Window() const { return m_State ? m_State->window : nullptr; }

void OpenEditor(HINSTANCE module, const std::wstring& shelfRoot, bool (*isLocked)(), void (*lockNow)()) noexcept
{
	try
	{
		if (!isLocked || isLocked()) return;
		if (activeHost && activeHost->Window()) { activeHost->Open(); return; }
		HostOptions options;
		options.module = module;
		options.shelfRoot = shelfRoot;
		options.isLocked = isLocked;
		options.lockNow = lockNow;
		options.reportError = [](const wchar_t* text)
		{
			MessageBoxW(nullptr, text, L"Rainmeter Cafe Lock - ShelfSuite", MB_OK | MB_ICONINFORMATION);
		};
		activeHost = std::make_shared<Host>(std::move(options));
		activeHost->Open();
	}
	catch (...) { RevokeAndClose(); }
}
void RevokeAndClose() noexcept
{
	auto host = std::move(activeHost);
	if (host) host->Close();
}
}
