#include "../Library/CafeShelf/Icons.h"
#include <windows.h>
#include <wincodec.h>
#include <wrl.h>
#include <iostream>
using namespace CafeShelf;
using Microsoft::WRL::ComPtr;
namespace {
int checks = 0, failures = 0;
void Check(const char* name, bool passed) {
	++checks; if (!passed) ++failures;
	std::cout << (passed ? "PASS " : "FAIL ") << name << '\n';
}
bool Decode(const PngImage& image, bool alpha) {
	if (image.bytes.empty() || image.bytes.size() > MAXDWORD) return false;
	ComPtr<IWICImagingFactory> factory;
	ComPtr<IWICStream> stream;
	ComPtr<IWICBitmapDecoder> decoder;
	ComPtr<IWICBitmapFrameDecode> frame;
	ComPtr<IWICFormatConverter> converter;
	if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory))) ||
		FAILED(factory->CreateStream(&stream)) ||
		FAILED(stream->InitializeFromMemory(const_cast<BYTE*>(image.bytes.data()), static_cast<DWORD>(image.bytes.size()))) ||
		FAILED(factory->CreateDecoderFromStream(stream.Get(), nullptr, WICDecodeMetadataCacheOnDemand, &decoder)) ||
		FAILED(decoder->GetFrame(0, &frame)) || FAILED(factory->CreateFormatConverter(&converter)) ||
		FAILED(converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppBGRA, WICBitmapDitherTypeNone, nullptr, 0, WICBitmapPaletteTypeCustom))) return false;
	UINT width = 0, height = 0;
	if (FAILED(frame->GetSize(&width, &height)) || width != image.width || height != image.height ||
		!width || !height || width > 256 || height > 256) return false;
	std::vector<BYTE> pixels(width * height * 4);
	if (FAILED(converter->CopyPixels(nullptr, width * 4, static_cast<UINT>(pixels.size()), pixels.data()))) return false;
	return !alpha || (width == 2 && height == 1 && pixels[3] == 0 && pixels[7] == 128 &&
		pixels[4] == 30 && pixels[5] == 20 && pixels[6] == 10);
}
}
int wmain(int argc, wchar_t** argv) {
	if (argc != 2 || FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED))) return 2;
	const std::wstring root = argv[1];
	auto icon = [&](const wchar_t* name) { return PrepareIcon({root + L"\\" + name, false}); };
	const auto png = icon(L"Transparent.png");
	Check("PNG transparency and colors survive encoding", png.ok && Decode(png.value, true));
	const auto ico = icon(L"Transparent.ico");
	Check("ICO decodes to transparent PNG", ico.ok && Decode(ico.value, true));
	const auto multi = icon(L"Multi.ico");
	Check("multi-resolution ICO chooses largest usable frame",multi.ok && multi.value.width==64 && multi.value.height==64 && Decode(multi.value,false));
	LauncherDraft folder; folder.iconSource=root+L"\\Folder.png";
	const auto folderIcon=PrepareLauncherIcon(folder);
	Check("folder named PNG uses its shell icon",folderIcon.ok && Decode(folderIcon.value,false));
	const auto exe = icon(L"Versioned application.exe");
	Check("EXE embedded icon becomes PNG without execution", exe.ok && Decode(exe.value, false));
	const auto lnk = icon(L"Original shortcut.lnk");
	Check("LNK configured icon becomes PNG", lnk.ok && Decode(lnk.value, false));
	const auto fallback = icon(L"Target icon.lnk");
	Check("LNK target icon fallback becomes PNG", fallback.ok && Decode(fallback.value, false));
	Check("corrupt PNG rejected", !icon(L"Corrupt.png").ok);
	Check("oversized PNG input rejected", !icon(L"Oversized.png").ok);
	Check("missing icon returns explicit failure", !icon(L"Missing.png").ok);
	Check("unsupported file is not decoded as an icon", !icon(L"Report.txt").ok);
	Check("empty and reserved names sanitized", IconBaseName(L"") == L"icon" && IconBaseName(L"CON") == L"_CON");
	Check("reserved stem before extension sanitized", IconBaseName(L"lpt1.txt") == L"_lpt1.txt");
	Check("separators and action punctuation sanitized", IconBaseName(L"a/b\\c:#%[]?") == L"a-b-c------");
	Check("trailing dots and spaces removed", IconBaseName(L"spotify. ") == L"spotify");
	Check("long names bounded", IconBaseName(std::wstring(400, L'a')).size() <= 80);
	CoUninitialize();
	std::cout << checks << " icon checks, " << failures << " failures\n";
	return failures ? 1 : 0;
}
