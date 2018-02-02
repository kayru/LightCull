#pragma once

#include <Rush/GfxCommon.h>

static constexpr Tuple2i   g_viridisTextureSize   = {256, 1};
static constexpr GfxFormat g_viridisTextureFormat = GfxFormat_RGB8_Unorm;
extern const u8            g_viridisTextureData[];
extern const size_t        g_viridisTextureDataSize;

static constexpr Tuple2i   g_lightMarkerTextureSize   = {128, 128};
static constexpr GfxFormat g_lightMarkerTextureFormat = GfxFormat_R8_Unorm;
extern const u8            g_lightMarkerTextureData[];
extern const size_t        g_lightMarkerTextureDataSize;
