#include <cmath> // fabs, sqrt

struct float2
{
    float x, y;
};

inline float si_absf(float x)
{
    return ::fabsf(x);
}

inline float si_sqrtf(float x)
{
    return ::sqrtf(x);
}

inline float si_sinf(float x)
{
    return ::sinf(x);
}

inline float si_cosf(float x)
{
    return ::cosf(x);
}

inline float si_atan2(float y, float x)
{
    return ::atan2f(y, x);
}

inline float lerp(float a, float b, float t)
{
    return a * (1.0f - t) + b * t;
}

inline float invlerp(float a, float b, float x)
{
    return (x - a) / (b - a);
}

inline bool iszero(const float2 &v)
{
    bool result;
    result = si_absf(v.x) < 0.0000001f && si_absf(v.y) < 0.0000001f;
    return result;
}

inline float2 operator-(const float2 &v)
{
    float2 result;
    result.x = -v.x;
    result.y = -v.y;
    return result;
}

inline float2 operator+(const float2 &a, const float2 &b)
{
    float2 result;
    result.x = a.x + b.x;
    result.y = a.y + b.y;
    return result;
}

inline float2 operator-(const float2 &a, const float2 &b)
{
    float2 result;
    result.x = a.x - b.x;
    result.y = a.y - b.y;
    return result;
}

inline float2 operator*(float k, const float2 &v)
{
    float2 result;
    result.x = k * v.x;
    result.y = k * v.y;
    return result;
}

inline float2 operator*(const float2 &v, float k)
{
    float2 result;
    result.x = k * v.x;
    result.y = k * v.y;
    return result;
}

inline float2 operator/(const float2 &v, float k)
{
    float2 result;
    result.x = v.x / k;
    result.y = v.y / k;
    return result;
}

inline float len(const float2 &v)
{
    float result = si_sqrtf(v.x * v.x + v.y * v.y);
    return result;
}

inline float dot(const float2 &a, const float2 &b)
{
    float result;
    result = a.x * b.x + a.y * b.y;
    return result;
}

inline float cross(const float2 &a, const float2 &b)
{
    float result;
    result = a.x * b.y - a.y * b.x;
    return result;
}

inline float2 perp(const float2 &v)
{
    float2 result;
    result.x = -v.y;
    result.y =  v.x;
    return result;
}

inline float cos(const float2 &a, const float2 &b)
{
    float result;
    result = dot(a, b);
    result /= len(a);
    result /= len(b);
    return result;
}

inline float sin(const float2 &a, const float2 &b)
{
    float result;
    result = cross(a, b);
    result /= len(a);
    result /= len(b);
    return result;
}

inline float2 lerp(const float2& a, const float2& b, float t)
{
    return a * (1.0f - t) + b * t;
}

inline float si_minf(float a, float b)
{
    if (a < b) return a;
    return b;
}

inline float si_maxf(float a, float b)
{
    if (a > b) return a;
    return b;
}


inline double si_mind(double a, double b)
{
    if (a < b) return a;
    return b;
}

inline double si_maxd(double a, double b)
{
    if (a > b) return a;
    return b;
}


inline float si_clampf(float x, float x0, float x1)
{
    return si_minf(si_maxf(x, x0), x1);
}

inline double si_clampd(double x, double x0, double x1)
{
    return si_mind(si_maxd(x, x0), x1);
}

const float si_pi = 3.1415926535f;
