/* sntrup761 - Streamlined NTRU Prime 761
 * Public Domain, Authors:
 *   Daniel J. Bernstein, Chitchanok Chuengsatiansup,
 *   Tanja Lange, Christine van Vredendaal
 *
 * Adapted from OpenSSH sntrup761.c (SUPERCOP reference implementation).
 * Random source changed from arc4random_buf to dssh_random/dssh_hash
 * (backend-neutral via deucessh-crypto.h).
 */

#include <stdint.h>
#include <string.h>

#include "deucessh-crypto.h"
#include "deucessh.h"
#include "sntrup761.h"
#ifdef DSSH_TESTING
 #include "ssh-internal.h"
 #include "test/dssh_test_ossl.h"
#endif

static void
sntrup_clear(void *buffer, size_t length)
{
	dssh_secure_zero(buffer, length);
#ifdef DSSH_TESTING
	/* Observe only while the cleared owner is still alive. */
	dssh_test_sntrup_cleared(buffer, length);
#endif
}

/* Reconstruct signed values numerically, without an out-of-range conversion.
 * The low part is0..INT32_MAX and the sign is0/1, so the sum fits int32_t. */
static inline int32_t
sntrup_signed32(uint32_t bits)
{
	uint32_t low = bits & INT32_MAX;
	uint32_t sign = bits >> 31;
	int32_t low_i32 = (int32_t)low;
	int32_t sign_i32 = (int32_t)sign;
	int32_t result = (int32_t)(low_i32 + INT32_MIN * sign_i32);
	return result;
}

/* Defined floor division by a power of two. All production counts are public
 * constants below32; the wider-count arm also avoids an invalid shift. */
static inline int32_t
sntrup_floor32(int32_t value, unsigned int shift)
{
	uint32_t bits = (uint32_t)value;
	uint32_t sign = bits >> 31;
	if (shift >= 32) {
		int32_t sign_i32 = (int32_t)sign;
		return -sign_i32;
	}
	uint32_t fill = UINT32_MAX ^ (UINT32_MAX >> shift);
	uint32_t result = (bits >> shift) | (fill * sign);
	return sntrup_signed32(result);
}

#define crypto_declassify(x, y) \
	do {                    \
	} while (0)

static int
randombytes(void *buf, long long len)
{
	return dssh_random(buf, (size_t)len) == 0 ? 0 : -1;
}

static int
crypto_hash_sha512(unsigned char *out, const unsigned char *in, unsigned long long inlen)
{
	return dssh_hash_oneshot("SHA-512", in, (size_t)inlen, out, 64) < 0 ? -1 : 0;
}

#define int8   crypto_int8
#define uint8  crypto_uint8
#define int16  crypto_int16
#define uint16 crypto_uint16
#define int32  crypto_int32
#define uint32 crypto_uint32
#define int64  crypto_int64
#define uint64 crypto_uint64
extern volatile crypto_int16 crypto_int16_optblocker;
extern volatile crypto_int32 crypto_int32_optblocker;
extern volatile crypto_int64 crypto_int64_optblocker;

/* Subset of supercop-20240808/cryptoint — only functions called by sntrup761.
 * Full auto-generated versions in SUPERCOP's cryptoint/{crypto_int16,
 * crypto_int32, crypto_int64}.h; unused helpers stripped.
 *
 * Kept (directly called):
 *   crypto_int16_negative_mask, crypto_int16_nonzero_mask,
 *   crypto_int32_negative_mask, crypto_int32_minmax,
 *   crypto_int64_bottombit_01, crypto_int64_bitmod_01
 * Kept (transitive dependencies, generic-C path only):
 *   crypto_int64_bottombit_mask, crypto_int64_bitinrangepublicpos_mask,
 *   crypto_int64_shrmod
 */

/* Portable unused-function suppressor for helpers only reached via
 * generic-C fallback paths (dead code on x86_64 and aarch64). */
#if defined(__GNUC__) || defined(__clang__)
 #define CRYPTOINT_MAYBE_UNUSED __attribute__((unused))
#elif defined(_MSC_VER)
 #define CRYPTOINT_MAYBE_UNUSED __pragma(warning(suppress: 4505))
#else
 #define CRYPTOINT_MAYBE_UNUSED
#endif

/* --- crypto_int16 --- */

#define crypto_int16          int16_t
#define crypto_int16_unsigned uint16_t

static inline crypto_int16
crypto_int16_negative_mask(crypto_int16 crypto_int16_x)
{
 #if !defined(DSSH_TEST_SNTRUP_GENERIC) && defined(__GNUC__) && defined(__x86_64__)
	__asm__("sarw $15,%0" : "+r"(crypto_int16_x) : : "cc");
	return crypto_int16_x;
 #elif !defined(DSSH_TEST_SNTRUP_GENERIC) && defined(__GNUC__) && defined(__aarch64__)
	crypto_int16 crypto_int16_y;
	__asm__("sbfx %w0,%w1,15,1" : "=r"(crypto_int16_y) : "r"(crypto_int16_x) :);
	return crypto_int16_y;
 #else
	uint16_t bits = (uint16_t)crypto_int16_x;
	uint16_t blocker = (uint16_t)crypto_int16_optblocker;
	bits ^= blocker;
	uint16_t sign = bits >> 15;
	int16_t result = (int16_t)sign; /* sign is0/1. */
	return -result;
 #endif
}

static inline crypto_int16
crypto_int16_nonzero_mask(crypto_int16 crypto_int16_x)
{
 #if !defined(DSSH_TEST_SNTRUP_GENERIC) && defined(__GNUC__) && defined(__x86_64__)
	crypto_int16 crypto_int16_q, crypto_int16_z;
	__asm__("xorw %0,%0\n movw $-1,%1\n testw %2,%2\n cmovnew %1,%0"
	    : "=&r"(crypto_int16_z), "=&r"(crypto_int16_q)
	    : "r"(crypto_int16_x)
	    : "cc");
	return crypto_int16_z;
 #elif !defined(DSSH_TEST_SNTRUP_GENERIC) && defined(__GNUC__) && defined(__aarch64__)
	crypto_int16 crypto_int16_z;
	__asm__("tst %w1,65535\n csetm %w0,ne" : "=r"(crypto_int16_z) : "r"(crypto_int16_x) : "cc");
	return crypto_int16_z;
 #else
	uint16_t bits = (uint16_t)crypto_int16_x;
	uint32_t extended = bits;
	/* The sum is65535..131070, even when int is only16 bits. */
	uint32_t nonzero = (extended + UINT32_C(65535)) >> 16;
	int16_t result = (int16_t)nonzero; /* nonzero is0/1. */
	return -result;
 #endif
}

/* --- crypto_int32 --- */

#define crypto_int32          int32_t
#define crypto_int32_unsigned uint32_t

CRYPTOINT_MAYBE_UNUSED static inline crypto_int32
crypto_int32_negative_mask(crypto_int32 crypto_int32_x)
{
 #if !defined(DSSH_TEST_SNTRUP_GENERIC) && defined(__GNUC__) && defined(__x86_64__)
	__asm__("sarl $31,%0" : "+r"(crypto_int32_x) : : "cc");
	return crypto_int32_x;
 #elif !defined(DSSH_TEST_SNTRUP_GENERIC) && defined(__GNUC__) && defined(__aarch64__)
	crypto_int32 crypto_int32_y;
	__asm__("asr %w0,%w1,31" : "=r"(crypto_int32_y) : "r"(crypto_int32_x) :);
	return crypto_int32_y;
 #else
	uint32_t bits = (uint32_t)crypto_int32_x;
	uint32_t blocker = (uint32_t)crypto_int32_optblocker;
	bits ^= blocker;
	uint32_t sign = bits >> 31;
	int32_t result = (int32_t)sign; /* sign is0/1. */
	return -result;
 #endif
}

static inline void
crypto_int32_minmax(crypto_int32 *crypto_int32_p, crypto_int32 *crypto_int32_q)
{
	crypto_int32 crypto_int32_x = *crypto_int32_p;
	crypto_int32 crypto_int32_y = *crypto_int32_q;
 #if !defined(DSSH_TEST_SNTRUP_GENERIC) && defined(__GNUC__) && defined(__x86_64__)
	crypto_int32 crypto_int32_z;
	__asm__("cmpl %2,%1\n movl %1,%0\n cmovgl %2,%1\n cmovgl %0,%2"
	    : "=&r"(crypto_int32_z), "+&r"(crypto_int32_x), "+r"(crypto_int32_y)
	    :
	    : "cc");
	*crypto_int32_p = crypto_int32_x;
	*crypto_int32_q = crypto_int32_y;
 #elif !defined(DSSH_TEST_SNTRUP_GENERIC) && defined(__GNUC__) && defined(__aarch64__)
	crypto_int32 crypto_int32_r, crypto_int32_s;
	__asm__("cmp %w2,%w3\n csel %w0,%w2,%w3,lt\n csel %w1,%w3,%w2,lt"
	    : "=&r"(crypto_int32_r), "=r"(crypto_int32_s)
	    : "r"(crypto_int32_x), "r"(crypto_int32_y)
	    : "cc");
	*crypto_int32_p = crypto_int32_r;
	*crypto_int32_q = crypto_int32_s;
 #else
	crypto_int64 crypto_int32_r = (crypto_int64)crypto_int32_y ^ (crypto_int64)crypto_int32_x;
	crypto_int64 crypto_int32_z = (crypto_int64)crypto_int32_y - (crypto_int64)crypto_int32_x;
	crypto_int32_z ^= crypto_int32_r & (crypto_int32_z ^ crypto_int32_y);
	/* Same signs give a32-bit difference. Opposite signs copy y's sign
	 * and upper bits, so the adjusted comparison also fits int32_t. */
	int32_t comparison = (int32_t)crypto_int32_z;
	crypto_int32_z = crypto_int32_negative_mask(comparison);
	crypto_int32_z &= crypto_int32_r;
	/* The0/-1 selection returns one of the original32-bit inputs. */
	int64_t low = (int64_t)crypto_int32_x ^ crypto_int32_z;
	int64_t high = (int64_t)crypto_int32_y ^ crypto_int32_z;
	int32_t low_i32 = (int32_t)low;
	int32_t high_i32 = (int32_t)high;
	*crypto_int32_p = low_i32;
	*crypto_int32_q = high_i32;
 #endif
}

/* --- crypto_int64 --- */

#define crypto_int64          int64_t
#define crypto_int64_unsigned uint64_t

CRYPTOINT_MAYBE_UNUSED static inline int64_t
sntrup_signed64(uint64_t bits)
{
	uint64_t low = bits & INT64_MAX;
	uint64_t sign = bits >> 63;
	int64_t low_i64 = (int64_t)low;
	int64_t sign_i64 = (int64_t)sign;
	/* Each conversion is in range; the conditional low+MIN sum also fits. */
	return low_i64 + INT64_MIN * sign_i64;
}

CRYPTOINT_MAYBE_UNUSED static inline crypto_int64
crypto_int64_bottombit_mask(crypto_int64 crypto_int64_x)
{
 #if !defined(DSSH_TEST_SNTRUP_GENERIC) && defined(__GNUC__) && defined(__x86_64__)
	__asm__("andq $1,%0" : "+r"(crypto_int64_x) : : "cc");
	return -crypto_int64_x;
 #elif !defined(DSSH_TEST_SNTRUP_GENERIC) && defined(__GNUC__) && defined(__aarch64__)
	crypto_int64 crypto_int64_y;
	__asm__("sbfx %0,%1,0,1" : "=r"(crypto_int64_y) : "r"(crypto_int64_x) :);
	return crypto_int64_y;
 #else
	crypto_int64_x &= 1 ^ crypto_int64_optblocker;
	return -crypto_int64_x;
 #endif
}

static inline crypto_int64
crypto_int64_bottombit_01(crypto_int64 crypto_int64_x)
{
 #if !defined(DSSH_TEST_SNTRUP_GENERIC) && defined(__GNUC__) && defined(__x86_64__)
	__asm__("andq $1,%0" : "+r"(crypto_int64_x) : : "cc");
	return crypto_int64_x;
 #elif !defined(DSSH_TEST_SNTRUP_GENERIC) && defined(__GNUC__) && defined(__aarch64__)
	crypto_int64 crypto_int64_y;
	__asm__("ubfx %0,%1,0,1" : "=r"(crypto_int64_y) : "r"(crypto_int64_x) :);
	return crypto_int64_y;
 #else
	crypto_int64_x &= 1 ^ crypto_int64_optblocker;
	return crypto_int64_x;
 #endif
}

CRYPTOINT_MAYBE_UNUSED static inline crypto_int64
crypto_int64_bitinrangepublicpos_mask(crypto_int64 crypto_int64_x, crypto_int64 crypto_int64_s)
{
 #if !defined(DSSH_TEST_SNTRUP_GENERIC) && defined(__GNUC__) && defined(__x86_64__)
	__asm__("sarq %%cl,%0" : "+r"(crypto_int64_x) : "c"(crypto_int64_s) : "cc");
 #elif !defined(DSSH_TEST_SNTRUP_GENERIC) && defined(__GNUC__) && defined(__aarch64__)
	__asm__("asr %0,%0,%1" : "+r"(crypto_int64_x) : "r"(crypto_int64_s) :);
 #else
	uint64_t bits = (uint64_t)crypto_int64_x;
	uint64_t positions = (uint64_t)crypto_int64_s;
	unsigned int position = (unsigned int)(positions & UINT64_C(63));
	uint64_t bit = (bits >> position) & UINT64_C(1);
	int64_t selected = (int64_t)bit; /* bit is0/1. */
	crypto_int64_x = selected;
 #endif
	return crypto_int64_bottombit_mask(crypto_int64_x);
}

static inline crypto_int64
crypto_int64_shrmod(crypto_int64 crypto_int64_x, crypto_int64 crypto_int64_s)
{
 #if !defined(DSSH_TEST_SNTRUP_GENERIC) && defined(__GNUC__) && defined(__x86_64__)
	__asm__("sarq %%cl,%0" : "+r"(crypto_int64_x) : "c"(crypto_int64_s) : "cc");
 #elif !defined(DSSH_TEST_SNTRUP_GENERIC) && defined(__GNUC__) && defined(__aarch64__)
	__asm__("asr %0,%0,%1" : "+r"(crypto_int64_x) : "r"(crypto_int64_s) :);
 #else
	int crypto_int64_k, crypto_int64_l;
	uint64_t bits = (uint64_t)crypto_int64_x;
	for (crypto_int64_l = 0, crypto_int64_k = 1; crypto_int64_k < 64; ++crypto_int64_l, crypto_int64_k *= 2) {
		uint64_t fill = UINT64_MAX ^ (UINT64_MAX >> crypto_int64_k);
		uint64_t shifted = (bits >> crypto_int64_k) | (fill * (bits >> 63));
		int64_t selected = crypto_int64_bitinrangepublicpos_mask(crypto_int64_s, crypto_int64_l);
		uint64_t mask = (uint64_t)selected;
		bits ^= (bits ^ shifted) & mask;
	}
	crypto_int64_x = sntrup_signed64(bits);
 #endif
	return crypto_int64_x;
}

static inline crypto_int64
crypto_int64_bitmod_01(crypto_int64 crypto_int64_x, crypto_int64 crypto_int64_s)
{
	crypto_int64_x = crypto_int64_shrmod(crypto_int64_x, crypto_int64_s);
	return crypto_int64_bottombit_01(crypto_int64_x);
}


/* from supercop-20240808/crypto_sort/int32/portable4/sort.c */
#define int32_MINMAX(a, b) crypto_int32_minmax(&a, &b)

static void
crypto_sort_int32(void *array, long long n)
{
	long long top, p, q, r, i, j;
	int32    *x = array;

	if (n < 2)
		return;
	top = 1;
	while (top < n - top)
		top += top;

	for (p = top; p >= 1; p >>= 1) {
		i = 0;
		while (i + 2 * p <= n) {
			for (j = i; j < i + p; ++j)
				int32_MINMAX(x[j], x[j + p]);
			i += 2 * p;
		}
		for (j = i; j < n - p; ++j)
			int32_MINMAX(x[j], x[j + p]);

		i = 0;
		j = 0;
		for (q = top; q > p; q >>= 1) {
			if (j != i)
				for (;;) {
					if (j == n - q)
						goto done;
					int32 a = x[j + p];
					for (r = q; r > p; r >>= 1)
						int32_MINMAX(a, x[j + r]);
					x[j + p] = a;
					++j;
					if (j == i + p) {
						i += 2 * p;
						break;
					}
				}
			while (i + p <= n - q) {
				for (j = i; j < i + p; ++j) {
					int32 a = x[j + p];
					for (r = q; r > p; r >>= 1)
						int32_MINMAX(a, x[j + r]);
					x[j + p] = a;
				}
				i += 2 * p;
			}
			/* now i + p > n - q */
			j = i;
			while (j < n - q) {
				int32 a = x[j + p];
				for (r = q; r > p; r >>= 1)
					int32_MINMAX(a, x[j + r]);
				x[j + p] = a;
				++j;
			}

done:;
		}
	}
}

/* from supercop-20240808/crypto_sort/uint32/useint32/sort.c */

/* can save time by vectorizing xor loops */
/* can save time by integrating xor loops with int32_sort */

static void
crypto_sort_uint32(void *array, long long n)
{
	crypto_uint32 *x = array;
	long long      j;
	for (j = 0; j < n; ++j)
		x[j] ^= 0x80000000;
	crypto_sort_int32(array, n);
	for (j = 0; j < n; ++j)
		x[j] ^= 0x80000000;
}

/* from supercop-20240808/crypto_kem/sntrup761/compact/kem.c */
// 20240806 djb: some automated conversion to cryptoint

#define p   761
#define q   4591
#define w   286
#define q12 ((q - 1) / 2)
typedef int8_t  small;
typedef int16_t Fq;
#define Hash_bytes  32
#define Small_bytes ((p + 3) / 4)
typedef small Inputs[p];
#define SecretKeys_bytes (2 * Small_bytes)
#define Confirm_bytes    32

static small
F3_freeze(int16_t x)
{
	/* The full signed16 product/sum fits signed32; the result is-2..2
	 * and is canonical-1..1 throughout the actual caller interval. */
	int32_t wide = x;
	int32_t product = (int32_t)(INT32_C(10923) * wide + INT32_C(16384));
	int32_t quotient = sntrup_floor32(product, 15);
	int32_t reduced = (int32_t)(wide - INT32_C(3) * quotient);
	small result = (small)reduced;
	return result;
}

static Fq
Fq_freeze(int32_t x)
{
	const int32_t q16 = (0x10000 + q / 2) / q;
	const int32_t q20 = (0x100000 + q / 2) / q;
	const int32_t q28 = (0x10000000 + q / 2) / q;
	/* Callers bound |x| by2*2295*2295. Every staged product and sum
	 * below fits signed32; promote before arithmetic on16-bit int. */
	int32_t modulus = q;
	int32_t product = (int32_t)(q16 * x);
	int32_t quotient = sntrup_floor32(product, 16);
	x -= modulus * quotient;
	product = (int32_t)(q20 * x);
	quotient = sntrup_floor32(product, 20);
	x -= modulus * quotient;
	product = (int32_t)(q28 * x + INT32_C(0x8000000));
	quotient = sntrup_floor32(product, 28);
	x -= modulus * quotient;
	/* The centered result is-2295..2295. */
	Fq result = (Fq)x;
	return result;
}

static int
Weightw_mask(small *r)
{
	int i, weight = 0;
	for (i = 0; i < p; ++i)
		weight += (int)crypto_int64_bottombit_01(r[i]);
	int16_t difference = (int16_t)(weight - w); /*-286..475. */
	return crypto_int16_nonzero_mask(difference);
}

static void
uint32_divmod_uint14(uint32_t *Q, uint16_t *r, uint32_t x, uint16_t m)
{
	/* Only public-key/ciphertext Decode calls this helper. Its public
	 * modulus is1..16383. Each reciprocal quotient underestimates x/m,
	 * so products/subtractions and the accumulated quotient stay in range.
	 * The first remainder is below3*m; the second is below2*m. */
	uint32_t qpart, v = UINT32_C(0x80000000) / m;
	qpart = (uint32_t)((x * (uint64_t)v) >> 31);
	x -= qpart * m;
	*Q    = qpart;
	qpart = (uint32_t)((x * (uint64_t)v) >> 31);
	x -= qpart * m;
	*Q += qpart;
	uint32_t correction = x >= m ? 1 : 0;
	x -= correction * m;
	*Q += correction;
	/* After correction0<=x<m<16384, so the result fits uint16_t. */
	uint16_t remainder = (uint16_t)x;
	*r = remainder;
}

static uint16_t
uint32_mod_uint14(uint32_t x, uint16_t m)
{
	uint32_t Q;
	uint16_t r;
	uint32_divmod_uint14(&Q, &r, x, m);
	return r;
}

static void
Encode(unsigned char *out, const uint16_t *R, const uint16_t *M, long long len)
{
	if (len == 1) {
		uint16_t r = R[0], m = M[0];
		while (m > 1) {
			*out++ = (unsigned char)r;
			r >>= 8;
			m = (uint16_t)((m + 255) >> 8);
		}
	}
	if (len > 1) {
		uint16_t  R2[(p + 1) / 2], M2[(p + 1) / 2];
		long long i;
		for (i = 0; i < len - 1; i += 2) {
			uint32_t m0 = M[i];
			uint32_t r  = R[i] + R[i + 1] * m0;
			uint32_t m  = M[i + 1] * m0;
			while (m >= 16384) {
				*out++ = (unsigned char)r;
				r >>= 8;
				m = (m + 255) >> 8;
			}
			R2[i / 2] = (uint16_t)r;
			M2[i / 2] = (uint16_t)m;
		}
		if (i < len) {
			R2[i / 2] = R[i];
			M2[i / 2] = M[i];
		}
		Encode(out, R2, M2, (len + 1) / 2);
		sntrup_clear(R2, sizeof(R2));
		sntrup_clear(M2, sizeof(M2));
	}
}

static void
Decode(uint16_t *out, const unsigned char *S, const uint16_t *M, long long len)
{
	if (len == 1) {
		if (M[0] == 1)
			*out = 0;
		else if (M[0] <= 256)
			*out = uint32_mod_uint14(S[0], M[0]);
		else
			*out = (uint16_t)uint32_mod_uint14(S[0] + (((uint32_t)S[1]) << 8), M[0]);
	}
	if (len > 1) {
		uint16_t  R2[(p + 1) / 2], M2[(p + 1) / 2], bottomr[p / 2];
		uint32_t  bottomt[p / 2];
		long long i;
		for (i = 0; i < len - 1; i += 2) {
			uint32_t m = M[i] * (uint32_t)M[i + 1];
			if (m > UINT32_C(256) * UINT32_C(16383)) {
				bottomt[i / 2] = UINT32_C(65536);
				uint32_t octets = S[0] + UINT32_C(256) * S[1];
				uint16_t low = (uint16_t)octets; /*0..65535. */
				bottomr[i / 2] = low;
				S += 2;
				M2[i / 2] = (uint16_t)((((m + 255) >> 8) + 255) >> 8);
			}
			else if (m >= 16384) {
				bottomt[i / 2] = 256;
				bottomr[i / 2] = S[0];
				S += 1;
				M2[i / 2] = (uint16_t)((m + 255) >> 8);
			}
			else {
				bottomt[i / 2] = 1;
				bottomr[i / 2] = 0;
				M2[i / 2]      = (uint16_t)m;
			}
		}
		if (i < len)
			M2[i / 2] = M[i];
		Decode(R2, S, M2, (len + 1) / 2);
		for (i = 0; i < len - 1; i += 2) {
			uint32_t r1, r = bottomr[i / 2];
			uint16_t r0;
			r += bottomt[i / 2] * R2[i / 2];
			uint32_divmod_uint14(&r1, &r0, r, M[i]);
			r1     = uint32_mod_uint14(r1, M[i + 1]);
			*out++ = r0;
			*out++ = (uint16_t)r1;
		}
		if (i < len)
			*out++ = R2[i / 2];
		sntrup_clear(R2, sizeof(R2));
		sntrup_clear(M2, sizeof(M2));
		sntrup_clear(bottomr, sizeof(bottomr));
		sntrup_clear(bottomt, sizeof(bottomt));
	}
}

static void
R3_fromRq(small *out, const Fq *r)
{
	int i;
	for (i = 0; i < p; ++i)
		out[i] = F3_freeze(r[i]);
}

static void
R3_mult(small *h, const small *f, const small *g)
{
	int16_t fg[p + p - 1];
	int     i, j;
	for (i = 0; i < p + p - 1; ++i)
		fg[i] = 0;
	for (i = 0; i < p; ++i)
		for (j = 0; j < p; ++j)
			fg[i + j] = (int16_t)(fg[i + j] + f[i] * (int16_t)g[j]);
	for (i = p; i < p + p - 1; ++i)
		fg[i - p] += fg[i];
	for (i = p; i < p + p - 1; ++i)
		fg[i - p + 1] += fg[i];
	for (i = 0; i < p; ++i)
		h[i] = F3_freeze(fg[i]);
	sntrup_clear(fg, sizeof(fg));
}

static int
R3_recip(small *out, const small *in)
{
	small f[p + 1], g[p + 1], v[p + 1], r[p + 1];
	int sign, i, loop;
	int64_t swap, delta = 1;
	for (i = 0; i < p + 1; ++i)
		v[i] = 0;
	for (i = 0; i < p + 1; ++i)
		r[i] = 0;
	r[0] = 1;
	for (i = 0; i < p; ++i)
		f[i] = 0;
	f[0]     = 1;
	f[p - 1] = f[p] = -1;
	for (i = 0; i < p; ++i)
		g[p - 1 - i] = in[i];
	g[p] = 0;
	for (loop = 0; loop < 2 * p - 1; ++loop) {
		for (i = p; i > 0; --i)
			v[i] = v[i - 1];
		v[0] = 0;
		sign = -g[0] * f[0];
		int16_t neg_delta = (int16_t)-delta; /* |delta|<=2*p. */
		swap = crypto_int16_negative_mask(neg_delta) & crypto_int16_nonzero_mask(g[0]);
		delta ^= swap & (delta ^ -delta);
		delta += 1;
		for (i = 0; i < p + 1; ++i) {
			/* Exact64-bit bit operations avoid promotion to a differently
			 * represented ordinary int. Every result selects an old input. */
			int64_t change_fg = swap & ((int64_t)f[i] ^ (int64_t)g[i]);
			int64_t change_vr = swap & ((int64_t)v[i] ^ (int64_t)r[i]);
			small next_f = (small)((int64_t)f[i] ^ change_fg);
			small next_g = (small)((int64_t)g[i] ^ change_fg);
			small next_v = (small)((int64_t)v[i] ^ change_vr);
			small next_r = (small)((int64_t)r[i] ^ change_vr);
			f[i] = next_f;
			g[i] = next_g;
			v[i] = next_v;
			r[i] = next_r;
		}
		for (i = 0; i < p + 1; ++i)
			g[i] = F3_freeze((int16_t)(g[i] + sign * f[i]));
		for (i = 0; i < p + 1; ++i)
			r[i] = F3_freeze((int16_t)(r[i] + sign * v[i]));
		for (i = 0; i < p; ++i)
			g[i] = g[i + 1];
		g[p] = 0;
	}
	sign = f[0];
	for (i = 0; i < p; ++i)
		out[i] = (small)(sign * v[p - 1 - i]);
	int16_t final_delta = (int16_t)delta; /* |delta|<=2*p. */
	int ret = crypto_int16_nonzero_mask(final_delta);
	sntrup_clear(f, sizeof(f));
	sntrup_clear(g, sizeof(g));
	sntrup_clear(v, sizeof(v));
	sntrup_clear(r, sizeof(r));
	return ret;
}

static void
Rq_mult_small(Fq *h, const Fq *f, const small *g)
{
	int32_t fg[p + p - 1];
	int     i, j;
	for (i = 0; i < p + p - 1; ++i)
		fg[i] = 0;
	for (i = 0; i < p; ++i)
		for (j = 0; j < p; ++j)
			fg[i + j] += f[i] * (int32_t)g[j];
	for (i = p; i < p + p - 1; ++i)
		fg[i - p] += fg[i];
	for (i = p; i < p + p - 1; ++i)
		fg[i - p + 1] += fg[i];
	for (i = 0; i < p; ++i)
		h[i] = Fq_freeze(fg[i]);
	sntrup_clear(fg, sizeof(fg));
}

static void
Rq_mult3(Fq *h, const Fq *f)
{
	int i;
	for (i = 0; i < p; ++i)
		h[i] = Fq_freeze(3 * f[i]);
}

static Fq
Fq_recip(Fq a1)
{
	int i  = 1;
	Fq  ai = a1;
	while (i < q - 2) {
		ai = Fq_freeze(a1 * (int32_t)ai);
		i += 1;
	}
	return ai;
}

static int
Rq_recip3(Fq *out, const small *in)
{
	Fq      f[p + 1], g[p + 1], v[p + 1], r[p + 1], scale;
	int i, loop;
	int64_t swap, delta = 1;
	int32_t f0, g0;
	for (i = 0; i < p + 1; ++i)
		v[i] = 0;
	for (i = 0; i < p + 1; ++i)
		r[i] = 0;
	r[0] = Fq_recip(3);
	for (i = 0; i < p; ++i)
		f[i] = 0;
	f[0]     = 1;
	f[p - 1] = f[p] = -1;
	for (i = 0; i < p; ++i)
		g[p - 1 - i] = in[i];
	g[p] = 0;
	for (loop = 0; loop < 2 * p - 1; ++loop) {
		for (i = p; i > 0; --i)
			v[i] = v[i - 1];
		v[0] = 0;
		int16_t neg_delta = (int16_t)-delta; /* |delta|<=2*p. */
		swap = crypto_int16_negative_mask(neg_delta) & crypto_int16_nonzero_mask(g[0]);
		delta ^= swap & (delta ^ -delta);
		delta += 1;
		for (i = 0; i < p + 1; ++i) {
			int64_t change_fg = swap & ((int64_t)f[i] ^ (int64_t)g[i]);
			int64_t change_vr = swap & ((int64_t)v[i] ^ (int64_t)r[i]);
			Fq next_f = (Fq)((int64_t)f[i] ^ change_fg);
			Fq next_g = (Fq)((int64_t)g[i] ^ change_fg);
			Fq next_v = (Fq)((int64_t)v[i] ^ change_vr);
			Fq next_r = (Fq)((int64_t)r[i] ^ change_vr);
			f[i] = next_f;
			g[i] = next_g;
			v[i] = next_v;
			r[i] = next_r;
		}
		f0 = f[0];
		g0 = g[0];
		for (i = 0; i < p + 1; ++i)
			g[i] = Fq_freeze(f0 * g[i] - g0 * f[i]);
		for (i = 0; i < p + 1; ++i)
			r[i] = Fq_freeze(f0 * r[i] - g0 * v[i]);
		for (i = 0; i < p; ++i)
			g[i] = g[i + 1];
		g[p] = 0;
	}
	scale = Fq_recip(f[0]);
	for (i = 0; i < p; ++i)
		out[i] = Fq_freeze(scale * (int32_t)v[p - 1 - i]);
	int16_t final_delta = (int16_t)delta; /* |delta|<=2*p. */
	int ret = crypto_int16_nonzero_mask(final_delta);
	sntrup_clear(f, sizeof(f));
	sntrup_clear(g, sizeof(g));
	sntrup_clear(v, sizeof(v));
	sntrup_clear(r, sizeof(r));
	return ret;
}

static void
Round(Fq *out, const Fq *a)
{
	int i;
	for (i = 0; i < p; ++i)
		out[i] = a[i] - F3_freeze(a[i]);
}

static void
Short_fromlist(small *out, const uint32_t *in)
{
	uint32_t L[p];
	int      i;
	for (i = 0; i < w; ++i)
		L[i] = in[i] & UINT32_C(0xfffffffe);
	for (i = w; i < p; ++i)
		L[i] = (in[i] & UINT32_C(0xfffffffd)) | 1;
	crypto_sort_uint32(L, p);
	for (i = 0; i < p; ++i) {
		uint32_t bucket = L[i] & UINT32_C(3);
		int32_t coefficient = (int32_t)bucket; /*0,1,2. */
		small selected = (small)(coefficient - 1);
		out[i] = selected;
	}
	sntrup_clear(L, sizeof(L));
}

static int
Hash_prefix(unsigned char *out, int b, const unsigned char *in, int inlen)
{
	unsigned char x[crypto_kem_sntrup761_PUBLICKEYBYTES + 1], h[64];
	int           i;
	int ret = -1;
	if (inlen < 0 || (size_t)inlen > sizeof(x) - 1)
		goto done;
	x[0] = (unsigned char)b;
	for (i = 0; i < inlen; ++i)
		x[i + 1] = in[i];
	if (crypto_hash_sha512(h, x, (unsigned long long)(inlen + 1)) < 0)
		goto done;
	for (i = 0; i < 32; ++i)
		out[i] = h[i];
	ret = 0;
done:
	sntrup_clear(x, sizeof(x));
	sntrup_clear(h, sizeof(h));
	return ret;
}

/* Preserve the reference urandom32 little-endian interpretation while keeping
 * one bulk entropy request and reusing the caller's actual word storage. Each
 * word's four octets are read before that word is overwritten. */
static void
decode_random_words(uint32_t words[p])
{
	unsigned char *bytes = (unsigned char *)words;
	for (size_t i = 0; i < p; i++) {
		uint32_t low = bytes[4 * i];
		uint32_t second = bytes[4 * i + 1];
		uint32_t third = bytes[4 * i + 2];
		uint32_t high = bytes[4 * i + 3];
		words[i] = low | (second << 8) | (third << 16) | (high << 24);
	}
}

static int
Short_random(small *out)
{
	uint32_t L[p];
	int ret = randombytes(L, sizeof(L));
	if (ret == 0) {
		decode_random_words(L);
		Short_fromlist(out, L);
	}
	sntrup_clear(L, sizeof(L));
	return ret;
}

static int
Small_random(small *out)
{
	int      i;
	uint32_t L[p];
	int ret = randombytes(L, sizeof(L));
	if (ret == 0) {
		decode_random_words(L);
		for (i = 0; i < p; ++i) {
			uint32_t bucket = ((L[i] & UINT32_C(0x3fffffff)) * UINT32_C(3)) >> 30;
			int32_t coefficient = (int32_t)bucket; /*0..2. */
			small selected = (small)(coefficient - 1);
			out[i] = selected;
		}
	}
	sntrup_clear(L, sizeof(L));
	return ret;
}

static int
KeyGen(Fq *h, small *f, small *ginv)
{
	small g[p];
	Fq    finv[p];
	int ret = -1;
	for (;;) {
		int result;
		if (Small_random(g) < 0)
			goto done;
		result = R3_recip(ginv, g);
		crypto_declassify(&result, sizeof result);
		if (result == 0)
			break;
	}
	if (Short_random(f) < 0)
		goto done;
	if (Rq_recip3(finv, f) < 0)
		goto done;
	Rq_mult_small(h, finv, g);
	ret = 0;
done:
	sntrup_clear(g, sizeof(g));
	sntrup_clear(finv, sizeof(finv));
	return ret;
}

static void
Encrypt(Fq *c, const small *r, const Fq *h)
{
	Fq hr[p];
	Rq_mult_small(hr, h, r);
	Round(c, hr);
	sntrup_clear(hr, sizeof(hr));
}

static void
Decrypt(small *r, const Fq *c, const small *f, const small *ginv)
{
	Fq    cf[p], cf3[p];
	small e[p], ev[p];
	int i;
	int64_t mask;
	Rq_mult_small(cf, c, f);
	Rq_mult3(cf3, cf);
	R3_fromRq(e, cf3);
	R3_mult(ev, e, ginv);
	mask = Weightw_mask(ev);
	for (i = 0; i < w; ++i) {
		small selected = (small)((((int64_t)ev[i] ^ 1) & ~mask) ^ 1);
		r[i] = selected;
	}
	for (i = w; i < p; ++i) {
		small selected = (small)((int64_t)ev[i] & ~mask);
		r[i] = selected;
	}
	sntrup_clear(cf, sizeof(cf));
	sntrup_clear(cf3, sizeof(cf3));
	sntrup_clear(e, sizeof(e));
	sntrup_clear(ev, sizeof(ev));
	sntrup_clear(&mask, sizeof(mask));
}

static void
Small_encode(unsigned char *s, const small *f)
{
	int i, j;
	for (i = 0; i < p / 4; ++i) {
		unsigned int byte = 0;
		for (j = 0; j < 4; ++j) {
			unsigned int coefficient = (unsigned int)(*f++ + 1);
			byte |= coefficient << (2 * j);
		}
		/* Generated/decoded small coefficients are-1,0,1, hence0..170. */
		unsigned char encoded = (unsigned char)byte;
		*s++ = encoded;
	}
	unsigned char tail = (unsigned char)(*f + 1); /*0..2. */
	*s = tail;
}

static void
Small_decode(small *f, const unsigned char *s)
{
	int i, j;
	for (i = 0; i < p / 4; ++i) {
		unsigned char x = *s++;
		for (j = 0; j < 4; ++j)
			*f++ = ((small)((x >> (2 * j)) & 3)) - 1;
	}
	*f++ = ((small)(*s & 3)) - 1;
}

static void
Rq_encode(unsigned char *s, const Fq *r)
{
	uint16_t R[p], M[p];
	int      i;
	for (i = 0; i < p; ++i)
		R[i] = (uint16_t)(r[i] + q12);
	for (i = 0; i < p; ++i)
		M[i] = q;
	Encode(s, R, M, p);
	sntrup_clear(R, sizeof(R));
	sntrup_clear(M, sizeof(M));
}

static void
Rq_decode(Fq *r, const unsigned char *s)
{
	uint16_t R[p], M[p];
	int      i;
	for (i = 0; i < p; ++i)
		M[i] = q;
	Decode(R, s, M, p);
	for (i = 0; i < p; ++i)
		r[i] = ((Fq)R[i]) - q12;
	sntrup_clear(R, sizeof(R));
	sntrup_clear(M, sizeof(M));
}

static void
Rounded_encode(unsigned char *s, const Fq *r)
{
	uint16_t R[p], M[p];
	int      i;
	for (i = 0; i < p; ++i) {
		int32_t shifted = r[i] + q12; /*0..4590. */
		int32_t product = (int32_t)(shifted * INT32_C(10923));
		uint16_t encoded = (uint16_t)(product >> 15); /*0..1530. */
		R[i] = encoded;
	}
	for (i = 0; i < p; ++i)
		M[i] = (q + 2) / 3;
	Encode(s, R, M, p);
	sntrup_clear(R, sizeof(R));
	sntrup_clear(M, sizeof(M));
}

static void
Rounded_decode(Fq *r, const unsigned char *s)
{
	uint16_t R[p], M[p];
	int      i;
	for (i = 0; i < p; ++i)
		M[i] = (q + 2) / 3;
	Decode(R, s, M, p);
	for (i = 0; i < p; ++i) {
		int32_t coefficient = R[i]; /*0..1530. */
		int32_t centered = (int32_t)(INT32_C(3) * coefficient - q12);
		Fq decoded = (Fq)centered; /*-2295..2295. */
		r[i] = decoded;
	}
	sntrup_clear(R, sizeof(R));
	sntrup_clear(M, sizeof(M));
}

static int
ZKeyGen(unsigned char *pk, unsigned char *sk)
{
	Fq    h[p];
	small f[p], v[p];
	int ret = KeyGen(h, f, v);
	if (ret == 0) {
		Rq_encode(pk, h);
		Small_encode(sk, f);
		Small_encode(sk + Small_bytes, v);
	}
	sntrup_clear(h, sizeof(h));
	sntrup_clear(f, sizeof(f));
	sntrup_clear(v, sizeof(v));
	return ret;
}

static void
ZEncrypt(unsigned char *C, const Inputs r, const unsigned char *pk)
{
	Fq h[p], c[p];
	Rq_decode(h, pk);
	Encrypt(c, r, h);
	Rounded_encode(C, c);
	sntrup_clear(h, sizeof(h));
	sntrup_clear(c, sizeof(c));
}

static void
ZDecrypt(Inputs r, const unsigned char *C, const unsigned char *sk)
{
	small f[p], v[p];
	Fq    c[p];
	Small_decode(f, sk);
	Small_decode(v, sk + Small_bytes);
	Rounded_decode(c, C);
	Decrypt(r, c, f, v);
	sntrup_clear(f, sizeof(f));
	sntrup_clear(v, sizeof(v));
	sntrup_clear(c, sizeof(c));
}

static int
HashConfirm(unsigned char *h, const unsigned char *r, const unsigned char *cache)
{
	unsigned char x[Hash_bytes * 2];
	int           i;
	int ret = Hash_prefix(x, 3, r, Small_bytes);
	if (ret < 0)
		goto done;
	for (i = 0; i < Hash_bytes; ++i)
		x[Hash_bytes + i] = cache[i];
	ret = Hash_prefix(h, 2, x, sizeof x);
done:
	sntrup_clear(x, sizeof(x));
	return ret;
}

static int
HashSession(unsigned char *k, int b, const unsigned char *y, const unsigned char *z)
{
	unsigned char x[Hash_bytes + crypto_kem_sntrup761_CIPHERTEXTBYTES];
	int           i;
	int ret = Hash_prefix(x, 3, y, Small_bytes);
	if (ret < 0)
		goto done;
	for (i = 0; i < crypto_kem_sntrup761_CIPHERTEXTBYTES; ++i)
		x[Hash_bytes + i] = z[i];
	ret = Hash_prefix(k, b, x, sizeof x);
done:
	sntrup_clear(x, sizeof(x));
	return ret;
}

DSSH_PRIVATE int
crypto_kem_sntrup761_keypair(unsigned char *pk, unsigned char *sk)
{
	int i;
	if (ZKeyGen(pk, sk) < 0)
		return -1;
	sk += SecretKeys_bytes;
	for (i = 0; i < crypto_kem_sntrup761_PUBLICKEYBYTES; ++i)
		*sk++ = pk[i];
	if (randombytes(sk, Small_bytes) < 0)
		return -1;
	if (Hash_prefix(sk + Small_bytes, 4, pk, crypto_kem_sntrup761_PUBLICKEYBYTES) < 0)
		return -1;
	return 0;
}

static int
Hide(unsigned char *c, unsigned char *r_enc, const Inputs r, const unsigned char *pk, const unsigned char *cache)
{
	Small_encode(r_enc, r);
	ZEncrypt(c, r, pk);
	return HashConfirm(c + crypto_kem_sntrup761_CIPHERTEXTBYTES - Confirm_bytes, r_enc, cache);
}

DSSH_PRIVATE int
crypto_kem_sntrup761_enc(unsigned char *c, unsigned char *k, const unsigned char *pk)
{
	Inputs        r;
	unsigned char r_enc[Small_bytes], cache[Hash_bytes];
	int ret = Hash_prefix(cache, 4, pk, crypto_kem_sntrup761_PUBLICKEYBYTES);
	if (ret < 0)
		goto done;
	ret = Short_random(r);
	if (ret < 0)
		goto done;
	ret = Hide(c, r_enc, r, pk, cache);
	if (ret < 0)
		goto done;
	ret = HashSession(k, 1, r_enc, c);
done:
	sntrup_clear(r, sizeof(r));
	sntrup_clear(r_enc, sizeof(r_enc));
	sntrup_clear(cache, sizeof(cache));
	return ret;
}

static int
Ciphertexts_diff_mask(const unsigned char *c, const unsigned char *c2)
{
	uint16_t differentbits = 0;
	int      len           = crypto_kem_sntrup761_CIPHERTEXTBYTES;
	while (len-- > 0)
		differentbits |= (*c++) ^ (*c2++);
	int64_t difference = differentbits; /*0..255. */
	difference -= 1;
	int64_t equal = crypto_int64_bitmod_01(difference, 8);
	int result = (int)(equal - 1); /*0/-1 on every int width. */
	return result;
}

DSSH_PRIVATE int
crypto_kem_sntrup761_dec(unsigned char *k, const unsigned char *c, const unsigned char *sk)
{
	const unsigned char *pk    = sk + SecretKeys_bytes;
	const unsigned char *rho   = pk + crypto_kem_sntrup761_PUBLICKEYBYTES;
	const unsigned char *cache = rho + Small_bytes;
	Inputs               r;
	unsigned char        r_enc[Small_bytes], cnew[crypto_kem_sntrup761_CIPHERTEXTBYTES];
	int i;
	int64_t mask;
	ZDecrypt(r, c, sk);
	int ret = Hide(cnew, r_enc, r, pk, cache);
	if (ret < 0)
		goto done;
	mask = Ciphertexts_diff_mask(c, cnew);
	for (i = 0; i < Small_bytes; ++i) {
		unsigned char selected = (unsigned char)(r_enc[i] ^ (mask & (r_enc[i] ^ rho[i])));
		r_enc[i] = selected;
	}
	int domain = (int)(1 + mask); /*0/1. */
	ret = HashSession(k, domain, r_enc, c);
done:
	sntrup_clear(r, sizeof(r));
	sntrup_clear(r_enc, sizeof(r_enc));
	sntrup_clear(cnew, sizeof(cnew));
	sntrup_clear(&mask, sizeof(mask));
	return ret;
}
