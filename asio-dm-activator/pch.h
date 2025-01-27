// pch.h: This is a precompiled header file.
// Files listed below are compiled only once, improving build performance for future builds.
// This also affects IntelliSense performance, including code completion and many code browsing features.
// However, files listed here are ALL re-compiled if any one of them is updated between builds.
// Do not add files here that you will be updating frequently as this negates the performance advantage.

#define _WIN32_WINNT 0x0601
#ifndef PCH_H
#define PCH_H

// add headers that you want to pre-compile here
#include <windows.h> 
#include <thread>
#include <shlwapi.h>
#pragma comment(lib, "Shlwapi.lib")
#include <string>
#include <vector>
#include <unordered_map>
#include <map>
#include <iostream>
#include <algorithm>
#include <format>
#include <comdef.h> 
#include <memory>
#include <guiddef.h>
#include <stdint.h>
#include <type_traits>
#include <locale>
#include <typeindex>
#include <optional>
#include <functional>

#endif //PCH_H
