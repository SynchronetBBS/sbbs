#include <stdlib.h>

#if defined(__unix__)
	#include <sys/utsname.h>
#endif

#include "genwrap.h"
#include "ini_file.h"
#include "os_info.h"

/****************************************************************************/
/* Write the version details of the current operating system into str		*/
/****************************************************************************/
char* os_version(char *str, size_t size)
{
#if defined(__OS2__) && defined(__BORLANDC__)

	safe_snprintf(str, size, "OS/2 %u.%u (%u.%u)", _osmajor / 10, _osminor / 10, _osmajor, _osminor);

#elif defined(_WIN32)

	/* Windows Version */
	char*         winflavor = "";
	OSVERSIONINFO winver;

	winver.dwOSVersionInfoSize = sizeof(winver);

	#pragma warning(suppress : 4996) // error C4996: 'GetVersionExA': was declared deprecated
	GetVersionEx(&winver);

	switch (winver.dwPlatformId) {
		case VER_PLATFORM_WIN32_NT:
			winflavor = "NT ";
			break;
		case VER_PLATFORM_WIN32s:
			winflavor = "Win32s ";
			break;
		case VER_PLATFORM_WIN32_WINDOWS:
			winver.dwBuildNumber &= 0xffff;
			break;
	}

	if (winver.dwMajorVersion == 6 && winver.dwMinorVersion == 1) {
		winver.dwMajorVersion = 7;
		winver.dwMinorVersion = 0;
	}
	else {
		static NTSTATUS (WINAPI * pRtlGetVersion)(PRTL_OSVERSIONINFOW lpVersionInformation) = NULL;
		if (pRtlGetVersion == NULL) {
			HINSTANCE ntdll = LoadLibrary("ntdll.dll");
			if (ntdll != NULL)
				pRtlGetVersion = (NTSTATUS (WINAPI *)(PRTL_OSVERSIONINFOW))GetProcAddress(ntdll, "RtlGetVersion");
		}
		if (pRtlGetVersion != NULL) {
			pRtlGetVersion((PRTL_OSVERSIONINFOW)&winver);
			if (winver.dwMajorVersion == 10 && winver.dwMinorVersion == 0 &&  winver.dwBuildNumber >= 22000)
				winver.dwMajorVersion = 11;
		}
	}

	safe_snprintf(str, size, "Windows %sVersion %lu.%lu"
	              , winflavor
	              , winver.dwMajorVersion, winver.dwMinorVersion);
	if (winver.dwBuildNumber)
		sprintf(str + strlen(str), " (Build %lu)", winver.dwBuildNumber);
	if (winver.szCSDVersion[0])
		sprintf(str + strlen(str), " %s", winver.szCSDVersion);

#elif defined(__unix__)
	FILE* fp = fopen("/etc/os-release", "r");
	if (fp == NULL)
		fp = fopen("/usr/lib/os-release", "r");
	if (fp != NULL) {
		char  value[INI_MAX_VALUE_LEN];
		char* p = iniReadString(fp, NULL, "PRETTY_NAME", "Unix", value);
		fclose(fp);
		SKIP_CHAR(p, '"');
		strncpy(str, p, size);
		p = lastchar(str);
		if (*p == '"')
			*p = '\0';
	} else {
		struct utsname unixver;

		if (uname(&unixver) != 0)
			safe_snprintf(str, size, "Unix (uname errno: %d)", errno);
		else
			safe_snprintf(str, size, "%s %s"
			              , unixver.sysname /* e.g. "Linux" */
			              , unixver.release /* e.g. "2.2.14-5.0" */
			              );
	}
#else   /* DOS */

	safe_snprintf(str, size, "DOS %u.%02u", _osmajor, _osminor);

#endif

	return str;
}

/****************************************************************************/
/* Write the CPU architecture according to the Operating System into str	*/
/****************************************************************************/
char* os_cpuarch(char *str, size_t size)
{
#if defined(_WIN32)
	SYSTEM_INFO sysinfo;

#if _WIN32_WINNT < 0x0501
	GetSystemInfo(&sysinfo);
#else
	GetNativeSystemInfo(&sysinfo);
#endif
	switch (sysinfo.wProcessorArchitecture) {
		case PROCESSOR_ARCHITECTURE_AMD64:
			safe_snprintf(str, size, "x64");
			break;
		case PROCESSOR_ARCHITECTURE_ARM:
			safe_snprintf(str, size, "ARM");
			break;
#if defined PROCESSOR_ARCHITECTURE_ARM64
		case PROCESSOR_ARCHITECTURE_ARM64:
			safe_snprintf(str, size, "ARM64");
			break;
#endif
		case PROCESSOR_ARCHITECTURE_IA64:
			safe_snprintf(str, size, "IA-64");
			break;
		case PROCESSOR_ARCHITECTURE_INTEL:
			safe_snprintf(str, size, "x86");
			break;
		default:
			safe_snprintf(str, size, "unknown");
			break;
	}

#elif defined(__unix__)

	struct utsname unixver;

	if (uname(&unixver) == 0)
		safe_snprintf(str, size, "%s", unixver.machine);
	else
		safe_snprintf(str, size, "unknown");

#endif

	return str;
}

char* os_cmdshell(void)
{
	char* shell = getenv(OS_CMD_SHELL_ENV_VAR);

#if defined(__unix__)
	if (shell == NULL)
#ifdef _PATH_BSHELL
		shell = _PATH_BSHELL;
#else
		shell = "/bin/sh";
#endif
#endif

	return shell;
}

/********************************************************/
