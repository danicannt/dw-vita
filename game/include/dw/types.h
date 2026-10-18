#ifndef DW_TYPES_H
#define DW_TYPES_H

#ifdef __MWERKS__

#ifndef NULL
#define NULL 0
#endif

typedef char int8_t;
typedef short int16_t;
typedef int int32_t;

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;

typedef int intptr_t;
typedef unsigned int uintptr_t;

#else

#include <stddef.h>
#include <stdint.h>

#endif

#endif
