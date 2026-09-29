/* Test for smb_new_msghdr index auto-repair (SHORT and LONG corruption cases) */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "smblib.h"
#include "CuTest.h"

#define TEST_SMB_PATH   "/tmp/smbidxtest"

static int add_test_msg(smb_t* smb, const char* from, const char* subj, const char* body)
{
	smbmsg_t msg;
	memset(&msg, 0, sizeof(msg));
	smb_hfield_str(&msg, SENDER, from);
	smb_hfield_str(&msg, RECIPIENT, "All");
	smb_hfield_str(&msg, SUBJECT, subj);
	smb_dfield(&msg, TEXT_BODY, strlen(body));
	int result = smb_addmsg(smb, &msg, SMB_SELFPACK, SMB_HASH_SOURCE_NONE, XLAT_NONE,
	                        (const uchar*)body, NULL);
	smb_freemsgmem(&msg);
	return result;
}

/* Create a fresh msgbase with N messages; return opened smb */
static smb_t create_test_smb(CuTest* tc, int count)
{
	/* Remove any stale files */
	remove(TEST_SMB_PATH ".shd");
	remove(TEST_SMB_PATH ".sid");
	remove(TEST_SMB_PATH ".sdt");
	remove(TEST_SMB_PATH ".sda");
	remove(TEST_SMB_PATH ".sha");

	smb_t smb;
	memset(&smb, 0, sizeof(smb));
	SAFECOPY(smb.file, TEST_SMB_PATH);
	smb.retry_time = 10;

	int i = smb_open(&smb);
	CuAssertIntEquals_Msg(tc, smb.last_error, SMB_SUCCESS, i);
	/* smb_addmsg auto-calls smb_create if .shd is empty, but do it explicitly to be safe */
	if (filelength(fileno(smb.shd_fp)) < 1) {
		i = smb_create(&smb);
		CuAssertIntEquals_Msg(tc, smb.last_error, SMB_SUCCESS, i);
	}

	for (int n = 0; n < count; n++) {
		char subj[32];
		safe_snprintf(subj, sizeof(subj), "Test message %d", n + 1);
		i = add_test_msg(&smb, "tester", subj, "body text");
		CuAssertIntEquals_Msg(tc, smb.last_error, SMB_SUCCESS, i);
	}
	return smb;
}

/* Corrupt .sid by truncating one index record off the end */
static void truncate_sid_by(CuTest* tc, smb_t* smb, int records)
{
	size_t idxreclen = smb_idxreclen(smb);
	off_t  sidlen = filelength(fileno(smb->sid_fp));
	CuAssertIntEquals_Msg(tc, "truncating .sid", 0
	                      , chsize(fileno(smb->sid_fp), sidlen - (records * idxreclen)));
}

/* Corrupt .sid by appending one extra (zeroed) index record */
static void extend_sid_by(smb_t* smb, int records)
{
	size_t idxreclen = smb_idxreclen(smb);
	fseek(smb->sid_fp, 0, SEEK_END);
	uchar  zero[64] = {0};
	for (int n = 0; n < records; n++)
		fwrite(zero, idxreclen, 1, smb->sid_fp);
	fflush(smb->sid_fp);
}

/*
 * SHORT case: .sid is 1 record shorter than total_msgs expects.
 * smb_new_msghdr should auto-correct total_msgs and proceed.
 */
void Test_ShortIndexByOne(CuTest* tc)
{
	smb_t smb = create_test_smb(tc, 5);
	CuAssertIntEquals(tc, 5, (int)smb.status.total_msgs);

	truncate_sid_by(tc, &smb, 1);

	/* Next add should auto-repair and succeed */
	int i = add_test_msg(&smb, "tester", "After repair", "body");
	CuAssertIntEquals_Msg(tc, smb.last_error, SMB_SUCCESS, i);

	/* Re-read status: should reflect 5 msgs (4 surviving + 1 new) */
	smb_getstatus(&smb);  /* re-read from disk to verify */
	CuAssertIntEquals(tc, 5, (int)smb.status.total_msgs);

	smb_close(&smb);
}

/*
 * SHORT case: .sid is 2 records shorter than total_msgs expects.
 */
void Test_ShortIndexByTwo(CuTest* tc)
{
	smb_t smb = create_test_smb(tc, 6);

	truncate_sid_by(tc, &smb, 2);

	int   i = add_test_msg(&smb, "tester", "After repair", "body");
	CuAssertIntEquals_Msg(tc, smb.last_error, SMB_SUCCESS, i);

	smb_getstatus(&smb);  /* re-read from disk to verify */
	CuAssertIntEquals(tc, 5, (int)smb.status.total_msgs);  /* 4 surviving + 1 new */

	smb_close(&smb);
}

/*
 * LONG case: .sid has 1 extra orphan record beyond total_msgs.
 * smb_new_msghdr should truncate the orphan and proceed.
 */
void Test_LongIndexByOne(CuTest* tc)
{
	smb_t smb = create_test_smb(tc, 5);

	extend_sid_by(&smb, 1);

	int   i = add_test_msg(&smb, "tester", "After repair", "body");
	CuAssertIntEquals_Msg(tc, smb.last_error, SMB_SUCCESS, i);

	smb_getstatus(&smb);  /* re-read from disk to verify */
	CuAssertIntEquals(tc, 6, (int)smb.status.total_msgs);  /* original 5 + 1 new */

	smb_close(&smb);
}

/*
 * Large mismatch (3+ records short): should still return SMB_ERR_FILE_LEN.
 */
void Test_LargeCorruptionFails(CuTest* tc)
{
	smb_t smb = create_test_smb(tc, 6);

	truncate_sid_by(tc, &smb, 3);

	int   i = add_test_msg(&smb, "tester", "Should fail", "body");
	CuAssertIntEquals(tc, SMB_ERR_FILE_LEN, i);

	smb_close(&smb);
}

/*
 * No corruption: normal add should still work.
 */
void Test_NoCorruption(CuTest* tc)
{
	smb_t smb = create_test_smb(tc, 3);

	int   i = add_test_msg(&smb, "tester", "Normal", "body");
	CuAssertIntEquals_Msg(tc, smb.last_error, SMB_SUCCESS, i);

	smb_getstatus(&smb);  /* re-read from disk to verify */
	CuAssertIntEquals(tc, 4, (int)smb.status.total_msgs);

	smb_close(&smb);
}

CuSuite* SmbIdxTestSuite(void)
{
	CuSuite* suite = CuSuiteNew();
	SUITE_ADD_TEST(suite, Test_NoCorruption);
	SUITE_ADD_TEST(suite, Test_ShortIndexByOne);
	SUITE_ADD_TEST(suite, Test_ShortIndexByTwo);
	SUITE_ADD_TEST(suite, Test_LongIndexByOne);
	SUITE_ADD_TEST(suite, Test_LargeCorruptionFails);
	return suite;
}

int main(void)
{
	CuSuite*  suite = SmbIdxTestSuite();
	CuString* output = CuStringNew();

	CuSuiteRun(suite);
	CuSuiteSummary(suite, output);
	CuSuiteDetails(suite, output);
	printf("%s\n", output->buffer);

	int failed = suite->failCount;
	CuSuiteDelete(suite);
	CuStringDelete(output);
	return failed ? 1 : 0;
}
