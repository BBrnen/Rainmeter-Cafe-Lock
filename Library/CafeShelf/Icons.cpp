#include "Icons.h"
namespace CafeShelf {
Result<PngImage> PrepareIcon(const SelectedFile&) { return {}; }
Result<PngImage> PrepareLauncherIcon(const LauncherDraft&) { return {}; }
std::wstring IconBaseName(const std::wstring&) { return {}; }
}
