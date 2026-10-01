#include "Storage.h"
#include "ShelfTemplate.h"
#include <windows.h>
#include <objbase.h>
#include <bcrypt.h>
#include <algorithm>
#include <set>
#include <sstream>
#include <cwctype>

namespace CafeShelf
{
namespace
{
struct Problem { Error code; std::wstring message; };
std::wstring ReadError(DWORD error);
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
std::shared_ptr<OwnedFile> Directory(const std::wstring& path,bool create)
{
	if(create)Need(CreateDirectoryW(path.c_str(),nullptr)!=FALSE);
	auto directory=std::make_shared<OwnedFile>();directory->path=path;directory->keep=!create;
	directory->handle=Handle(CreateFileW(path.c_str(),FILE_READ_ATTRIBUTES|DELETE,FILE_SHARE_READ|FILE_SHARE_WRITE,
		nullptr,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OPEN_REPARSE_POINT,nullptr));
	BY_HANDLE_FILE_INFORMATION info={};
	Need(directory->handle.value!=INVALID_HANDLE_VALUE && GetFileInformationByHandle(directory->handle.value,&info));
	Need((info.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY) && !(info.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT),Error::Unsupported);
	return directory;
}
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
		if(h.value==INVALID_HANDLE_VALUE || !GetFileInformationByHandle(h.value,&info))
			throw Problem{Error::AccessDenied,L"The ShelfSuite folder cannot be opened safely: "+ReadError(GetLastError())};
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
struct ReadResult { std::string bytes, version; DWORD attributes=0; };
std::wstring ReadError(DWORD error)
{
	const wchar_t* reason=L"The file could not be opened for safe reading";
	switch(error)
	{
	case ERROR_FILE_NOT_FOUND:reason=L"The file is missing";break;
	case ERROR_PATH_NOT_FOUND:reason=L"The containing folder is missing";break;
	case ERROR_ACCESS_DENIED:reason=L"Windows denied read access";break;
	case ERROR_SHARING_VIOLATION:case ERROR_LOCK_VIOLATION:reason=L"The file is in use or locked";break;
	}
	return std::wstring(reason)+L" (Windows error "+std::to_wstring(error)+L").";
}
ReadResult Read(const std::wstring& path,Handle* lease=nullptr,bool allowRename=false)
{
	Handle file(CreateFileW(path.c_str(),GENERIC_READ|(allowRename?DELETE:0),
		FILE_SHARE_READ|(allowRename?FILE_SHARE_DELETE:0),nullptr,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,nullptr));
	if(file.value==INVALID_HANDLE_VALUE)
	{
		const auto error=GetLastError();
		throw Problem{error==ERROR_FILE_NOT_FOUND || error==ERROR_PATH_NOT_FOUND?Error::NotFound:Error::AccessDenied,ReadError(error)};
	}
	BY_HANDLE_FILE_INFORMATION info={};
	if(!GetFileInformationByHandle(file.value,&info))throw Problem{Error::IoError,ReadError(GetLastError())};
	Need(!(info.dwFileAttributes&(FILE_ATTRIBUTE_REPARSE_POINT|FILE_ATTRIBUTE_DIRECTORY)) && info.nNumberOfLinks==1,
		Error::Unsupported,L"Redirected or hard-linked configuration files are read-only.");
	Need(info.nFileSizeHigh==0 && info.nFileSizeLow<=4*1024*1024,Error::Unsupported,L"The configuration is too large.");
	ReadResult result; result.attributes=info.dwFileAttributes; result.bytes.resize(info.nFileSizeLow); DWORD read=0;
	if(!ReadFile(file.value,result.bytes.empty()?nullptr:&result.bytes[0],info.nFileSizeLow,&read,nullptr))
		throw Problem{Error::IoError,ReadError(GetLastError())};
	Need(read==info.nFileSizeLow,Error::Conflict,L"The file changed while it was being read. Reload the shelf.");
	result.version=std::to_string(info.dwVolumeSerialNumber)+":"+std::to_string(info.nFileIndexHigh)+":"+
		std::to_string(info.nFileIndexLow)+":"+Hash(result.bytes);
	if(lease)*lease=std::move(file);
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
bool RenameHandle(HANDLE file,const std::wstring& path,bool replace)
{
	const size_t length=path.size()*sizeof(wchar_t);
	std::vector<BYTE> buffer(sizeof(FILE_RENAME_INFO)+length,0);
	auto rename=reinterpret_cast<FILE_RENAME_INFO*>(buffer.data());
	rename->ReplaceIfExists=replace?TRUE:FALSE;
	rename->FileNameLength=static_cast<DWORD>(length);
	memcpy(rename->FileName,path.data(),length);
	return SetFileInformationByHandle(file,FileRenameInfo,rename,static_cast<DWORD>(buffer.size()))!=FALSE;
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
struct ThemeSpan { size_t begin=0,end=0; std::wstring name; };
bool KnownTheme(const std::wstring& name)
{
	return name==L"DeepOcean" || name==L"Forest" || name==L"Terracotta" || name==L"Obsidian";
}
std::string ThemeName(const std::wstring& name)
{
	Need(KnownTheme(name),Error::Unsupported);
	std::string result;for(const auto ch:name)result+=static_cast<char>(ch);
	return result;
}
ThemeSpan Theme(const std::string& ini)
{
	ThemeSpan result; bool rainmeter=false,found=false;
	for(size_t start=0;start<ini.size();)
	{
		const auto newline=ini.find('\n',start), next=newline==std::string::npos?ini.size():newline+1;
		auto end=newline==std::string::npos?ini.size():newline;
		while(end>start && (ini[end-1]=='\r' || ini[end-1]==' ' || ini[end-1]=='\t'))--end;
		auto first=start;while(first<end && (ini[first]==' ' || ini[first]=='\t'))++first;
		const auto line=ini.substr(first,end-first);
		if(!line.empty() && line.front()=='[')rainmeter=_stricmp(line.c_str(),"[Rainmeter]")==0;
		else if(rainmeter && !line.empty() && line.front()!=';')
		{
			const auto equal=line.find('=');
			if(equal!=std::string::npos)
			{
				auto key=line.substr(0,equal);while(!key.empty() && (key.back()==' ' || key.back()=='\t'))key.pop_back();
				if(_stricmp(key.c_str(),"@IncludeTheme")==0)
				{
					if(found)return {};found=true;
					result.begin=first+equal+1;while(result.begin<end && (ini[result.begin]==' ' || ini[result.begin]=='\t'))++result.begin;
					result.end=end;const auto value=ini.substr(result.begin,end-result.begin);
					for(const auto name:{L"DeepOcean",L"Forest",L"Terracotta",L"Obsidian"})
					{
						const std::wstring wide=name; const auto narrow=ThemeName(wide);
						if(value=="#@#Themes\\"+narrow+".inc")result.name=wide;
					}
				}
			}
		}
		start=next;
	}
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
	std::shared_ptr<OwnedFile> directory, skinIni;
	std::shared_ptr<OwnedFile> temporary, icon, backup;
	std::wstring iconName;
	bool used=false;
	bool theme=false;
	EditKind kind=EditKind::SetItem;
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
		std::vector<ShelfInfo> shelves; std::wstring warnings;
		do
		{
			if(!(entry.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY) || !ShelfId(entry.cFileName))continue;
			const std::wstring id=entry.cFileName;
			// A broken neighbor is a local warning, not a failed installation.
			// Keep all containment and file-identity checks; never repair user files.
			try
			{
				auto childPins=Pin(root+L"\\"+id);
				shelves.push_back(Info(id,Read(root+L"\\"+id+L"\\Shelf.ini").bytes));
			}
			catch(const Problem& p)
			{
				warnings+=L"Not editable: "+id+L"\\Shelf.ini: "+p.message+L"\n";
			}
			catch(...){warnings+=L"Not editable: "+id+L"\\Shelf.ini: The shelf could not be inspected safely.\n";}
		}while(FindNextFileW(find,&entry));
		Need(GetLastError()==ERROR_NO_MORE_FILES);
		std::sort(shelves.begin(),shelves.end(),[](const ShelfInfo& a,const ShelfInfo& b){return a.id<b.id;});
		return {true,std::move(shelves),Error::None,std::move(warnings)};
	}
	catch(const Problem& p){return Failed<std::vector<ShelfInfo>>(p);}
	catch(...){return Failed<std::vector<ShelfInfo>>({Error::IoError,L"ShelfSuite folders could not be read."});}
}
Result<Snapshot> Storage::Load(const std::wstring& shelf) const
{
	std::wstring relative;
	try
	{
		Need(ShelfId(shelf),Error::InvalidInput,L"Choose an installed shelf.");
		relative=shelf+L"\\Shelf.ini";
		const auto root=Canonical(m_Root); const auto folder=root+L"\\"+shelf; auto pins=Pin(folder);
		const auto ini=Read(folder+L"\\Shelf.ini");
		Snapshot snapshot; snapshot.shelf=Info(shelf,ini.bytes);
		snapshot.iniSource=ini.bytes;snapshot.theme=Theme(ini.bytes).name;
		ReadResult config;
		relative=shelf+L"\\config.lua";
		try {config=Read(folder+L"\\config.lua");}
		catch(const Problem& p)
		{
			if(p.code!=Error::NotFound)throw;
			relative=shelf+L"\\config.example.lua";
			config=Read(folder+L"\\config.example.lua");snapshot.example=true;
		}
		auto document=ParseConfig(config.bytes);
		Need(document.ok,document.code, L"This configuration contains unsupported Lua or encoding. It is read-only; existing contents are unchanged.");
		snapshot.document=std::move(document.value);
		snapshot.version=ini.version+"|"+config.version+(snapshot.example?"|example":"|config");
		return {true,std::move(snapshot),Error::None,{}};
	}
	catch(const Problem& p){return Failed<Snapshot>({p.code,relative.empty()?p.message:relative+L": "+p.message});}
	catch(...){return Failed<Snapshot>({Error::IoError,relative+L": The shelf could not be loaded safely."});}
}
Result<std::shared_ptr<PreparedSave>> Storage::Prepare(const Snapshot& snapshot,const Edit& edit,
	const PngImage* image,const std::wstring& iconName) const
{
	try
	{
		if(edit.kind==EditKind::AddShelf)
		{
			Need(!image && !edit.label.empty() && KnownTheme(edit.action),Error::InvalidInput);
			auto name=NameString(edit.label);Need(name.ok,name.code,name.message.c_str());
			auto save=std::make_shared<PreparedSave>();save->kind=edit.kind;save->root=Canonical(m_Root);
			save->parents=Pin(save->root+L"\\@Resources\\Themes");
			Read(save->root+L"\\@Resources\\Themes\\"+edit.action+L".inc");
			save->directory=Directory(save->root+L"\\@Resources\\CafeShelfPending-"+GuidName(),true);
			std::string ini=ShelfTemplate;const auto span=Theme(ini);
			Need(!span.name.empty());ini.replace(span.begin,span.end-span.begin,"#@#Themes\\"+ThemeName(edit.action)+".inc");
			save->skinIni=Create(save->directory->path+L"\\Shelf.ini",ini);
			save->temporary=Create(save->directory->path+L"\\config.lua","ShelfConfig={defaultIcon=\"folder.png\",tabs={{name="+name.value+",items={}}}}\n");
			return {true,save,Error::None,{}};
		}
		auto current=Load(snapshot.shelf.id);
		Need(current.ok,current.code,L"Reload this shelf before saving.");
		Need(current.value.version==snapshot.version,Error::Conflict,L"This shelf changed outside the editor. Reload it before saving.");
		auto save=std::make_shared<PreparedSave>();
		save->root=Canonical(m_Root); const auto folder=save->root+L"\\"+snapshot.shelf.id;
		save->kind=edit.kind;
		if(edit.kind==EditKind::RemoveShelf)
		{
			Need(!image,Error::InvalidInput);save->parents=Pin(save->root+L"\\@Resources");
			save->directory=Directory(folder,false);save->snapshot=snapshot;
			return {true,save,Error::None,{}};
		}
		save->parents=Pin(folder);
		auto iconPins=Pin(save->root+L"\\@Resources\\Icons");
		for(auto& pin:iconPins)save->parents.push_back(std::move(pin));
		save->target=folder+L"\\config.lua";
		save->source=folder+(snapshot.example?L"\\config.example.lua":L"\\config.lua");
		save->snapshot=snapshot; Edit finalEdit=edit;
		if(edit.kind==EditKind::SetTheme)
		{
			const auto span=Theme(snapshot.iniSource);
			Need(!image && KnownTheme(edit.label) && !span.name.empty(),Error::Unsupported,L"Choose a supported theme. Custom or ambiguous theme includes are left unchanged.");
			auto themePins=Pin(save->root+L"\\@Resources\\Themes");
			for(auto& pin:themePins)save->parents.push_back(std::move(pin));
			Read(save->root+L"\\@Resources\\Themes\\"+edit.label+L".inc");
			auto edited=snapshot.iniSource;
			edited.replace(span.begin,span.end-span.begin,"#@#Themes\\"+ThemeName(edit.label)+".inc");
			save->theme=true;save->source=save->target=folder+L"\\Shelf.ini";
			save->temporary=Create(folder+L"\\Shelf.cafe-"+GuidName()+L".tmp",edited);
			save->backup=Create(folder+L"\\Shelf.cafe-backup-"+GuidName()+L".bak",snapshot.iniSource);
			return {true,save,Error::None,{}};
		}
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
		if(save->kind==EditKind::AddShelf)
		{
			Need(authorized(),Error::Locked,L"Maintenance Mode ended. Nothing was saved.");
			// Preserve the private prepared folder on an ambiguous rename failure.
			save->skinIni->keep=true;save->skinIni->handle.Close();
			save->temporary->keep=true;save->temporary->handle.Close();save->directory->keep=true;
			for(unsigned n=1;n<100000;++n)
			{
				const auto id=L"Shelf"+std::to_wstring(n),target=save->root+L"\\"+id;
				if(RenameHandle(save->directory->handle.value,target,false))
				{
					save->directory->handle.Close();
					return {true,{L"",L"",L"New shelf created. Use Manage / Refresh all to discover and load it.",id},Error::None,{}};
				}
				if(GetFileAttributesW(target.c_str())==INVALID_FILE_ATTRIBUTES)break;
			}
			save->directory->handle.Close();
			return {false,{},Error::IoError,L"The shelf could not be published. Its prepared files were retained under @Resources/CafeShelfPending-."};
		}
		const auto folder=save->root+L"\\"+save->snapshot.shelf.id;
		if(save->kind==EditKind::RemoveShelf)
		{
			const auto ini=Read(folder+L"\\Shelf.ini");
			const auto config=Read(folder+(save->snapshot.example?L"\\config.example.lua":L"\\config.lua"));
			Need(ini.version+"|"+config.version+(save->snapshot.example?"|example":"|config")==save->snapshot.version,
				Error::Conflict,L"The shelf changed. Reload it before removing it.");
			Need(authorized(),Error::Locked,L"Maintenance Mode ended. Nothing was removed.");
			const auto recovery=save->root+L"\\@Resources\\CafeShelfRemoved-"+save->snapshot.shelf.id+L"-"+GuidName();
			Need(RenameHandle(save->directory->handle.value,recovery,false));
			save->directory->handle.Close();
			return {true,{L"",recovery,L"Shelf removed. Its complete folder is retained in the recovery location.",save->snapshot.shelf.id},Error::None,{}};
		}
		// Keep the INI stable, and deny in-place config writes through replacement.
		// DELETE sharing is needed by ReplaceFile; an outside rename is recovered
		// in its actual-source backup, not discarded as an older snapshot.
		Handle iniLease,sourceLease;
		const auto ini=Read(folder+L"\\Shelf.ini",&iniLease,save->theme);
		const auto config=Read(folder+(save->snapshot.example?L"\\config.example.lua":L"\\config.lua"),&sourceLease,!save->theme && !save->snapshot.example);
		const auto& source=save->theme?ini:config;
		Need(ini.version+"|"+config.version+(save->snapshot.example?"|example":"|config")==save->snapshot.version,
			Error::Conflict,L"The configuration changed. Reload it before saving.");
		Need(!(source.attributes&FILE_ATTRIBUTE_READONLY),Error::AccessDenied,L"The configuration is read-only. Nothing was saved.");
		Need(authorized(),Error::Locked,L"Maintenance Mode ended. Nothing was saved.");
		std::wstring recovery=save->backup->path, warning;
		if(save->snapshot.example && !save->theme)
		{
			// Never replace a config another writer created after the example load.
			Need(RenameHandle(save->temporary->handle.value,save->target,false));
		}
		else
		{
			// Reserve a unique actual-version backup separately from our snapshot.
			auto actual=Create(folder+(save->theme?L"\\Shelf.cafe-previous-":L"\\config.cafe-previous-")+GuidName()+(save->theme?L".bak":L".lua"),"");
			recovery=actual->path;
			// ReplaceFile opens its replacement with no sharing. From this point,
			// preserve recovery/temp files even on ambiguous partial failures.
			actual->keep=true;actual->handle.Close();
			save->backup->keep=true;save->backup->handle.Close();
			save->temporary->keep=true;save->temporary->handle.Close();
			if(save->icon){save->icon->keep=true;save->icon->handle.Close();}
			if(!ReplaceFileW(save->target.c_str(),save->temporary->path.c_str(),recovery.c_str(),0,nullptr,nullptr))
			{
				const auto error=GetLastError();
				// Documented partial failure may move the old file to the backup.
				// Restore only into an absent destination, through the verified handle.
				if(error==ERROR_UNABLE_TO_MOVE_REPLACEMENT_2)
				{
					try { Handle old;Read(recovery,&old,true);RenameHandle(old.value,save->target,false); }
					catch(...) {}
				}
				return {false,{},Error::IoError,L"The save failed. Reload the shelf. Recovery and temporary files were kept beside config.lua."};
			}
			try
			{
				const auto previous=Read(recovery,nullptr,true);
				if(previous.version!=source.version)warning=L"An outside edit arrived during saving. Its complete version was kept in the recovery copy.";
			}
			catch(...) { warning=L"Saved, but the recovery copy could not be verified. Keep the recovery files beside config.lua."; }
		}
		save->temporary->keep=true; save->temporary->handle.Close();
		save->backup->keep=true; save->backup->handle.Close();
		if(save->icon){save->icon->keep=true;save->icon->handle.Close();}
		return {true,{save->iconName,recovery,warning,save->snapshot.shelf.id},Error::None,{}};
	}
	catch(const Problem& p)
	{
		if(save){save->temporary.reset();save->icon.reset();save->backup.reset();save->skinIni.reset();save->directory.reset();}
		return Failed<SaveResult>(p);
	}
	catch(...)
	{
		if(save){save->temporary.reset();save->icon.reset();save->backup.reset();save->skinIni.reset();save->directory.reset();}
		return Failed<SaveResult>({Error::IoError,L"The save failed; keep the recovery backup and reload the shelf."});
	}
}
}
