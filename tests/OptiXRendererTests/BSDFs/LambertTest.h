// Test OptiXRenderer's Lambert BSDF.
// ---------------------------------------------------------------------------
// Copyright (C) 2015-2016, Cogwheel. See AUTHORS.txt for authors
//
// This program is open source and distributed under the New BSD License. See
// LICENSE.txt for more detail.
// ---------------------------------------------------------------------------

#ifndef _OPTIXRENDERER_BSDFS_LAMBERT_TEST_H_
#define _OPTIXRENDERER_BSDFS_LAMBERT_TEST_H_

#include <Utils.h>

#include <OptiXRenderer/RNG.h>
#include <OptiXRenderer/Shading/BSDFs/Lambert.h>

#include <gtest/gtest.h>

namespace OptiXRenderer {

GTEST_TEST(Lambert, power_conservation) {
    using namespace optix;

    const unsigned int MAX_SAMPLES = 1024u;
    const float3 tint = make_float3(1.0f, 1.0f, 1.0f);

    float ws[MAX_SAMPLES];
    for (unsigned int i = 0u; i < MAX_SAMPLES; ++i) {
        BSDFSample sample = Shading::BSDFs::Lambert::sample(tint, RNG::sample02(i));
        ws[i] = sample.weight.x * sample.direction.z / sample.PDF; // f * ||cos_theta|| / pdf
    }
    
    float average_w = sort_and_pairwise_summation(ws, ws + MAX_SAMPLES) / float(MAX_SAMPLES);
    EXPECT_TRUE(almost_equal_eps(average_w, 1.0f, 0.0001f));
}

} // NS OptiXRenderer

#endif // _OPTIXRENDERER_BSDFS_LAMBERT_TEST_H_