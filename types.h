#pragma once

#include <stdint.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

typedef int32_t b32;

typedef float f32;
typedef double f64;

#ifndef __cplusplus
typedef union v2 v2;
#endif

union v2
{
    struct
    {
        f32 x, y;
    };
    struct
    {
        f32 u, v;
    };
    struct
    {
        f32 Width, Height;
    };
    f32 E[2];
};

#define ArrayCount(Array) (sizeof(Array) / sizeof((Array)[0]))

#include <intrin.h>
#define Assert(cond) do { if (!(cond)) __debugbreak(); } while (0)