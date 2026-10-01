// Disposable CI-only UI Automation observer. Never packaged with Rainmeter.
#include <windows.h>
#include <shellapi.h>
#include <UIAutomation.h>
#include <wrl/client.h>
#include <string>
#include <cwchar>
#include <cstdlib>
using Microsoft::WRL::ComPtr;
const wchar_t* phase = L"dialog identity";

HRESULT Observe(HWND window, DWORD expectedProcess, const wchar_t* output, int action)
{
	DWORD process = 0; GetWindowThreadProcessId(window, &process);
	wchar_t title[128]{}; GetWindowTextW(window, title, _countof(title));
	const bool permittedTitle = action == 2 ? std::wstring(title).rfind(L"ShelfSuite F1 - ", 0) == 0 : wcscmp(title, L"ShelfSuite F1 compatibility") == 0;
	if (process != expectedProcess || !permittedTitle || action < 0 || action > 2) return E_ACCESSDENIED;
	phase = L"UI Automation activation";
	ComPtr<IUIAutomation> automation;
	HRESULT result = CoCreateInstance(__uuidof(CUIAutomation), nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&automation));
	if (FAILED(result)) return result;
	ComPtr<IUIAutomationElement> root;
	phase = L"UI Automation dialog element";
	if (FAILED(result = automation->ElementFromHandle(window, &root))) return result;
	if (action)
	{
		phase = action == 2 ? L"result OK button" : L"file-list expander";
		VARIANT name{}; name.vt = VT_BSTR; name.bstrVal = SysAllocString(action == 2 ? L"OK" : L"Show file list");
		ComPtr<IUIAutomationCondition> condition;
		result = automation->CreatePropertyCondition(UIA_NamePropertyId, name, &condition); VariantClear(&name);
		if (FAILED(result)) return result;
		ComPtr<IUIAutomationElement> button;
		if (FAILED(result = root->FindFirst(TreeScope_Descendants, condition.Get(), &button)) || !button) return E_FAIL;
		ComPtr<IUIAutomationInvokePattern> invoke;
		if (FAILED(result = button->GetCurrentPatternAs(UIA_InvokePatternId, IID_PPV_ARGS(&invoke)))) return result;
		if (FAILED(result = invoke->Invoke()) && IsWindow(window)) return result;
	}
	std::wstring text = action == 2 ? L"Result dismissed.\r\n" : L"";
	if (action != 2)
	{
	ComPtr<IUIAutomationCondition> all;
	phase = L"preview text enumeration";
	if (FAILED(result = automation->CreateTrueCondition(&all))) return result;
	ComPtr<IUIAutomationElementArray> elements;
	if (FAILED(result = root->FindAll(TreeScope_Descendants, all.Get(), &elements))) return result;
	int length = 0;
	if (FAILED(result = elements->get_Length(&length)) || length > 10000) return E_FAIL;
	for (int index = 0; index < length; ++index)
	{
		ComPtr<IUIAutomationElement> element;
		if (FAILED(result = elements->GetElement(index, &element))) return result;
		BSTR label = nullptr;
		if (FAILED(result = element->get_CurrentName(&label))) return result;
		if (label) { text.append(label, SysStringLen(label)); SysFreeString(label); }
		text += L"\r\n";
		if (text.size() > 1024 * 1024) return E_FAIL;
	}
	}
	const int size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
	phase = L"preview snapshot write";
	if (!size) return E_FAIL;
	std::string bytes(static_cast<size_t>(size), '\0');
	if (!WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), &bytes[0], size, nullptr, nullptr)) return E_FAIL;
	HANDLE file = CreateFileW(output, GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (file == INVALID_HANDLE_VALUE) return HRESULT_FROM_WIN32(GetLastError());
	DWORD written = 0;
	const bool complete = WriteFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr) && written == bytes.size() && FlushFileBuffers(file);
	CloseHandle(file); return complete ? S_OK : E_FAIL;
}
int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
	int count = 0; auto arguments = CommandLineToArgvW(GetCommandLineW(), &count);
	if (!arguments || count != 5) { if (arguments) LocalFree(arguments); return 2; }
	HRESULT result = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
	if (SUCCEEDED(result))
	{
		result = Observe(reinterpret_cast<HWND>(static_cast<UINT_PTR>(_wcstoui64(arguments[1], nullptr, 10))),
			static_cast<DWORD>(wcstoul(arguments[2], nullptr, 10)), arguments[3], _wtoi(arguments[4]));
		CoUninitialize();
	}
	if (FAILED(result))
	{
		const std::wstring detail = std::wstring(L"Observer failure at ") + phase + L"; HRESULT=" + std::to_wstring(static_cast<DWORD>(result));
		HANDLE file = CreateFileW(arguments[3], GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (file != INVALID_HANDLE_VALUE) { DWORD written = 0; WriteFile(file, detail.data(), static_cast<DWORD>(detail.size() * sizeof(wchar_t)), &written, nullptr); FlushFileBuffers(file); CloseHandle(file); }
	}
	LocalFree(arguments); return SUCCEEDED(result) ? 0 : 1;
}
