/* Tests for file names that differ only in case, in one file base */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "smblib.h"
#include "CuTest.h"

#define TEST_SMB_PATH   "/tmp/smbfilenametest"

static smb_t open_test_smb(CuTest* tc, bool create)
{
	smb_t smb;

	if (create) {
		remove(TEST_SMB_PATH ".shd");
		remove(TEST_SMB_PATH ".sid");
		remove(TEST_SMB_PATH ".sdt");
		remove(TEST_SMB_PATH ".sda");
		remove(TEST_SMB_PATH ".sha");
		remove(TEST_SMB_PATH ".hash");
	}
	memset(&smb, 0, sizeof(smb));
	SAFECOPY(smb.file, TEST_SMB_PATH);
	smb.retry_time = 10;
	CuAssertIntEquals_Msg(tc, smb.last_error, SMB_SUCCESS, smb_open(&smb));
	if (create) {
		smb.status.attr = SMB_FILE_DIRECTORY | SMB_NOHASH;
		CuAssertIntEquals_Msg(tc, smb.last_error, SMB_SUCCESS, smb_create(&smb));
	}
	return smb;
}

static void add_file(CuTest* tc, smb_t* smb, const char* name, const char* desc)
{
	smbfile_t file;

	memset(&file, 0, sizeof(file));
	smb_hfield_str(&file, SMB_FILENAME, name);
	smb_hfield_str(&file, SMB_FILEDESC, desc);
	file.name = (char*)name;
	CuAssertIntEquals_Msg(tc, smb->last_error, SMB_SUCCESS
	                      , smb_addfile(smb, &file, SMB_SELFPACK, NULL, NULL, /* path: */ NULL));
	smb_freefilemem(&file);
}

/* A base holding a.txt ("first"), A.txt ("second") and c.txt ("third").
   smb_addfile() refuses a name differing only in case, so A.txt is added as
   b.txt and renamed the way the Terminal Server's file name editor renames,
   without the duplicate check that editor could once skip. */
static smb_t create_case_dupes(CuTest* tc)
{
	smb_t     smb = open_test_smb(tc, true);
	smbfile_t file;

	add_file(tc, &smb, "a.txt", "first");
	add_file(tc, &smb, "b.txt", "second");
	add_file(tc, &smb, "c.txt", "third");
	CuAssertIntEquals_Msg(tc, smb.last_error, SMB_SUCCESS
	                      , smb_loadfile(&smb, "b.txt", &file, file_detail_normal));
	CuAssertIntEquals_Msg(tc, smb.last_error, SMB_SUCCESS
	                      , smb_new_hfield_str(&file, SMB_FILENAME, "A.txt"));
	CuAssertIntEquals_Msg(tc, smb.last_error, SMB_SUCCESS, smb_updatemsg(&smb, &file));
	smb_freefilemem(&file);
	return smb;
}

/* Load 'name' and check which file was found, by its description */
static void check_load(CuTest* tc, smb_t* smb, const char* name, const char* expect_name, const char* expect_desc)
{
	smbfile_t file;

	CuAssertIntEquals_Msg(tc, name, SMB_SUCCESS, smb_loadfile(smb, name, &file, file_detail_normal));
	CuAssertStrEquals_Msg(tc, name, expect_name, file.name);
	CuAssertStrEquals_Msg(tc, name, expect_desc, file.desc);
	smb_freefilemem(&file);
}

static uint32_t file_number(CuTest* tc, smb_t* smb, const char* name)
{
	smbfile_t file;
	uint32_t  number;

	CuAssertIntEquals_Msg(tc, name, SMB_SUCCESS, smb_loadfile(smb, name, &file, file_detail_normal));
	number = file.idx.number;
	smb_freefilemem(&file);
	return number;
}

void Test_FindPrefersExactCase(CuTest* tc)
{
	smb_t smb = create_case_dupes(tc);

	check_load(tc, &smb, "a.txt", "a.txt", "first");
	check_load(tc, &smb, "A.txt", "A.txt", "second");
	/* No exact match: the first of the case-insensitive ones */
	check_load(tc, &smb, "A.TXT", "a.txt", "first");
	CuAssertIntEquals(tc, SMB_ERR_NOT_FOUND, smb_findfile(&smb, "z.txt", NULL));
	smb_close(&smb);
}

void Test_FindOtherSkipsOnlyTheNamedFile(CuTest* tc)
{
	smb_t    smb = create_case_dupes(tc);
	uint32_t a = file_number(tc, &smb, "a.txt");
	uint32_t c = file_number(tc, &smb, "c.txt");

	/* Renaming c.txt to a new case of its own name duplicates nothing */
	CuAssertIntEquals(tc, SMB_ERR_NOT_FOUND, smb_findfile_other(&smb, "C.TXT", c));
	/* ...but renaming it to a new case of another's name does */
	CuAssertIntEquals(tc, SMB_SUCCESS, smb_findfile_other(&smb, "a.TXT", c));
	/* Renaming a.txt to A.txt would duplicate the other file's name */
	CuAssertIntEquals(tc, SMB_SUCCESS, smb_findfile_other(&smb, "A.txt", a));
	CuAssertIntEquals(tc, SMB_ERR_NOT_FOUND, smb_findfile_other(&smb, "z.txt", a));
	smb_close(&smb);
}

void Test_RemoveOnlyTheFilesOwnRecord(CuTest* tc)
{
	smb_t smb = create_case_dupes(tc);

	CuAssertIntEquals_Msg(tc, smb.last_error, SMB_SUCCESS, smb_removefile_by_name(&smb, "A.txt"));
	CuAssertIntEquals_Msg(tc, smb.last_error, SMB_SUCCESS, smb_getstatus(&smb));
	CuAssertIntEquals(tc, 2, smb.status.total_files);
	check_load(tc, &smb, "a.txt", "a.txt", "first");
	check_load(tc, &smb, "A.txt", "a.txt", "first");
	check_load(tc, &smb, "c.txt", "c.txt", "third");
	smb_close(&smb);
}

/* The index record removed is the one the file was loaded from, even when
   its header's copy of the message number doesn't match */
void Test_RemoveByIndexNumber(CuTest* tc)
{
	smb_t     smb = create_case_dupes(tc);
	smbfile_t file;

	CuAssertIntEquals_Msg(tc, smb.last_error, SMB_SUCCESS
	                      , smb_loadfile(&smb, "A.txt", &file, file_detail_normal));
	file.hdr.number = 99;
	CuAssertIntEquals_Msg(tc, smb.last_error, SMB_SUCCESS, smb_putmsghdr(&smb, &file));
	smb_freefilemem(&file);

	CuAssertIntEquals_Msg(tc, smb.last_error, SMB_SUCCESS
	                      , smb_loadfile(&smb, "A.txt", &file, file_detail_normal));
	CuAssertIntEquals(tc, 99, file.hdr.number);
	CuAssertIntEquals_Msg(tc, smb.last_error, SMB_SUCCESS, smb_removefile(&smb, &file));
	smb_freefilemem(&file);
	CuAssertIntEquals_Msg(tc, smb.last_error, SMB_SUCCESS, smb_getstatus(&smb));
	CuAssertIntEquals(tc, 2, smb.status.total_files);
	check_load(tc, &smb, "a.txt", "a.txt", "first");
	check_load(tc, &smb, "c.txt", "c.txt", "third");
	smb_close(&smb);
}

/* A file with no index record loaded is refused before anything is changed */
void Test_RemoveWithoutIndexRecordChangesNothing(CuTest* tc)
{
	smb_t     smb = create_case_dupes(tc);
	smbfile_t file;

	CuAssertIntEquals_Msg(tc, smb.last_error, SMB_SUCCESS
	                      , smb_loadfile(&smb, "A.txt", &file, file_detail_normal));
	file.idx.number = 0;
	CuAssertIntEquals(tc, SMB_ERR_NOT_FOUND, smb_removefile(&smb, &file));
	CuAssertIntEquals(tc, 0, file.hdr.attr & MSG_DELETE);
	smb_freefilemem(&file);
	CuAssertIntEquals_Msg(tc, smb.last_error, SMB_SUCCESS, smb_getstatus(&smb));
	CuAssertIntEquals(tc, 3, smb.status.total_files);
	CuAssertIntEquals_Msg(tc, smb.last_error, SMB_SUCCESS
	                      , smb_loadfile(&smb, "A.txt", &file, file_detail_normal));
	CuAssertIntEquals(tc, 0, file.hdr.attr & MSG_DELETE);
	smb_freefilemem(&file);
	smb_close(&smb);
}

CuSuite* SmbFileNameTestSuite(void)
{
	CuSuite* suite = CuSuiteNew();

	SUITE_ADD_TEST(suite, Test_FindPrefersExactCase);
	SUITE_ADD_TEST(suite, Test_FindOtherSkipsOnlyTheNamedFile);
	SUITE_ADD_TEST(suite, Test_RemoveOnlyTheFilesOwnRecord);
	SUITE_ADD_TEST(suite, Test_RemoveByIndexNumber);
	SUITE_ADD_TEST(suite, Test_RemoveWithoutIndexRecordChangesNothing);
	return suite;
}

int main(void)
{
	CuSuite*  suite = SmbFileNameTestSuite();
	CuString* output = CuStringNew();
	int       failed;

	CuSuiteRun(suite);
	CuSuiteSummary(suite, output);
	CuSuiteDetails(suite, output);
	printf("%s\n", output->buffer);
	failed = suite->failCount;
	CuSuiteDelete(suite);
	CuStringDelete(output);
	return failed ? EXIT_FAILURE : EXIT_SUCCESS;
}
