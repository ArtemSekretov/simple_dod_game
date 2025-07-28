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

void
setup_projection_matrix(f32 projection_martix[16], v2 game_area)
{
    v2 half_game_area = v2_scale(game_area, 0.5f);

    f32 left = -half_game_area.x;
    f32 right = half_game_area.x;
    f32 bottom = -half_game_area.y;
    f32 top = half_game_area.y;
    f32 near_clip_plane = 0.0f;
    f32 far_clip_plane = 1.0f;

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
    projection_martix[0]  =  a;
    projection_martix[3]  = -a1;
    projection_martix[5]  =  b;
    projection_martix[7]  = -b1;
    projection_martix[10] =  d;
    projection_martix[11] =  e;
    projection_martix[15] =  1;
}