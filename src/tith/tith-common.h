#ifndef TITH_COMMON_HEADER
#define TITH_COMMON_HEADER

#include <setjmp.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdnoreturn.h>
#include <threads.h>

#include "hydro/hydrogen.h"
#include "tith.h"

#define TITH_OPTIONAL 0
#define TITH_REQUIRED 1

struct TITH_TLV {
	struct TITH_TLV *first;  // The first TLV in this list
	struct TITH_TLV *next;   // The TLV after this one in a sequence
	struct TITH_TLV *child;  // The first child TLV
	struct TITH_TLV *parent;
	char *fileName;          // The name of a file that holds the value
	uint64_t length;
	int type;
	bool parsed;             // Indicates the value has been parsed as a TLV sequence
	bool added;              // Indicates the value was added by the program, not read from somewhere else
	bool signaturePrepared;  // Indicates that an added SignedData has been signed
	uint8_t *value;
};

struct TITH_BundleHeader {
	struct TITH_TLV *origin;
	const struct TITH_TLV *originPublicKey;
	struct TITH_TLV *header;
	struct TITH_TLV *destination;
	const struct TITH_TLV *destinationPublicKey;
	struct TITH_TLV *timestamp;
};

/*
 * The handle for the TITH_main() invocation active on this thread.  It is used
 * to read and write data and is set by TITH_main().
 */
extern thread_local void *tith_handle;

/*
 * TITH code will longjmp(tith_exitJmpBuf, EXIT_FAILURE) on error.  This value
 * must be set via setjmp(tith_exitJmpBuf) by an entry point before calling
 * routines which can call tith_logError().
 */
extern thread_local jmp_buf tith_exitJmpBuf;

/*
 * This is the current working root TLV.
 */
extern thread_local struct TITH_TLV *tith_TLV;

/*
 * Gets the root TLV
 */
void tith_getTLV(void);

/*
 * Reads a sibling of tlv from the current handle.  If required is false and
 * the next type does not match, its type is retained for the next call.
 */
struct TITH_TLV *tith_getNextTLV(struct TITH_TLV *tlv, int type, bool required);

/*
 * Frees the root TLV
 */
void tith_freeTLV(void);

/*
 * Parses the value of tlv as a sequence of TLVs, puts the resulting
 * chain in tlv->child, linked by tlv->next
 */
void tith_parseTLV(struct TITH_TLV *tlv);

/*
 * Logs an error and exits
 */
noreturn void tith_logError(const char *str);

/*
 * Must be called before returning from TITH_main(), frees all remaining
 * allocations
 */
void tith_cleanup(void);

/*
 * Basically a clone of the POSIX strdup()
 */
char *tith_strDup(const char *str);

/*
 * Pops the last allocation off the allocation stack, this should be called
 * whenever an allocation is free()d or returned.
 */
void *tith_popAlloc(void);

/*
 * Pushes an allocation result onto the allocation stack.  Will call
 * tith_logError() if a NULL is pushed.
 */
void tith_pushAlloc(void *ptr);

/*
 * As above but with file pointers
 */
void tith_pushFile(FILE *file);
FILE *tith_popFile(void);


/*
 * Validates that addr conforms to TTS-0004
 */
void tith_validateAddress(const char *addr);

/*
 * Validates an address TLV and the conditional PublicKey immediately after it.
 */
void tith_validateAddressValue(const struct TITH_TLV *address);
bool tith_isUnlistedAddressString(const char *address);
bool tith_isUnlistedAddress(const struct TITH_TLV *address);
const struct TITH_TLV *tith_getAddressPublicKey(const struct TITH_TLV *address);

/*
 * Constructs and reads the fixed portion of a Bundle.
 */
struct TITH_TLV *tith_allocBundleOrigin(const char *address);
struct TITH_TLV *tith_addBundleHeader(struct TITH_TLV *tail,
    const char *destination,
    const uint8_t destinationPublicKey[hydro_sign_PUBLICKEYBYTES]);
void tith_readBundleHeader(struct TITH_BundleHeader *bundle);

/*
 * Allocates a new root TLV that will have data added to
 */
void tith_allocTLV(int type);

/*
 * Allocates a root TLV containing a copy of data.
 */
void tith_allocDataTLV(int type, uint64_t len, const void *data);

/*
 * Copies len bytes from data into a newly allocated tith_TLV * of type
 * type which becomes either the last TLV in the next chain of tlv or
 * the child depending on the value of child.
 */
struct TITH_TLV *tith_addData(struct TITH_TLV *tlv, int type, uint64_t len, void *data, bool child);

/*
 * As above but sets the data to NULs
 */
struct TITH_TLV *tith_addNullData(struct TITH_TLV *tlv, int type, uint64_t len, bool child);

/*
 * As above, but adds the data in filename as the value
 * The file must not be deleted or modified from when this is called to
 * when tith_freeTLV() is called.
 */
struct TITH_TLV *tith_addFile(struct TITH_TLV *tlv, int type, const char *filename, bool child);

/*
 * As above, but this is a container for adding new TLVs to
 */
struct TITH_TLV *tith_addContainer(struct TITH_TLV *tlv, int type, bool child);

/*
 * Adds a TTS-0002 encoded number.
 */
struct TITH_TLV *tith_addNumber(struct TITH_TLV *tlv, int type, uint64_t value, bool child);

/*
 * Adds a TTS-0007 encoded signed number.
 */
struct TITH_TLV *tith_addSignedNumber(struct TITH_TLV *tlv, int type, int64_t value, bool child);

/*
 * Decodes a TTS-0002 number that occupies the complete Value of tlv.
 */
uint64_t tith_getNumberValue(const struct TITH_TLV *tlv);

/*
 * Decodes a TTS-0007 signed number that occupies the complete Value of tlv.
 */
int64_t tith_getSignedNumberValue(const struct TITH_TLV *tlv);

/*
 * Signs an added SignedTLV and hashes one complete encoded TLV.
 */
void tith_prepareSignedTLV(struct TITH_TLV *tlv);
void tith_hashTLV(const struct TITH_TLV *tlv, uint8_t hash[hydro_hash_BYTES]);

/*
 * Prepares and verifies the bare Signatures used by Message and File.
 */
void tith_prepareItemSignature(struct TITH_TLV *tlv);
void tith_verifyItemSignatures(struct TITH_TLV *tlv);
void tith_getItemIdentity(struct TITH_TLV *tlv,
    uint8_t identity[hydro_hash_BYTES]);

/*
 * Adds the mixed TLV/string Value defined for Via by TTS-0005.
 * publicKey is required exactly when address is unlisted.
 */
struct TITH_TLV *tith_addVia(struct TITH_TLV *tlv, const char *address,
    const uint8_t publicKey[hydro_sign_PUBLICKEYBYTES], uint64_t timestamp,
    const char *program, bool child);

/*
 * Sends the current root TLV
 */
void tith_sendTLV(void);

/*
 * Verifies a TLV type and the types in the sequence of TLVs in it.
 * numargs is the number of required/type pairs that follow, each pair
 * starts with either TITH_OPTIONAL or TITH_REQUIRED indicating that the
 * type is required or optional, then a type.
 */
void tith_validateTLV(struct TITH_TLV *tlv, int command, int numargs, ...);

/*
 * Calls vsnprintf() with the given format and args, then passes the
 * result (if any) to logString()
 */
void tith_logf(const char *format, ...);

#endif
