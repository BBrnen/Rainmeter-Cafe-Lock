#include "Host.h"
#include <utility>
namespace CafeShelf
{
struct Host::State {};
Host::Host(HostOptions options) : m_Options(std::move(options)) {}
Host::~Host() { Close(); }
bool Host::Open() { return false; }
void Host::Close() {}
HWND Host::Window() const { return nullptr; }
void OpenEditor(HINSTANCE, const std::wstring&, bool (*)(), void (*)()) noexcept {}
void RevokeAndClose() noexcept {}
}
