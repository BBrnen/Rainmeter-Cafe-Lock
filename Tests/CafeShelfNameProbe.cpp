#include "../Library/CafeShelf/Config.h"
#include <fstream>
#include <iostream>
using namespace CafeShelf;
int wmain(int argc,wchar_t** argv)
{
 if(argc!=2)return 2;
 std::ifstream input(argv[1],std::ios::binary);
 const std::string source(std::istreambuf_iterator<char>(input),{});input.close();
 const auto parsed=ParseConfig(source);
 if(!parsed.ok || parsed.value.tabs.empty() || parsed.value.tabs[0].items.empty())return 3;
 const auto& old=parsed.value.tabs[0].items[0];
 Edit edit{EditKind::SetItem,0,0,L"#CURRENTCONFIG#",old.action,old.icon};
 if(ApplyEdit(parsed.value,edit,5,18).ok){std::cerr<<"Unsafe Rainmeter variable name was accepted\n";return 1;}
 edit.label=L"O'Brien <Cafe>";
 const auto literal=ApplyEdit(parsed.value,edit,5,18);
 if(!literal.ok)return 4;
 std::ofstream output(argv[1],std::ios::binary|std::ios::trunc);
 output.write(literal.value.data(),static_cast<std::streamsize>(literal.value.size()));output.close();
 if(!output)return 5;
 std::cout<<"PASS unsupported name rejected; supported literal name saved for loaded-skin verification\n";
 return 0;
}
