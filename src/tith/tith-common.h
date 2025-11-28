#ifndef TITH_COMMON_HEADER
#define TITH_COMMON_HEADER

#include <setjmp.h>
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
	uint8_t *value;
};

/*
 * This value should be set to the handle passed to TITH_main()
 * Used to read and write data.
 */
extern thread_local void *tith_handle;

/*
 * TITH_main() will longjmp(tith_exitJmpBuf, EXIT_FAILURE) on error, and
 * return EXIT_SUCCESS on success.  This value must be set via
 * setjmp(tith_exitJmpBuf) by the caller before calling TITH_mail()
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
 * Allocates a new root TLV that will have data added to
 */
void tith_allocTLV(int type);

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
 * Sends the current root TLV
 */
void tith_sendTLV(void);

/*
 * Verifies a TLV type and the types if the sequence of TLVs in it.
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
