//
// Created by mj on 1/11/26.
//

#pragma once

#include <vector>
#include <cstdint>
#include <android/asset_manager.h>

namespace AssetUtils {

std::vector<uint32_t> loadSpirvFromAssets(AAssetManager* assetManager, const char* filename);

} // namespace AssetUtils