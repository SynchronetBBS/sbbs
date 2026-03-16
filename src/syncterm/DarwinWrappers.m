#include <stdbool.h>
#include <string.h>
#include "syncterm.h"
#include "dirwrap.h"
#undef BOOL

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
