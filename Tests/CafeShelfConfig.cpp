#include "../Library/CafeShelf/Config.h"
#include <windows.h>
#include <iostream>
using namespace CafeShelf;
namespace {
int checks = 0, failures = 0;
void Check(const char* name, bool value) {
	++checks; if (!value) ++failures;
	std::cout << (value ? "PASS " : "FAIL ") << name << '\n';
}
}
int main() {
	const std::string original =
		"-- owner comment\r\nShelfConfig = {\r\n defaultIcon='file.png', extra={flag=true,number=-2.5},\r\n"
		" tabs={{name='Apps', items={{label='One', action='one.exe', icon='one.png', private='KEEP'},"
		"{'Two', 'two.exe', 'two.png'}}}}\r\n}\r\n-- footer\r\n";
	const auto parsed = ParseConfig(original);
	Check("literal named and tuple items load", parsed.ok && parsed.value.tabs.size()==1 && parsed.value.tabs[0].items.size()==2);
	Check("unknown literal fields retained in document", parsed.ok && parsed.value.source==original);
	Edit change{EditKind::SetItem, 0, 0, L"O'Brien", L"C:\\Tools\\app.exe", L"new.png"};
	const auto changed = ApplyEdit(parsed.value, change, 5, 18);
	Check("edit succeeds", changed.ok);
	Check("owner comments unknown fields and CRLF survive", changed.ok && changed.value.find("-- owner comment\r\n")==0 &&
		changed.value.find("private='KEEP'")!=std::string::npos && changed.value.find("extra={flag=true,number=-2.5}")!=std::string::npos &&
		changed.value.find("-- footer\r\n")!=std::string::npos);
	const auto again=ParseConfig(changed.value);
	Check("quotes and backslashes round trip", again.ok && again.value.tabs[0].items[0].label==change.label && again.value.tabs[0].items[0].action==change.action);
	change.item=1;
	const auto tuple=ApplyEdit(parsed.value,change,5,18);
	Check("tuple item edit preserves tuple representation", tuple.ok && tuple.value.find("{\"O'Brien\",")!=std::string::npos);
	change.kind=EditKind::AddItem; change.item=0;
	const auto added=ApplyEdit(parsed.value,change,5,18);
	const auto addedDoc=ParseConfig(added.value);
	Check("add item loads with existing items intact", addedDoc.ok && addedDoc.value.tabs[0].items.size()==3 && addedDoc.value.tabs[0].items[1].label==L"Two");
	Check("meter capacity enforced", !ApplyEdit(parsed.value,change,5,2).ok);
	change.kind=EditKind::RemoveItem; change.item=0;
	const auto removed=ApplyEdit(parsed.value,change,5,18);
	const auto removedDoc=ParseConfig(removed.value);
	Check("remove item leaves a valid table", removedDoc.ok && removedDoc.value.tabs[0].items.size()==1 && removedDoc.value.tabs[0].items[0].label==L"Two");
	change.kind=EditKind::RenameTab; change.label=L"New tab";
	const auto renamed=ApplyEdit(parsed.value,change,5,18);
	Check("rename tab works", renamed.ok && ParseConfig(renamed.value).value.tabs[0].name==L"New tab");
	change.kind=EditKind::AddTab;
	Check("add tab works", ParseConfig(ApplyEdit(parsed.value,change,5,18).value).value.tabs.size()==2);
	Check("tab capacity enforced", !ApplyEdit(parsed.value,change,1,18).ok);
	Check("executable statement rejected", !ParseConfig(original+"os.execute('bad')").ok);
	Check("function call rejected", !ParseConfig("ShelfConfig={tabs=loadTabs()}").ok);
	Check("duplicate key rejected", !ParseConfig("ShelfConfig={tabs={},tabs={}}").ok);
	Check("computed expression rejected", !ParseConfig("ShelfConfig={tabs={},x=1+2}").ok);
	Check("unterminated string rejected", !ParseConfig("ShelfConfig={tabs={},x='oops}").ok);
	Check("long literal string and comment supported", ParseConfig("--[=[comment]=]\nShelfConfig={tabs={{name=[=[A]=],items={}}}}").ok);
	Check("BOM config is read-only for the pinned Lua 5.1 loader", !ParseConfig(std::string("\xEF\xBB\xBF")+original).ok);
	Check("UTF16 config rejected rather than written for non-Unicode Lua dofile", !ParseConfig(std::string("\xFF\xFE")+original).ok);
	Check("large config rejected", !ParseConfig(std::string(4*1024*1024+1,' ')).ok);
	std::wstring unicode=L"Caf\u00e9";
	const auto escaped=LuaString(unicode);
	Check("representable Unicode uses Lua byte escapes", escaped.ok && escaped.value.find("\\")!=std::string::npos);
	Check("embedded NUL rejected", !LuaString(std::wstring(L"a\0b",3)).ok);
	change.kind=EditKind::SetItem; change.action=L"good.exe\"][!Quit]";
	Check("edited action cannot inject Rainmeter bangs", !ApplyEdit(parsed.value,change,5,18).ok);
	change.action=L"app.exe"; change.icon=L"..\\outside.png";
	Check("new icon reference cannot escape Icons", !ApplyEdit(parsed.value,change,5,18).ok);
	std::string deep="ShelfConfig={tabs={},x="; for(int i=0;i<65;++i)deep+="{";
	deep+="true"; for(int i=0;i<65;++i)deep+="}"; deep+="}";
	Check("nested input bounded", !ParseConfig(deep).ok);
	std::cout<<checks<<" config checks, "<<failures<<" failures\n";
	return failures?1:0;
}
