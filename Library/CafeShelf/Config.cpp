#include "Config.h"
namespace CafeShelf {
Result<ShelfDocument> ParseConfig(const std::string&) { return {}; }
Result<std::string> ApplyEdit(const ShelfDocument&, const Edit&, size_t, size_t) { return {}; }
Result<std::string> LuaString(const std::wstring&) { return {}; }
}
