#pragma once

#if __linux__
#define OS_PLATFORM_LINUX
#elif TARGET_OS_MAC
#define OS_PLATFORM_MAC
#elif defined _WIN32 || defined _WIN64
#define OS_PLATFORM_WINDOWS
#endif

#include <string>

class VersionInfo {
public:
    static std::string GetVersionInfo();
};