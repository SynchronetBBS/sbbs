/*
 * key_algo/private-key-file-win32.c -- securely create private key files.
 */

#include "private-key-file.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <aclapi.h>
#include <fcntl.h>
#include <io.h>
#include <stdint.h>
#include <stdlib.h>

static HANDLE
current_access_token(void)
{
	HANDLE token;
	if (OpenThreadToken(GetCurrentThread(), TOKEN_QUERY, TRUE, &token))
		return token;
	if (GetLastError() != ERROR_NO_TOKEN)
		return NULL;
	if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token))
		return NULL;
	return token;
}

static PACL
acl_for_sid(PSID sid)
{
	EXPLICIT_ACCESS_A access = {0};
	access.grfAccessPermissions = GENERIC_ALL;
	access.grfAccessMode = SET_ACCESS;
	access.grfInheritance = NO_INHERITANCE;
	access.Trustee.TrusteeForm = TRUSTEE_IS_SID;
	access.Trustee.TrusteeType = TRUSTEE_IS_UNKNOWN;
	access.Trustee.ptstrName = (LPSTR)sid;
	PACL acl = NULL;
	if (SetEntriesInAclA(1, &access, NULL, &acl) != ERROR_SUCCESS)
		return NULL;
	return acl;
}

FILE *
dssh_private_key_open(const char *path)
{
	HANDLE token = current_access_token();
	if (token == NULL)
		return NULL;

	DWORD token_len = 0;
	GetTokenInformation(token, TokenUser, NULL, 0, &token_len);
	if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
		CloseHandle(token);
		return NULL;
	}
	TOKEN_USER *token_user = malloc(token_len);
	if (token_user == NULL) {
		CloseHandle(token);
		return NULL;
	}
	if (!GetTokenInformation(token, TokenUser, token_user, token_len,
	    &token_len)) {
		free(token_user);
		CloseHandle(token);
		return NULL;
	}
	CloseHandle(token);

	PACL create_acl = acl_for_sid(token_user->User.Sid);
	if (create_acl == NULL) {
		free(token_user);
		return NULL;
	}

	SECURITY_DESCRIPTOR descriptor;
	if (!InitializeSecurityDescriptor(&descriptor,
	    SECURITY_DESCRIPTOR_REVISION)
	    || !SetSecurityDescriptorOwner(&descriptor,
	    token_user->User.Sid, FALSE)
	    || !SetSecurityDescriptorDacl(&descriptor, TRUE, create_acl, FALSE)
	    || !SetSecurityDescriptorControl(&descriptor, SE_DACL_PROTECTED,
	    SE_DACL_PROTECTED)) {
		LocalFree(create_acl);
		free(token_user);
		return NULL;
	}
	SECURITY_ATTRIBUTES attributes;
	attributes.nLength = sizeof(attributes);
	attributes.lpSecurityDescriptor = &descriptor;
	attributes.bInheritHandle = FALSE;

	HANDLE file = CreateFileA(path, GENERIC_READ | GENERIC_WRITE
	    | READ_CONTROL | WRITE_DAC, 0, &attributes,
	    OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	DWORD create_status = GetLastError();
	if (file == INVALID_HANDLE_VALUE) {
		LocalFree(create_acl);
		free(token_user);
		return NULL;
	}

	PACL private_acl = create_acl;
	PSECURITY_DESCRIPTOR owner_descriptor = NULL;
	if (create_status == ERROR_ALREADY_EXISTS) {
		PSID owner = NULL;
		DWORD status = GetSecurityInfo(file, SE_FILE_OBJECT,
		    OWNER_SECURITY_INFORMATION, &owner, NULL, NULL, NULL,
		    &owner_descriptor);
		if (status != ERROR_SUCCESS) {
			CloseHandle(file);
			LocalFree(create_acl);
			free(token_user);
			return NULL;
		}
		private_acl = acl_for_sid(owner);
		if (private_acl == NULL) {
			LocalFree(owner_descriptor);
			CloseHandle(file);
			LocalFree(create_acl);
			free(token_user);
			return NULL;
		}
	}

	DWORD status = SetSecurityInfo(file, SE_FILE_OBJECT,
	    DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION,
	    NULL, NULL, private_acl, NULL);
	if (private_acl != create_acl)
		LocalFree(private_acl);
	if (owner_descriptor != NULL)
		LocalFree(owner_descriptor);
	LocalFree(create_acl);
	free(token_user);
	if (status != ERROR_SUCCESS) {
		CloseHandle(file);
		return NULL;
	}

	LARGE_INTEGER start;
	start.QuadPart = 0;
	if (!SetFilePointerEx(file, start, NULL, FILE_BEGIN)
	    || !SetEndOfFile(file)) {
		CloseHandle(file);
		return NULL;
	}

	int fd = _open_osfhandle((intptr_t)file,
	    _O_BINARY | _O_NOINHERIT | _O_WRONLY);
	if (fd < 0) {
		CloseHandle(file);
		return NULL;
	}
	FILE *fp = _fdopen(fd, "wb");
	if (fp == NULL)
		_close(fd);
	return fp;
}
