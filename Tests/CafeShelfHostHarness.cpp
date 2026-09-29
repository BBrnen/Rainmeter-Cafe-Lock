#include "../Library/CafeShelf/Host.h"
#include <WebView2.h>
#include <wrl.h>
#include <iostream>
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
void Pump()
{
	MSG message;
	while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
	{
		TranslateMessage(&message);
		DispatchMessageW(&message);
	}
}
}
int main()
{
	if (FAILED(OleInitialize(nullptr))) return 2;
	bool locked = true;
	int starts = 0, errors = 0;
	ComPtr<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler> pending;
	HostOptions options;
	options.module = GetModuleHandleW(nullptr);
	options.isLocked = [&locked]() { return locked; };
	options.reportError = [&errors](const wchar_t*) { ++errors; };
	options.createEnvironment = [&starts, &pending](LPCWSTR profile,
		ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler* callback)
	{
		++starts;
		Check("host supplies existing private profile", profile &&
			(GetFileAttributesW(profile) & FILE_ATTRIBUTE_DIRECTORY) &&
			GetFileAttributesW(profile) != INVALID_FILE_ATTRIBUTES);
		pending = callback;
		return S_OK;
	};
	Host host(options);
	Check("locked host creates no window", !host.Open() && !host.Window() && starts == 0);
	locked = false;
	Check("maintenance starts browser initialization", host.Open() && starts == 1 && IsWindow(host.Window()));
	const auto firstWindow = host.Window();
	Check("repeat open focuses same window", host.Open() && host.Window() == firstWindow && starts == 1);
	locked = true;
	host.Close();
	Check("lock closes pending host", !host.Window() && !IsWindow(firstWindow));
	if (pending) pending->Invoke(E_FAIL, nullptr);
	Pump();
	Check("late browser failure cannot revive closed host", !host.Window() && errors == 0);
	locked = false;
	Check("new maintenance creates new initialization", host.Open() && starts == 2);
	if (pending) pending->Invoke(E_FAIL, nullptr);
	Pump();
	Check("failed initialization closes with useful error", !host.Window() && errors == 1);
	options.createEnvironment = [](LPCWSTR, ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler*) { return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND); };
	Host missingRuntime(options);
	Check("missing runtime fails without leaving editor window", !missingRuntime.Open() && !missingRuntime.Window() && errors == 2);
	options.createEnvironment = [&pending](LPCWSTR, ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler* callback) { pending = callback; return S_OK; };
	{
		Host destroyed(options);
		Check("temporary host starts", destroyed.Open());
	}
	if (pending) pending->Invoke(E_FAIL, nullptr);
	Pump();
	Check("callback after host destruction is harmless", errors == 2);
	Host closeMessage(options);
	Check("close-message host starts", closeMessage.Open());
	if (closeMessage.Window()) SendMessageW(closeMessage.Window(), WM_CLOSE, 0, 0);
	Check("normal close releases host window", !closeMessage.Window());
	pending.Reset();
	OleUninitialize();
	std::cout << checks << " host lifecycle checks, " << failures << " failures\n";
	return failures ? 1 : 0;
}
