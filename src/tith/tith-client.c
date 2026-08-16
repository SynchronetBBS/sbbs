#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>

#include "base64.h"
#include "hydro/hydrogen.h"
#include "tith.h"
#include "tith-common.h"
#include "tith-config.h"
#include "tith-interface.h"
#include "tith-strings.h"

static bool
valueEquals(const struct TITH_TLV *tlv, const char *value)
{
	size_t len = strlen(value);
	return tlv->length == len && memcmp(tlv->value, value, len) == 0;
}

static struct TITH_TLV *
addPollPayload(struct TITH_TLV *header, uint64_t requestIdentifier,
    uint8_t payloadHash[hydro_hash_BYTES])
{
	uint8_t headerHash[hydro_hash_BYTES];
	tith_hashTLV(header, headerHash);
	struct TITH_TLV *payload = tith_addContainer(header, TITH_SignedTLV, false);
	struct TITH_TLV *signedData = tith_addContainer(payload, TITH_SignedData, true);
	struct TITH_TLV *tail = tith_addData(signedData, TITH_TLVHash,
	    sizeof(headerHash), headerHash, true);
	struct TITH_TLV *poll = tith_addContainer(tail, TITH_PollMessages, false);
	tith_addNumber(poll, TITH_RequestIdentifier, requestIdentifier, true);
	tith_addNullData(signedData, TITH_Signature, hydro_sign_BYTES, false);
	tith_prepareSignedTLV(payload);
	tith_hashTLV(payload, payloadHash);
	return payload;
}

static void
validateExpectedPublicKey(const struct TITH_TLV *publicKey,
    const uint8_t expected[hydro_sign_PUBLICKEYBYTES], const char *error)
{
	if (expected == NULL) {
		if (publicKey)
			tith_logError(error);
		return;
	}
	if (publicKey == NULL || memcmp(publicKey->value, expected,
	    hydro_sign_PUBLICKEYBYTES) != 0)
		tith_logError(error);
}

static struct TITH_TLV *
readHeader(const char *expectedOrigin,
    const uint8_t expectedOriginPublicKey[hydro_sign_PUBLICKEYBYTES],
    const char *expectedDestination,
    const uint8_t expectedDestinationPublicKey[hydro_sign_PUBLICKEYBYTES])
{
	struct TITH_BundleHeader bundle;
	tith_readBundleHeader(&bundle);
	if (!valueEquals(bundle.origin, expectedOrigin))
		tith_logError("Reply Bundle has incorrect Origin");
	validateExpectedPublicKey(bundle.originPublicKey,
	    expectedOriginPublicKey, "Reply Bundle has incorrect Origin PublicKey");
	if (!valueEquals(bundle.destination, expectedDestination))
		tith_logError("Reply Bundle has incorrect Destination");
	validateExpectedPublicKey(bundle.destinationPublicKey,
	    expectedDestinationPublicKey,
	    "Reply Bundle has incorrect Destination PublicKey");
	return bundle.header;
}

static struct TITH_TLV *
readPayload(struct TITH_TLV *header)
{
	struct TITH_TLV *payload = tith_getNextTLV(header, TITH_SignedTLV, true);
	tith_parseTLV(payload);
	struct TITH_TLV *signedData = payload->child;
	if (signedData == NULL || signedData->type != TITH_SignedData ||
	    signedData->next == NULL || signedData->next->type != TITH_Signature ||
	    signedData->next->next != NULL)
		tith_logError("Invalid payload SignedTLV");
	tith_parseTLV(signedData);
	struct TITH_TLV *headerHash = signedData->child;
	if (headerHash == NULL || headerHash->type != TITH_TLVHash ||
	    headerHash->length != hydro_hash_BYTES)
		tith_logError("Payload does not begin with a Header TLVHash");
	uint8_t expected[hydro_hash_BYTES];
	tith_hashTLV(header, expected);
	if (memcmp(headerHash->value, expected, sizeof(expected)) != 0)
		tith_logError("Payload Header TLVHash does not match");
	tith_verifyItemSignatures(headerHash->next);
	return headerHash->next;
}

static void
validateAccepted(struct TITH_TLV *accepted, uint64_t requestIdentifier,
    const uint8_t payloadHash[hydro_hash_BYTES])
{
	if (accepted == NULL || accepted->type != TITH_Accepted || accepted->next)
		tith_logError("Expected one Accepted response");
	tith_parseTLV(accepted);
	struct TITH_TLV *identifier = accepted->child;
	if (identifier == NULL || identifier->type != TITH_RequestIdentifier ||
	    identifier->next == NULL || identifier->next->type != TITH_TLVHash ||
	    identifier->next->length != hydro_hash_BYTES ||
	    identifier->next->next != NULL)
		tith_logError("Invalid Accepted response");
	if (tith_getNumberValue(identifier) != requestIdentifier)
		tith_logError("Accepted has incorrect RequestIdentifier");
	if (memcmp(identifier->next->value, payloadHash, hydro_hash_BYTES) != 0)
		tith_logError("Accepted has incorrect TLVHash");
}

static void
sendEmptyReply(const char *source, const uint8_t *sourcePublicKey,
    const char *destination, const uint8_t *destinationPublicKey)
{
	struct TITH_TLV *tail = tith_allocBundleOrigin(source);
	if (sourcePublicKey && memcmp(tith_TLV->next->value, sourcePublicKey,
	    hydro_sign_PUBLICKEYBYTES) != 0)
		tith_logError("Configured source PublicKey changed");
	tith_addBundleHeader(tail, destination, destinationPublicKey);
	tith_sendTLV();
	tith_freeTLV();
	shutdownWrite(tith_handle);
}

int
tith_client(int argc, char **argv, void *handle)
{
	tith_handle = handle;
	if (argc < 1)
		tith_logError("No destination");
	if (argc < 2)
		tith_logError("No source address");
	if (argc > 3)
		tith_logError("Too many client arguments");
	char *dest = argv[0];
	char *source = argv[1];
	tith_validateAddress(dest);
	tith_validateAddress(source);
	if (cfg->outbound == NULL)
		tith_logError("No outbound");

	uint8_t destinationPublicKey[hydro_sign_PUBLICKEYBYTES];
	const uint8_t *destinationKey = NULL;
	if (tith_isUnlistedAddressString(dest)) {
		if (argc != 3)
			tith_logError("No PublicKey for unlisted destination");
		if (strlen(argv[2]) != 43 || !b64_decode(destinationPublicKey,
		    sizeof(destinationPublicKey), argv[2]))
			tith_logError("Invalid destination PublicKey");
		destinationKey = destinationPublicKey;
	}
	else if (argc == 3)
		tith_logError("PublicKey supplied for listed destination");

	const uint64_t requestIdentifier = 0;
	uint8_t payloadHash[hydro_hash_BYTES];
	uint8_t sourcePublicKey[hydro_sign_PUBLICKEYBYTES];
	const uint8_t *sourceKey = NULL;
	struct TITH_TLV *tail = tith_allocBundleOrigin(source);
	if (tith_isUnlistedAddress(tith_TLV)) {
		memcpy(sourcePublicKey, tith_TLV->next->value,
		    sizeof(sourcePublicKey));
		sourceKey = sourcePublicKey;
	}
	struct TITH_TLV *header = tith_addBundleHeader(tail, dest,
	    destinationKey);
	addPollPayload(header, requestIdentifier, payloadHash);
	tith_sendTLV();
	tith_freeTLV();

	header = readHeader(dest, destinationKey, source, sourceKey);
	struct TITH_TLV *response = readPayload(header);
	validateAccepted(response, requestIdentifier, payloadHash);
	tith_logf("PollMessages request %" PRIu64 " accepted", requestIdentifier);
	tith_freeTLV();

	sendEmptyReply(source, sourceKey, dest, destinationKey);
	return EXIT_SUCCESS;
}
