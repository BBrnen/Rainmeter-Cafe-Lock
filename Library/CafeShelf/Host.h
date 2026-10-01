#pragma once
#include <windows.h>
#include <functional>
#include <memory>
#include <string>

struct ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler;

namespace CafeShelf
{
// All supplied by native code. No browser message configures this structure.
struct HostOptions
{
	HINSTANCE module = nullptr;
	std::wstring shelfRoot;
	std::function<bool()> isLocked;
	std::function<void()> lockNow;
	std::function<void(const std::wstring&, bool)> refreshShelf;
	std::function<void(const wchar_t*)> reportError;
	// Dependency seam for standalone lifecycle tests. Production leaves it empty.
	std::function<HRESULT(LPCWSTR, ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler*)> createEnvironment;
};

class Host
{
public:
	explicit Host(HostOptions options);
	~Host();
	Host(const Host&) = delete;
	Host& operator=(const Host&) = delete;
	bool Open();
	void Close();
	HWND Window() const;

private:
	struct State;
	HostOptions m_Options;
	std::shared_ptr<State> m_State;
};

// Rainmeter entry points: no web-callable/exported editing or unlocking API.
bool TryOpen(const wchar_t* file);
void OpenEditor(HINSTANCE module, const std::wstring& shelfRoot,
	bool (*isLocked)(), void (*lockNow)(), void (*refreshShelf)(const std::wstring&, bool)) noexcept;
void RevokeAndClose() noexcept;
}
