#include "../Library/CafeShelf/Storage.h"
#include <windows.h>
#include <objbase.h>
#include <fstream>
#include <iostream>
using namespace CafeShelf;
namespace {
int checks=0, failures=0;
void Check(const char* name,bool ok) { ++checks; if(!ok)++failures; std::cout<<(ok?"PASS ":"FAIL ")<<name<<'\n'; }
std::string Read(const std::wstring& path) {
	HANDLE file=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,0,nullptr);
	if(file==INVALID_HANDLE_VALUE)return {};
	LARGE_INTEGER size={}; std::string bytes; DWORD read=0;
	if(GetFileSizeEx(file,&size) && size.QuadPart>=0 && size.QuadPart<=4*1024*1024) {
		bytes.resize(static_cast<size_t>(size.QuadPart));
		if(!ReadFile(file,bytes.empty()?nullptr:&bytes[0],static_cast<DWORD>(bytes.size()),&read,nullptr) || read!=bytes.size())bytes.clear();
	}
	CloseHandle(file);return bytes;
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
	loaded=storage.Load(L"Shelf1");
	auto first=storage.Prepare(loaded.value,edit,&image.value,L"parallel");
	auto second=storage.Prepare(loaded.value,edit,&image.value,L"parallel");
	Check("simultaneous reservations use unique complete icons",first.ok && second.ok &&
		Read(icons+L"\\parallel.png")==Read(icons+L"\\parallel-2.png") && !Read(icons+L"\\parallel.png").empty());
	storage.Commit(first.value,[](){return false;}); storage.Commit(second.value,[](){return false;});
	Check("cancelled reservations clean up only owned files",GetFileAttributesW((icons+L"\\parallel.png").c_str())==INVALID_FILE_ATTRIBUTES &&
		GetFileAttributesW((icons+L"\\parallel-2.png").c_str())==INVALID_FILE_ATTRIBUTES && Read(icons+L"\\spotify.png")=="owner icon");
	prepared=storage.Prepare(loaded.value,edit,nullptr,L"");
	Check("prepared save pins shelf against redirection",prepared.ok && !MoveFileW(shelf.c_str(),(root+L"\\MovedShelf").c_str()));
	storage.Commit(prepared.value,[](){return false;});
	prepared=storage.Prepare(loaded.value,edit,&image.value,L"failed-save");
	const auto beforeFailure=Read(hard);
	SetFileAttributesW(hard.c_str(),FILE_ATTRIBUTE_READONLY);
	auto readOnly=storage.Commit(prepared.value,[](){return true;});
	Check("replacement failure keeps old config and removes new icon",!readOnly.ok && Read(hard)==beforeFailure &&
		GetFileAttributesW((icons+L"\\failed-save.png").c_str())==INVALID_FILE_ATTRIBUTES);
	SetFileAttributesW(hard.c_str(),FILE_ATTRIBUTE_NORMAL);
	Check("redirected editing root rejected",!Storage(root+L"\\RedirectedRoot").Discover().ok);
	// Inject an outside edit at the final authorization boundary: commit must not overwrite it.
	prepared=storage.Prepare(loaded.value,edit,nullptr,L"");
	int gates=0; const auto validated=Read(hard); const auto raced=validated+"\n-- final boundary edit\n";
	bool writeBlocked=false;
	auto race=storage.Commit(prepared.value,[&](){
		if(++gates==2) {
			HANDLE writer=CreateFileW(hard.c_str(),GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,0,nullptr);
			writeBlocked=writer==INVALID_HANDLE_VALUE;
			if(!writeBlocked){CloseHandle(writer);Write(hard,raced);}
		}
		return true;
	});
	Check("final read lease blocks ordinary concurrent writes",race.ok && writeBlocked && Read(race.value.backup)==validated);
	loaded=storage.Load(L"Shelf1");prepared=storage.Prepare(loaded.value,edit,nullptr,L"");
	const auto outsideVersion=Read(hard)+"\n-- outside replacement\n";
	const auto external=root+L"\\external.lua";Write(external,outsideVersion);gates=0;bool moved=false;
	auto renameRace=storage.Commit(prepared.value,[&](){
		if(++gates==2)moved=MoveFileExW(external.c_str(),hard.c_str(),MOVEFILE_REPLACE_EXISTING)!=FALSE;
		return true;
	});
	Check("concurrent replacement is preserved and reported",renameRace.ok && moved && Read(renameRace.value.backup)==outsideVersion && !renameRace.value.warning.empty());
	loaded=storage.Load(L"Shelf1");prepared=storage.Prepare(loaded.value,edit,nullptr,L"");
	const auto busyOriginal=Read(hard);
	HANDLE busyWriter=CreateFileW(hard.c_str(),GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,0,nullptr);
	auto busySave=storage.Commit(prepared.value,[](){return true;});
	Check("existing writer causes safe save failure",busyWriter!=INVALID_HANDLE_VALUE && !busySave.ok && Read(hard)==busyOriginal);
	if(busyWriter!=INVALID_HANDLE_VALUE)CloseHandle(busyWriter);
	const auto example=shelf+L"\\config.example.lua";
	MoveFileW(hard.c_str(),example.c_str());loaded=storage.Load(L"Shelf1");
	prepared=storage.Prepare(loaded.value,edit,nullptr,L"");gates=0;
	auto created=storage.Commit(prepared.value,[&](){if(++gates==2)Write(hard,busyOriginal+"\n-- owner created config\n");return true;});
	Check("example-based save never overwrites newly created config",loaded.ok && loaded.value.example && prepared.ok && !created.ok &&
		Read(hard)==busyOriginal+"\n-- owner created config\n");
	// Compare native replacement behavior in isolated files, never user data.
	const auto replaceOld=root+L"\\replace-old.txt",replaceNew=root+L"\\replace-new.txt",replaceBackup=root+L"\\replace-backup.txt";
	Write(replaceOld,"actual outside edit");Write(replaceNew,"prepared edit");
	HANDLE held=CreateFileW(replaceOld.c_str(),GENERIC_READ|DELETE,FILE_SHARE_READ|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,nullptr);
	BOOL replaced=ReplaceFileW(replaceOld.c_str(),replaceNew.c_str(),replaceBackup.c_str(),0,nullptr,nullptr);
	Check("native replacement preserves actual replaced bytes with read lease",held!=INVALID_HANDLE_VALUE && replaced &&
		Read(replaceOld)=="prepared edit" && Read(replaceBackup)=="actual outside edit");
	if(held!=INVALID_HANDLE_VALUE)CloseHandle(held);
	const auto outside=root+L"\\replace-outside.txt",link=root+L"\\replace-link.txt",linkNew=root+L"\\replace-link-new.txt",linkBackup=root+L"\\replace-link-backup.txt";
	Write(outside,"outside owner");Write(linkNew,"replacement");
	const BOOL linkMade=CreateSymbolicLinkW(link.c_str(),outside.c_str(),0);
	Check("symbolic replacement fixture created",linkMade!=FALSE);
	if(linkMade) {
		const BOOL linkReplaced=ReplaceFileW(link.c_str(),linkNew.c_str(),linkBackup.c_str(),0,nullptr,nullptr);
		std::cout<<"ReplaceFile link result="<<linkReplaced<<" error="<<(linkReplaced?0:GetLastError())<<'\n';
		Check("native replacement does not alter a symbolic target",Read(outside)=="outside owner");
	}
	CoUninitialize();
	std::cout<<checks<<" storage checks, "<<failures<<" failures\n";
	return failures?1:0;
}
