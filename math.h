#pragma once

#define kQ4ToFloat 0.0625f
#define kQ7ToFloat 0.0078125f
#define kQ8ToFloat 0.00390625f

inline v2
V2(f32 x, f32 y)
{
    v2 result;

    result.x = x;
    result.y = y;

    return result;
}

inline v2
v2_add(v2 a, v2 b)
{
    v2 result;

    result.x = a.x + b.x;
    result.y = a.y + b.y;

    return result;
}

inline v2
v2_sub(v2 a, v2 b)
{
    v2 result;

    result.x = a.x - b.x;
    result.y = a.y - b.y;

    return result;
}

inline f32
v2_dot(v2 a, v2 b)
{
    f32 result = a.x*b.x + a.y*b.y;

    return result;
}

inline f32
v2_length_sq(v2 a)
{
    f32 result = v2_dot(a, a);

    return result;
}

inline f32
v2_length(v2 a)
{
    f32 result = sqrtf(v2_length_sq(a));
    return result;
}

inline v2
v2_scale(v2 a, f32 b)
{
    v2 result;

    result.x = a.x * b;
    result.y = a.y * b;

    return result;
}