/* Tests for data block reference counting (#1253) */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "smblib.h"
#include "xpendian.h"
#include "CuTest.h"

#define TEST_SMB_PATH   "/tmp/smballoctest"
#define MAX_BLOCKS      16

/* A 200-byte body and a 100-byte tail: 304 bytes with the xlat terminators,
   so 2 data blocks, and the tail starts in block 0 and spills into block 1. */
static smb_t add_body_and_tail(CuTest* tc, smbmsg_t* msg)
{
	smb_t smb;
	char  body[201];
	char  tail[101];

	remove(TEST_SMB_PATH ".shd");
	remove(TEST_SMB_PATH ".sid");
	remove(TEST_SMB_PATH ".sdt");
	remove(TEST_SMB_PATH ".sda");
	remove(TEST_SMB_PATH ".sha");
	remove(TEST_SMB_PATH ".hash");
	memset(&smb, 0, sizeof(smb));
	SAFECOPY(smb.file, TEST_SMB_PATH);
	smb.retry_time = 10;
	CuAssertIntEquals_Msg(tc, smb.last_error, SMB_SUCCESS, smb_open(&smb));
	CuAssertIntEquals_Msg(tc, smb.last_error, SMB_SUCCESS, smb_create(&smb));

	memset(body, 'B', sizeof(body) - 1);
	body[sizeof(body) - 1] = '\0';
	memset(tail, 'T', sizeof(tail) - 1);
	tail[sizeof(tail) - 1] = '\0';
	memset(msg, 0, sizeof(*msg));
	smb_hfield_str(msg, SENDER, "sender");
	smb_hfield_str(msg, RECIPIENT, "recipient");
	smb_hfield_str(msg, SUBJECT, "refs");
	CuAssertIntEquals_Msg(tc, smb.last_error, SMB_SUCCESS
	                      , smb_addmsg(&smb, msg, SMB_SELFPACK, SMB_HASH_SOURCE_NONE, XLAT_NONE
	                                   , (uchar*)body, (uchar*)tail));
	CuAssertIntEquals(tc, 2, msg->hdr.total_dfields);
	CuAssertIntEquals(tc, 2, (int)smb_datblocks(smb_getmsgdatlen(msg)));
	return smb;
}

/* Reference count of each block of 'msg', as a string: "1 1"
   (freeing the last blocks of a base truncates the .sda: those are free) */
static const char* counts(CuTest* tc, smbmsg_t* msg)
{
	static char str[MAX_BLOCKS * 6];
	uint16_t    rec[MAX_BLOCKS];
	FILE*       fp;
	size_t      i;
	size_t      first = msg->hdr.offset / SDT_BLOCK_LEN;
	size_t      blocks = smb_datblocks(smb_getmsgdatlen(msg));

	CuAssertTrue(tc, blocks <= MAX_BLOCKS);
	memset(rec, 0, sizeof(rec));
	fp = fopen(TEST_SMB_PATH ".sda", "rb");
	CuAssertPtrNotNull(tc, fp);
	if (fseek(fp, (long)(first * sizeof(*rec)), SEEK_SET) == 0)
		CuAssertTrue(tc, fread(rec, sizeof(*rec), blocks, fp) <= blocks);
	fclose(fp);
	str[0] = '\0';
	for (i = 0; i < blocks; i++)
		sprintf(str + strlen(str), "%s%u", i ? " " : "", LE_INT16(rec[i]));
	return str;
}

static void delete_copy(CuTest* tc, smb_t* smb, smbmsg_t* msg)
{
	CuAssertIntEquals_Msg(tc, smb->last_error, SMB_SUCCESS, smb_freemsg_dfields(smb, msg, 1));
}

void Test_DeleteFreesEveryBlock(CuTest* tc)
{
	smbmsg_t msg;
	smb_t    smb = add_body_and_tail(tc, &msg);

	CuAssertStrEquals(tc, "1 1", counts(tc, &msg));
	delete_copy(tc, &smb, &msg);
	CuAssertStrEquals(tc, "0 0", counts(tc, &msg));
	smb_freemsgmem(&msg);
	smb_close(&smb);
}

/* Mail delivered to several local recipients shares one copy of the data */
void Test_SharedCopiesReferencedByMessage(CuTest* tc)
{
	smbmsg_t msg;
	smb_t    smb = add_body_and_tail(tc, &msg);

	CuAssertIntEquals_Msg(tc, smb.last_error, SMB_SUCCESS, smb_incmsg_dfields(&smb, &msg, 2));
	CuAssertStrEquals(tc, "3 3", counts(tc, &msg));
	delete_copy(tc, &smb, &msg);
	CuAssertStrEquals(tc, "2 2", counts(tc, &msg));
	delete_copy(tc, &smb, &msg);
	CuAssertStrEquals(tc, "1 1", counts(tc, &msg));
	delete_copy(tc, &smb, &msg);
	CuAssertStrEquals(tc, "0 0", counts(tc, &msg));
	smb_freemsgmem(&msg);
	smb_close(&smb);
}

/* smbutil pack references shared data with smb_incmsgdat() over the whole
   data length; deleting a copy must not free blocks the others still use. */
void Test_SharedCopiesReferencedByPack(CuTest* tc)
{
	smbmsg_t msg;
	smb_t    smb = add_body_and_tail(tc, &msg);

	CuAssertIntEquals_Msg(tc, smb.last_error, SMB_SUCCESS, smb_open_da(&smb));
	CuAssertIntEquals_Msg(tc, smb.last_error, SMB_SUCCESS, smb_locksmbhdr(&smb));
	CuAssertIntEquals(tc, SMB_SUCCESS, smb_incmsgdat(&smb, msg.hdr.offset, smb_getmsgdatlen(&msg), 2));
	smb_unlocksmbhdr(&smb);
	smb_close_da(&smb);
	CuAssertStrEquals(tc, "3 3", counts(tc, &msg));
	delete_copy(tc, &smb, &msg);
	CuAssertStrEquals(tc, "2 2", counts(tc, &msg));
	delete_copy(tc, &smb, &msg);
	CuAssertStrEquals(tc, "1 1", counts(tc, &msg));
	delete_copy(tc, &smb, &msg);
	CuAssertStrEquals(tc, "0 0", counts(tc, &msg));
	smb_freemsgmem(&msg);
	smb_close(&smb);
}

/* The data runs from the header's offset to the end of the furthest field */
void Test_DataLengthIsExtent(CuTest* tc)
{
	smbmsg_t msg;
	dfield_t dfield[3];

	memset(&msg, 0, sizeof(msg));
	memset(dfield, 0, sizeof(dfield));
	msg.dfield = dfield;

	dfield[0].type = TEXT_BODY;
	dfield[0].length = 202;
	dfield[1].type = TEXT_TAIL;
	dfield[1].offset = 202;
	dfield[1].length = 102;
	msg.hdr.total_dfields = 2;
	CuAssertIntEquals(tc, 304, (int)smb_getmsgdatlen(&msg));

	/* editmsg() leaves the fields it no longer uses UNUSED, empty, at 0 */
	dfield[1].type = UNUSED;
	dfield[1].offset = 0;
	dfield[1].length = 0;
	CuAssertIntEquals(tc, 202, (int)smb_getmsgdatlen(&msg));

	/* Fields out of order, or with a gap between them */
	dfield[0].type = TEXT_TAIL;
	dfield[0].offset = SDT_BLOCK_LEN;
	dfield[0].length = 10;
	dfield[1].type = TEXT_BODY;
	dfield[1].offset = 0;
	dfield[1].length = 100;
	CuAssertIntEquals(tc, SDT_BLOCK_LEN + 10, (int)smb_getmsgdatlen(&msg));
}

CuSuite* SmbAllocTestSuite(void)
{
	CuSuite* suite = CuSuiteNew();

	SUITE_ADD_TEST(suite, Test_DeleteFreesEveryBlock);
	SUITE_ADD_TEST(suite, Test_SharedCopiesReferencedByMessage);
	SUITE_ADD_TEST(suite, Test_SharedCopiesReferencedByPack);
	SUITE_ADD_TEST(suite, Test_DataLengthIsExtent);
	return suite;
}

int main(void)
{
	CuSuite*  suite = SmbAllocTestSuite();
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
