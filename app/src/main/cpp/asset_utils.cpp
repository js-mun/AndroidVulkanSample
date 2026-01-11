//
// Created by mj on 1/11/26.
//

#include "asset_utils.h"
#include "Log.h"

namespace AssetUtils {

    std::vector<uint32_t> loadSpirvFromAssets(AAssetManager* assetManager, const char* filename) {
        AAsset* asset = AAssetManager_open(assetManager, filename, AASSET_MODE_BUFFER);

        if (!asset) {
            LOGE("Failed to open asset: %s", filename);
            return {};
        }

        size_t size = AAsset_getLength(asset);
        if (size % 4 != 0) {
            LOGE("SPIR-V file size is not multiple of 4: %s", filename);
        }

        std::vector<uint32_t> buffer(size / 4);
        AAsset_read(asset, buffer.data(), size);
        AAsset_close(asset);

        return buffer;
    }
}