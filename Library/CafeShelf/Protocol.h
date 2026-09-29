#pragma once

#include "Types.h"
#include "../json/json.hpp"

namespace CafeShelf
{
using Json = nlohmann::json;

enum class Operation
{
	Load, BrowseLauncher, BrowseFolder, BrowseIcon, ImportDrop,
	SaveEdits, CancelDraft, LockNow
};

struct Request
{
	uint64_t id = 0;
	Operation operation = Operation::Load;
	Json payload = Json::object();
};

struct Response
{
	uint64_t id = 0;
	bool ok = false;
	Error code = Error::InvalidInput;
	Json data = Json::object();
};

Result<Request> DecodeRequest(const std::string& utf8);
Json EncodeResponse(const Response& response);
}
