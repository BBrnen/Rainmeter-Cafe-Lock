#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace CafeShelf
{
enum class Error
{
	None, Locked, Stale, InvalidInput, Unsupported, NotFound, Conflict,
	AccessDenied, IoError, Cancelled, RuntimeMissing
};

template<typename T>
struct Result
{
	bool ok = false;
	T value{};
	Error code = Error::InvalidInput;
	std::wstring message;
};

struct Ticket
{
	uint64_t editor = 0;
	uint64_t generation = 0;
};

constexpr size_t MaxMessageBytes = 1024 * 1024;
constexpr size_t MaxJsonDepth = 32;
constexpr size_t MaxStringUnits = 32767;
}
