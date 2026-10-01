#pragma once
#include "Types.h"
#include <functional>
#include <string>
#include <vector>

namespace CafeShelf { namespace F1
{
enum class Status { Preview, AlreadyCompatible, Refused, Updated, FailedRecovered, ManualRecoveryRequired };
struct Change { std::wstring relativePath, originalHash, outputHash, identity; };
struct Preview { Status status = Status::Refused; std::wstring shelfRoot, proposedBackup, message, payloadPath; std::vector<Change> changes, compatible; };
struct ApplyResult { Status status = Status::Refused; std::wstring backup, message; std::vector<std::wstring> changed, manualRecovery; };
Result<Preview> Inspect(const std::wstring& skinPath, const std::wstring& payloadPath);
Result<ApplyResult> Apply(const Preview& preview, const std::function<bool()>& authorized);
}
}
