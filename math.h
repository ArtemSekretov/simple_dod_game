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

m4x4_inv
orthographic_projection(f32 left, f32 right, f32 bottom, f32 top, f32 near_clip_plane, f32 far_clip_plane)
{
    f32 a = 2.0f / (right - left);
    f32 b = 2.0f / (top - bottom);

    f32 a1 = (left + right) / (right - left);
    f32 b1 = (top + bottom) / (top - bottom);

    f32 n = near_clip_plane;
    f32 f = far_clip_plane;
    f32 d = 2.0f / (n - f);
    f32 e = (n + f) / (n - f);

    // a, 0, 0, -a1,
    // 0, b, 0, -b1,
    // 0, 0, d,  e,
    // 0, 0, 0,  1,
    m4x4_inv result = { 0 };
    result.forward.E[0][0] = a;
    result.forward.E[0][3] = -a1;
    result.forward.E[1][1] = b;
    result.forward.E[1][3] = -b1;
    result.forward.E[2][2] = d;
    result.forward.E[2][3] = e;
    result.forward.E[3][3] = 1.0f;

    result.inverse.E[0][0] = 1.0f / a;
    result.inverse.E[0][3] = a1 / a;
    result.inverse.E[1][1] = 1.0f / b;
    result.inverse.E[1][3] = b1 / b;
    result.inverse.E[2][2] = 1.0f / d;
    result.inverse.E[2][3] = -e / d;
    result.inverse.E[3][3] = 1.0f;
    return result;
}

inline v2
transform(m4x4 m, v2 p)
{
    v2 r;

    r.x = p.x * m.E[0][0] + p.y * m.E[0][1] + m.E[0][3];
    r.y = p.x * m.E[1][0] + p.y * m.E[1][1] + m.E[1][3];

    return r;
}

inline f32
clamp(f32 min, f32 value, f32 Max)
{
    f32 result = value;

    if(result < min)
    {
        result = min;
    }
    else if(result > Max)
    {
        result = Max;
    }

    return result;
}

inline f32
clamp01(f32 value)
{
    f32 result = clamp(0.0f, value, 1.0f);

    return result;
}

inline f32
clamp01_map_to_range(f32 min, f32 t, f32 max)
{
    f32 result = 0.0f;

    f32 range = max - min;
    if(range != 0.0f)
    {
        result = clamp01((t - min) / range);
    }

    return result;
}

inline f32
clamp_binormal_map_to_range(f32 min, f32 t, f32 max)
{
    f32 result = -1.0f + 2.0f * clamp01_map_to_range(min, t, max);
    return result;
}