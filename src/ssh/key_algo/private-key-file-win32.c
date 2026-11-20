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
	LPSTR owner_name = (LPSTR)sid;
	access.Trustee.ptstrName = owner_name;
	PACL acl = NULL;
	if (SetEntriesInAclA(1, &access, NULL, &acl) != ERROR_SUCCESS)
		return NULL;
	return acl;
}

/* Open without following the final reparse point or changing existing data.
 * Validate the acquired object, including its actual owner, before changing
 * its ACL or length. Parent-path coordination belongs to the application. */
FILE *
dssh_private_key_open(const char *path)
{
	HANDLE token = current_access_token();
	HANDLE file = INVALID_HANDLE_VALUE;
	TOKEN_USER *token_user = NULL;
	PACL private_acl = NULL;
	PSECURITY_DESCRIPTOR owner_descriptor = NULL;
	FILE *stream = NULL;
	bool valid = false;
	if (token == NULL)
		return NULL;

	DWORD token_len = 0;
	if (GetTokenInformation(token, TokenUser, NULL, 0, &token_len)
	    || GetLastError() != ERROR_INSUFFICIENT_BUFFER
	    || token_len < sizeof(*token_user))
		goto cleanup;
	token_user = malloc(token_len);
	if (token_user == NULL)
		goto cleanup;
	if (!GetTokenInformation(token, TokenUser, token_user, token_len, &token_len)
	    || !IsValidSid(token_user->User.Sid))
		goto cleanup;
	private_acl = acl_for_sid(token_user->User.Sid);
	if (private_acl == NULL)
		goto cleanup;

	SECURITY_DESCRIPTOR descriptor;
	if (!InitializeSecurityDescriptor(&descriptor, SECURITY_DESCRIPTOR_REVISION)
	    || !SetSecurityDescriptorOwner(&descriptor, token_user->User.Sid, FALSE)
	    || !SetSecurityDescriptorDacl(&descriptor, TRUE, private_acl, FALSE)
	    || !SetSecurityDescriptorControl(&descriptor, SE_DACL_PROTECTED, SE_DACL_PROTECTED))
		goto cleanup;
	SECURITY_ATTRIBUTES attributes;
	attributes.nLength = sizeof(attributes);
	attributes.lpSecurityDescriptor = &descriptor;
	attributes.bInheritHandle = FALSE;
	file = CreateFileA(path, GENERIC_READ | GENERIC_WRITE | READ_CONTROL | WRITE_DAC,
	    0, &attributes, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, NULL);
	if (file == INVALID_HANDLE_VALUE)
		goto cleanup;
	BY_HANDLE_FILE_INFORMATION info;
	if (GetFileType(file) != FILE_TYPE_DISK || !GetFileInformationByHandle(file, &info))
		goto cleanup;
	if ((info.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) != 0
	    || info.nNumberOfLinks != 1) {
		SetLastError(ERROR_ACCESS_DENIED);
		goto cleanup;
	}
	PSID owner = NULL;
	DWORD status = GetSecurityInfo(file, SE_FILE_OBJECT, OWNER_SECURITY_INFORMATION,
	    &owner, NULL, NULL, NULL, &owner_descriptor);
	if (status != ERROR_SUCCESS) {
		SetLastError(status);
		goto cleanup;
	}
	if (owner == NULL || !IsValidSid(owner) || !EqualSid(owner, token_user->User.Sid)) {
		SetLastError(ERROR_ACCESS_DENIED);
		goto cleanup;
	}
	status = SetSecurityInfo(file, SE_FILE_OBJECT,
	    DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION,
	    NULL, NULL, private_acl, NULL);
	if (status != ERROR_SUCCESS) {
		SetLastError(status);
		goto cleanup;
	}
	LARGE_INTEGER start;
	start.QuadPart = 0;
	if (!SetFilePointerEx(file, start, NULL, FILE_BEGIN) || !SetEndOfFile(file))
		goto cleanup;
	intptr_t native_file = (intptr_t)file;
	int fd = _open_osfhandle(native_file, _O_BINARY | _O_NOINHERIT | _O_WRONLY);
	if (fd < 0)
		goto cleanup;
	/* The CRT owns the handle after successful association. */
	file = INVALID_HANDLE_VALUE;
	stream = _fdopen(fd, "wb");
	if (stream == NULL) {
		if (_close(fd) != 0)
			goto cleanup;
		goto cleanup;
	}
	valid = true;
cleanup:
	/* Retire each native acquisition once. A cleanup failure also prevents
	 * publishing a writable stream; B permits an already-truncated file. */
	if (file != INVALID_HANDLE_VALUE && !CloseHandle(file))
		valid = false;
	if (!CloseHandle(token))
		valid = false;
	if (owner_descriptor != NULL && LocalFree(owner_descriptor) != NULL)
		valid = false;
	if (private_acl != NULL && LocalFree(private_acl) != NULL)
		valid = false;
	free(token_user);
	if (!valid && stream != NULL) {
		if (fclose(stream) != 0)
			return NULL;
		stream = NULL;
	}
	return stream;
}

/* The CRT N mode sets non-inheritance as part of opening the stream. Paths
 * have already been checked for NULL/empty values by the public wrappers. */
DSSH_PRIVATE FILE *
dssh_key_file_open_read(const char *path)
{
	return fopen(path, "rbN");
}

DSSH_PRIVATE FILE *
dssh_key_file_open_public(const char *path)
{
	return fopen(path, "wN");
}
