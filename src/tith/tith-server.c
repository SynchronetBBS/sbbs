#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hydro/hydrogen.h"
#include "tith.h"
#include "tith-common.h"
#include "tith-config.h"
#include "tith-strings.h"

static bool
valueEquals(const struct TITH_TLV *tlv, const char *value)
{
	size_t len = strlen(value);
	return tlv->length == len && memcmp(tlv->value, value, len) == 0;
}

static char *
copyValue(const struct TITH_TLV *tlv)
{
	if (tlv->length > SIZE_MAX)
		tith_logError("TLV value is too large");
	char *ret = tith_memDup(tlv->value, (size_t)tlv->length);
	tith_pushAlloc(ret);
	return ret;
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

static void
addAcceptedPayload(struct TITH_TLV *header, uint64_t requestIdentifier,
    const uint8_t requestHash[hydro_hash_BYTES])
{
	uint8_t headerHash[hydro_hash_BYTES];
	tith_hashTLV(header, headerHash);
	struct TITH_TLV *payload = tith_addContainer(header, TITH_SignedTLV, false);
	struct TITH_TLV *signedData = tith_addContainer(payload, TITH_SignedData, true);
	struct TITH_TLV *tail = tith_addData(signedData, TITH_TLVHash,
	    sizeof(headerHash), headerHash, true);
	struct TITH_TLV *accepted = tith_addContainer(tail, TITH_Accepted, false);
	struct TITH_TLV *identifier = tith_addNumber(accepted,
	    TITH_RequestIdentifier, requestIdentifier, true);
	tith_addData(identifier, TITH_TLVHash, hydro_hash_BYTES,
	    (void *)requestHash, false);
	tith_addNullData(signedData, TITH_Signature, hydro_sign_BYTES, false);
}

static void
validateHeader(const struct TITH_BundleHeader *bundle,
    const char *expectedOrigin,
    const uint8_t expectedOriginPublicKey[hydro_sign_PUBLICKEYBYTES],
    const char *expectedDestination,
    const uint8_t expectedDestinationPublicKey[hydro_sign_PUBLICKEYBYTES])
{
	if (expectedOrigin && !valueEquals(bundle->origin, expectedOrigin))
		tith_logError("Reply Bundle has incorrect Origin");
	if (expectedOrigin)
		validateExpectedPublicKey(bundle->originPublicKey,
		    expectedOriginPublicKey,
		    "Reply Bundle has incorrect Origin PublicKey");
	if (expectedDestination &&
	    !valueEquals(bundle->destination, expectedDestination))
		tith_logError("Reply Bundle has incorrect Destination");
	if (expectedDestination)
		validateExpectedPublicKey(bundle->destinationPublicKey,
		    expectedDestinationPublicKey,
		    "Reply Bundle has incorrect Destination PublicKey");
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

static uint64_t
validatePoll(struct TITH_TLV *poll)
{
	if (poll == NULL || poll->type != TITH_PollMessages || poll->next)
		tith_logError("Expected one PollMessages request");
	tith_parseTLV(poll);
	struct TITH_TLV *identifier = poll->child;
	if (identifier == NULL || identifier->type != TITH_RequestIdentifier ||
	    identifier->next != NULL)
		tith_logError("Invalid PollMessages request");
	return tith_getNumberValue(identifier);
}

int
tith_server(void *handle)
{
	tith_handle = handle;
	if (cfg->inbound == NULL)
		tith_logError("No inbound");

	struct TITH_BundleHeader requestBundle;
	tith_readBundleHeader(&requestBundle);
	struct TITH_Node *localNode = tith_getNode(cfg,
	    requestBundle.destination);
	if (localNode == NULL || !localNode->hasSecretKey)
		tith_logError("Bundle Destination does not identify this Server");
	if (requestBundle.destinationPublicKey &&
	    memcmp(requestBundle.destinationPublicKey->value, localNode->kp.pk,
	    hydro_sign_PUBLICKEYBYTES) != 0)
		tith_logError("Bundle Destination PublicKey does not identify this Server");
	char *clientAddress = copyValue(requestBundle.origin);
	char *serverAddress = copyValue(requestBundle.destination);
	uint8_t clientPublicKey[hydro_sign_PUBLICKEYBYTES];
	const uint8_t *clientKey = NULL;
	if (requestBundle.originPublicKey) {
		memcpy(clientPublicKey, requestBundle.originPublicKey->value,
		    sizeof(clientPublicKey));
		clientKey = clientPublicKey;
	}
	const uint8_t *serverKey = requestBundle.destinationPublicKey ?
	    localNode->kp.pk : NULL;

	struct TITH_TLV *request = readPayload(requestBundle.header);
	struct TITH_TLV *payload = requestBundle.header->next;
	uint64_t requestIdentifier = validatePoll(request);
	uint8_t requestHash[hydro_hash_BYTES];
	tith_hashTLV(payload, requestHash);
	tith_freeTLV();

	struct TITH_TLV *tail = tith_allocBundleOrigin(serverAddress);
	struct TITH_TLV *header = tith_addBundleHeader(tail, clientAddress,
	    clientKey);
	addAcceptedPayload(header, requestIdentifier, requestHash);
	tith_sendTLV();
	tith_freeTLV();
	tith_logf("Accepted PollMessages request %" PRIu64, requestIdentifier);

	struct TITH_BundleHeader replyBundle;
	tith_readBundleHeader(&replyBundle);
	validateHeader(&replyBundle, clientAddress, clientKey,
	    serverAddress, serverKey);
	tith_freeTLV();

	tith_popAlloc();
	free(serverAddress);
	tith_popAlloc();
	free(clientAddress);
	return EXIT_SUCCESS;
}
