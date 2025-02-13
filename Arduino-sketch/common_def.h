/*
   FileName:   common_def.h

   File contains MJB's common definitions for 32-bit MCU embedded applications.

   Note:  This code was migrated to Arduino from another IDE (Microchip MPLAB.X)
          which might help to explain some oddities in data typedefs, etc.
*/
#ifndef MJB_SYSTEM_DEF_H
#define MJB_SYSTEM_DEF_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#define USE_CONSOLE_CLI  0    // CLI omitted in REMI 3 version
#define FIXED_POINT_FORMAT_12_20_BITS   // range: +/-2047,  precision: +/-0.000001

#ifndef LITTLE_ENDIAN
#define LITTLE_ENDIAN   1   // True for Teensy 3.2
#endif

typedef signed char         int8;
typedef unsigned char       uint8,  uchar;
typedef unsigned char       bitmap_t;

typedef signed short        int16;
typedef unsigned short      uint16, ushort;

typedef signed long         int32;
typedef unsigned long       uint32, ulong;

typedef signed long long    int64;
typedef unsigned long long  uint64;

typedef signed long         fixed_t;   // 32-bit fixed point

#ifndef BOOL
typedef unsigned char       BOOL;
#endif

typedef void (* pfnvoid)(void);     // pointer to void function

#ifndef FALSE
#define FALSE   (0)
#define TRUE    (!FALSE)
#endif

#ifndef PRIVATE
#define PRIVATE      // dummy keyword!
#endif

#define until(exp)  while(!(exp))   // Usage:  do { ... } until (exp);

#define TEST_BIT(u, nb)   ((u) & (1<<nb))  // nb is bit number (0, 1, 2, ...)
#define SET_BIT(u, nb)    ((u) |= (1<<nb))
#define CLEAR_BIT(u, nb)  ((u) &= ~(1<<nb))

#define SWAP(w)     ((((w) & 0xFF) << 8) | (((w) >> 8) & 0xFF))

#define HI_BYTE(w)  (((w) >> 8) & 0xFF)   // Extract high-order byte from unsigned word
#define LO_BYTE(w)  ((w) & 0xFF)          // Extract low-order byte from unsigned word

#define LESSER_OF(arg1,arg2)  ((arg1)<=(arg2)?(arg1):(arg2))
#define ARRAY_SIZE(a)  (sizeof(a)/sizeof(a[0]))
#define MIN(x,y)       ((x > y)? y: x)

// Macros for manipulating 32-bit (12:20) fixed-point numbers, type fixed_t.
// Integer part:      12 bits, signed, max. range +/-2047
// Fractional part:   20 bits, precision: +/-0.000001 (approx.)
//
#define IntToFixedPt(i)     (i << 20)                    // convert int to fixed-pt
#define FloatToFixed(r)     (fixed_t)(r * 1048576)       // convert float (r) to fixed-pt
#define FixedToFloat(z)     ((float)z / 1048576)         // convert fixed-pt (z) to float
#define IntegerPart(z)      (z >> 20)                    // get integer part of fixed-pt
#define FractionPart(z,n)   ((z & 0xFFFFF) >> (20 - n))  // get n MS bits of fractional part
#define MultiplyFixed(v,w)  (((int64)v * w) >> 20)       // product of two fixed-pt numbers

//***** Commonly used symbolic constants *****
// Note: Arduino has intrinsic defs for ERROR, HIGH, LOW, NULL
#define SUCCESS     0
#define FAIL       -1
#define ERROR      -1
#define LO          0
#define HI          1
#define DISABLE     0
#define ENABLE      1

#endif // MJB_SYSTEM_DEF_H
