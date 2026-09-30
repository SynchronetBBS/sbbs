/* Tests for replacing a message's text in place (#1252, #1270) */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "smblib.h"
#include "xpendian.h"
#include "CuTest.h"

#define TEST_SMB_PATH   "/tmp/smbupdatetest"

static smb_t open_new_smb(CuTest* tc)
{
	smb_t smb;

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
	return smb;
}

/* A 200-byte body and a 100-byte tail: 2 data blocks */
static void add_msg(CuTest* tc, smb_t* smb, smbmsg_t* msg)
{
	char body[201];
	char tail[101];

	memset(body, 'B', sizeof(body) - 1);
	body[sizeof(body) - 1] = '\0';
	memset(tail, 'T', sizeof(tail) - 1);
	tail[sizeof(tail) - 1] = '\0';
	memset(msg, 0, sizeof(*msg));
	smb_hfield_str(msg, SENDER, "sender");
	smb_hfield_str(msg, RECIPIENT, "recipient");
	smb_hfield_str(msg, SUBJECT, "update");
	CuAssertIntEquals_Msg(tc, smb->last_error, SMB_SUCCESS
	                      , smb_addmsg(smb, msg, SMB_SELFPACK, SMB_HASH_SOURCE_NONE, XLAT_NONE
	                                   , (uchar*)body, (uchar*)tail));
}

/* Reference count of data block 'block' (blocks past the end of the .sda are free) */
static uint16_t refs(CuTest* tc, off_t block)
{
	uint16_t rec = 0;
	FILE*    fp = fopen(TEST_SMB_PATH ".sda", "rb");

	CuAssertPtrNotNull(tc, fp);
	if (fseek(fp, (long)(block * sizeof(rec)), SEEK_SET) == 0 && fread(&rec, sizeof(rec), 1, fp) != 1)
		rec = 0;
	fclose(fp);
	return LE_INT16(rec);
}

/* Re-reads the message by number, as a reader would, and checks its text */
static void check_msg(CuTest* tc, smb_t* smb, uint32_t number, const char* body, const char* tail)
{
	smbmsg_t msg;
	char*    txt;
	char     expect[256];

	memset(&msg, 0, sizeof(msg));
	msg.hdr.number = number;
	CuAssertIntEquals_Msg(tc, smb->last_error, SMB_SUCCESS, smb_getmsgidx(smb, &msg));
	CuAssertIntEquals_Msg(tc, smb->last_error, SMB_SUCCESS, smb_getmsghdr(smb, &msg));
	CuAssertIntEquals(tc, 2, msg.hdr.total_dfields);
	CuAssertIntEquals(tc, TEXT_BODY, msg.dfield[0].type);
	CuAssertIntEquals(tc, TEXT_TAIL, msg.dfield[1].type);
	CuAssertIntEquals(tc, msg.dfield[0].offset + msg.dfield[0].length, msg.dfield[1].offset);
	/* smb_getmsgtxt() returns each data field's text followed by a CRLF */
	txt = smb_getmsgtxt(smb, &msg, GETMSGTXT_BODY_ONLY);
	snprintf(expect, sizeof(expect), "%s\r\n", body);
	CuAssertStrEquals(tc, expect, txt);
	smb_freemsgtxt(txt);
	txt = smb_getmsgtxt(smb, &msg, GETMSGTXT_TAIL_ONLY);
	snprintf(expect, sizeof(expect), "%s\r\n", tail);
	CuAssertStrEquals(tc, expect, txt);
	smb_freemsgtxt(txt);
	smb_freemsgmem(&msg);
}

void Test_UpdateMsgText(CuTest* tc)
{
	smbmsg_t msg;
	smb_t    smb = open_new_smb(tc);
	off_t    old_block;

	add_msg(tc, &smb, &msg);
	old_block = msg.hdr.offset / SDT_BLOCK_LEN;
	CuAssertIntEquals_Msg(tc, smb.last_error, SMB_SUCCESS
	                      , smb_updatemsgtxt(&smb, &msg, SMB_SELFPACK, "Edited body.", "-- \r\nSig"));
	/* Written to new blocks before the old ones were freed */
	CuAssertTrue(tc, msg.hdr.offset / SDT_BLOCK_LEN != old_block);
	CuAssertIntEquals(tc, 0, refs(tc, old_block));
	CuAssertIntEquals(tc, 0, refs(tc, old_block + 1));
	CuAssertIntEquals(tc, 1, refs(tc, msg.hdr.offset / SDT_BLOCK_LEN));
	check_msg(tc, &smb, msg.hdr.number, "Edited body.", "-- \r\nSig");
	smb_freemsgmem(&msg);
	smb_close(&smb);
}

/* Mail to several recipients shares one copy of the data */
void Test_UpdateSharedMsgText(CuTest* tc)
{
	smbmsg_t msg;
	smb_t    smb = open_new_smb(tc);
	off_t    old_block;

	add_msg(tc, &smb, &msg);
	CuAssertIntEquals_Msg(tc, smb.last_error, SMB_SUCCESS, smb_incmsg_dfields(&smb, &msg, 1));
	old_block = msg.hdr.offset / SDT_BLOCK_LEN;
	CuAssertIntEquals(tc, 2, refs(tc, old_block));
	CuAssertIntEquals_Msg(tc, smb.last_error, SMB_SUCCESS
	                      , smb_updatemsgtxt(&smb, &msg, SMB_SELFPACK, "Edited body.", "-- \r\nSig"));
	/* The other copy still uses the original data */
	CuAssertIntEquals(tc, 1, refs(tc, old_block));
	CuAssertIntEquals(tc, 1, refs(tc, old_block + 1));
	CuAssertIntEquals(tc, 1, refs(tc, msg.hdr.offset / SDT_BLOCK_LEN));
	check_msg(tc, &smb, msg.hdr.number, "Edited body.", "-- \r\nSig");
	smb_freemsgmem(&msg);
	smb_close(&smb);
}

CuSuite* SmbUpdateTestSuite(void)
{
	CuSuite* suite = CuSuiteNew();

	SUITE_ADD_TEST(suite, Test_UpdateMsgText);
	SUITE_ADD_TEST(suite, Test_UpdateSharedMsgText);
	return suite;
}

int main(void)
{
	CuSuite*  suite = SmbUpdateTestSuite();
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
