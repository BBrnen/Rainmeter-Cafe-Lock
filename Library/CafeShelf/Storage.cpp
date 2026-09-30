#include "Storage.h"
#include <windows.h>
#include <bcrypt.h>
#include <algorithm>
#include <set>
#include <sstream>
#include <cwctype>

namespace CafeShelf
{
namespace
{
struct Problem { Error code; const wchar_t* message; };
void Need(bool ok, Error code=Error::IoError, const wchar_t* message=L"The save could not complete. Your previous configuration is retained.")
{
	if(!ok)throw Problem{code,message};
}
struct Handle
{
	HANDLE value=INVALID_HANDLE_VALUE;
	Handle()=default;
	explicit Handle(HANDLE h):value(h){}
	Handle(const Handle&)=delete;
	Handle& operator=(const Handle&)=delete;
	Handle(Handle&& other) noexcept:value(other.value){other.value=INVALID_HANDLE_VALUE;}
	Handle& operator=(Handle&& other) noexcept { if(this!=&other){Close();value=other.value;other.value=INVALID_HANDLE_VALUE;}return *this; }
	void Close(){if(value!=INVALID_HANDLE_VALUE){CloseHandle(value);value=INVALID_HANDLE_VALUE;}}
	~Handle(){Close();}
};
struct OwnedFile
{
	Handle handle;
	std::wstring path;
	bool keep=false;
	~OwnedFile()
	{
		// Delete by our still-open identity, never by a potentially replaced path.
		if(!keep && handle.value!=INVALID_HANDLE_VALUE)
		{
			FILE_DISPOSITION_INFO disposition={TRUE};
			SetFileInformationByHandle(handle.value,FileDispositionInfo,&disposition,sizeof(disposition));
		}
	}
};
using Pins=std::vector<Handle>;
std::wstring Canonical(std::wstring root)
{
	while(root.size()>3 && root.back()==L'\\')root.pop_back();
	Need(root.size()>3 && root.size()<30000 && root[1]==L':' && root[2]==L'\\' &&
		((root[0]>=L'A' && root[0]<=L'Z') || (root[0]>=L'a' && root[0]<=L'z')) &&
		root.find_first_of(L"/?#%[]\r\n")==std::wstring::npos && root.find(L'\0')==std::wstring::npos &&
		root.find(L':',2)==std::wstring::npos,Error::Unsupported,L"Editing requires an ordinary local ShelfSuite folder.");
	const auto drive=root.substr(0,3);
	Need(GetDriveTypeW(drive.c_str())==DRIVE_FIXED,Error::Unsupported,L"Editing a network or removable ShelfSuite folder is not supported.");
	size_t p=3;
	while(p<root.size())
	{
		const auto end=root.find(L'\\',p);
		const auto part=root.substr(p,end==std::wstring::npos?root.size()-p:end-p);
		Need(!part.empty() && part!=L"." && part!=L".." && part.back()!=L'.' && part.back()!=L' ',
			Error::Unsupported,L"The ShelfSuite folder has an ambiguous path.");
		p=end==std::wstring::npos?root.size():end+1;
	}
	return root;
}
Pins Pin(const std::wstring& path)
{
	const auto root=Canonical(path); Pins pins;
	size_t end=2;
	while(end<root.size())
	{
		end=root.find(L'\\',end+1);
		const auto prefix=end==std::wstring::npos?root:root.substr(0,end);
		Handle h(CreateFileW(prefix.c_str(),FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,
			OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OPEN_REPARSE_POINT,nullptr));
		BY_HANDLE_FILE_INFORMATION info={};
		Need(h.value!=INVALID_HANDLE_VALUE && GetFileInformationByHandle(h.value,&info),Error::AccessDenied,L"The ShelfSuite folder cannot be opened safely.");
		Need((info.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)!=0 && !(info.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT),
			Error::Unsupported,L"Editing through a redirected folder is not supported.");
		// FileCaseSensitiveInfo (Windows 10+) has one ULONG Flags field.
		ULONG flags=0;
		if(GetFileInformationByHandleEx(h.value,static_cast<FILE_INFO_BY_HANDLE_CLASS>(23),&flags,sizeof(flags)))
			Need(!(flags&1),Error::Unsupported,L"Case-sensitive editing folders are not supported.");
		pins.push_back(std::move(h));
		if(end==std::wstring::npos)break;
	}
	return pins;
}
bool ShelfId(const std::wstring& id)
{
	return id.size()>5 && id.compare(0,5,L"Shelf")==0 && id[5]!=L'0' &&
		std::all_of(id.begin()+5,id.end(),[](wchar_t c){return c>=L'0' && c<=L'9';});
}
std::string Hash(const std::string& bytes)
{
	BCRYPT_ALG_HANDLE algorithm=nullptr; BCRYPT_HASH_HANDLE hash=nullptr;
	Need(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)>=0);
	struct Clean { BCRYPT_ALG_HANDLE& a; BCRYPT_HASH_HANDLE& h; ~Clean(){if(h)BCryptDestroyHash(h);if(a)BCryptCloseAlgorithmProvider(a,0);} } clean{algorithm,hash};
	Need(BCryptCreateHash(algorithm,&hash,nullptr,0,nullptr,0,0)>=0);
	Need(BCryptHashData(hash,reinterpret_cast<PUCHAR>(const_cast<char*>(bytes.data())),static_cast<ULONG>(bytes.size()),0)>=0);
	BYTE digest[32]={}; Need(BCryptFinishHash(hash,digest,sizeof(digest),0)>=0);
	std::string result; const char* digits="0123456789abcdef";
	for(const auto b:digest){result+=digits[b>>4];result+=digits[b&15];} return result;
}
struct ReadResult { std::string bytes, version; };
ReadResult Read(const std::wstring& path)
{
	Handle file(CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,nullptr));
	if(file.value==INVALID_HANDLE_VALUE)
	{
		const auto error=GetLastError();
		throw Problem{error==ERROR_FILE_NOT_FOUND?Error::NotFound:Error::AccessDenied,L"The configuration is missing, busy or not readable."};
	}
	BY_HANDLE_FILE_INFORMATION info={};
	Need(GetFileInformationByHandle(file.value,&info)!=FALSE);
	Need(!(info.dwFileAttributes&(FILE_ATTRIBUTE_REPARSE_POINT|FILE_ATTRIBUTE_DIRECTORY)) && info.nNumberOfLinks==1,
		Error::Unsupported,L"Redirected or hard-linked configuration files are read-only.");
	Need(info.nFileSizeHigh==0 && info.nFileSizeLow<=4*1024*1024,Error::Unsupported,L"The configuration is too large.");
	ReadResult result; result.bytes.resize(info.nFileSizeLow); DWORD read=0;
	Need(ReadFile(file.value,result.bytes.empty()?nullptr:&result.bytes[0],info.nFileSizeLow,&read,nullptr)!=FALSE && read==info.nFileSizeLow);
	result.version=std::to_string(info.dwVolumeSerialNumber)+":"+std::to_string(info.nFileIndexHigh)+":"+
		std::to_string(info.nFileIndexLow)+":"+Hash(result.bytes);
	return result;
}
std::wstring GuidName()
{
	GUID guid={}; wchar_t text[40]={};
	Need(SUCCEEDED(CoCreateGuid(&guid)) && StringFromGUID2(guid,text,40)>0);
	return text;
}
std::shared_ptr<OwnedFile> Create(const std::wstring& path,const std::string& bytes)
{
	auto file=std::make_shared<OwnedFile>(); file->path=path;
	file->handle=Handle(CreateFileW(path.c_str(),GENERIC_READ|GENERIC_WRITE|DELETE,FILE_SHARE_READ,nullptr,
		CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr));
	Need(file->handle.value!=INVALID_HANDLE_VALUE,GetLastError()==ERROR_FILE_EXISTS?Error::Conflict:Error::IoError);
	DWORD written=0;
	Need(WriteFile(file->handle.value,bytes.data(),static_cast<DWORD>(bytes.size()),&written,nullptr)!=FALSE &&
		written==bytes.size() && FlushFileBuffers(file->handle.value));
	return file;
}
ShelfInfo Info(const std::wstring& id,const std::string& ini)
{
	std::set<std::string> sections;
	std::istringstream input(ini); std::string line; bool engine=false;
	while(std::getline(input,line))
	{
		if(!line.empty() && line.back()=='\r')line.pop_back();
		const auto first=line.find_first_not_of(" \t"); if(first==std::string::npos)continue;
		line=line.substr(first,line.find_last_not_of(" \t")-first+1);
		if(line=="ScriptFile=#@#ShelfEngine.lua")engine=true;
		if(!line.empty() && line.front()=='[' && line.back()==']')Need(sections.insert(line).second,Error::Unsupported,L"Duplicate skin sections cannot be edited.");
	}
	Need(engine,Error::Unsupported,L"This folder does not use the supported ShelfSuite engine.");
	ShelfInfo result; result.id=id;
	for(size_t i=1;i<=5;++i)
	{
		if(!sections.count("[MeterTab"+std::to_string(i)+"Bg]") || !sections.count("[MeterTab"+std::to_string(i)+"Text]"))break;
		result.tabCapacity=i;
	}
	for(size_t i=1;i<=18;++i)
	{
		if(!sections.count("[MeterIcon"+std::to_string(i)+"]") || !sections.count("[MeterIcon"+std::to_string(i)+"Text]"))break;
		result.itemCapacity=i;
	}
	Need(result.tabCapacity && result.itemCapacity,Error::Unsupported,L"The shelf has no recognized launcher meters.");
	return result;
}
template<class T> Result<T> Failed(const Problem& p){Result<T> r;r.code=p.code;r.message=p.message;return r;}
}
class PreparedSave
{
public:
	std::wstring root, target, source;
	Snapshot snapshot;
	Pins parents;
	std::shared_ptr<OwnedFile> temporary, icon, backup;
	std::wstring iconName;
	bool used=false;
};
Storage::Storage(std::wstring root):m_Root(std::move(root)){}
Result<std::vector<ShelfInfo>> Storage::Discover() const
{
	try
	{
		const auto root=Canonical(m_Root); auto pins=Pin(root);
		WIN32_FIND_DATAW entry={}; const auto find=FindFirstFileW((root+L"\\Shelf*").c_str(),&entry);
		Need(find!=INVALID_HANDLE_VALUE,Error::NotFound,L"No installed ShelfSuite shelves were found.");
		struct FindCloseGuard{HANDLE h;~FindCloseGuard(){FindClose(h);}} guard{find};
		std::vector<ShelfInfo> shelves;
		do
		{
			if(!(entry.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY) || !ShelfId(entry.cFileName))continue;
			const std::wstring id=entry.cFileName;
			auto childPins=Pin(root+L"\\"+id);
			shelves.push_back(Info(id,Read(root+L"\\"+id+L"\\Shelf.ini").bytes));
		}while(FindNextFileW(find,&entry));
		Need(GetLastError()==ERROR_NO_MORE_FILES);
		std::sort(shelves.begin(),shelves.end(),[](const ShelfInfo& a,const ShelfInfo& b){return a.id<b.id;});
		return {true,std::move(shelves),Error::None,{}};
	}
	catch(const Problem& p){return Failed<std::vector<ShelfInfo>>(p);}
	catch(...){return Failed<std::vector<ShelfInfo>>({Error::IoError,L"ShelfSuite folders could not be read."});}
}
Result<Snapshot> Storage::Load(const std::wstring& shelf) const
{
	try
	{
		Need(ShelfId(shelf),Error::InvalidInput,L"Choose an installed shelf.");
		const auto root=Canonical(m_Root); const auto folder=root+L"\\"+shelf; auto pins=Pin(folder);
		const auto ini=Read(folder+L"\\Shelf.ini");
		Snapshot snapshot; snapshot.shelf=Info(shelf,ini.bytes);
		ReadResult config;
		try {config=Read(folder+L"\\config.lua");}
		catch(const Problem& p){if(p.code!=Error::NotFound)throw;config=Read(folder+L"\\config.example.lua");snapshot.example=true;}
		auto document=ParseConfig(config.bytes);
		Need(document.ok,document.code, L"This configuration contains unsupported Lua or encoding. It is read-only; existing contents are unchanged.");
		snapshot.document=std::move(document.value);
		snapshot.version=ini.version+"|"+config.version+(snapshot.example?"|example":"|config");
		return {true,std::move(snapshot),Error::None,{}};
	}
	catch(const Problem& p){return Failed<Snapshot>(p);}
	catch(...){return Failed<Snapshot>({Error::IoError,L"The shelf could not be loaded safely."});}
}
Result<std::shared_ptr<PreparedSave>> Storage::Prepare(const Snapshot& snapshot,const Edit& edit,
	const PngImage* image,const std::wstring& iconName) const
{
	try
	{
		auto current=Load(snapshot.shelf.id);
		Need(current.ok,current.code,L"Reload this shelf before saving.");
		Need(current.value.version==snapshot.version,Error::Conflict,L"This shelf changed outside the editor. Reload it before saving.");
		auto save=std::make_shared<PreparedSave>();
		save->root=Canonical(m_Root); const auto folder=save->root+L"\\"+snapshot.shelf.id;
		save->parents=Pin(folder);
		auto iconPins=Pin(save->root+L"\\@Resources\\Icons");
		for(auto& pin:iconPins)save->parents.push_back(std::move(pin));
		save->target=folder+L"\\config.lua";
		save->source=folder+(snapshot.example?L"\\config.example.lua":L"\\config.lua");
		save->snapshot=snapshot; Edit finalEdit=edit;
		if(image)
		{
			Need(!image->bytes.empty() && image->bytes.size()<=1024*1024 && image->width>0 && image->height>0 &&
				image->width<=256 && image->height<=256,Error::InvalidInput,L"The prepared icon is invalid.");
			const std::string png(reinterpret_cast<const char*>(image->bytes.data()),image->bytes.size());
			const auto base=IconBaseName(iconName);
			for(unsigned number=1;number<100000;++number)
			{
				const auto name=base+(number==1?L"":L"-"+std::to_wstring(number))+L".png";
				try
				{
					save->icon=Create(save->root+L"\\@Resources\\Icons\\"+name,png);
					save->iconName=name; finalEdit.icon=name; break;
				}
				catch(const Problem& p){if(p.code!=Error::Conflict)throw;}
			}
			Need(save->icon!=nullptr,Error::Conflict,L"Too many icons share this name. Choose another name.");
		}
		const auto edited=ApplyEdit(snapshot.document,finalEdit,snapshot.shelf.tabCapacity,snapshot.shelf.itemCapacity);
		Need(edited.ok,edited.code,L"The item cannot be saved safely. Check its text, icon name, action and the shelf's capacity.");
		save->temporary=Create(folder+L"\\config.cafe-"+GuidName()+L".tmp",edited.value);
		save->backup=Create(folder+L"\\config.cafe-backup-"+GuidName()+L".lua",snapshot.document.source);
		return {true,save,Error::None,{}};
	}
	catch(const Problem& p){return Failed<std::shared_ptr<PreparedSave>>(p);}
	catch(...){return Failed<std::shared_ptr<PreparedSave>>({Error::IoError,L"The save could not be prepared. Existing files are retained."});}
}
Result<SaveResult> Storage::Commit(const std::shared_ptr<PreparedSave>& save,const std::function<bool()>& authorized) const
{
	try
	{
		Need(save && !save->used,Error::Stale,L"This save is no longer valid.");
		save->used=true;
		Need(authorized && authorized(),Error::Locked,L"Maintenance Mode ended. Nothing was saved.");
		Need(Canonical(m_Root)==save->root,Error::InvalidInput);
		const auto current=Load(save->snapshot.shelf.id);
		Need(current.ok && current.value.version==save->snapshot.version,Error::Conflict,L"The configuration changed. Reload it before saving.");
		Need(authorized(),Error::Locked,L"Maintenance Mode ended. Nothing was saved.");
		// Rename the exact flushed temporary handle. This never follows a target
		// symlink or writes through a hard link; parent handles deny redirection.
		const size_t length=save->target.size()*sizeof(wchar_t);
		std::vector<BYTE> buffer(sizeof(FILE_RENAME_INFO)+length,0);
		auto rename=reinterpret_cast<FILE_RENAME_INFO*>(buffer.data());
		rename->ReplaceIfExists=save->snapshot.example?FALSE:TRUE;
		rename->FileNameLength=static_cast<DWORD>(length);
		memcpy(rename->FileName,save->target.data(),length);
		Need(SetFileInformationByHandle(save->temporary->handle.value,FileRenameInfo,rename,static_cast<DWORD>(buffer.size()))!=FALSE);
		save->temporary->keep=true; save->temporary->handle.Close();
		save->backup->keep=true; save->backup->handle.Close();
		if(save->icon){save->icon->keep=true;save->icon->handle.Close();}
		return {true,{save->iconName,save->backup->path},Error::None,{}};
	}
	catch(const Problem& p)
	{
		if(save){save->temporary.reset();save->icon.reset();save->backup.reset();}
		return Failed<SaveResult>(p);
	}
	catch(...)
	{
		if(save){save->temporary.reset();save->icon.reset();save->backup.reset();}
		return Failed<SaveResult>({Error::IoError,L"The save failed; keep the recovery backup and reload the shelf."});
	}
}
}
