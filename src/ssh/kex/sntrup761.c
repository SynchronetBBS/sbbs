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
#endif

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
 #if defined(__GNUC__) && defined(__x86_64__)
	__asm__("sarw $15,%0" : "+r"(crypto_int16_x) : : "cc");
	return crypto_int16_x;
 #elif defined(__GNUC__) && defined(__aarch64__)
	crypto_int16 crypto_int16_y;
	__asm__("sbfx %w0,%w1,15,1" : "=r"(crypto_int16_y) : "r"(crypto_int16_x) :);
	return crypto_int16_y;
 #else
	crypto_int16_x >>= 16 - 6;
	crypto_int16_x ^= crypto_int16_optblocker;
	crypto_int16_x >>= 5;
	return crypto_int16_x;
 #endif
}

static inline crypto_int16
crypto_int16_nonzero_mask(crypto_int16 crypto_int16_x)
{
 #if defined(__GNUC__) && defined(__x86_64__)
	crypto_int16 crypto_int16_q, crypto_int16_z;
	__asm__("xorw %0,%0\n movw $-1,%1\n testw %2,%2\n cmovnew %1,%0"
	    : "=&r"(crypto_int16_z), "=&r"(crypto_int16_q)
	    : "r"(crypto_int16_x)
	    : "cc");
	return crypto_int16_z;
 #elif defined(__GNUC__) && defined(__aarch64__)
	crypto_int16 crypto_int16_z;
	__asm__("tst %w1,65535\n csetm %w0,ne" : "=r"(crypto_int16_z) : "r"(crypto_int16_x) : "cc");
	return crypto_int16_z;
 #else
	crypto_int16_x |= -crypto_int16_x;
	return crypto_int16_negative_mask(crypto_int16_x);
 #endif
}

/* --- crypto_int32 --- */

#define crypto_int32          int32_t
#define crypto_int32_unsigned uint32_t

static inline crypto_int32
crypto_int32_negative_mask(crypto_int32 crypto_int32_x)
{
 #if defined(__GNUC__) && defined(__x86_64__)
	__asm__("sarl $31,%0" : "+r"(crypto_int32_x) : : "cc");
	return crypto_int32_x;
 #elif defined(__GNUC__) && defined(__aarch64__)
	crypto_int32 crypto_int32_y;
	__asm__("asr %w0,%w1,31" : "=r"(crypto_int32_y) : "r"(crypto_int32_x) :);
	return crypto_int32_y;
 #else
	crypto_int32_x >>= 32 - 6;
	crypto_int32_x ^= crypto_int32_optblocker;
	crypto_int32_x >>= 5;
	return crypto_int32_x;
 #endif
}

static inline void
crypto_int32_minmax(crypto_int32 *crypto_int32_p, crypto_int32 *crypto_int32_q)
{
	crypto_int32 crypto_int32_x = *crypto_int32_p;
	crypto_int32 crypto_int32_y = *crypto_int32_q;
 #if defined(__GNUC__) && defined(__x86_64__)
	crypto_int32 crypto_int32_z;
	__asm__("cmpl %2,%1\n movl %1,%0\n cmovgl %2,%1\n cmovgl %0,%2"
	    : "=&r"(crypto_int32_z), "+&r"(crypto_int32_x), "+r"(crypto_int32_y)
	    :
	    : "cc");
	*crypto_int32_p = crypto_int32_x;
	*crypto_int32_q = crypto_int32_y;
 #elif defined(__GNUC__) && defined(__aarch64__)
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
	crypto_int32_z = crypto_int32_negative_mask(crypto_int32_z);
	crypto_int32_z &= crypto_int32_r;
	crypto_int32_x ^= crypto_int32_z;
	crypto_int32_y ^= crypto_int32_z;
	*crypto_int32_p = crypto_int32_x;
	*crypto_int32_q = crypto_int32_y;
 #endif
}

/* --- crypto_int64 --- */

#define crypto_int64          int64_t
#define crypto_int64_unsigned uint64_t

CRYPTOINT_MAYBE_UNUSED static inline crypto_int64
crypto_int64_bottombit_mask(crypto_int64 crypto_int64_x)
{
 #if defined(__GNUC__) && defined(__x86_64__)
	__asm__("andq $1,%0" : "+r"(crypto_int64_x) : : "cc");
	return -crypto_int64_x;
 #elif defined(__GNUC__) && defined(__aarch64__)
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
 #if defined(__GNUC__) && defined(__x86_64__)
	__asm__("andq $1,%0" : "+r"(crypto_int64_x) : : "cc");
	return crypto_int64_x;
 #elif defined(__GNUC__) && defined(__aarch64__)
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
 #if defined(__GNUC__) && defined(__x86_64__)
	__asm__("sarq %%cl,%0" : "+r"(crypto_int64_x) : "c"(crypto_int64_s) : "cc");
 #elif defined(__GNUC__) && defined(__aarch64__)
	__asm__("asr %0,%0,%1" : "+r"(crypto_int64_x) : "r"(crypto_int64_s) :);
 #else
	crypto_int64_x >>= crypto_int64_s ^ crypto_int64_optblocker;
 #endif
	return crypto_int64_bottombit_mask(crypto_int64_x);
}

static inline crypto_int64
crypto_int64_shrmod(crypto_int64 crypto_int64_x, crypto_int64 crypto_int64_s)
{
 #if defined(__GNUC__) && defined(__x86_64__)
	__asm__("sarq %%cl,%0" : "+r"(crypto_int64_x) : "c"(crypto_int64_s) : "cc");
 #elif defined(__GNUC__) && defined(__aarch64__)
	__asm__("asr %0,%0,%1" : "+r"(crypto_int64_x) : "r"(crypto_int64_s) :);
 #else
	int crypto_int64_k, crypto_int64_l;
	for (crypto_int64_l = 0, crypto_int64_k = 1; crypto_int64_k < 64; ++crypto_int64_l, crypto_int64_k *= 2)
		crypto_int64_x ^= (crypto_int64_x ^ (crypto_int64_x >> crypto_int64_k))
		                  & crypto_int64_bitinrangepublicpos_mask(crypto_int64_s, crypto_int64_l);
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
	return (small)(x - 3 * ((10923 * x + 16384) >> 15));
}

static Fq
Fq_freeze(int32_t x)
{
	const int32_t q16 = (0x10000 + q / 2) / q;
	const int32_t q20 = (0x100000 + q / 2) / q;
	const int32_t q28 = (0x10000000 + q / 2) / q;
	x -= q * ((q16 * x) >> 16);
	x -= q * ((q20 * x) >> 20);
	return (Fq)(x - q * ((q28 * x + 0x8000000) >> 28));
}

static int
Weightw_mask(small *r)
{
	int i, weight = 0;
	for (i = 0; i < p; ++i)
		weight += (int)crypto_int64_bottombit_01(r[i]);
	return (int16_t)crypto_int16_nonzero_mask((crypto_int16)(weight - w));
}

static void
uint32_divmod_uint14(uint32_t *Q, uint16_t *r, uint32_t x, uint16_t m)
{
	uint32_t qpart, mask, v = 0x80000000 / m;
	qpart = (uint32_t)((x * (uint64_t)v) >> 31);
	x -= qpart * m;
	*Q    = qpart;
	qpart = (uint32_t)((x * (uint64_t)v) >> 31);
	x -= qpart * m;
	*Q += qpart;
	x -= m;
	*Q += 1;
	mask = (uint32_t)crypto_int32_negative_mask((crypto_int32)x);
	x += mask & (uint32_t)m;
	*Q += mask;
	*r = (uint16_t)x;
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
		uint16_t  R2[(len + 1) / 2], M2[(len + 1) / 2];
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
		uint16_t  R2[(len + 1) / 2], M2[(len + 1) / 2], bottomr[len / 2];
		uint32_t  bottomt[len / 2];
		long long i;
		for (i = 0; i < len - 1; i += 2) {
			uint32_t m = M[i] * (uint32_t)M[i + 1];
			if (m > 256 * 16383) {
				bottomt[i / 2] = 256 * 256;
				bottomr[i / 2] = (uint16_t)(S[0] + 256 * S[1]);
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
}

static int
R3_recip(small *out, const small *in)
{
	small f[p + 1], g[p + 1], v[p + 1], r[p + 1];
	int   sign, swap, t, i, loop, delta = 1;
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
		swap = (int16_t)(crypto_int16_negative_mask((crypto_int16)(-delta))
				 & crypto_int16_nonzero_mask(g[0]));
		delta ^= swap & (delta ^ -delta);
		delta += 1;
		for (i = 0; i < p + 1; ++i) {
			t    = swap & (f[i] ^ g[i]);
			f[i] = (small)(f[i] ^ t);
			g[i] = (small)(g[i] ^ t);
			t    = swap & (v[i] ^ r[i]);
			v[i] = (small)(v[i] ^ t);
			r[i] = (small)(r[i] ^ t);
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
	return (int16_t)crypto_int16_nonzero_mask((crypto_int16)delta);
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
	int     swap, t, i, loop, delta = 1;
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
		swap = (int16_t)(crypto_int16_negative_mask((crypto_int16)(-delta))
				 & crypto_int16_nonzero_mask(g[0]));
		delta ^= swap & (delta ^ -delta);
		delta += 1;
		for (i = 0; i < p + 1; ++i) {
			t    = swap & (f[i] ^ g[i]);
			f[i] = (Fq)(f[i] ^ t);
			g[i] = (Fq)(g[i] ^ t);
			t    = swap & (v[i] ^ r[i]);
			v[i] = (Fq)(v[i] ^ t);
			r[i] = (Fq)(r[i] ^ t);
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
	return (int16_t)crypto_int16_nonzero_mask((crypto_int16)delta);
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
		L[i] = in[i] & (uint32_t)-2;
	for (i = w; i < p; ++i)
		L[i] = (in[i] & (uint32_t)-3) | 1;
	crypto_sort_uint32(L, p);
	for (i = 0; i < p; ++i)
		out[i] = (small)((L[i] & 3) - 1);
}

static int
Hash_prefix(unsigned char *out, int b, const unsigned char *in, int inlen)
{
	unsigned char x[inlen + 1], h[64];
	int           i;
	x[0] = (unsigned char)b;
	for (i = 0; i < inlen; ++i)
		x[i + 1] = in[i];
	if (crypto_hash_sha512(h, x, (unsigned long long)(inlen + 1)) < 0)
		return -1;
	for (i = 0; i < 32; ++i)
		out[i] = h[i];
	return 0;
}

static int
Short_random(small *out)
{
	uint32_t L[p];
	if (randombytes(L, sizeof(L)) < 0)
		return -1;
	Short_fromlist(out, L);
	explicit_bzero(L, sizeof(L));
	return 0;
}

static int
Small_random(small *out)
{
	int      i;
	uint32_t L[p];
	if (randombytes(L, sizeof(L)) < 0)
		return -1;
	for (i = 0; i < p; ++i)
		out[i] = (small)((((L[i] & 0x3fffffff) * 3) >> 30) - 1);
	explicit_bzero(L, sizeof(L));
	return 0;
}

static int
KeyGen(Fq *h, small *f, small *ginv)
{
	small g[p];
	Fq    finv[p];
	for (;;) {
		int result;
		if (Small_random(g) < 0)
			return -1;
		result = R3_recip(ginv, g);
		crypto_declassify(&result, sizeof result);
		if (result == 0)
			break;
	}
	if (Short_random(f) < 0)
		return -1;
	Rq_recip3(finv, f);
	Rq_mult_small(h, finv, g);
	return 0;
}

static void
Encrypt(Fq *c, const small *r, const Fq *h)
{
	Fq hr[p];
	Rq_mult_small(hr, h, r);
	Round(c, hr);
}

static void
Decrypt(small *r, const Fq *c, const small *f, const small *ginv)
{
	Fq    cf[p], cf3[p];
	small e[p], ev[p];
	int   mask, i;
	Rq_mult_small(cf, c, f);
	Rq_mult3(cf3, cf);
	R3_fromRq(e, cf3);
	R3_mult(ev, e, ginv);
	mask = Weightw_mask(ev);
	for (i = 0; i < w; ++i)
		r[i] = (small)(((ev[i] ^ 1) & ~mask) ^ 1);
	for (i = w; i < p; ++i)
		r[i] = (small)(ev[i] & ~mask);
}

static void
Small_encode(unsigned char *s, const small *f)
{
	int i, j;
	for (i = 0; i < p / 4; ++i) {
		small x = 0;
		for (j = 0; j < 4; ++j)
			x = (small)(x + ((*f++ + 1) << (2 * j)));
		*s++ = (unsigned char)x;
	}
	*s = (unsigned char)(*f++ + 1);
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
}

static void
Rounded_encode(unsigned char *s, const Fq *r)
{
	uint16_t R[p], M[p];
	int      i;
	for (i = 0; i < p; ++i)
		R[i] = (uint16_t)(((r[i] + q12) * 10923) >> 15);
	for (i = 0; i < p; ++i)
		M[i] = (q + 2) / 3;
	Encode(s, R, M, p);
}

static void
Rounded_decode(Fq *r, const unsigned char *s)
{
	uint16_t R[p], M[p];
	int      i;
	for (i = 0; i < p; ++i)
		M[i] = (q + 2) / 3;
	Decode(R, s, M, p);
	for (i = 0; i < p; ++i)
		r[i] = (Fq)(R[i] * 3 - q12);
}

static int
ZKeyGen(unsigned char *pk, unsigned char *sk)
{
	Fq    h[p];
	small f[p], v[p];
	if (KeyGen(h, f, v) < 0)
		return -1;
	Rq_encode(pk, h);
	Small_encode(sk, f);
	Small_encode(sk + Small_bytes, v);
	return 0;
}

static void
ZEncrypt(unsigned char *C, const Inputs r, const unsigned char *pk)
{
	Fq h[p], c[p];
	Rq_decode(h, pk);
	Encrypt(c, r, h);
	Rounded_encode(C, c);
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
}

static int
HashConfirm(unsigned char *h, const unsigned char *r, const unsigned char *cache)
{
	unsigned char x[Hash_bytes * 2];
	int           i;
	if (Hash_prefix(x, 3, r, Small_bytes) < 0)
		return -1;
	for (i = 0; i < Hash_bytes; ++i)
		x[Hash_bytes + i] = cache[i];
	return Hash_prefix(h, 2, x, sizeof x);
}

static int
HashSession(unsigned char *k, int b, const unsigned char *y, const unsigned char *z)
{
	unsigned char x[Hash_bytes + crypto_kem_sntrup761_CIPHERTEXTBYTES];
	int           i;
	if (Hash_prefix(x, 3, y, Small_bytes) < 0)
		return -1;
	for (i = 0; i < crypto_kem_sntrup761_CIPHERTEXTBYTES; ++i)
		x[Hash_bytes + i] = z[i];
	return Hash_prefix(k, b, x, sizeof x);
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
	if (Hash_prefix(cache, 4, pk, crypto_kem_sntrup761_PUBLICKEYBYTES) < 0)
		return -1;
	if (Short_random(r) < 0)
		return -1;
	if (Hide(c, r_enc, r, pk, cache) < 0)
		return -1;
	return HashSession(k, 1, r_enc, c);
}

static int
Ciphertexts_diff_mask(const unsigned char *c, const unsigned char *c2)
{
	uint16_t differentbits = 0;
	int      len           = crypto_kem_sntrup761_CIPHERTEXTBYTES;
	while (len-- > 0)
		differentbits |= (*c++) ^ (*c2++);
	return (int)((crypto_int64_bitmod_01((differentbits - 1), 8)) - 1);
}

DSSH_PRIVATE int
crypto_kem_sntrup761_dec(unsigned char *k, const unsigned char *c, const unsigned char *sk)
{
	const unsigned char *pk    = sk + SecretKeys_bytes;
	const unsigned char *rho   = pk + crypto_kem_sntrup761_PUBLICKEYBYTES;
	const unsigned char *cache = rho + Small_bytes;
	Inputs               r;
	unsigned char        r_enc[Small_bytes], cnew[crypto_kem_sntrup761_CIPHERTEXTBYTES];
	int                  mask, i;
	ZDecrypt(r, c, sk);
	if (Hide(cnew, r_enc, r, pk, cache) < 0)
		return -1;
	mask = Ciphertexts_diff_mask(c, cnew);
	for (i = 0; i < Small_bytes; ++i)
		r_enc[i] = (unsigned char)(r_enc[i] ^ (mask & (r_enc[i] ^ rho[i])));
	return HashSession(k, 1 + mask, r_enc, c);
}
