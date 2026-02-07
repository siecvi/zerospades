#include "VersionInfo.h"

#if defined(OS_PLATFORM_WINDOWS)
	#define WIN32_LEAN_AND_MEAN
	#include <Windows.h>

	typedef LONG (WINAPI* RtlGetVersionPtr)(PRTL_OSVERSIONINFOW);

	static bool GetWindowsVersion(RTL_OSVERSIONINFOW& info) {
		HMODULE hNtdll = GetModuleHandleW(L"ntdll.dll");
		if (!hNtdll)
			return false;

		auto fn = reinterpret_cast<RtlGetVersionPtr>(
			GetProcAddress(hNtdll, "RtlGetVersion"));
		if (!fn)
			return false;

		info.dwOSVersionInfoSize = sizeof(info);
		return fn(&info) == 0;
	}

	static bool IsWindowsServer(const RTL_OSVERSIONINFOW& info) {
		OSVERSIONINFOEXW ex = {};
		ex.dwOSVersionInfoSize = sizeof(ex);
		ex.wProductType = VER_NT_WORKSTATION;

		DWORDLONG mask = VerSetConditionMask(0, VER_PRODUCT_TYPE, VER_EQUAL);
		
		return !VerifyVersionInfoW(&ex, VER_PRODUCT_TYPE, mask);
	}
#endif

std::string VersionInfo::GetVersionInfo() {
	std::string result;

#if defined(OS_PLATFORM_LINUX)
	result = "Linux";
#elif defined(OS_PLATFORM_MAC)
	result = "Mac OS X";
#elif defined(OS_PLATFORM_WINDOWS)
	RTL_OSVERSIONINFOW osv = {};
	if (!GetWindowsVersion(osv))
		return "Windows (Unknown)";

	const int major = osv.dwMajorVersion;
	const int minor = osv.dwMinorVersion;
	const int build = osv.dwBuildNumber;

	if (major == 5)
		result = "Windows XP";
	else if (major == 6 && minor == 0)
		result = "Windows Vista";
	else if (major == 6 && minor == 1)
		result = "Windows 7";
	else if (major == 6 && minor == 2)
		result = "Windows 8";
	else if (major == 6 && minor == 3)
		result = "Windows 8.1";
	else if (major == 10)
		result = (build >= 22000) ? "Windows 11" :  "Windows 10";
	else
		result = "Windows (Unknown)";

	if (IsWindowsServer(osv))
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

	return result;
}

std::string VersionInfo::GetAppArchitecture() {
	std::string result;

#if defined(__i386__) || defined(_M_IX86)
	result = "x86";
#elif defined(__amd64__) || defined(__x86_64__) || defined(_M_X64)
	result = "x64";
#elif defined(__aarch64__) || defined(_M_ARM64)
	result = "ARM64";
#else
	result = "Unknown Arch";
#endif

	return result;
}