#include "Protocol.h"
#include <set>
#include <vector>

namespace CafeShelf
{
namespace
{
// SAX validates limits before building a DOM. Returning false stops the parser
// immediately, including at the first container beyond the allowed depth.
class BoundedJson : public nlohmann::json_sax<Json>
{
public:
	bool null() override { return true; }
	bool boolean(bool) override { return true; }
	bool number_integer(number_integer_t) override { return true; }
	bool number_unsigned(number_unsigned_t) override { return true; }
	bool number_float(number_float_t, const string_t&) override { return true; }
	bool string(string_t& value) override { return StringAllowed(value); }
	bool binary(binary_t&) override { return false; }
	bool start_object(size_t) override { return Start(true); }
	bool end_object() override { return End(true); }
	bool start_array(size_t) override { return Start(false); }
	bool end_array() override { return End(false); }
	bool key(string_t& value) override
	{
		return !m_Stack.empty() && m_Stack.back().object &&
			StringAllowed(value) && m_Stack.back().keys.insert(value).second;
	}
	bool parse_error(size_t, const std::string&, const nlohmann::detail::exception&) override
	{
		return false;
	}

private:
	struct Frame
	{
		bool object;
		std::set<std::string> keys;
	};
	std::vector<Frame> m_Stack;

	static bool StringAllowed(const std::string& value)
	{
		// The JSON lexer has already validated UTF-8. Count UTF-16 units,
		// including both surrogate units for a supplementary code point.
		size_t units = 0;
		for (const unsigned char ch : value)
		{
			if (!ch) return false;
			if ((ch & 0xc0) != 0x80) units += ch >= 0xf0 ? 2 : 1;
			if (units > MaxStringUnits) return false;
		}
		return true;
	}
	bool Start(bool object)
	{
		if (m_Stack.size() >= MaxJsonDepth) return false;
		m_Stack.push_back({object, {}});
		return true;
	}
	bool End(bool object)
	{
		if (m_Stack.empty() || m_Stack.back().object != object) return false;
		m_Stack.pop_back();
		return true;
	}
};

const char* ErrorName(Error code)
{
	switch (code)
	{
	case Error::None: return "None";
	case Error::Locked: return "Locked";
	case Error::Stale: return "Stale";
	case Error::InvalidInput: return "InvalidInput";
	case Error::Unsupported: return "Unsupported";
	case Error::NotFound: return "NotFound";
	case Error::Conflict: return "Conflict";
	case Error::AccessDenied: return "AccessDenied";
	case Error::IoError: return "IoError";
	case Error::Cancelled: return "Cancelled";
	case Error::RuntimeMissing: return "RuntimeMissing";
	default: return "InvalidInput";
	}
}
}

Result<Request> DecodeRequest(const std::string& utf8)
{
	Result<Request> result;
	if (utf8.empty() || utf8.size() > MaxMessageBytes ||
		utf8.find('\0') != std::string::npos) return result;
	try
	{
		BoundedJson validator;
		if (!Json::sax_parse(utf8, &validator)) return result;
		const auto json = Json::parse(utf8);
		if (!json.is_object() || json.size() != 3 || !json.contains("id") ||
			!json.contains("op") || !json.contains("payload")) return result;
		if (!json["id"].is_number_unsigned() || !json["op"].is_string() ||
			!json["payload"].is_object()) return result;
		const auto id = json["id"].get<uint64_t>();
		if (!id || id > 9007199254740991ULL) return result;
		const auto& op = json["op"].get_ref<const std::string&>();
		const auto& payload = json["payload"];
		Operation operation;
		if (op == "importDrop")
		{
			if (payload.size() != 1 || !payload.contains("purpose") ||
				!payload["purpose"].is_string()) return result;
			const auto& purpose = payload["purpose"].get_ref<const std::string&>();
			if (purpose != "launcher" && purpose != "icon") return result;
			operation = Operation::ImportDrop;
		}
		else
		{
			if (!payload.empty()) return result;
			if (op == "load") operation = Operation::Load;
			else if (op == "browseLauncher") operation = Operation::BrowseLauncher;
			else if (op == "browseFolder") operation = Operation::BrowseFolder;
			else if (op == "browseIcon") operation = Operation::BrowseIcon;
			else if (op == "cancelDraft") operation = Operation::CancelDraft;
			else if (op == "lockNow") operation = Operation::LockNow;
			else
			{
				// saveEdits is deliberately unavailable until its typed schema
				// and transactional storage are implemented together.
				result.code = Error::Unsupported;
				return result;
			}
		}
		result.value.id = id;
		result.value.operation = operation;
		result.value.payload = payload;
		result.ok = true;
		result.code = Error::None;
	}
	catch (const std::exception&)
	{
		// Never echo raw page input or exception text into application logs.
	}
	return result;
}

Json EncodeResponse(const Response& response)
{
	return {{"id", response.id}, {"ok", response.ok},
		{"code", ErrorName(response.code)}, {"data", response.data}};
}
}
