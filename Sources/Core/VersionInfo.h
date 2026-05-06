#pragma once

// Version macros (ZEROSPADES_VERSION_*, ZEROSPADES_VER_STR)
// live in ZeroSpades.h, which is included by every TU that needs them.

#if __linux__
	#define OS_PLATFORM_LINUX
#elif defined(__APPLE__)
	#define OS_PLATFORM_MAC
#elif defined _WIN32 || defined _WIN64
	#define OS_PLATFORM_WINDOWS
#endif

#include <string>

class VersionInfo {
public:
	static std::string GetVersionInfo();
	static std::string GetAppArchitecture();
};