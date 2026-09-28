// Rainmeter Cafe Lock. GNU GPL v2 or later; see LICENSE.
#include "StdAfx.h"
#include "CafeLock.h"
#include "../Common/CafeAuthorization.h"
#include "Rainmeter.h"
#include "Skin.h"
#include "TrayIcon.h"
#include "DialogManage.h"
#include "DialogNewSkin.h"
#include "DialogAbout.h"
#include "GameMode.h"

namespace
{
CafeSecurity::Authorization authorization;
bool requesting = false;
}

bool CafeLock::IsLocked() { return !authorization.IsAuthorized(); }

void CafeLock::RequestMaintenance()
{
	if (!IsLocked() || requesting || authorization.IsPending()) return;
	requesting = true; // ShellExecute may dispatch messages while showing UAC.
	const bool started = authorization.Begin(GetRainmeter().GetWindow(),
		GetRainmeter().GetPath() + L"RainmeterCafeMaintenance.exe");
	requesting = false;
	if (started)
	{
		if (!SetTimer(GetRainmeter().GetWindow(), MaintenanceTimer, 100, nullptr)) authorization.Lock();
	}
	// UAC cancellation and launch failure leave the instance locked.
}

void CafeLock::PollMaintenance()
{
	if (requesting) return;
	if (authorization.Poll())
	{
		GetGameMode().Initialize();
		GetRainmeter().GetTrayIcon()->SetTrayIcon(true, true);
	}
	if (!authorization.IsPending()) KillTimer(GetRainmeter().GetWindow(), MaintenanceTimer);
}

void CafeLock::LockNow()
{
	// Stop automatic game-mode transitions; keep the currently configured scene.
	if (!IsLocked()) GetGameMode().SuspendForCafeLock();
	authorization.Lock();
	KillTimer(GetRainmeter().GetWindow(), MaintenanceTimer);
	EndMenu();
	ReleaseCapture();
	DialogManage::CloseDialog();
	DialogNewSkin::CloseDialog();
	DialogAbout::CloseDialog();
	for (const auto& item : GetRainmeter().GetAllSkins())
	{
		SendMessage(item.second->GetWindow(), WM_CANCELMODE, 0, 0);
		item.second->Deselect();
	}
	GetRainmeter().GetTrayIcon()->SetTrayIcon(true, true);
}

void CafeLock::Shutdown()
{
	authorization.Lock();
	KillTimer(GetRainmeter().GetWindow(), MaintenanceTimer);
}

void CafeLock::ShowLockedTrayMenu(HWND owner)
{
	HMENU menu = CreatePopupMenu();
	if (!menu) return;
	AppendMenuW(menu, MF_STRING | (requesting || authorization.IsPending() ? MF_GRAYED : 0),
		MaintenanceCommand, L"Enter Maintenance Mode (administrator)...");
	POINT pos;
	GetCursorPos(&pos);
	SetForegroundWindow(owner);
	const UINT chosen = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON, pos.x, pos.y, 0, owner, nullptr);
	DestroyMenu(menu);
	PostMessage(owner, WM_NULL, 0, 0);
	if (chosen == MaintenanceCommand) RequestMaintenance();
}
