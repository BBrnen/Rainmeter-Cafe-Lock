#include "../Library/CafeShelf/Controller.h"
#include <iostream>
using namespace CafeShelf;
namespace
{
int checks = 0, failures = 0;
void Check(const char* name, bool passed)
{
	++checks;
	std::cout << (passed ? "PASS " : "FAIL ") << name << '\n';
	if (!passed) ++failures;
}
}
int main()
{
	bool locked = true;
	Controller controller([&locked]() { return locked; });
	auto denied = controller.Open();
	Check("locked editor never opens", !controller.Allows(denied) && denied.editor == 0);
	locked = false;
	auto ticket = controller.Open();
	Check("maintenance editor opens", controller.Allows(ticket));
	auto focused = controller.Open();
	Check("opening twice focuses same lifetime", ticket.editor != 0 && ticket.editor == focused.editor && ticket.generation == focused.generation);
	SelectedFile shortcut{L"C:\\Test\\original.lnk", false};
	auto selection = controller.Remember(ticket, shortcut);
	Check("native selection receives opaque id", selection.ok && selection.value != 0);
	auto read = controller.GetSelection(ticket, selection.value);
	Check("selection preserves original path", read.ok && read.value.path == shortcut.path && !read.value.directory);
	Check("unknown selection rejected", !controller.GetSelection(ticket, selection.value + 1).ok);
	const auto request = R"({"id":1,"op":"load","payload":{}})";
	Check("trusted request allowed", controller.Receive(ticket, HostPolicy::Page(), true, request).ok);
	Check("request replay rejected", !controller.Receive(ticket, HostPolicy::Page(), true, request).ok);
	locked = true;
	Check("lock is read live", !controller.Allows(ticket));
	Check("locked selection cannot be read", !controller.GetSelection(ticket, selection.value).ok);
	Check("locked callback cannot retain new selection", !controller.Remember(ticket, shortcut).ok);
	controller.Revoke();
	locked = false;
	auto second = controller.Open();
	Check("old callback stays invalid after unlock", !controller.Allows(ticket));
	Check("new lifetime works after unlock", controller.Allows(second));
	Check("old selections are cleared", !controller.GetSelection(second, selection.value).ok);
	auto replacement = controller.Remember(second, shortcut);
	controller.CancelDraft(second);
	Check("cancel removes pending selections", !controller.GetSelection(second, replacement.value).ok);
	Check("cancel does not close editor", controller.Allows(second));
	Check("empty native selection rejected", !controller.Remember(second, SelectedFile{}).ok);
	Check("page cannot supply arbitrary path", !controller.Receive(second, HostPolicy::Page(), true,
		R"({"id":2,"op":"importDrop","payload":{"purpose":"launcher","path":"C:\\secret"}})").ok);
	Controller missingAuthority({});
	Check("missing native authority fails closed", missingAuthority.Open().editor == 0);
	std::cout << checks << " controller checks, " << failures << " failures\n";
	return failures ? 1 : 0;
}
