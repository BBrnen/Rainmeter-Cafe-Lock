#pragma once
#include "Launcher.h"
#include <vector>

namespace CafeShelf
{
struct PngImage
{
	std::vector<uint8_t> bytes;
	unsigned width = 0;
	unsigned height = 0;
};
Result<PngImage> PrepareIcon(const SelectedFile& source);
Result<PngImage> PrepareLauncherIcon(const LauncherDraft& launcher);
std::wstring IconBaseName(const std::wstring& displayName);
}
