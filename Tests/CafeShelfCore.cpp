#include "../Library/CafeShelf/Session.h"
#include "../Library/CafeShelf/Protocol.h"
#include <iostream>

using namespace CafeShelf;
namespace
{
int failures = 0;
int checks = 0;
void Check(const char* name, bool passed)
{
	++checks;
	std::cout << (passed ? "PASS " : "FAIL ") << name << '\n';
	if (!passed) ++failures;
}
std::string Message(const char* op, Json payload = Json::object())
{
	return Json{{"id", 1}, {"op", op}, {"payload", payload}}.dump();
}
}

int main()
{
	Session session;
	Check("no editor authorized initially", !session.Allows({}, false));
	const auto denied = session.Open(true);
	Check("locked open cannot become authorized", !session.Allows(denied, false));
	const auto first = session.Open(false);
	Check("maintenance opens native editor session", session.Allows(first, false));
	Check("live lock overrides ticket", !session.Allows(first, true));
	Check("wrong editor is rejected", !session.Allows({first.editor + 1, first.generation}, false));
	Check("first request accepted", session.Accept(first, false, 1));
	Check("duplicate request rejected", !session.Accept(first, false, 1));
	Check("zero request rejected", !session.Accept(first, false, 0));
	Check("increasing request accepted", session.Accept(first, false, 7));
	Check("out of order request rejected", !session.Accept(first, false, 6));
	session.Revoke();
	Check("revocation invalidates ticket", !session.Allows(first, false));
	const auto second = session.Open(false);
	Check("new editor can open", session.Allows(second, false));
	Check("old ticket stays revoked after reopen", !session.Allows(first, false));
	Check("request sequence belongs to new editor", session.Accept(second, false, 1));
	Session other;
	Check("another session ticket is rejected", !session.Allows(other.Open(false), false));
	session.Open(true);
	Check("locked open revokes existing session", !session.Allows(second, false));

	for (const char* operation : {"load", "browseLauncher", "browseFolder", "browseIcon", "cancelDraft", "lockNow"})
	{
		Check(operation, DecodeRequest(Message(operation)).ok);
	}
	auto drop = DecodeRequest(Message("importDrop", {{"purpose", "launcher"}}));
	Check("typed launcher drop", drop.ok && drop.value.operation == Operation::ImportDrop);
	Check("typed icon drop", DecodeRequest(Message("importDrop", {{"purpose", "icon"}})).ok);
	Check("no unlock operation", !DecodeRequest(Message("unlock")).ok);
	Check("no generic file write", !DecodeRequest(Message("writeFile")).ok);
	Check("empty save request rejected", !DecodeRequest(Message("saveEdits")).ok);
	Check("load accepts no page path", !DecodeRequest(Message("load", {{"path", "C:/config.lua"}})).ok);
	Check("drop accepts no page path", !DecodeRequest(Message("importDrop", {{"purpose", "icon"}, {"path", "C:/x"}})).ok);
	Check("unknown drop purpose", !DecodeRequest(Message("importDrop", {{"purpose", "execute"}})).ok);
	Check("payload must be object", !DecodeRequest(Message("load", Json::array())).ok);
	Check("page cannot grant authorization", !DecodeRequest(R"({"id":1,"op":"load","payload":{},"unlocked":true})").ok);
	Check("duplicate envelope key", !DecodeRequest(R"({"id":1,"id":2,"op":"load","payload":{}})").ok);
	Check("duplicate payload key", !DecodeRequest(R"({"id":1,"op":"importDrop","payload":{"purpose":"icon","purpose":"launcher"}})").ok);
	for (const char* badId : {"-1", "1.5", "0", "9007199254740992"})
	{
		Check("invalid request id", !DecodeRequest(std::string("{\"id\":") + badId + ",\"op\":\"load\",\"payload\":{}}").ok);
	}
	Check("malformed JSON", !DecodeRequest("{").ok);
	Check("trailing JSON", !DecodeRequest(Message("load") + "{}").ok);
	Check("oversized message", !DecodeRequest(std::string(MaxMessageBytes + 1, ' ')).ok);
	Check("invalid UTF-8", !DecodeRequest(std::string("{\"id\":1,\"op\":\"") + '\xff' + "\",\"payload\":{}}").ok);
	Check("embedded null", !DecodeRequest(Message("importDrop", {{"purpose", std::string("icon\0", 5)}})).ok);
	Check("oversized string", !DecodeRequest(Message("importDrop", {{"purpose", std::string(MaxStringUnits + 1, 'a')}})).ok);
	Check("excessive nesting", !DecodeRequest(std::string(MaxJsonDepth + 1, '[') + "0" + std::string(MaxJsonDepth + 1, ']')).ok);

	Json edits={{"shelf","Shelf1"},{"version","native-version"},{"iconId",0},
		{"edit",{{"kind","addItem"},{"tab",0},{"item",0},{"label","App"},{"action","app.exe"},{"icon","file.png"}}}};
	Check("typed launcher save accepted", DecodeRequest(Message("saveEdits",edits)).ok);
	edits["path"]="C:/outside";
	Check("save cannot choose a destination path", !DecodeRequest(Message("saveEdits",edits)).ok);
	edits.erase("path"); edits["edit"]["lua"]="os.execute('bad')";
	Check("save cannot contain executable Lua", !DecodeRequest(Message("saveEdits",edits)).ok);
	edits["edit"].erase("lua"); edits["edit"]["kind"]="unlock";
	Check("edit kinds cannot authorize unlock", !DecodeRequest(Message("saveEdits",edits)).ok);
	auto valid = DecodeRequest(R"({"id":42,"op":"browseFolder","payload":{}})");
	Check("request preserves typed fields", valid.ok && valid.value.id == 42 && valid.value.operation == Operation::BrowseFolder);
	Response response;
	response.id = 42;
	response.ok = true;
	response.code = Error::None;
	response.data = {{"name", u8"<script>\"\u2615\"</script>"}};
	const auto encoded = EncodeResponse(response);
	Check("response carries request id", encoded.value("id", uint64_t(0)) == 42);
	Check("response carries success", encoded.value("ok", false));
	Check("response values remain data", Json::parse(encoded.dump()).value("data", Json::object()) == response.data);
	std::cout << checks << " checks, " << failures << " failures\n";
	return failures ? 1 : 0;
}
