// OptiXRenderer testing utils.
// ---------------------------------------------------------------------------
// Copyright (C) 2015-2016, Cogwheel. See AUTHORS.txt for authors
//
// This program is open source and distributed under the New BSD License. See
// LICENSE.txt for more detail.
// ---------------------------------------------------------------------------

#ifndef _OPTIXRENDERERTEST_UTILS_H_
#define _OPTIXRENDERERTEST_UTILS_H_

#include <optixu/optixu_math_namespace.h>

#include <string>

//-----------------------------------------------------------------------------
// Comparison helpers.
//-----------------------------------------------------------------------------

inline bool almost_equal_eps(float lhs, float rhs, float eps) {
    return lhs < rhs + eps && lhs + eps > rhs;
}

#define EXPECT_FLOAT_EQ_EPS(expected, actual, epsilon) EXPECT_PRED3(almost_equal_eps, expected, actual, epsilon)

inline bool equal_float3_eps(optix::float3 lhs, optix::float3 rhs, optix::float3 epsilon) {
    return abs(lhs.x - rhs.x) < epsilon.z && abs(lhs.y - rhs.y) < epsilon.y && abs(lhs.z - rhs.z) < epsilon.z;
}

#define EXPECT_COLOR_EQ_EPS(expected, actual, epsilon) EXPECT_PRED3(equal_float3_eps, expected, actual, epsilon)

// TODO Safely dot the normals and test how far apart they are.
inline bool equal_normal_eps(optix::float3 lhs, optix::float3 rhs, float epsilon) {
    return abs(lhs.x - rhs.x) < epsilon && abs(lhs.y - rhs.y) < epsilon && abs(lhs.z - rhs.z) < epsilon;
}

#define EXPECT_NORMAL_EQ(expected, actual, epsilon) EXPECT_PRED3(equal_normal_eps, expected, actual, epsilon)

//-----------------------------------------------------------------------------
// To string functions.
//-----------------------------------------------------------------------------

inline std::ostream& operator<<(std::ostream& s, const optix::float3 v) {
    return s << "[x: " << v.x << ", y: " << v.y << ", z: " << v.z << "]";
}

#endif // _OPTIXRENDERERTEST_UTILS_H_