/*
 * This is used by the C output mode of the 68k assembler.
 * Loads 68k executable into m68k memory and applies all the relocations.
 * Executable is in an atari .prg format.
 */

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "m68000.h"

/* Determine host byte order explicitly.
 *
 * This used to be "#elif LITTLE_ENDIAN", which is only correct by accident:
 * LITTLE_ENDIAN is not guaranteed to be visible here, and when it is not,
 * the #else branch silently selects the big-endian no-op path and every
 * word/long access into 68k memory is byte-swapped wrong. That is a silent
 * data corruption bug on any little-endian non-i386 host, ARM included. */
#if defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__)
/* GCC and clang both predefine these, on Android/bionic as well. */
# define HOST_LITTLE_ENDIAN	(__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
#else
# include <endian.h>
# if defined(__BYTE_ORDER) && defined(__LITTLE_ENDIAN)
#  define HOST_LITTLE_ENDIAN	(__BYTE_ORDER == __LITTLE_ENDIAN)
# elif defined(BYTE_ORDER) && defined(LITTLE_ENDIAN)
#  define HOST_LITTLE_ENDIAN	(BYTE_ORDER == LITTLE_ENDIAN)
# else
#  error "Cannot determine host byte order - refusing to guess."
# endif
#endif

//#define likely(x)       __builtin_expect((x),1)
//#define unlikely(x)     __builtin_expect((x),0)

#define LOAD_BASE	0x0
#define MEM_SIZE	(0x110000)

union Reg {
	u16	word[2];
	u32	_u32;
	u16	_u16;
	u8	_u8;
	s32	_s32;
	s16	_s16;
	s8	_s8;
};

/* m68000 state ----------------------------------------------------- */
extern s8 m68kram[MEM_SIZE];
extern union Reg Regs[16];
/* status flags */
/* it is an optimisation that instead of having a Z (zero) flag we have
 * an nZ (not zero) flag, because this way we can usually just stick
 * the result in nZ. */
extern s32 N,nZ,V,C,X;
extern s32 bN,bnZ,bV,bC,bX;
extern s32 rdest; /* return address from interrupt. zero if none in service */
extern s32 exceptions_pending;
extern s32 exceptions_pending_nums[32];
extern u32 exception_handlers[32];

void SetReg (int reg, int val);
int GetReg (int reg);

#define GetZFlag() (!nZ)
#define GetNFlag() (N)
#define GetCFlag() (C)
#define GetVFlag() (V)
#define GetXFlag() (X)
#define SetZFlag(val) nZ = !(val)

void FlagException (int num);
void load_binfile (const char *bin_filename);

#ifdef M68K_DEBUG
#define BOUNDS_CHECK
#if 0
static inline void BOUNDS_CHECK (u32 pos, int num)
{
	if ((pos+num) > MEM_SIZE) {
		printf ("Error. 68K memory access out of bounds (address $%x, line %d).\n", pos, line_no);
		abort ();
	}
}
#endif
#endif /* M68K_DEBUG */


/* 68k memory is stored big-endian. Access it through memcpy + a byteswap
 * builtin rather than a direct cast: 68k longs are only word-aligned, so a
 * cast-and-dereference is an unaligned load, which is undefined behaviour
 * and can fault or get miscompiled on ARM. Every compiler folds the 2/4-byte
 * memcpy into a single load, so this costs nothing and emits rev/bswap. */
static inline u32 do_get_mem_long(u32 *a)
{
    u32 val;
    memcpy (&val, a, sizeof (val));
#if HOST_LITTLE_ENDIAN
    return __builtin_bswap32 (val);
#else
    return val;
#endif
}

static inline u16 do_get_mem_word(u16 *a)
{
    u16 val;
    memcpy (&val, a, sizeof (val));
#if HOST_LITTLE_ENDIAN
    return __builtin_bswap16 (val);
#else
    return val;
#endif
}

static inline u8 do_get_mem_byte(u8 *a)
{
    return *a;
}

static inline void do_put_mem_long(u32 *a, u32 v)
{
#if HOST_LITTLE_ENDIAN
    v = __builtin_bswap32 (v);
#endif
    memcpy (a, &v, sizeof (v));
}

static inline void do_put_mem_word(u16 *a, u16 v)
{
#if HOST_LITTLE_ENDIAN
    v = __builtin_bswap16 (v);
#endif
    memcpy (a, &v, sizeof (v));
}

static inline void do_put_mem_byte(u8 *a, u8 v)
{
    *a = v;
}

static inline s32 rdlong (u32 pos)
{
#ifdef M68K_DEBUG
	BOUNDS_CHECK (pos,4);
#endif /* M68K_DEBUG */
	return do_get_mem_long ((u32 *)(m68kram+pos));
}
static inline s16 rdword (u32 pos)
{
#ifdef M68K_DEBUG
	BOUNDS_CHECK (pos,2);
#endif /* M68K_DEBUG */
	return do_get_mem_word ((u16 *)(m68kram+pos));
}
static inline s8 rdbyte (u32 pos)
{
#ifdef M68K_DEBUG
	BOUNDS_CHECK (pos,1);
#endif /* M68K_DEBUG */
	return do_get_mem_byte ((u8 *)(m68kram+pos));
}
static inline void wrbyte (u32 pos, int val)
{
#ifdef M68K_DEBUG
	BOUNDS_CHECK (pos,1);
#endif /* M68K_DEBUG */
	do_put_mem_byte ((u8 *)(m68kram+pos), (u8)val);
}
static inline void wrword (u32 pos, int val)
{
#ifdef M68K_DEBUG
	BOUNDS_CHECK (pos,2);
#endif /* M68K_DEBUG */
	do_put_mem_word ((u16 *)(m68kram+pos), (u16)val);
}
static inline void wrlong (u32 pos, int val)
{
#ifdef M68K_DEBUG
	BOUNDS_CHECK (pos,4);
#endif /* M68K_DEBUG */
	do_put_mem_long ((u32 *)(m68kram+pos), (u32)val);
}

#ifdef M68K_DEBUG
void m68k_print_line_no ();
#endif /* M68K_DEBUG */
