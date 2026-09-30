// Rainmeter Cafe Lock. GNU GPL v2 or later; see LICENSE.
#include "StdAfx.h"
#include "CafeLock.h"
#include "CafeShelf/Host.h"
#include "../Common/CafePassword.h"
#include "Rainmeter.h"
#include "Skin.h"
#include "TrayIcon.h"
#include "DialogManage.h"
#include "DialogNewSkin.h"
#include "DialogAbout.h"
#include "GameMode.h"
#include <memory>

namespace
{
std::unique_ptr<CafeSecurity::PasswordSession> session;
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

bool CafeShelf::TryOpen(const wchar_t* file)
{
	const auto& skinPath = GetRainmeter().GetSkinPath();
	if (!CafeLock::IsShelfSuiteConfigurator(file, skinPath)) return false;
	// Always consume this path; never fall back to an externally writable page.
	if (!CafeLock::IsLocked())
	{
		OpenEditor(GetRainmeter().GetModuleInstance(), skinPath + L"Shelf Suite\\",
			CafeLock::IsLocked, CafeLock::LockNow, [](const std::wstring& shelf)
			{
				if (CafeLock::IsLocked()) return;
				if (auto skin = GetRainmeter().GetSkin(L"Shelf Suite\\" + shelf))
					PostMessageW(skin->GetWindow(), WM_METERWINDOW_DELAYED_REFRESH, 0, 0);
			});
	}
	return true;
}
