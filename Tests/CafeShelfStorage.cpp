#include "../Library/CafeShelf/Storage.h"
#include <windows.h>
#include <fstream>
#include <iostream>
using namespace CafeShelf;
namespace {
int checks=0, failures=0;
void Check(const char* name,bool ok) { ++checks; if(!ok)++failures; std::cout<<(ok?"PASS ":"FAIL ")<<name<<'\n'; }
std::string Read(const std::wstring& path) {
	std::ifstream in(path,std::ios::binary); return std::string(std::istreambuf_iterator<char>(in),{});
}
void Write(const std::wstring& path,const std::string& bytes) {
	std::ofstream out(path,std::ios::binary); out.write(bytes.data(),static_cast<std::streamsize>(bytes.size()));
}
}
int wmain(int argc,wchar_t** argv) {
	if(argc!=2 || FAILED(CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED)))return 2;
	const std::wstring root=argv[1], shelf=root+L"\\Shelf1", icons=root+L"\\@Resources\\Icons";
	Storage storage(root);
	auto discovered=storage.Discover();
	Check("recognized shelf discovered with real meter capacities", discovered.ok && discovered.value.size()==1 &&
		discovered.value[0].tabCapacity==5 && discovered.value[0].itemCapacity==18);
	auto loaded=storage.Load(L"Shelf1");
	Check("existing literal config loads with snapshot version", loaded.ok && !loaded.value.version.empty());
	Check("traversal shelf rejected", !storage.Load(L"..\\outside").ok);
	Check("unknown shelf rejected", !storage.Load(L"Shelf999").ok);
	Check("UNC editing root rejected", !Storage(L"\\\\server\\share").Discover().ok);
	const std::string original=Read(shelf+L"\\config.lua");
	auto image=PrepareIcon({root+L"\\Transparent.png",false});
	Check("storage fixture icon prepared", image.ok);
	Edit edit{EditKind::AddItem,0,0,L"Spotify",L"notepad.exe",L"file.png"};
	Write(icons+L"\\spotify.png","owner icon");
	auto prepared=storage.Prepare(loaded.value,edit,image.ok?&image.value:nullptr,L"spotify");
	Check("save prepares without replacing config", prepared.ok && Read(shelf+L"\\config.lua")==original);
	auto denied=storage.Commit(prepared.value,[](){return false;});
	Check("revoked save cannot commit", !denied.ok && Read(shelf+L"\\config.lua")==original);
	prepared=storage.Prepare(loaded.value,edit,image.ok?&image.value:nullptr,L"Spotify");
	auto saved=storage.Commit(prepared.value,[](){return true;});
	Check("case-insensitive icon collision picks new filename", saved.ok && saved.value.icon==L"Spotify-2.png");
	Check("existing icon is never overwritten", Read(icons+L"\\spotify.png")=="owner icon");
	Check("saved PNG exactly matches preview bytes", saved.ok && Read(icons+L"\\"+saved.value.icon)==
		std::string(reinterpret_cast<const char*>(image.value.bytes.data()),image.value.bytes.size()));
	Check("previous config has recoverable backup", saved.ok && Read(saved.value.backup)==original);
	auto after=storage.Load(L"Shelf1");
	Check("saved config references imported icon", after.ok && after.value.document.tabs[0].items.back().icon==saved.value.icon);
	Check("same prepared transaction cannot commit twice", !storage.Commit(prepared.value,[](){return true;}).ok);
	auto stale=storage.Prepare(loaded.value,edit,nullptr,L"");
	Check("changed snapshot cannot overwrite later edit", !stale.ok ||
		!storage.Commit(stale.value,[](){return true;}).ok);
	loaded=storage.Load(L"Shelf1");
	prepared=storage.Prepare(loaded.value,edit,nullptr,L"");
	const auto edited=Read(shelf+L"\\config.lua")+"\n-- concurrent owner edit\n";
	Write(shelf+L"\\config.lua",edited);
	Check("concurrent source edit rejects replacement", !storage.Commit(prepared.value,[](){return true;}).ok &&
		Read(shelf+L"\\config.lua")==edited);
	const auto hard=shelf+L"\\config.lua";
	const auto alias=root+L"\\hardlink.lua";
	Check("hardlink fixture created", CreateHardLinkW(alias.c_str(),hard.c_str(),nullptr)!=FALSE);
	Check("hardlinked config is read-only to editor", !storage.Load(L"Shelf1").ok);
	DeleteFileW(alias.c_str());
	CoUninitialize();
	std::cout<<checks<<" storage checks, "<<failures<<" failures\n";
	return failures?1:0;
}
