#if __linux__
	#define OS_PLATFORM_LINUX
#elif TARGET_OS_MAC
	#define OS_PLATFORM_MAC
#elif defined _WIN32 || defined _WIN64
	#define OS_PLATFORM_WINDOWS
	#include <Windows.h>
	#include <sstream>
	#include <VersionHelpers.h> // Requires windows 8.1 sdk at least
#endif

#include "VersionInfo.h"

std::string VersionInfo::GetVersionInfo() {
#if defined(OS_PLATFORM_LINUX)
	return std::string("Linux");
#elif defined(TARGET_OS_MAC)
	return std::string("Mac OS X");
#elif defined(OS_PLATFORM_WINDOWS)
	std::string windowsVersion;
	if (IsWindowsXPOrGreater() && !IsWindowsVistaOrGreater()) {
		windowsVersion = "Windows XP";
	} else if (IsWindowsVistaOrGreater() && !IsWindows7OrGreater()) {
		windowsVersion = "Windows Vista";
	} else if (IsWindows7OrGreater() && !IsWindows8OrGreater()) {
		windowsVersion = "Windows 7";
	} else if (IsWindows8OrGreater() && !IsWindows8Point1OrGreater()) {
		windowsVersion = "Windows 8";
	} else if (IsWindows8Point1OrGreater() && !IsWindows10OrGreater()) {
		windowsVersion = "Windows 8.1";
	} else if (IsWindows10OrGreater()) {
		windowsVersion = "Windows 10";
	} else {
		windowsVersion = "Windows 11";
	}

	if (IsWindowsServer())
		windowsVersion += " Server";

	return windowsVersion + " | ZeroSpades 0.0.4";
#elif defined(__FreeBSD__)
	return std::string("FreeBSD");
	#elif defined(__DragonFly__)
	return std::string("DragonFly BSD");
#elif defined(__OpenBSD__)
	return std::string("OpenBSD");
#elif defined(__NetBSD__)
	return std::string("NetBSD");
#elif defined(__HAIKU__)
	return std::string("Haiku");
#else
	return std::string("Unknown OS");
#endif
}