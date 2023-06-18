#include "VersionInfo.h"

#if defined(OS_PLATFORM_WINDOWS)
	#include <Windows.h>
	#include <sstream>
	#include <VersionHelpers.h> // Requires windows 8.1 sdk at least
#endif

std::string VersionInfo::GetVersionInfo() {
	std::string result;

#if defined(OS_PLATFORM_LINUX)
	result = "Linux";
#elif defined(OS_PLATFORM_MAC)
	result = "Mac OS X";
#elif defined(OS_PLATFORM_WINDOWS)
	if (IsWindowsXPOrGreater() && !IsWindowsVistaOrGreater()) {
		result = "Windows XP";
	} else if (IsWindowsVistaOrGreater() && !IsWindows7OrGreater()) {
		result = "Windows Vista";
	} else if (IsWindows7OrGreater() && !IsWindows8OrGreater()) {
		result = "Windows 7";
	} else if (IsWindows8OrGreater() && !IsWindows8Point1OrGreater()) {
		result = "Windows 8";
	} else if (IsWindows8Point1OrGreater() && !IsWindows10OrGreater()) {
		result = "Windows 8.1";
	} else if (IsWindows10OrGreater()) {
		result = "Windows 10";
	} else {
		result = "Windows 11";
	}

	if (IsWindowsServer())
		result += " Server";
#elif defined(__FreeBSD__)
	result = "FreeBSD";
#elif defined(__DragonFly__)
	result = "DragonFly BSD";
#elif defined(__OpenBSD__)
	result = "OpenBSD";
#elif defined(__NetBSD__)
	result = "NetBSD";
#elif defined(__HAIKU__)
	result = "Haiku";
#else
	result = "Unknown OS";
#endif

	result += " | ZeroSpades 0.0.5 " GIT_COMMIT_HASH;

	return result;
}