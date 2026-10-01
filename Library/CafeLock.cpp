// Rainmeter Cafe Lock. GNU GPL v2 or later; see LICENSE.
#include "StdAfx.h"
#include "CafeLock.h"
#include "CafeShelf/Host.h"
#include "CafeShelf/F1Compatibility.h"
#include "../Common/CafePassword.h"
#include "Rainmeter.h"
#include "Skin.h"
#include "TrayIcon.h"
#include "DialogManage.h"
#include "DialogNewSkin.h"
#include "DialogAbout.h"
#include "GameMode.h"
#include <memory>
#include <commctrl.h>

namespace
{
std::unique_ptr<CafeSecurity::PasswordSession> session;
// Cancellation belongs only to this operation; it never authorizes a session.
bool f1Active = false, f1Revoked = false, f1Shutdown = false;
class PasswordDialog : public Dialog
{
public:
 void Open(bool change)
 {
  if (m_Window) { SetForegroundWindow(m_Window); return; }
  changing = change;
  setup = !change && session->NeedsSetup();
  ShowDialogWindow(change ? L"Change Cafe Lock Password" : setup ? L"Create Cafe Lock Password" : L"Enter Cafe Lock Password",
   0, 0, 270, change ? 172 : setup ? 137 : 102,
   DS_CENTER | WS_POPUP | WS_CAPTION | WS_SYSMENU, WS_EX_APPWINDOW, nullptr);
 }
 void Close()
 {
  if (!m_Window) return;
  if (f1Active) f1Revoked = true;
  for (int id : {100, 101, 102}) SetDlgItemTextW(m_Window, id, L"");
  DestroyWindow(m_Window);
  m_Window = nullptr;
 }
private:
 bool changing = false, setup = false;
 void Add(const WCHAR* cls, const WCHAR* text, int id, int x, int y, int width, int height, DWORD style)
 {
  RECT r = {x, y, x + width, y + height}; MapDialogRect(m_Window, &r);
  HWND control = CreateWindowExW(wcscmp(cls, L"Edit") == 0 ? WS_EX_CLIENTEDGE : 0,
   cls, text, WS_CHILD | WS_VISIBLE | style, r.left, r.top, r.right-r.left, r.bottom-r.top,
   m_Window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), nullptr, nullptr);
  SendMessage(control, WM_SETFONT, reinterpret_cast<WPARAM>(m_Font), TRUE);
  if (wcscmp(cls, L"Edit") == 0) SendMessage(control, EM_SETLIMITTEXT, 256, 0);
 }
 INT_PTR HandleMessage(UINT msg, WPARAM wp, LPARAM lp) override
 {
  if (msg == WM_ACTIVATE) return OnActivate(wp, lp);
  if (msg == WM_QUERYENDSESSION) return TRUE;
  if (msg == WM_ENDSESSION) { if (wp) Close(); return TRUE; }
  if (msg == WM_CLOSE) { Close(); return TRUE; }
  if (msg == WM_INITDIALOG)
  {
   int y = 10;
   if (changing) {
    Add(L"Static", L"Current password", 200, 10,y,250,10,0);
    Add(L"Edit", L"", 102, 10,y+12,250,15,WS_TABSTOP|ES_PASSWORD|ES_AUTOHSCROLL); y += 35;
   }
   Add(L"Static", changing ? L"New password" : setup ? L"Create password" : L"Password", 201,10,y,250,10,0);
   Add(L"Edit", L"", 100,10,y+12,250,15,WS_TABSTOP|ES_PASSWORD|ES_AUTOHSCROLL); y += 35;
   if (changing || setup) {
    Add(L"Static", L"Confirm password",202,10,y,250,10,0);
    Add(L"Edit",L"",101,10,y+12,250,15,WS_TABSTOP|ES_PASSWORD|ES_AUTOHSCROLL); y += 35;
   }
   Add(L"Static",L"",203,10,y,250,20,0);
   Add(L"Button",changing ? L"Change password" : setup ? L"Create and unlock" : L"Unlock",IDOK,85,y+25,100,20,WS_TABSTOP|BS_DEFPUSHBUTTON);
   Add(L"Button",L"Cancel",IDCANCEL,190,y+25,70,20,WS_TABSTOP);
   SetFocus(GetControl(changing ? 102 : 100)); return FALSE;
  }
  if (msg == WM_COMMAND && LOWORD(wp) == IDCANCEL) { Close(); return TRUE; }
  if (msg == WM_COMMAND && LOWORD(wp) == IDOK)
  {
   WCHAR password[257] = {}, confirmation[257] = {}, current[257] = {};
   GetDlgItemTextW(m_Window,100,password,257);
   GetDlgItemTextW(m_Window,101,confirmation,257);
   GetDlgItemTextW(m_Window,102,current,257);
   const bool mismatch = (changing || setup) && wcscmp(password,confirmation) != 0;
   const bool empty = !password[0];
   const bool ok = !mismatch && !empty && (changing ? session->Change(current,password,confirmation) :
    setup ? session->Setup(password,confirmation) : session->Unlock(password));
   SecureZeroMemory(password,sizeof(password)); SecureZeroMemory(confirmation,sizeof(confirmation)); SecureZeroMemory(current,sizeof(current));
   for (int id : {100,101,102}) SetDlgItemTextW(m_Window,id,L"");
   if (ok) {
    const bool wasChanging = changing;
    Close();
    if (!wasChanging) { GetGameMode().Initialize(); GetRainmeter().GetTrayIcon()->SetTrayIcon(true,true); }
   } else {
    SetDlgItemTextW(m_Window,203,mismatch ? L"Passwords do not match." : empty ? L"Enter a password." :
     setup ? L"Could not save password. Remain locked." : changing ? L"Incorrect current password or unable to save." : L"Incorrect password or invalid password settings.");
    SetFocus(GetControl(changing ? 102 : 100));
   }
   return TRUE;
  }
  return FALSE;
 }
};
std::unique_ptr<PasswordDialog> passwordDialog;
}

bool CafeLock::IsLocked() { return !session || session->IsLocked(); }
void CafeLock::RequestMaintenance()
{
 if (!IsLocked()) return;
 if (!session) session.reset(new CafeSecurity::PasswordSession(GetRainmeter().GetSettingsPath() + L"CafeLock.ini"));
 if (!passwordDialog) passwordDialog.reset(new PasswordDialog());
 passwordDialog->Open(false);
}
void CafeLock::ChangePassword()
{
 if (IsLocked()) return;
 if (!passwordDialog) passwordDialog.reset(new PasswordDialog());
 passwordDialog->Open(true);
}
void CafeLock::PromptForManagement(Bang bang)
{
 if (bang == Bang::Manage || bang == Bang::EditSkin) RequestMaintenance();
}

void CafeLock::LockNow()
{
	if (f1Active) f1Revoked = true;
	// Stop automatic game-mode transitions; keep the currently configured scene.
	if (!IsLocked()) GetGameMode().SuspendForCafeLock();
	if (session) session->Lock();
	CafeShelf::RevokeAndClose();
	if (passwordDialog) passwordDialog->Close();
	EndMenu();
	ReleaseCapture();
	DialogManage::CloseDialog();
	DialogNewSkin::CloseDialog();
	DialogAbout::CloseDialog();
	for (const auto& item : GetRainmeter().GetAllSkins())
	{
		SendMessage(item.second->GetWindow(), WM_CANCELMODE, 0, 0);
		if (item.second->IsSelected()) item.second->Deselect();
	}
	GetRainmeter().GetTrayIcon()->SetTrayIcon(true, true);
}

void CafeLock::Shutdown()
{
	if (f1Active) { f1Revoked = true; f1Shutdown = true; }
	if (session) session->Lock();
	CafeShelf::RevokeAndClose();
	if (passwordDialog) passwordDialog->Close();
}

void CafeLock::ShowLockedTrayMenu(HWND owner)
{
	HMENU menu = CreatePopupMenu();
	if (!menu) return;
	AppendMenuW(menu, MF_STRING,
		MaintenanceCommand, L"Unlock / Enter Maintenance Mode");
	POINT pos;
	GetCursorPos(&pos);
	SetForegroundWindow(owner);
	const UINT chosen = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON, pos.x, pos.y, 0, owner, nullptr);
	DestroyMenu(menu);
	PostMessage(owner, WM_NULL, 0, 0);
	if (chosen == MaintenanceCommand) RequestMaintenance();
}

namespace
{
constexpr int F1ApplyButton = 1001, F1LockButton = 1002;
bool F1Authorized()
{
	if (f1Revoked || !CafeLock::AllowsF1Compatibility()) return false;
	// Service normal UI messages between mutations so Lock Now and shutdown
	// can revoke a synchronous update. Revocation stays latched even if a later
	// message opens a new Maintenance session.
	MSG message{};
	for (unsigned int count = 0; count < 64 && PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE); ++count)
	{
		if (message.message == WM_QUIT)
		{
			f1Revoked = true; f1Shutdown = true;
			PostQuitMessage(static_cast<int>(message.wParam)); return false;
		}
		TranslateMessage(&message); DispatchMessageW(&message);
		if (f1Revoked || !CafeLock::AllowsF1Compatibility()) return false;
	}
	return true;
}
HRESULT CALLBACK F1PreviewCallback(HWND window, UINT notification, WPARAM button, LPARAM, LONG_PTR)
{
	if (notification == TDN_BUTTON_CLICKED && button == F1LockButton)
	{
		CafeLock::LockNow();
		SendMessageW(window, TDM_CLICK_BUTTON, IDCANCEL, 0); return S_FALSE;
	}
	if (notification == TDN_TIMER && (f1Revoked || !CafeLock::AllowsF1Compatibility()))
		SendMessageW(window, TDM_CLICK_BUTTON, IDCANCEL, 0);
	return S_OK;
}
void F1Result(HWND owner, const wchar_t* status, const std::wstring& message)
{
	if (f1Shutdown || !IsWindow(owner)) return;
	const std::wstring title = std::wstring(L"ShelfSuite F1 - ") + status;
	MessageBoxW(owner, message.c_str(), title.c_str(), MB_OK | MB_ICONINFORMATION);
}
}

void CafeLock::OpenShelfSuiteF1Compatibility(HWND owner)
{
	// No other Rainmeter entry point calls this operation. Check again even
	// when a forged Settings command reaches its handler.
	if (!AllowsF1Compatibility() || f1Active) return;
	const HWND tray = GetRainmeter().GetTrayIcon()->GetWindow();
	if (!IsWindow(tray)) return;
	(void)owner; // Manage can be destroyed by Lock Now; use the stable tray owner.
	f1Active = true; f1Revoked = false; f1Shutdown = false;
	struct ActiveGuard { ~ActiveGuard() { f1Active = false; } } activeGuard;
	const auto preview = CafeShelf::F1::Inspect(GetRainmeter().GetSkinPath(),
		GetRainmeter().GetPath() + L"Compatibility\\ShelfSuiteF1");
	if (!F1Authorized()) return;
	if (!preview.ok || preview.value.status == CafeShelf::F1::Status::Refused)
	{
		F1Result(tray, L"Refused", preview.message.empty() ? preview.value.message : preview.message); return;
	}
	if (preview.value.status == CafeShelf::F1::Status::AlreadyCompatible)
	{
		F1Result(tray, L"Already compatible", L"This ShelfSuite installation already matches the approved F1 files. No files were changed and no backup was created."); return;
	}
	std::wstring details = L"Files requiring changes:\r\n";
	for (const auto& change : preview.value.changes) details += change.relativePath + L"\r\n";
	details += L"\r\nAlready compatible:\r\n";
	for (const auto& change : preview.value.compatible) details += change.relativePath + L"\r\n";
	const std::wstring content = L"Installation: " + preview.value.shelfRoot + L"\r\n\r\nFiles requiring changes: " +
		std::to_wstring(preview.value.changes.size()) + L"\r\nAlready compatible: " + std::to_wstring(preview.value.compatible.size()) +
		L"\r\n\r\nVerified backup will be saved to:\r\n" + preview.value.proposedBackup +
		L"\r\n\r\nApply only the approved F1 compatibility files? Your configuration, icons, themes and positions are not updated. Loaded shelves will not be refreshed automatically.";
	const TASKDIALOG_BUTTON buttons[] = {{ F1ApplyButton, L"Apply" }, { F1LockButton, L"Lock Now" }};
	TASKDIALOGCONFIG dialog{}; dialog.cbSize = sizeof(dialog); dialog.hwndParent = tray;
	dialog.dwFlags = TDF_ALLOW_DIALOG_CANCELLATION | TDF_CALLBACK_TIMER;
	dialog.dwCommonButtons = TDCBF_CANCEL_BUTTON;
	dialog.pszWindowTitle = L"ShelfSuite F1 compatibility"; dialog.pszMainInstruction = L"Review the ShelfSuite F1 update";
	dialog.pszContent = content.c_str(); dialog.pszExpandedInformation = details.c_str();
	dialog.pszExpandedControlText = L"Show file list"; dialog.pszCollapsedControlText = L"Hide file list";
	dialog.cButtons = _countof(buttons); dialog.pButtons = buttons; dialog.nDefaultButton = IDCANCEL;
	dialog.pfCallback = F1PreviewCallback;
	int selected = IDCANCEL;
	if (FAILED(TaskDialogIndirect(&dialog, &selected, nullptr, nullptr)) || selected != F1ApplyButton || !F1Authorized()) return;
	const auto result = CafeShelf::F1::Apply(preview.value, F1Authorized);
	const wchar_t* status = L"Refused";
	switch (result.value.status)
	{
	case CafeShelf::F1::Status::Updated: status = L"Updated"; break;
	case CafeShelf::F1::Status::AlreadyCompatible: status = L"Already compatible"; break;
	case CafeShelf::F1::Status::FailedRecovered: status = L"Recovered after failure"; break;
	case CafeShelf::F1::Status::ManualRecoveryRequired: status = L"Manual recovery required"; break;
	default: break;
	}
	std::wstring summary = result.value.message;
	if (!result.value.backup.empty()) summary += L"\r\n\r\nBackup: " + result.value.backup;
	for (const auto& path : result.value.changed) summary += L"\r\nUpdated: " + path;
	for (const auto& path : result.value.manualRecovery) summary += L"\r\nManual recovery: " + path;
	F1Result(tray, status, summary);
}

bool CafeShelf::TryOpen(const wchar_t* file)
{
	const auto& skinPath = GetRainmeter().GetSkinPath();
	if (!CafeLock::IsShelfSuiteConfigurator(file, skinPath)) return false;
	// Always consume this path; never fall back to an externally writable page.
	if (!CafeLock::IsLocked())
	{
		OpenEditor(GetRainmeter().GetModuleInstance(), skinPath + L"Shelf Suite\\",
			CafeLock::IsLocked, CafeLock::LockNow, [](const std::wstring& shelf, bool removed)
			{
				if (CafeLock::IsLocked()) return;
				if (auto skin = GetRainmeter().GetSkin(L"Shelf Suite\\" + shelf))
				{
					if (removed) GetRainmeter().DeactivateSkin(skin, -1);
					else PostMessageW(skin->GetWindow(), WM_METERWINDOW_DELAYED_REFRESH, 0, 0);
				}
			});
	}
	return true;
}
