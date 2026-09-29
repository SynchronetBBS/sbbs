/* Tests for replacing a file record's extended description (#1269) */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "smblib.h"
#include "CuTest.h"

#define TEST_SMB_PATH   "/tmp/smbfiletest"
#define TEST_FILENAME   "test.zip"

static const char* auxdata_json =
	"{\"archive\":{\"format\":\"ZIP\",\"files\":["
	"{\"name\":\"README.TXT\",\"size\":1234},"
	"{\"name\":\"FILE_ID.DIZ\",\"size\":321},"
	"{\"name\":\"PROGRAM.EXE\",\"size\":65432},"
	"{\"name\":\"DATA/LEVEL01.DAT\",\"size\":20480},"
	"{\"name\":\"DATA/LEVEL02.DAT\",\"size\":20480}]}}";

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

static void add_test_file(CuTest* tc, const char* extdesc, const char* auxdata)
{
	smb_t     smb = open_test_smb(tc, true);
	smbfile_t file;

	memset(&file, 0, sizeof(file));
	smb_hfield_str(&file, SMB_FILENAME, TEST_FILENAME);
	smb_hfield_str(&file, SMB_FILEDESC, "short description");
	file.name = TEST_FILENAME;
	CuAssertIntEquals_Msg(tc, smb.last_error, SMB_SUCCESS
	                      , smb_addfile(&smb, &file, SMB_SELFPACK, extdesc, auxdata, /* path: */ NULL));
	smb_freefilemem(&file);
	smb_close(&smb);
}

/* The sequence sbbs_t::editfileextdesc() uses: the record is loaded as the
   file lister loads it (without auxdata), the stored tail is read back and
   passed through unchanged with the new extended description. */
static void replace_extdesc(CuTest* tc, const char* new_extdesc, const char* tags)
{
	smb_t     smb = open_test_smb(tc, false);
	smbfile_t file;
	char*     auxdata;

	CuAssertIntEquals_Msg(tc, smb.last_error, SMB_SUCCESS
	                      , smb_loadfile(&smb, TEST_FILENAME, &file, file_detail_extdesc));
	if (tags != NULL)
		smb_new_hfield_str(&file, SMB_TAGS, tags);
	if ((auxdata = smb_getmsgtxt(&smb, &file, GETMSGTXT_TAIL_ONLY)) != NULL) {
		size_t len = strlen(auxdata);
		if (len >= 2 && auxdata[len - 2] == '\r' && auxdata[len - 1] == '\n')
			auxdata[len - 2] = '\0';
	}
	CuAssertIntEquals_Msg(tc, smb.last_error, SMB_SUCCESS, smb_getmsgidx(&smb, &file));
	CuAssertIntEquals_Msg(tc, smb.last_error, SMB_SUCCESS
	                      , smb_updatefile(&smb, &file, SMB_SELFPACK, new_extdesc, auxdata));
	smb_freemsgtxt(auxdata);
	smb_freefilemem(&file);
	smb_close(&smb);
}

/* smb_getmsgtxt() returns each data field's text followed by a CRLF */
static void check_file(CuTest* tc, const char* extdesc, const char* auxdata, const char* tags)
{
	smb_t     smb = open_test_smb(tc, false);
	smbfile_t file;
	char      expect[1024];

	CuAssertIntEquals_Msg(tc, smb.last_error, SMB_SUCCESS
	                      , smb_loadfile(&smb, TEST_FILENAME, &file, file_detail_auxdata));
	snprintf(expect, sizeof(expect), "%s\r\n", extdesc);
	CuAssertStrEquals(tc, expect, file.extdesc);
	snprintf(expect, sizeof(expect), "%s\r\n", auxdata);
	CuAssertStrEquals(tc, expect, file.auxdata);
	if (tags != NULL)
		CuAssertStrEquals(tc, tags, file.tags);
	CuAssertIntEquals(tc, 2, file.hdr.total_dfields);
	CuAssertIntEquals(tc, TEXT_BODY, file.dfield[0].type);
	CuAssertIntEquals(tc, TEXT_TAIL, file.dfield[1].type);
	CuAssertIntEquals(tc, file.dfield[0].offset + file.dfield[0].length, file.dfield[1].offset);
	smb_freefilemem(&file);
	smb_close(&smb);
}

void Test_ReplaceExtdescKeepsAuxdata(CuTest* tc)
{
	add_test_file(tc, "The original extended description,\r\nwhich runs to two lines.", auxdata_json);
	replace_extdesc(tc, "Shorter.", NULL);
	check_file(tc, "Shorter.", auxdata_json, NULL);
	replace_extdesc(tc, "A much longer replacement description that no longer fits in the"
	                " space the original one occupied, so the data must move.", NULL);
	check_file(tc, "A much longer replacement description that no longer fits in the"
	           " space the original one occupied, so the data must move.", auxdata_json, NULL);
}

void Test_AddExtdescToAuxdataOnlyRecord(CuTest* tc)
{
	const char* tags = "archive game dos shareware retro level-pack";

	add_test_file(tc, /* extdesc: */ NULL, auxdata_json);
	replace_extdesc(tc, "A new extended description.", tags);
	check_file(tc, "A new extended description.", auxdata_json, tags);
}

CuSuite* SmbFileTestSuite(void)
{
	CuSuite* suite = CuSuiteNew();

	SUITE_ADD_TEST(suite, Test_ReplaceExtdescKeepsAuxdata);
	SUITE_ADD_TEST(suite, Test_AddExtdescToAuxdataOnlyRecord);
	return suite;
}

int main(void)
{
	CuSuite*  suite = SmbFileTestSuite();
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
