#include <stdbool.h>
#include <string.h>
#include "syncterm.h"
#include "dirwrap.h"
#undef BOOL

#if 1 // TODO: Make this conditional on min version...
#import <Foundation/Foundation.h>

char *
get_OSX_filename(char *fn, int fnlen, int type, int shared)
{
	NSSearchPathDomainMask domain = shared ? NSLocalDomainMask : NSUserDomainMask;
	NSSearchPathDirectory path;
	switch(type) {
		case SYNCTERM_PATH_INI:
			path = NSLibraryDirectory;
			break;
		case SYNCTERM_PATH_LIST:
			path = NSLibraryDirectory;
			break;
		case SYNCTERM_DEFAULT_TRANSFER_PATH:
			path = NSDownloadsDirectory;
			break;
		case SYNCTERM_PATH_CACHE:
			path = NSCachesDirectory;
			break;
		case SYNCTERM_PATH_KEYS:
			path = NSLibraryDirectory;
			break;
	}

	NSError *error = nil;
	NSURL *none = nil;
	NSURL *result = [[NSFileManager defaultManager] URLForDirectory:path inDomain:domain appropriateForURL:none create:NO error:&error];
	if (result == nil) {
		strlcpy(fn, error.localizedDescription.UTF8String, fnlen);
		return NULL;
	}
	strlcpy(fn, result.path.UTF8String, fnlen);

	if (type == SYNCTERM_DEFAULT_TRANSFER_PATH) {
		strlcat(fn, "/", fnlen);
		return fn;
	}

	switch(type) {
		case SYNCTERM_PATH_INI:
			strlcat(fn, "/Preferences/SyncTERM", fnlen);
			mkpath(fn);
			strlcat(fn, "/SyncTERM.ini", fnlen);
			break;
		case SYNCTERM_PATH_LIST:
			strlcat(fn, "/Preferences/SyncTERM", fnlen);
			mkpath(fn);
			strlcat(fn, "/SyncTERM.lst", fnlen);
			break;
		case SYNCTERM_PATH_CACHE:
			strlcat(fn, "/SyncTERM", fnlen);
			mkpath(fn);
			strlcat(fn, "/", fnlen);
			break;
		case SYNCTERM_PATH_KEYS:
			strlcat(fn, "/Preferences/SyncTERM", fnlen);
			mkpath(fn);
			strlcat(fn, "/SyncTERM.ssh", fnlen);
			break;
	}

	return fn;
}

#else
#include <CoreServices/CoreServices.h> // FSFindFolder() and friends

char *
get_OSX_filename(char *fn, int fnlen, int type, int shared)
{
	FSRef ref;
	char *ret = fn;
	if (fnlen > 0)
		fn[0] = 0;

        /* First, get the path */
	switch (type) {
		case SYNCTERM_PATH_INI:
		case SYNCTERM_PATH_LIST:
		case SYNCTERM_PATH_KEYS:
			if (FSFindFolder(shared ? kLocalDomain : kUserDomain, kPreferencesFolderType, kCreateFolder,
			    &ref) != noErr)
				ret = NULL;
			if (FSRefMakePath(&ref, (unsigned char *)fn, fnlen) != noErr)
				ret = NULL;
			backslash(fn);
			strncat(fn, "SyncTERM", fnlen - strlen(fn) - 1);
			backslash(fn);
			if (!isdir(fn)) {
				if (MKDIR(fn))
					ret = NULL;
			}
			break;

		case SYNCTERM_DEFAULT_TRANSFER_PATH:
                        /* I'd love to use the "right" setting here, but don't know how */
			if (FSFindFolder(shared ? kLocalDomain : kUserDomain, kDownloadsFolderType, kCreateFolder,
			    &ref) != noErr)
				ret = NULL;
			if (FSRefMakePath(&ref, (unsigned char *)fn, fnlen) != noErr)
				ret = NULL;
			backslash(fn);
			if (!isdir(fn)) {
				if (MKDIR(fn))
					ret = NULL;
			}
			break;
		case SYNCTERM_PATH_CACHE:
			if (FSFindFolder(shared ? kLocalDomain : kUserDomain, kCachedDataFolderType, kCreateFolder,
			    &ref) != noErr)
				ret = NULL;
			if (FSRefMakePath(&ref, (unsigned char *)fn, fnlen) != noErr)
				ret = NULL;
			backslash(fn);
			break;
	}

	switch (type) {
		case SYNCTERM_PATH_INI:
			strncat(fn, "SyncTERM.ini", fnlen - strlen(fn) - 1);
			break;
		case SYNCTERM_PATH_LIST:
			strncat(fn, "SyncTERM.lst", fnlen - strlen(fn) - 1);
			break;
		case SYNCTERM_PATH_KEYS:
			strncat(fn, "SyncTERM.ssh", fnlen - strlen(fn) - 1);
			break;
		case SYNCTERM_PATH_CACHE:
			strncat(fn, "SyncTERM/", fnlen - strlen(fn) - 1);
			break;
	}
	return ret;
}

#endif
