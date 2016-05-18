#include <stdint.h>
#ifdef _MSC_VER
#include <intrin.h>     /* _BitScanForward, _BitScanReverse */
#endif

/*
 * L E A D I N G   A N D   T R A I L I N G   Z E R O S
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */


/* First define ctz and clz functions; these are compiler-dependent if
 * you want them to be fast. */
#if defined(__GNUC__) && !defined(TIMEOUT_DISABLE_GNUC_BITOPS)

/* On GCC and clang and some others, we can use __builtin functions. They
 * are not defined for n==0, but timeout.s never calls them with n==0. */

#define ctz64(n) __builtin_ctzll(n)
#define clz64(n) __builtin_clzll(n)
#if LONG_BITS == 32
#define ctz32(n) __builtin_ctzl(n)
#define clz32(n) __builtin_clzl(n)
#else
#define ctz32(n) __builtin_ctz(n)
#define clz32(n) __builtin_clz(n)
#endif

#elif defined(_MSC_VER) && !defined(TIMEOUT_DISABLE_MSVC_BITOPS)

/* On MSVC, we have these handy functions. We can ignore their return
 * values, since we will never supply val == 0. */

static __inline int ctz32(unsigned long val)
{
	DWORD zeros = 0;
	_BitScanForward(&zeros, val);
	return zeros;
}
static __inline int clz32(unsigned long val)
{
	DWORD zeros = 0;
	_BitScanReverse(&zeros, val);
	return zeros;
}
#ifdef _WIN64
/* According to the documentation, these only exist on Win64. */
static __inline int ctz64(uint64_t val)
{
	DWORD zeros = 0;
	_BitScanForward64(&zeros, val);
	return zeros;
}
static __inline int clz64(uint64_t val)
{
	DWORD zeros = 0;
	_BitScanReverse64(&zeros, val);
	return zeros;
}
#else
static __inline int ctz64(uint64_t val)
{
	uint32_t lo = (uint32_t) val;
	uint32_t hi = (uint32_t) (val >> 32);
	return lo ? ctz32(lo) : 32 + ctz32(hi);
}
static __inline int clz64(uint64_t val)
{
	uint32_t lo = (uint32_t) val;
	uint32_t hi = (uint32_t) (val >> 32);
	return hi ? clz32(hi) : 32 + clz32(lo);
}
#endif

/* End of MSVC case. */

#else

/* TODO: There are more clever ways to do this in the generic case. */


#define process_(one, cz_bits, bits)					\
	if (x < ( one << (cz_bits - bits))) { rv += bits; x <<= bits; }

#define process64(bits) process_((UINT64_C(1)), 64, (bits))
static inline int clz64(uint64_t x)
{
	int rv = 0;

	process64(32);
	process64(16);
	process64(8);
	process64(4);
	process64(2);
	process64(1);
	return rv;
}
#define process32(bits) process_((UINT32_C(1)), 32, (bits))
static inline int clz32(uint32_t x)
{
	int rv = 0;

	process32(16);
	process32(8);
	process32(4);
	process32(2);
	process32(1);
	return rv;
}

#undef process_
#undef process32
#undef process64
#define process_(one, bits)						\
	if ((x & ((one << (bits))-1)) == 0) { rv += bits; x >>= bits; }

#define process64(bits) process_((UINT64_C(1)), bits)
static inline int ctz64(uint64_t x)
{
	int rv = 0;

	process64(32);
	process64(16);
	process64(8);
	process64(4);
	process64(2);
	process64(1);
	return rv;
}

#define process32(bits) process_((UINT32_C(1)), bits)
static inline int ctz32(uint32_t x)
{
	int rv = 0;

	process32(16);
	process32(8);
	process32(4);
	process32(2);
	process32(1);
	return rv;
}

#undef process32
#undef process64
#undef process_

/* End of generic case */

#endif /* End of defining ctz and clz. */


/*
 * B I T W I S E   R O T A T I O N
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/* We need to find a rotate idiom that accepts 0-sized rotates.  So we can't
 * use the "normal" (v<<c) | (v >> (bits-c)), since if c==0, then v >>
 * (bits-c) will be undefined.
 *
 * The definitions below will also work okay when c is greater than the
 * bit width, though we don't actually require those.
 */

#if defined(_MSC_VER) && !defined(TIMEOUT_DISABLE_MSVC_BITOPS)
/* MSVC provides these declarations in stdlib.h and intrin.h */

#define rotr8(v,c) _rotr8((v),(c)&7)
#define rotl8(v,c) _rotl8((v),(c)&7)
#define rotr16(v,c) _rotr16((v),(c)&15)
#define rotl16(v,c) _rotl16((v),(c)&15)
#define rotr32(v,c) _rotr32((v),(c)&31)
#define rotl32(v,c) _rotl32((v),(c)&31)
#define rotr64(v,c) _rotr64((v),(c)&63)
#define rotl64(v,c) _rotl64((v),(c)&63)

#define DECLARE_ROTATE_(bits, type)

#elif 1
/* Many modern compilers recognize the idiom here as equivalent to rotr/rotl,
 * and emit a single instruction.
 */
#define DECLARE_ROTATE_(bits, type)					\
	static inline type rotl##bits(const type v, int c) {		\
		const int mask = (bits)-1;				\
		c &= mask;						\
									\
		return (v << c) | (v >> (-c & mask));			\
	} /* rotl() */							\
									\
	static inline type rotr##bits(const type v, int c) {		\
		const int mask = (bits)-1;				\
		c &= mask;						\
									\
		return (v >> c) | (v << (-c & mask));			\
	} /* rotr() */

#else
/* We aren't using this unless we find somewhere that it's faster. */

#define DECLARE_ROTATE_(bits, type)					\
	static inline type rotl##bits(const type v, int c)	{ 	\
		if (!(c &= ((bits)-1)))					\
			return v;					\
									\
		return (v << c) | (v >> ((bits) - c));			\
	} /* rotl() */							\
									\
	static inline type rotr##bits(const type v, int c) {		\
		if (!(c &= ((bits)-1)))					\
			return v;					\
									\
		return (v >> c) | (v << ((bits) - c));			\
	} /* rotr() */
#endif
#define DECLARE_ROTATE(bits)                    \
	DECLARE_ROTATE_(bits, uint##bits##_t)

DECLARE_ROTATE(64);
DECLARE_ROTATE(32);
DECLARE_ROTATE(16);
DECLARE_ROTATE(8);

#ifdef TEST_BITOPS
#include <stdio.h>
#include <stdlib.h>

static uint64_t testcases[] = {
	13371337 * 10,
	100,
	385789752,
	82574,
	(((uint64_t)1)<<63) + (((uint64_t)1)<<31) + 10101
};

static int
naive_clz(int bits, uint64_t v)
{
	int r = 0;
	uint64_t bit = ((uint64_t)1) << (bits-1);
	while (bit && 0 == (v & bit)) {
		r++;
		bit >>= 1;
	}
	/* printf("clz(%d,%lx) -> %d\n", bits, v, r); */
	return r;
}

static int
naive_ctz(int bits, uint64_t v)
{
	int r = 0;
	uint64_t bit = 1;
	while (bit && 0 == (v & bit)) {
		r++;
		bit <<= 1;
		if (r == bits)
			break;
	}
	/* printf("ctz(%d,%lx) -> %d\n", bits, v, r); */
	return r;
}

static uint64_t naive_rotl(int n_bits, uint64_t v, int c)
{
	uint64_t r = 0;
	int i;
	for (i = 0; i < n_bits; ++i) {
		if (v & ((uint64_t)1)<<i) {
			r |= ((uint64_t)1) << ((i+c)%n_bits);
		}
	}
	return r;
}
static uint64_t naive_rotr(int n_bits, uint64_t v, int c)
{
	c %= n_bits;
	return naive_rotl(n_bits, v, n_bits-c);
}


static int
check(uint64_t vv)
{
	uint32_t v32 = (uint32_t) vv;
	int j;

	for (j = 0; j < 64; ++j) {
		if (rotr64(vv, j) != naive_rotr(64, vv, j)) {
			printf("mismatch with rotr64");
			exit(1);
		}
		if (rotl64(vv, j) != naive_rotl(64, vv, j)) {
			printf("mismatch with rotl64");
			exit(1);
		}
		if (rotr32(vv, j) != naive_rotr(32, vv, j)) {
			printf("mismatch with rotr32");
			exit(1);
		}
		if (rotl32(vv, j) != naive_rotl(32, vv, j)) {
			printf("mismatch with rotl32");
			exit(1);
		}
		if (rotr16(vv, j) != naive_rotr(16, vv, j)) {
			printf("mismatch with rotr16");
			exit(1);
		}
		if (rotl16(vv, j) != naive_rotl(16, vv, j)) {
			printf("mismatch with rotl16");
			exit(1);
		}
		if (rotr8(vv, j) != naive_rotr(8, vv, j)) {
			printf("mismatch with rotr8");
			exit(1);
		}
		if (rotl8(vv, j) != naive_rotl(8, vv, j)) {
			printf("mismatch with rotl8");
			exit(1);
		}
	}

	if (vv == 0)
		return 1; /* c[tl]z64(0) is undefined. */

	if (ctz64(vv) != naive_ctz(64, vv)) {
		printf("mismatch with ctz64: %d\n", ctz64(vv));
				exit(1);
		return 0;
	}
	if (clz64(vv) != naive_clz(64, vv)) {
		printf("mismatch with clz64: %d\n", clz64(vv));
				exit(1);
		return 0;
	}

	if (v32 == 0)
		return 1; /* c[lt]z(0) is undefined. */

	if (ctz32(v32) != naive_ctz(32, v32)) {
		printf("mismatch with ctz32: %d\n", ctz32(v32));
		exit(1);
		return 0;
	}
	if (clz32(v32) != naive_clz(32, v32)) {
		printf("mismatch with clz32: %d\n", clz32(v32));
				exit(1);
		return 0;
	}
	return 1;
}

int
main(int c, char **v)
{
	unsigned int i;
	const unsigned int n = sizeof(testcases)/sizeof(testcases[0]);
	int result = 0;

	for (i = 0; i <= 63; ++i) {
		uint64_t x = 1 << i;
		if (!check(x))
			result = 1;
		--x;
		if (!check(x))
			result = 1;
	}

	for (i = 0; i < n; ++i) {
		if (! check(testcases[i]))
			result = 1;
	}
	if (result) {
		puts("FAIL");
	} else {
		puts("OK");
	}
	return result;
}
#endif

