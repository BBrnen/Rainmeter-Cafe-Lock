// Standalone policy regression test, built with the Windows SDK/MSVC.
#include "../Library/CafeLock.h"
#include <cassert>

int main()
{
	assert(CafeLock::IsLocked());
	assert(!CafeLock::AllowsBang(Bang::Manage));
	assert(!CafeLock::AllowsBang(Bang::DeactivateConfig));
	assert(!CafeLock::AllowsBang(Bang::LoadLayout));
	assert(!CafeLock::AllowsBang(Bang::WriteKeyValue));
	assert(CafeLock::AllowsBang(Bang::CommandMeasure));
	assert(CafeLock::AllowsBang(Bang::SetOption));
	assert(CafeLock::AllowsBang(Bang::SetVariable));
	assert(CafeLock::AllowsBang(Bang::MoveMeter));
	assert(CafeLock::AllowsBang(Bang::Redraw));
	assert(CafeLock::IsShelfSuiteConfigurator(
		L"C:/Skins/shelf suite/@Resources/../@Resources/CONFIGURATOR.HTML", L"C:\\Skins\\"));
	assert(!CafeLock::IsShelfSuiteConfigurator(L"C:\\Documents\\configurator.html", L"C:\\Skins\\"));
	assert(!CafeLock::IsShelfSuiteConfigurator(L"https://example.com", L"C:\\Skins\\"));
	assert(!CafeLock::IsShelfSuiteConfigurator(L"C:\\Windows\\notepad.exe", L"C:\\Skins\\"));
	return 0;
}
