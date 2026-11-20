/* Native acceptance of the selected in-place key-save policy and owner ACL. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <aclapi.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "deucessh.h"
#include "deucessh-algorithms.h"
#include "deucessh-crypto.h"

static bool
key_identity(const char *path, BY_HANDLE_FILE_INFORMATION *identity)
{
	HANDLE file = CreateFileA(path, FILE_READ_ATTRIBUTES, FILE_SHARE_READ,
	    NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (file == INVALID_HANDLE_VALUE)
		return false;
	bool valid = GetFileInformationByHandle(file, identity) != 0;
	if (!CloseHandle(file))
		valid = false;
	return valid;
}

static bool
key_permissions(const char *path)
{
	PSID owner = NULL;
	PACL dacl = NULL;
	PSECURITY_DESCRIPTOR descriptor = NULL;
	DWORD status = GetNamedSecurityInfoA(path, SE_FILE_OBJECT,
	    OWNER_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION,
	    &owner, NULL, &dacl, NULL, &descriptor);
	if (status != ERROR_SUCCESS)
		return false;
	SECURITY_DESCRIPTOR_CONTROL control = 0;
	DWORD revision = 0;
	void *raw = NULL;
	bool valid = owner != NULL && dacl != NULL && dacl->AceCount == 1
	    && GetSecurityDescriptorControl(descriptor, &control, &revision)
	    && (control & SE_DACL_PROTECTED) != 0 && GetAce(dacl, 0, &raw);
	if (valid) {
		ACCESS_ALLOWED_ACE *ace = raw;
		DWORD mask = ace->Mask;
		GENERIC_MAPPING mapping = {
			FILE_GENERIC_READ, FILE_GENERIC_WRITE, FILE_GENERIC_EXECUTE, FILE_ALL_ACCESS,
		};
		MapGenericMask(&mask, &mapping);
		valid = ace->Header.AceType == ACCESS_ALLOWED_ACE_TYPE
		    && mask == FILE_ALL_ACCESS && EqualSid(owner, &ace->SidStart);
	}
	if (LocalFree(descriptor) != NULL)
		valid = false;
	return valid;
}

static bool
key_digest(const char *path, uint8_t digest[32])
{
	FILE *file = fopen(path, "rb");
	if (file == NULL)
		return false;
	dssh_hash_ctx *hash = NULL;
	size_t length = 0;
	bool valid = dssh_hash_init(&hash, "SHA-256", &length) == 0 && length == 32;
	uint8_t buffer[1024];
	while (valid) {
		size_t count = fread(buffer, 1, sizeof(buffer), file);
		if (count != 0)
			valid = dssh_hash_update(hash, buffer, count) == 0;
		if (count < sizeof(buffer)) {
			valid &= ferror(file) == 0;
			break;
		}
	}
	if (fclose(file) != 0)
		valid = false;
	if (valid)
		valid = dssh_hash_final(hash, digest, 32) == 0;
	dssh_hash_free(hash);
	return valid;
}

static int
unexpected_password(char *buffer, int capacity, int writing, void *opaque)
{
	(void)buffer;
	(void)capacity;
	(void)writing;
	unsigned *calls = opaque;
	(*calls)++;
	return -1;
}

static bool
key_rejected(const char *target, const char *original)
{
	uint8_t before[32], after[32];
	unsigned calls = 0;
	return key_digest(original, before)
	    && dssh_ed25519_save_key_file(target, unexpected_password, &calls) < 0
	    && calls == 0 && key_digest(original, after)
	    && memcmp(before, after, sizeof(before)) == 0 && key_permissions(original);
}

static bool
key_target_checks(const char *path)
{
	size_t length = strlen(path);
	if (length > SIZE_MAX - 16)
		return false;
	char *other = malloc(length + 16);
	if (other == NULL)
		return false;
	memcpy(other, path, length);
	memcpy(other + length, ".hardlink", sizeof(".hardlink"));
	bool created = CreateHardLinkA(other, path, NULL) != 0;
	bool valid = created && key_rejected(other, path);
	if (created && !DeleteFileA(other))
		valid = false;
	if (valid) {
		memcpy(other + length, ".symlink", sizeof(".symlink"));
		created = CreateSymbolicLinkA(other, path, SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE) != 0;
		valid = created && key_rejected(other, path);
		if (created && !DeleteFileA(other))
			valid = false;
	}
	if (valid) {
		memcpy(other + length, ".directory", sizeof(".directory"));
		created = CreateDirectoryA(other, NULL) != 0;
		valid = created && key_rejected(other, path);
		if (created && !RemoveDirectoryA(other))
			valid = false;
	}
	free(other);
	return valid;
}

int
main(int argc, char **argv)
{
	if (argc != 2)
		return 2;
	if (!DeleteFileA(argv[1]) && GetLastError() != ERROR_FILE_NOT_FOUND)
		return 2;
	BY_HANDLE_FILE_INFORMATION first = {0}, second = {0};
	bool valid = dssh_register_ssh_ed25519() == 0
	    && dssh_ed25519_generate_key() == 0
	    && dssh_ed25519_save_key_file(argv[1], NULL, NULL) == 0
	    && key_permissions(argv[1]) && key_identity(argv[1], &first)
	    && dssh_ed25519_generate_key() == 0
	    && dssh_ed25519_save_key_file(argv[1], NULL, NULL) == 0
	    && key_permissions(argv[1]) && key_identity(argv[1], &second)
	    && first.dwVolumeSerialNumber == second.dwVolumeSerialNumber
	    && first.nFileIndexHigh == second.nFileIndexHigh
	    && first.nFileIndexLow == second.nFileIndexLow
	    && dssh_ed25519_load_key_file(argv[1], NULL, NULL) == 0
	    && key_target_checks(argv[1]);
	if (!DeleteFileA(argv[1]))
		valid = false;
	if (printf("key_permissions owner=1 protected=1 same_file=1 rejected=3 valid=%d\n", valid ? 1 : 0) < 0
	    || fflush(stdout) != 0)
		return 2;
	return valid ? 0 : 1;
}
