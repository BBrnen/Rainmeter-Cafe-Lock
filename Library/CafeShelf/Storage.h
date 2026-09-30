#pragma once
#include "Config.h"
#include "Icons.h"
#include <functional>
#include <memory>

namespace CafeShelf
{
struct ShelfInfo
{
	std::wstring id;
	size_t tabCapacity = 0, itemCapacity = 0;
};
struct Snapshot
{
	ShelfInfo shelf;
	ShelfDocument document;
	std::string version;
	bool example = false;
};
struct SaveResult { std::wstring icon, backup, warning; };
class PreparedSave;
class Storage
{
public:
	explicit Storage(std::wstring root);
	Result<std::vector<ShelfInfo>> Discover() const;
	Result<Snapshot> Load(const std::wstring& shelf) const;
	Result<std::shared_ptr<PreparedSave>> Prepare(const Snapshot& snapshot,
		const Edit& edit, const PngImage* icon, const std::wstring& iconName) const;
	// Called only on the native UI thread, serialized with Lock Now.
	Result<SaveResult> Commit(const std::shared_ptr<PreparedSave>& save,
		const std::function<bool()>& authorized) const;
private:
	std::wstring m_Root;
};
}
