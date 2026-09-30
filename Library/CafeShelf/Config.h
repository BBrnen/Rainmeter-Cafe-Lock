#pragma once
#include "Types.h"
#include <vector>

namespace CafeShelf
{
struct LuaNode
{
	enum class Kind { Table, String, Literal } kind = Kind::Literal;
	size_t begin = 0, end = 0;
	std::string text;
	std::vector<LuaNode> children;
	std::vector<std::string> keys;
	std::vector<size_t> starts, ends;
};
struct LauncherItem { std::wstring label, action, icon; };
struct ShelfTab { std::wstring name; std::vector<LauncherItem> items; };
struct ShelfDocument
{
	std::string source;
	LuaNode root;
	std::vector<ShelfTab> tabs;
	std::wstring defaultIcon;
};
enum class EditKind { SetItem, AddItem, RemoveItem, RenameTab, AddTab, RemoveTab, SetTheme, AddShelf, RemoveShelf };
struct Edit
{
	EditKind kind = EditKind::SetItem;
	size_t tab = 0, item = 0;
	std::wstring label, action, icon;
};
Result<ShelfDocument> ParseConfig(const std::string& bytes);
Result<std::string> ApplyEdit(const ShelfDocument& document, const Edit& edit,
	size_t tabCapacity, size_t itemCapacity);
Result<std::string> LuaString(const std::wstring& text);
Result<std::string> NameString(const std::wstring& text);
}
