#pragma once
// Keep the bundled library unchanged, but isolate its exception-enabled
// template instantiations from Rainmeter's exception-disabled update checker.
// Only these editor translation units include this adapter (without the PCH).
#ifdef INCLUDE_NLOHMANN_JSON_HPP_
#error Include CafeShelf/Json.h before any other nlohmann header in editor code.
#endif
#define nlohmann CafeShelfJson
#include "../json/json.hpp"
#undef nlohmann
