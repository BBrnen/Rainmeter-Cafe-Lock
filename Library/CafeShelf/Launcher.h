#pragma once
#include "Controller.h"

namespace CafeShelf
{
struct LauncherDraft
{
	std::wstring name;
	// Raw original filesystem path, not a command line or expanded shortcut.
	std::wstring action;
	// The original shell item is also the icon source, preserving .lnk icons.
	std::wstring iconSource;
};
// Call on a COM-initialized worker; this performs metadata reads only.
// Authorization is checked by the caller before accepting a resulting draft.
Result<LauncherDraft> InspectLauncher(const SelectedFile& selection);
}
