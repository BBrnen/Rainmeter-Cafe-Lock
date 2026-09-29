#include "Protocol.h"

namespace CafeShelf
{
// Fail-closed scaffolding: no page request currently grants an operation.
Result<Request> DecodeRequest(const std::string&) { return {}; }
Json EncodeResponse(const Response&) { return Json::object(); }
}
