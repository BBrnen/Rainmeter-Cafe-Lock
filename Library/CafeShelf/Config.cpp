#include "Config.h"
#include <windows.h>
#include <algorithm>
#include <set>
#include <stdexcept>
#include <cctype>

namespace CafeShelf
{
namespace
{
struct Unsupported {};
void Require(bool value) { if (!value) throw Unsupported(); }
bool Letter(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; }
bool Digit(char c) { return c >= '0' && c <= '9'; }
class Parser
{
public:
	explicit Parser(const std::string& source) : s(source) {}
	LuaNode Run()
	{
		if (s.compare(0,3,"\xEF\xBB\xBF")==0) p=3;
		Skip(); Require(Identifier()=="ShelfConfig"); Skip(); Require(Take('='));
		auto root=Value(0); Skip(); Take(';'); Skip(); Require(p==s.size());
		Require(root.kind==LuaNode::Kind::Table);
		return root;
	}
private:
	const std::string& s;
	size_t p=0, nodes=0;
	bool Take(char c) { if(p<s.size() && s[p]==c){++p;return true;} return false; }
	bool LongStart(size_t start,size_t& equal) const
	{
		if(start>=s.size() || s[start]!='[')return false;
		size_t q=start+1; while(q<s.size() && s[q]=='=')++q;
		equal=q-start-1;
		return q<s.size() && s[q]=='[';
	}
	std::string Long()
	{
		size_t equal=0; Require(LongStart(p,equal)); p+=equal+2;
		const std::string close="]"+std::string(equal,'=')+"]";
		auto end=s.find(close,p); Require(end!=std::string::npos);
		if(p<end && s[p]=='\r') { ++p; if(p<end && s[p]=='\n')++p; }
		else if(p<end && s[p]=='\n')++p;
		auto value=s.substr(p,end-p); p=end+close.size(); return value;
	}
	void Skip()
	{
		for(;;)
		{
			while(p<s.size() && (s[p]==' ' || s[p]=='\t' || s[p]=='\r' || s[p]=='\n' || s[p]=='\f'))++p;
			if(s.compare(p,2,"--")!=0)return;
			p+=2; size_t equal=0;
			if(LongStart(p,equal)) { Long(); continue; }
			while(p<s.size() && s[p]!='\r' && s[p]!='\n')++p;
		}
	}
	std::string Identifier()
	{
		Require(p<s.size() && Letter(s[p])); const auto start=p++;
		while(p<s.size() && (Letter(s[p]) || Digit(s[p])))++p;
		return s.substr(start,p-start);
	}
	std::string Quoted()
	{
		const char quote=s[p++]; std::string value;
		while(p<s.size() && s[p]!=quote)
		{
			char c=s[p++]; Require(c!='\r' && c!='\n' && c!=0);
			if(c=='\\')
			{
				Require(p<s.size()); c=s[p++];
				if(Digit(c))
				{
					unsigned number=static_cast<unsigned>(c-'0');
					for(int n=1;n<3 && p<s.size() && Digit(s[p]);++n)number=number*10+static_cast<unsigned>(s[p++]-'0');
					Require(number>0 && number<=255); c=static_cast<char>(number);
				}
				else switch(c)
				{
				case 'a': c='\a';break; case 'b': c='\b';break; case 'f':c='\f';break;
				case 'n': c='\n';break; case 'r':c='\r';break; case 't':c='\t';break; case 'v':c='\v';break;
				case '\\': case '\'': case '"':break;
				case '\r': if(p<s.size() && s[p]=='\n')++p; c='\n';break;
				case '\n':break;
				default: throw Unsupported();
				}
			}
			value+=c; Require(value.size()<=MaxStringUnits*4);
		}
		Require(Take(quote)); return value;
	}
	LuaNode Value(size_t depth)
	{
		Require(depth<64 && ++nodes<=100000); Skip(); Require(p<s.size());
		LuaNode n; n.begin=p; size_t equal=0;
		if(s[p]=='{')
		{
			++p; n.kind=LuaNode::Kind::Table; std::set<std::string> seen; size_t array=0;
			Skip();
			while(!Take('}'))
			{
				Require(p<s.size()); const auto start=p; std::string key;
				if(Letter(s[p]))
				{
					const auto before=p; const auto identifier=Identifier(); Skip();
					if(Take('='))key=identifier; else p=before;
				}
				else if(s[p]=='[' && !LongStart(p,equal))
				{
					++p; auto index=Value(depth+1); Skip(); Require(Take(']')); Skip(); Require(Take('='));
					if(index.kind==LuaNode::Kind::String) { Require(!index.text.empty() && index.text[0]!='@'); key=index.text; }
					else { Require(index.kind==LuaNode::Kind::Literal && !index.text.empty() &&
						std::all_of(index.text.begin(),index.text.end(),Digit)); key="@"+index.text; }
				}
				if(key.empty())key="@"+std::to_string(++array);
				Require(seen.insert(key).second);
				auto child=Value(depth+1); Skip();
				if(!Take(',') && !Take(';'))Require(p<s.size() && s[p]=='}');
				n.starts.push_back(start); n.ends.push_back(p);
				n.keys.push_back(key); n.children.push_back(std::move(child)); Skip();
			}
		}
		else if(s[p]=='\'' || s[p]=='"') { n.kind=LuaNode::Kind::String; n.text=Quoted(); }
		else if(LongStart(p,equal)) { n.kind=LuaNode::Kind::String; n.text=Long(); Require(n.text.find('\0')==std::string::npos); }
		else if(Letter(s[p]))
		{
			n.text=Identifier(); Require(n.text=="true" || n.text=="false" || n.text=="nil");
		}
		else
		{
			const auto start=p; Take('-');
			bool digits=false; while(p<s.size() && Digit(s[p])){digits=true;++p;}
			if(Take('.'))while(p<s.size() && Digit(s[p])){digits=true;++p;}
			Require(digits);
			if(p<s.size() && (s[p]=='e' || s[p]=='E'))
			{
				++p; if(!Take('+'))Take('-'); const auto exponent=p;
				while(p<s.size() && Digit(s[p]))++p; Require(p>exponent);
			}
			n.text=s.substr(start,p-start);
		}
		n.end=p; return n;
	}
};
const LuaNode* Field(const LuaNode& table,const std::string& key)
{
	for(size_t i=0;i<table.keys.size();++i)if(table.keys[i]==key)return &table.children[i];
	return nullptr;
}
std::wstring Decode(const LuaNode* node,const std::wstring& fallback=L"")
{
	if(!node)return fallback;
	Require(node->kind==LuaNode::Kind::String);
	const auto& text=node->text; if(text.empty())return {};
	Require(text.size()<=MaxStringUnits*4);
	const auto size=MultiByteToWideChar(CP_ACP,MB_ERR_INVALID_CHARS,text.data(),static_cast<int>(text.size()),nullptr,0);
	Require(size>0 && size<=static_cast<int>(MaxStringUnits));
	std::wstring output(size,L'\0');
	Require(MultiByteToWideChar(CP_ACP,MB_ERR_INVALID_CHARS,text.data(),static_cast<int>(text.size()),&output[0],size)>0);
	return output;
}
void Array(const LuaNode& node)
{
	Require(node.kind==LuaNode::Kind::Table);
	for(size_t i=0;i<node.keys.size();++i)Require(node.keys[i]=="@"+std::to_string(i+1));
}
const LuaNode* ItemField(const LuaNode& item,const char* name,const char* index)
{
	auto named=Field(item,name), tuple=Field(item,index);
	Require(!(named && tuple)); return named?named:tuple;
}
Result<std::string> TextFailure()
{
	Result<std::string> r; r.code=Error::Unsupported;
	r.message=L"This text cannot be represented safely by the installed ShelfSuite engine. Use a simpler name or an ASCII-named Windows shortcut.";
	return r;
}
struct Patch { size_t begin,end; std::string text; };
std::string Added(const LuaNode& table,const std::string& source,const std::string& value)
{
	bool comma=false;
	if(!table.children.empty())
	{
		const auto end=table.children.back().end;
		const auto fieldEnd=table.ends.back();
		comma=fieldEnd==end || (source[fieldEnd-1]!=',' && source[fieldEnd-1]!=';');
	}
	const auto newline=source.find("\r\n")!=std::string::npos?"\r\n":"\n";
	return std::string(comma?",":"")+newline+"    "+value+","+newline;
}
}
Result<std::string> LuaString(const std::wstring& text)
{
	if(text.size()>MaxStringUnits || text.find(L'\0')!=std::wstring::npos)return TextFailure();
	if(text.empty())return {true,"\"\"",Error::None,{}};
	const UINT page=GetACP();
	BOOL replaced=FALSE;
	const int count=WideCharToMultiByte(page,page==CP_UTF8?WC_ERR_INVALID_CHARS:WC_NO_BEST_FIT_CHARS,
		text.data(),static_cast<int>(text.size()),nullptr,0,nullptr,page==CP_UTF8?nullptr:&replaced);
	if(count<=0 || replaced)return TextFailure();
	std::string bytes(count,'\0');
	if(!WideCharToMultiByte(page,page==CP_UTF8?WC_ERR_INVALID_CHARS:WC_NO_BEST_FIT_CHARS,text.data(),
		static_cast<int>(text.size()),&bytes[0],count,nullptr,page==CP_UTF8?nullptr:&replaced) || replaced)return TextFailure();
	LuaNode node; node.kind=LuaNode::Kind::String; node.text=bytes;
	try { if(Decode(&node)!=text)return TextFailure(); } catch(...) { return TextFailure(); }
	std::string result="\"";
	for(const unsigned char c:bytes)
	{
		if(c=='\\' || c=='"') { result+='\\';result+=static_cast<char>(c); }
		else if(c<32 || c>=127) { char escape[5]={}; sprintf_s(escape,"\\%03u",static_cast<unsigned>(c));result+=escape; }
		else result+=static_cast<char>(c);
	}
	result+='"'; return {true,result,Error::None,{}};
}
Result<ShelfDocument> ParseConfig(const std::string& bytes)
{
	Result<ShelfDocument> result; result.code=Error::Unsupported;
	result.message=L"This configuration contains unsupported or ambiguous Lua. It is read-only; its contents have not been changed.";
	if(bytes.empty() || bytes.size()>4*1024*1024 || bytes.find('\0')!=std::string::npos ||
		bytes.compare(0,2,"\xFF\xFE")==0 || bytes.compare(0,2,"\xFE\xFF")==0)return result;
	try
	{
		ShelfDocument d; d.source=bytes; d.root=Parser(bytes).Run();
		d.defaultIcon=Decode(Field(d.root,"defaultIcon"),L"folder.png");
		const auto tabs=Field(d.root,"tabs"); Require(tabs!=nullptr); Array(*tabs);
		for(const auto& tab:tabs->children)
		{
			Require(tab.kind==LuaNode::Kind::Table);
			ShelfTab model; model.name=Decode(Field(tab,"name"));
			const auto items=Field(tab,"items"); Require(items!=nullptr); Array(*items);
			for(const auto& item:items->children)
			{
				Require(item.kind==LuaNode::Kind::Table);
				model.items.push_back({Decode(ItemField(item,"label","@1")),Decode(ItemField(item,"action","@2")),
					Decode(ItemField(item,"icon","@3"),d.defaultIcon)});
			}
			d.tabs.push_back(std::move(model));
		}
		result.value=std::move(d); result.ok=true; result.code=Error::None; result.message.clear();
	}
	catch(...) {}
	return result;
}
Result<std::string> ApplyEdit(const ShelfDocument& document,const Edit& edit,size_t tabCapacity,size_t itemCapacity)
{
	auto failure=TextFailure();
	try
	{
		const auto tabs=Field(document.root,"tabs"); Require(tabs!=nullptr); Array(*tabs);
		Require(tabCapacity>0 && itemCapacity>0 && tabs->children.size()<=tabCapacity);
		auto literal=[&](const std::wstring& value) { auto result=LuaString(value); Require(result.ok);return result.value; };
		std::vector<Patch> patches;
		auto replace=[&](const LuaNode* node,const std::wstring& value) {
			Require(node!=nullptr); patches.push_back({node->begin,node->end,literal(value)});
		};
		if(edit.kind==EditKind::AddTab)
		{
			Require(tabs->children.size()<tabCapacity && !edit.label.empty());
			patches.push_back({tabs->end-1,tabs->end-1,Added(*tabs,document.source,"{name="+literal(edit.label)+",items={}}")});
		}
		else
		{
			Require(edit.tab<tabs->children.size());
			const auto& tab=tabs->children[edit.tab];
			if(edit.kind==EditKind::RemoveTab)
			{
				Require(tabs->children.size()>1);
				patches.push_back({tabs->starts[edit.tab],tabs->ends[edit.tab],""});
			}
			else if(edit.kind==EditKind::RenameTab) { Require(!edit.label.empty()); replace(Field(tab,"name"),edit.label); }
			else
			{
				const auto items=Field(tab,"items"); Require(items!=nullptr); Array(*items);
				if(edit.kind==EditKind::RemoveItem)
				{
					Require(edit.item<items->children.size());
					patches.push_back({items->starts[edit.item],items->ends[edit.item],""});
				}
				else
				{
					Require(!edit.label.empty() && !edit.action.empty() && !edit.icon.empty());
					Require(edit.action.find_first_of(L"\"[]#%\r\n")==std::wstring::npos);
					Require(edit.icon.find_first_of(L"/\\:#%[]\"\r\n")==std::wstring::npos &&
						edit.icon!=L"." && edit.icon!=L".." && edit.icon.back()!=L'.' && edit.icon.back()!=L' ');
					const auto label=literal(edit.label), action=literal(edit.action), icon=literal(edit.icon);
					if(edit.kind==EditKind::AddItem)
					{
						Require(items->children.size()<itemCapacity);
						patches.push_back({items->end-1,items->end-1,Added(*items,document.source,
							"{label="+label+",action="+action+",icon="+icon+"}")});
					}
					else
					{
						Require(edit.kind==EditKind::SetItem && edit.item<items->children.size());
						const auto& item=items->children[edit.item];
						std::string missing;
						const char* names[]={"label","action","icon"};
						const char* indices[]={"@1","@2","@3"};
						const std::wstring* values[]={&edit.label,&edit.action,&edit.icon};
						for(size_t i=0;i<3;++i)
						{
							const auto node=ItemField(item,names[i],indices[i]);
							if(node) replace(node,*values[i]);
							else { if(!missing.empty())missing+=","; missing+=std::string(names[i])+"="+literal(*values[i]); }
						}
						if(!missing.empty())patches.push_back({item.end-1,item.end-1,Added(item,document.source,missing)});
					}
				}
			}
		}
		std::sort(patches.begin(),patches.end(),[](const Patch& a,const Patch& b){return a.begin>b.begin;});
		auto output=document.source;
		for(const auto& patch:patches)output.replace(patch.begin,patch.end-patch.begin,patch.text);
		Require(ParseConfig(output).ok);
		return {true,output,Error::None,{}};
	}
	catch(...) { return failure; }
}
}
