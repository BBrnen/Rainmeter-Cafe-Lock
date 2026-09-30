// The Windows 7 shell helpers require IE7 declarations in the SDK.
// Keep this local; upstream Rainmeter's build target is unchanged.
#if defined(_WIN32_IE) && _WIN32_IE < 0x0700
#undef _WIN32_IE
#define _WIN32_IE 0x0700
#endif
#include "Icons.h"
#include <windows.h>
#include <shlobj.h>
#include <wincodec.h>
#include <shobjidl.h>
#include <shlwapi.h>
#include <wrl.h>
#include <algorithm>
#include <cwctype>

using Microsoft::WRL::ComPtr;
namespace CafeShelf
{
namespace
{
constexpr DWORD MaxInput = 32 * 1024 * 1024;
struct File
{
	HANDLE value = INVALID_HANDLE_VALUE;
	~File() { if (value != INVALID_HANDLE_VALUE) CloseHandle(value); }
};
struct Bitmap
{
	HBITMAP value = nullptr;
	~Bitmap() { if (value) DeleteObject(value); }
};
Result<PngImage> Failure(const wchar_t* message)
{
	Result<PngImage> result;
	result.code = Error::Unsupported;
	result.message = message;
	return result;
}
bool Dimensions(UINT w, UINT h)
{
	return w && h && w <= 4096 && h <= 4096 &&
		static_cast<uint64_t>(w) * h <= 16 * 1024 * 1024;
}
Result<PngImage> Encode(IWICImagingFactory* factory, IWICBitmapSource* source)
{
	UINT w = 0, h = 0;
	if (FAILED(source->GetSize(&w, &h)) || !Dimensions(w, h))
		return Failure(L"Icon dimensions must be at most 4096 by 4096 pixels.");
	ComPtr<IWICBitmapSource> sized(source);
	ComPtr<IWICBitmapScaler> scaler;
	if (w > 256 || h > 256)
	{
		const UINT longest = (std::max)(w, h);
		w = (std::max)(1U, w * 256 / longest);
		h = (std::max)(1U, h * 256 / longest);
		if (FAILED(factory->CreateBitmapScaler(&scaler)) ||
			FAILED(scaler->Initialize(source, w, h, WICBitmapInterpolationModeFant)))
			return Failure(L"Windows could not resize this icon.");
		scaler.As(&sized);
	}
	ComPtr<IWICFormatConverter> converter;
	ComPtr<IStream> output;
	ComPtr<IWICBitmapEncoder> encoder;
	ComPtr<IWICBitmapFrameEncode> frame;
	WICPixelFormatGUID format = GUID_WICPixelFormat32bppBGRA;
	if (FAILED(factory->CreateFormatConverter(&converter)) ||
		FAILED(converter->Initialize(sized.Get(), format, WICBitmapDitherTypeNone, nullptr, 0, WICBitmapPaletteTypeCustom)) ||
		FAILED(CreateStreamOnHGlobal(nullptr, TRUE, &output)) ||
		FAILED(factory->CreateEncoder(GUID_ContainerFormatPng, nullptr, &encoder)) ||
		FAILED(encoder->Initialize(output.Get(), WICBitmapEncoderNoCache)) ||
		FAILED(encoder->CreateNewFrame(&frame, nullptr)) ||
		FAILED(frame->Initialize(nullptr)) || FAILED(frame->SetSize(w, h)) ||
		FAILED(frame->SetPixelFormat(&format)) || format != GUID_WICPixelFormat32bppBGRA ||
		FAILED(frame->WriteSource(converter.Get(), nullptr)) ||
		FAILED(frame->Commit()) || FAILED(encoder->Commit()))
		return Failure(L"Windows could not convert this icon to PNG.");
	STATSTG stat = {};
	if (FAILED(output->Stat(&stat, STATFLAG_NONAME)) || !stat.cbSize.QuadPart ||
		stat.cbSize.QuadPart > 1024 * 1024) return Failure(L"The converted icon is too large.");
	Result<PngImage> result;
	result.value.bytes.resize(static_cast<size_t>(stat.cbSize.QuadPart));
	LARGE_INTEGER zero = {};
	ULONG read = 0;
	if (FAILED(output->Seek(zero, STREAM_SEEK_SET, nullptr)) ||
		FAILED(output->Read(result.value.bytes.data(), static_cast<ULONG>(result.value.bytes.size()), &read)) ||
		read != result.value.bytes.size()) return Failure(L"The converted icon could not be read.");
	result.value.width = w; result.value.height = h;
	result.ok = true; result.code = Error::None;
	return result;
}
Result<PngImage> FromImage(IWICImagingFactory* factory, const std::wstring& path, bool png)
{
	File file;
	file.value = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL, nullptr);
	LARGE_INTEGER size = {};
	if (file.value == INVALID_HANDLE_VALUE || !GetFileSizeEx(file.value, &size) ||
		size.QuadPart <= 0 || size.QuadPart > MaxInput)
		return Failure(L"Choose a readable PNG or ICO no larger than 32 MB.");
	std::vector<BYTE> bytes(static_cast<size_t>(size.QuadPart));
	DWORD read = 0;
	if (!ReadFile(file.value, bytes.data(), static_cast<DWORD>(bytes.size()), &read, nullptr) ||
		read != bytes.size()) return Failure(L"The image could not be read.");
	ComPtr<IStream> stream;
	stream.Attach(SHCreateMemStream(bytes.data(), static_cast<UINT>(bytes.size())));
	ComPtr<IWICBitmapDecoder> decoder;
	// Select only the system PNG/ICO decoders, never an arbitrary installed codec.
	if (!stream || FAILED(CoCreateInstance(png ? CLSID_WICPngDecoder : CLSID_WICIcoDecoder,
		nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&decoder))) ||
		FAILED(decoder->Initialize(stream.Get(), WICDecodeMetadataCacheOnDemand)))
		return Failure(L"This is not a valid PNG or ICO image.");
	UINT count = 0;
	if (FAILED(decoder->GetFrameCount(&count)) || !count || count > 256)
		return Failure(L"This icon has an unsupported number of images.");
	ComPtr<IWICBitmapFrameDecode> chosen;
	uint64_t score = 0;
	for (UINT i = 0; i < count; ++i)
	{
		ComPtr<IWICBitmapFrameDecode> candidate;
		UINT w = 0, h = 0;
		if (FAILED(decoder->GetFrame(i, &candidate)) || FAILED(candidate->GetSize(&w, &h)) || !Dimensions(w, h))
			return Failure(L"This icon contains an oversized or invalid image.");
		// Prefer the largest frame up to 256, then the smallest larger frame.
		const uint64_t area = static_cast<uint64_t>(w) * h;
		const uint64_t rank = w <= 256 && h <= 256 ? 100000000 + area : 100000000 - area;
		if (!chosen || rank > score) { chosen = candidate; score = rank; }
	}
	return Encode(factory, chosen.Get());
}
Result<PngImage> FromShell(IWICImagingFactory* factory, const std::wstring& path)
{
	ComPtr<IShellItemImageFactory> item;
	Bitmap bitmap;
	ComPtr<IWICBitmap> source;
	if (FAILED(SHCreateItemFromParsingName(path.c_str(), nullptr, IID_PPV_ARGS(&item))) ||
		FAILED(item->GetImage({256, 256}, SIIGBF_ICONONLY, &bitmap.value)) || !bitmap.value ||
		FAILED(factory->CreateBitmapFromHBITMAP(bitmap.value, nullptr, WICBitmapUsePremultipliedAlpha, &source)))
		return Failure(L"Windows could not obtain an icon. Choose a custom icon or keep the existing/default icon.");
	return Encode(factory, source.Get());
}
Result<PngImage> Prepare(const SelectedFile& source, bool launcher)
{
	try
	{
		if (source.path.empty() || source.path.size() > MaxStringUnits ||
			source.path.find(L'\0') != std::wstring::npos ||
			GetFileAttributesW(source.path.c_str()) == INVALID_FILE_ATTRIBUTES)
			return Failure(L"The selected icon source is no longer available.");
		ComPtr<IWICImagingFactory> factory;
		if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory))))
			return Failure(L"Windows imaging is unavailable.");
		const auto dot = source.path.find_last_of(L'.');
		const auto extension = dot == std::wstring::npos ? L"" : source.path.c_str() + dot;
		if (!source.directory && _wcsicmp(extension, L".png") == 0) return FromImage(factory.Get(), source.path, true);
		if (!source.directory && _wcsicmp(extension, L".ico") == 0) return FromImage(factory.Get(), source.path, false);
		if (launcher || (!source.directory && (_wcsicmp(extension, L".lnk") == 0 || _wcsicmp(extension, L".exe") == 0)))
			return FromShell(factory.Get(), source.path);
		return Failure(L"Choose a PNG, ICO, EXE or Windows shortcut for the icon.");
	}
	catch (...) { return Failure(L"Windows could not prepare this icon."); }
}
}
Result<PngImage> PrepareIcon(const SelectedFile& source) { return Prepare(source, false); }
Result<PngImage> PrepareLauncherIcon(const LauncherDraft& launcher)
{
	return Prepare({launcher.iconSource, false}, true);
}
std::wstring IconBaseName(const std::wstring& name)
{
	// ASCII filenames avoid the pinned skin engine's legacy-codepage ambiguity.
	std::wstring result;
	for (const wchar_t c : name)
	{
		if (result.size() >= 80) break;
		result += (c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z') ||
			(c >= L'0' && c <= L'9') || c == L' ' || c == L'.' || c == L'_' || c == L'-' ? c : L'-';
	}
	while (!result.empty() && (result.back() == L'.' || result.back() == L' ')) result.pop_back();
	if (result.empty() || result == L"-") result = L"icon";
	const auto stem = result.substr(0, result.find(L'.'));
	auto upper = stem;
	std::transform(upper.begin(), upper.end(), upper.begin(), [](wchar_t c) { return static_cast<wchar_t>(towupper(c)); });
	if (upper == L"CON" || upper == L"PRN" || upper == L"AUX" || upper == L"NUL" ||
		(upper.size() == 4 && (upper.substr(0,3) == L"COM" || upper.substr(0,3) == L"LPT") &&
			upper[3] >= L'1' && upper[3] <= L'9')) result.insert(result.begin(), L'_');
	return result;
}
}
