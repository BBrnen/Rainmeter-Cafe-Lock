#include "Host.h"
#include "Controller.h"
#include "Selection.h"
#include "Storage.h"
#include <atomic>
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

	struct Job
	{
		std::atomic<bool> done{false}, cancelled{false};
		Work work;
		Response reply;
		std::map<std::wstring, Snapshot> snapshots;
		SelectedFile selected;
		Result<LauncherDraft> launcher;
		Result<PngImage> image;
		bool iconOnly = false;
		std::shared_ptr<PreparedSave> prepared;
	};
	std::shared_ptr<Job> job;
	std::map<std::wstring, Snapshot> snapshots;
	std::map<SelectionId, std::shared_ptr<PngImage>> icons;

	static std::string Preview(const PngImage& image)
	{
		static const char alphabet[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
		std::string text="data:image/png;base64,";
		for(size_t i=0;i<image.bytes.size();i+=3)
		{
			const unsigned a=image.bytes[i], b=i+1<image.bytes.size()?image.bytes[i+1]:0,
				c=i+2<image.bytes.size()?image.bytes[i+2]:0;
			text+=alphabet[a>>2]; text+=alphabet[((a&3)<<4)|(b>>4)];
			text+=i+1<image.bytes.size()?alphabet[((b&15)<<2)|(c>>6)]:'=';
			text+=i+2<image.bytes.size()?alphabet[c&63]:'=';
		}
		return text;
	}
	static Json Model(const Snapshot& snapshot)
	{
		Json tabs=Json::array();
		for(const auto& tab:snapshot.document.tabs)
		{
			Json items=Json::array();
			for(const auto& item:tab.items)items.push_back({{"label",Utf8(item.label)},{"action",Utf8(item.action)},{"icon",Utf8(item.icon)}});
			tabs.push_back({{"name",Utf8(tab.name)},{"items",std::move(items)}});
		}
		return {{"id",Utf8(snapshot.shelf.id)},{"version",snapshot.version},
			{"tabCapacity",snapshot.shelf.tabCapacity},{"itemCapacity",snapshot.shelf.itemCapacity},
			{"defaultIcon",Utf8(snapshot.document.defaultIcon)},{"theme",Utf8(snapshot.theme)},{"tabs",std::move(tabs)}};
	}
	void StartJob(const Work& work, std::function<void(Job&)> action)
	{
		struct Context { std::shared_ptr<Job> job; std::function<void(Job&)> action; };
		job=std::make_shared<Job>(); job->work=work; job->work.object.Reset();
		job->reply={work.request.id,false,Error::IoError,Json::object()};
		auto context=new Context{job,std::move(action)};
		TP_CALLBACK_ENVIRON environmentOptions;
		InitializeThreadpoolEnvironment(&environmentOptions);
		// Threadpool retains the DLL through callback return, including shutdown.
		SetThreadpoolCallbackLibrary(&environmentOptions,options.module);
		const auto submitted=TrySubmitThreadpoolCallback([](PTP_CALLBACK_INSTANCE instance,void* raw)
		{
			CallbackMayRunLong(instance);
			std::unique_ptr<Context> context(static_cast<Context*>(raw));
			auto& task=*context->job;
			const auto initialized=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
			try
			{
				if(FAILED(initialized))task.reply.data={{"message","Windows could not start the file operation."}};
				else if(!task.cancelled.load())context->action(task);
			}
			catch(...)
			{
				task.reply.ok=false; task.reply.code=Error::IoError;
				task.reply.data={{"message","The operation failed. Existing configuration has not been replaced."}};
			}
			if(SUCCEEDED(initialized))CoUninitialize();
			if(task.cancelled.load())task.prepared.reset();
			task.done.store(true);
		},context,&environmentOptions);
		DestroyThreadpoolEnvironment(&environmentOptions);
		if(!submitted) { delete context; job.reset(); busy=false; FailLater(L"Windows could not start the editor operation."); return; }
		if(!SetTimer(window,1,30,nullptr)) { job->cancelled.store(true); FailLater(L"The editor could not monitor its operation."); }
	}
	void FinishJob()
	{
		if(!job || !job->done.load())return;
		KillTimer(window,1);
		auto completed=std::move(job);
		if(!Allowed() || !control.Allows(completed->work.ticket)) { busy=false; return; }
		auto& reply=completed->reply;
		switch(completed->work.request.operation)
		{
		case Operation::Load:
			if(reply.ok)snapshots=std::move(completed->snapshots);
			break;
		case Operation::BrowseLauncher: case Operation::BrowseFolder:
		case Operation::BrowseIcon: case Operation::ImportDrop:
			if(reply.ok)
			{
				const auto retained=control.Remember(completed->work.ticket,completed->selected);
				if(!retained.ok) {reply.ok=false;reply.code=retained.code;reply.data={{"message","Close this draft and try again."}};break;}
				reply.data={{"selectionId",retained.value}};
				if(!completed->iconOnly)
				{
					reply.data["name"]=Utf8(completed->launcher.value.name);
					reply.data["action"]=Utf8(completed->launcher.value.action);
				}
				if(completed->image.ok)
				{
					icons[retained.value]=std::make_shared<PngImage>(std::move(completed->image.value));
					const auto name=completed->iconOnly?L"icon":completed->launcher.value.name;
					reply.data["iconId"]=retained.value;
					reply.data["iconName"]=Utf8(IconBaseName(name)+L".png");
					reply.data["preview"]=Preview(*icons[retained.value]);
				}
				else reply.data["warning"]=Utf8(completed->image.message);
			}
			break;
		case Operation::SaveEdits:
			if(completed->prepared)
			{
				const auto result=Storage(options.shelfRoot).Commit(completed->prepared,
					[this,t=completed->work.ticket](){return Allowed() && control.Allows(t);});
				reply.ok=result.ok; reply.code=result.code;
				reply.data=result.ok?Json{{"backup",Utf8(result.value.backup)},{"icon",Utf8(result.value.icon)},{"warning",Utf8(result.value.warning)},{"shelf",Utf8(result.value.shelf)}}:
					Json{{"message",Utf8(result.message)}};
				if(result.ok)
				{
					icons.clear(); control.CancelDraft(completed->work.ticket);
					if(options.refreshShelf && completed->work.request.payload["edit"]["kind"]!="addShelf")
						options.refreshShelf(result.value.shelf,completed->work.request.payload["edit"]["kind"]=="removeShelf");
				}
			}
			break;
		default:break;
		}
		busy=false; Reply(reply);
		if(!queue.empty() && Allowed())PostMessageW(window,WorkMessage,0,0);
	}
	void LoadShelves(const Work& work)
	{
		const auto root=options.shelfRoot;
		StartJob(work,[root](Job& task)
		{
			task.reply.ok=true;task.reply.code=Error::None;
			task.reply.data={{"mode","maintenance"},{"canSave",false},{"shelves",Json::array()}};
			if(root.empty())return;
			const Storage storage(root); const auto found=storage.Discover();
			if(!found.ok){task.reply.data["message"]=Utf8(found.message);return;}
			for(const auto& shelf:found.value)
			{
				if(task.cancelled.load())return;
				auto loaded=storage.Load(shelf.id);
				if(loaded.ok)
				{
					task.reply.data["shelves"].push_back(Model(loaded.value));
					task.snapshots.emplace(shelf.id,std::move(loaded.value));
				}
				else task.reply.data["shelves"].push_back({{"id",Utf8(shelf.id)},{"tabs",Json::array()},{"error",Utf8(loaded.message)}});
			}
			task.reply.data["canSave"]=!task.snapshots.empty();
		});
	}
	void InspectSelection(const Work& work,SelectedFile selected)
	{
		const bool iconOnly=work.request.operation==Operation::BrowseIcon ||
			(work.request.operation==Operation::ImportDrop && work.request.payload["purpose"]=="icon");
		const bool dropped=work.request.operation==Operation::ImportDrop;
		StartJob(work,[selected,iconOnly,dropped](Job& task) mutable
		{
			const auto attributes=GetFileAttributesW(selected.path.c_str());
			if(attributes==INVALID_FILE_ATTRIBUTES){task.reply.code=Error::NotFound;task.reply.data={{"message","The selected item is unavailable."}};return;}
			const bool directory=(attributes & FILE_ATTRIBUTE_DIRECTORY)!=0;
			if(!dropped && selected.directory!=directory){task.reply.code=Error::Conflict;task.reply.data={{"message","The selected item changed. Choose it again."}};return;}
			selected.directory=directory;
			task.selected=selected;task.iconOnly=iconOnly;
			if(!iconOnly)
			{
				task.launcher=InspectLauncher(selected);
				if(!task.launcher.ok){task.reply.code=task.launcher.code;task.reply.data={{"message",Utf8(task.launcher.message)}};return;}
				task.image=PrepareLauncherIcon(task.launcher.value);
			}
			else task.image=PrepareIcon(selected);
			if(iconOnly && !task.image.ok){task.reply.code=task.image.code;task.reply.data={{"message",Utf8(task.image.message)}};return;}
			task.reply.ok=true;task.reply.code=Error::None;
		});
	}
	void PrepareSave(const Work& work)
	{
		const auto& payload=work.request.payload;
		const bool addingShelf=payload["edit"]["kind"]=="addShelf";
		const auto found=snapshots.find(Wide(payload["shelf"].get<std::string>()));
		if(!addingShelf && (found==snapshots.end() || payload["version"]!=found->second.version))
		{
			busy=false;Reply({work.request.id,false,Error::Stale,{{"message","Reload this shelf before saving."}}});
			PostMessageW(window,WorkMessage,0,0);return;
		}
		const auto iconId=payload["iconId"].get<uint64_t>();
		std::shared_ptr<PngImage> image;
		if(iconId)
		{
			const auto icon=icons.find(iconId);
			if(icon==icons.end() || !control.GetSelection(work.ticket,iconId).ok)
			{
				busy=false;Reply({work.request.id,false,Error::Stale,{{"message","Choose the icon again."}}});
				PostMessageW(window,WorkMessage,0,0);return;
			}
			image=icon->second;
		}
		const auto& data=payload["edit"];
		const std::map<std::string,EditKind> kinds={{"setItem",EditKind::SetItem},{"addItem",EditKind::AddItem},
			{"removeItem",EditKind::RemoveItem},{"renameTab",EditKind::RenameTab},{"addTab",EditKind::AddTab},{"removeTab",EditKind::RemoveTab},{"setTheme",EditKind::SetTheme},
			{"addShelf",EditKind::AddShelf},{"removeShelf",EditKind::RemoveShelf}};
		Edit edit{kinds.at(data["kind"].get<std::string>()),data["tab"].get<size_t>(),data["item"].get<size_t>(),
			Wide(data["label"].get<std::string>()),Wide(data["action"].get<std::string>()),Wide(data["icon"].get<std::string>())};
		const auto snapshot=addingShelf?Snapshot{}:found->second; const auto root=options.shelfRoot;
		StartJob(work,[root,snapshot,edit,image](Job& task)
		{
			const auto prepared=Storage(root).Prepare(snapshot,edit,image.get(),edit.label);
			task.reply.ok=prepared.ok;task.reply.code=prepared.code;
			if(prepared.ok)task.prepared=prepared.value;
			else task.reply.data={{"message",Utf8(prepared.message)}};
		});
	}


	void Close()
	{
		// This ordering applies even while a native picker pumps nested messages.
		control.Revoke();
		closed = true;
		if(job){job->cancelled.store(true);job.reset();}
		icons.clear(); snapshots.clear();
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
			case WM_TIMER:
				if(wp==1)self->FinishJob();
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
			icons.clear();
			reply.ok = true; reply.code = Error::None;
			break;
		case Operation::Load:
			LoadShelves(work);
			return;
		case Operation::SaveEdits:
			PrepareSave(work);
			return;
		case Operation::BrowseLauncher:
		case Operation::BrowseFolder:
		case Operation::BrowseIcon:
		case Operation::ImportDrop:
		{
			auto selected = work.request.operation == Operation::ImportDrop ?
				selection.CaptureDrop(work.object.Get(), work.ticket) :
				selection.Pick(window, work.request.operation == Operation::BrowseFolder ? SelectionKind::Folder :
					work.request.operation == Operation::BrowseIcon ? SelectionKind::Icon : SelectionKind::LauncherFile, work.ticket, true);
			if (!control.Allows(work.ticket)) { busy = false; return; }
			if (!selected.ok)
			{
				reply.code = selected.code;
				reply.data = {{"message", Utf8(selected.message)}};
				break;
			}
			InspectSelection(work,std::move(selected.value));
			return;
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

void OpenEditor(HINSTANCE module, const std::wstring& shelfRoot, bool (*isLocked)(), void (*lockNow)(), void (*refreshShelf)(const std::wstring&, bool)) noexcept
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
		options.refreshShelf = refreshShelf;
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
