// Rainmeter Cafe Lock. Licensed under GNU GPL v2 or later; see LICENSE.
#pragma once

#include "CommandHandler.h"
#include <algorithm>

namespace CafeLock
{
// Process-local state; only a verified password authorizes this instance.
bool IsLocked();
constexpr UINT MaintenanceCommand = 4090;
constexpr UINT LockCommand = 4091;
void RequestMaintenance();
void ChangePassword();
void PromptForManagement(Bang bang);
void LockNow();
void Shutdown();
void ShowLockedTrayMenu(HWND owner);
void OpenShelfSuiteF1Compatibility(HWND owner);
inline bool AllowsF1Compatibility() { return !IsLocked(); }

inline bool AllowsBang(Bang bang)
{
	if (!IsLocked()) return true;
	switch (bang)
	{
	case Bang::ActivateConfig:
	case Bang::DeactivateConfig:
	case Bang::DeactivateConfigGroup:
	case Bang::ToggleConfig:
	case Bang::LoadLayout:
	case Bang::Manage:
	case Bang::EditSkin:
	case Bang::About:
	case Bang::SkinMenu:
	case Bang::SkinCustomMenu:
	case Bang::TrayMenu:
	case Bang::Quit:
	case Bang::RefreshApp:
	case Bang::WriteKeyValue:
	case Bang::Move:
	case Bang::SetWindowPosition:
	case Bang::SetAnchor:
	case Bang::ZPos:
	case Bang::ChangeZPos:
	case Bang::ClickThrough:
	case Bang::Draggable:
	case Bang::SnapEdges:
	case Bang::KeepOnScreen:
	case Bang::AutoSelectScreen:
	case Bang::LsBoxHook:
		return false;
	default:
		// In-memory meter changes, Lua, updates, and ordinary launcher actions
		// remain available. Group aliases use the same Bang values.
		return true;
	}
}

inline std::wstring FullPath(const std::wstring& path)
{
	const DWORD length = GetFullPathNameW(path.c_str(), 0, nullptr, nullptr);
	if (!length) return std::wstring();
	std::wstring full(length, L'\0');
	const DWORD written = GetFullPathNameW(path.c_str(), length, &full[0], nullptr);
	if (!written || written >= length) return std::wstring();
	full.resize(written);
	std::replace(full.begin(), full.end(), L'/', L'\\');
	return full;
}

inline bool IsShelfSuiteConfigurator(const WCHAR* file, const std::wstring& skinPath)
{
	// Guard the stock ShelfSuite gear at the shell-launch boundary without
	// changing the skin or blocking ordinary HTML documents and web launchers.
	const auto target = FullPath(file);
	const auto configurator = FullPath(skinPath + L"Shelf Suite\\@Resources\\configurator.html");
	return !target.empty() && !configurator.empty() &&
		_wcsicmp(target.c_str(), configurator.c_str()) == 0;
}

inline bool AllowsLaunch(const WCHAR* file, const std::wstring& skinPath)
{
	return !IsLocked() || !IsShelfSuiteConfigurator(file, skinPath);
}
}
