#include "Storage.h"
namespace CafeShelf {
Storage::Storage(std::wstring root) : m_Root(std::move(root)) {}
Result<std::vector<ShelfInfo>> Storage::Discover() const { return {}; }
Result<Snapshot> Storage::Load(const std::wstring&) const { return {}; }
Result<std::shared_ptr<PreparedSave>> Storage::Prepare(const Snapshot&, const Edit&, const PngImage*, const std::wstring&) const { return {}; }
Result<SaveResult> Storage::Commit(const std::shared_ptr<PreparedSave>&, const std::function<bool()>&) const { return {}; }
}
